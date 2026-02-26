// ==========================================================================
// FILE: src/Analysis/ImageProcessor.cpp
// Default profile extractor — averages ROI rows into 1D signal
// ==========================================================================
#include "phoenix/Analysis/Interfaces.h"
#include <esp_log.h>
#include <cmath>

namespace phoenix {

static const char* TAG = "ImageProc";

class DefaultProfileExtractor : public IProfileExtractor {
public:
    Result<Profile1D> extractProfile(
        const ImageBuffer& image, const ROI& roi) override
    {
        if (!image.data || image.width == 0 || image.height == 0) {
            return Err<Profile1D>(ErrorCategory::INVALID_PARAMETER,
                                  "Invalid image buffer");
        }

        // Clamp ROI to image bounds
        uint16_t x0 = roi.x;
        uint16_t y0 = roi.y;
        uint16_t x1 = static_cast<uint16_t>(
            (roi.x + roi.width  < image.width)  ? roi.x + roi.width  : image.width);
        uint16_t y1 = static_cast<uint16_t>(
            (roi.y + roi.height < image.height) ? roi.y + roi.height : image.height);

        if (x1 <= x0 || y1 <= y0) {
            return Err<Profile1D>(ErrorCategory::INVALID_PARAMETER,
                                  "ROI has zero area");
        }

        uint16_t profile_len = static_cast<uint16_t>(x1 - x0);
        if (profile_len > MAX_PROFILE_LENGTH) {
            profile_len = MAX_PROFILE_LENGTH;
        }

        Profile1D profile;
        profile.length  = profile_len;
        profile.x_start = 0.0f;
        profile.x_step  = 1.0f / static_cast<float>(profile_len);

        uint16_t row_count = static_cast<uint16_t>(y1 - y0);

        // Average pixel values vertically across ROI height
        for (uint16_t x = 0; x < profile_len; ++x) {
            float sum = 0.0f;
            for (uint16_t y = y0; y < y1; ++y) {
                sum += static_cast<float>(
                    image.data[y * image.width + (x + x0)]);
            }
            // Normalize to 0.0 - 1.0 range
            profile.values[x] = sum / (static_cast<float>(row_count) * 255.0f);
        }

        ESP_LOGD(TAG, "Profile extracted: %u samples from ROI (%u,%u)-(%u,%u)",
                 profile_len, x0, y0, x1, y1);
        return Ok(profile);
    }
};

// Global instance (stateless, safe as singleton)
static DefaultProfileExtractor s_default_extractor;

IProfileExtractor* getDefaultProfileExtractor() {
    return &s_default_extractor;
}

} // namespace phoenix


// ==========================================================================
// FILE: src/Analysis/BaselineEstimator.cpp
// Median-based rolling baseline estimation
// ==========================================================================
namespace phoenix {

class MedianBaselineEstimator : public IBaselineEstimator {
public:
    explicit MedianBaselineEstimator(uint16_t window = 51)
        : window_(window | 1) {}  // Ensure odd window

    Result<Profile1D> estimateBaseline(const Profile1D& raw) override {
        if (raw.length < window_) {
            return Err<Profile1D>(ErrorCategory::INVALID_PARAMETER,
                                  "Profile too short for baseline");
        }

        Profile1D baseline;
        baseline.length  = raw.length;
        baseline.x_start = raw.x_start;
        baseline.x_step  = raw.x_step;

        uint16_t half = window_ / 2;

        for (uint16_t i = 0; i < raw.length; ++i) {
            // Collect window values
            float window_vals[101] = {};  // max window 101
            uint16_t count = 0;

            uint16_t start = (i >= half) ? static_cast<uint16_t>(i - half) : 0;
            uint16_t end   = static_cast<uint16_t>(
                (i + half < raw.length) ? i + half + 1 : raw.length);

            for (uint16_t j = start; j < end && count < 101; ++j) {
                window_vals[count++] = raw.values[j];
            }

            // Simple selection sort for median (small window, acceptable)
            for (uint16_t a = 0; a < count - 1; ++a) {
                for (uint16_t b = static_cast<uint16_t>(a + 1); b < count; ++b) {
                    if (window_vals[b] < window_vals[a]) {
                        float tmp = window_vals[a];
                        window_vals[a] = window_vals[b];
                        window_vals[b] = tmp;
                    }
                }
            }
            baseline.values[i] = window_vals[count / 2];
        }

        return Ok(baseline);
    }

    Result<Profile1D> subtractBaseline(
        const Profile1D& raw, const Profile1D& baseline) override
    {
        if (raw.length != baseline.length) {
            return Err<Profile1D>(ErrorCategory::INVALID_PARAMETER,
                                  "Length mismatch");
        }

        Profile1D corrected;
        corrected.length  = raw.length;
        corrected.x_start = raw.x_start;
        corrected.x_step  = raw.x_step;

        for (uint16_t i = 0; i < raw.length; ++i) {
            corrected.values[i] = raw.values[i] - baseline.values[i];
            if (corrected.values[i] < 0.0f) corrected.values[i] = 0.0f;
        }

        return Ok(corrected);
    }

private:
    uint16_t window_;
};

static MedianBaselineEstimator s_default_baseline(51);

IBaselineEstimator* getDefaultBaselineEstimator() {
    return &s_default_baseline;
}

} // namespace phoenix


// ==========================================================================
// FILE: src/Analysis/PeakFinder.cpp
// Simple peak detection with SNR calculation
// ==========================================================================
namespace phoenix {

class SimplePeakFinder : public IPeakFinder {
public:
    Result<PeakResult> findPeaks(
        const Profile1D& corrected,
        float min_height,
        float min_distance) override
    {
        if (corrected.length < 5) {
            return Err<PeakResult>(ErrorCategory::INVALID_PARAMETER,
                                   "Profile too short");
        }

        PeakResult result;

        // Estimate noise floor (std dev of first 10%)
        uint16_t noise_end = corrected.length / 10;
        if (noise_end < 3) noise_end = 3;

        float noise_mean = 0.0f;
        for (uint16_t i = 0; i < noise_end; ++i) {
            noise_mean += corrected.values[i];
        }
        noise_mean /= static_cast<float>(noise_end);

        float noise_var = 0.0f;
        for (uint16_t i = 0; i < noise_end; ++i) {
            float d = corrected.values[i] - noise_mean;
            noise_var += d * d;
        }
        result.noise_floor  = sqrtf(noise_var / static_cast<float>(noise_end));
        result.baseline_mean = noise_mean;

        if (result.noise_floor < 1e-6f) {
            result.noise_floor = 1e-6f;  // Prevent division by zero
        }

        // Find local maxima
        uint16_t min_dist_samples = static_cast<uint16_t>(
            min_distance / corrected.x_step);
        if (min_dist_samples < 3) min_dist_samples = 3;

        for (uint16_t i = 2; i < corrected.length - 2; ++i) {
            float val = corrected.values[i];

            // Must be above minimum height
            if (val < min_height) continue;

            // Must be local maximum (5-point check)
            if (val <= corrected.values[i - 1] ||
                val <= corrected.values[i + 1] ||
                val <= corrected.values[i - 2] ||
                val <= corrected.values[i + 2]) {
                continue;
            }

            // Check distance from last peak
            if (result.count > 0) {
                float last_pos = result.peaks[result.count - 1].position;
                float this_pos = corrected.x_start +
                    static_cast<float>(i) * corrected.x_step;
                if ((this_pos - last_pos) < min_distance) {
                    // Keep the higher peak
                    if (val > result.peaks[result.count - 1].height) {
                        result.count--;  // Replace last
                    } else {
                        continue;  // Skip this one
                    }
                }
            }

            if (result.count >= MAX_PEAKS) break;

            Peak& p   = result.peaks[result.count];
            p.position = corrected.x_start +
                static_cast<float>(i) * corrected.x_step;
            p.height   = val;
            p.snr      = val / result.noise_floor;

            // Estimate FWHM
            float half_max = val / 2.0f;
            uint16_t left = i, right = i;
            while (left > 0 && corrected.values[left] > half_max) left--;
            while (right < corrected.length - 1 &&
                   corrected.values[right] > half_max) right++;
            p.width = static_cast<float>(right - left) * corrected.x_step;

            // Integrate area (trapezoidal, within FWHM region)
            p.area = 0.0f;
            for (uint16_t j = left; j < right; ++j) {
                p.area += (corrected.values[j] + corrected.values[j + 1]) *
                          0.5f * corrected.x_step;
            }

            result.count++;
        }

        ESP_LOGI("PeakFind", "Found %u peaks (noise=%.4f)",
                 result.count,
                 static_cast<double>(result.noise_floor));

        return Ok(result);
    }
};

static SimplePeakFinder s_default_peak_finder;

IPeakFinder* getDefaultPeakFinder() {
    return &s_default_peak_finder;
}

} // namespace phoenix
