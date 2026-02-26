// ==========================================================================
// Test: Peak Detection Algorithm — 7-point descriptor
// Verifies: IEC 62304 REQ-ALGO-002 — Peak center, boundaries, metrics
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Analysis/Dx365Algorithm.h"

using namespace phoenix;

// ── Synthetic profile generation ─────────────────────────────────────────

static LineProfile makeGaussProfile(float baseline, float depth, float center,
                                    float sigma, uint16_t length = 350) {
    LineProfile p = {};
    p.length = length;
    for (int i = 0; i < p.length; i++) {
        float x = (static_cast<float>(i) - center) / sigma;
        p.data[i] = baseline - depth * expf(-0.5f * x * x);
    }
    return p;
}

static LineProfile makeMultiPeakProfile() {
    LineProfile p = {};
    p.length = 350;
    for (int i = 0; i < p.length; i++) p.data[i] = 165.0f;

    // CL at idx 47, depth 130
    for (int i = 20; i < 75; i++) {
        float x = (static_cast<float>(i) - 47.0f) / 10.0f;
        p.data[i] = 165.0f - 130.0f * expf(-0.5f * x * x);
    }
    // TL1 at idx 138, depth 50
    for (int i = 110; i < 165; i++) {
        float x = (static_cast<float>(i) - 138.0f) / 10.0f;
        p.data[i] = 165.0f - 50.0f * expf(-0.5f * x * x);
    }
    // TL2 at idx 247, depth 15
    for (int i = 220; i < 275; i++) {
        float x = (static_cast<float>(i) - 247.0f) / 10.0f;
        p.data[i] = 165.0f - 15.0f * expf(-0.5f * x * x);
    }
    return p;
}

static void setupIgELines(AssayLine lines[3], uint8_t& num) {
    lines[0] = {"ctrl", true,  47.0f, 44.0f, 0, "ctrl"};
    lines[1] = {"tl1",  false, 138.3f, 45.6f, 2, "IgE"};
    lines[2] = {"tl2",  false, 247.1f, 43.9f, 3, "IgE"};
    num = 3;
}

// ── Tests ────────────────────────────────────────────────────────────────

TEST_SUITE(peak_single_gaussian) {
    // Single Gaussian dip at center=150, sigma=10, depth=100, baseline=165
    LineProfile profile = makeGaussProfile(165.0f, 100.0f, 150.0f, 10.0f);

    AssayLine line = {"test", false, 150.0f, 40.0f, 0, "test"};
    Dx365PeakDetector detector;
    PeakDescriptor peaks[1];
    uint8_t count = 0;

    auto result = detector.detectPeaks(profile, &line, 1, peaks, count);
    ASSERT_RESULT_OK(result);
    ASSERT_EQ(count, 1);
    ASSERT_TRUE(peaks[0].valid);

    // Center should be near 150
    ASSERT_TRUE(abs(static_cast<int>(peaks[0].center_idx) - 150) <= 2);

    // Height should be near 100
    ASSERT_TRUE(peaks[0].height > 80.0f);
    ASSERT_TRUE(peaks[0].height < 120.0f);
}

TEST_SUITE(peak_multi_peak_separation) {
    LineProfile profile = makeMultiPeakProfile();

    AssayLine lines[3];
    uint8_t num_lines = 0;
    setupIgELines(lines, num_lines);

    Dx365PeakDetector detector;
    PeakDescriptor peaks[MAX_LINES];
    uint8_t count = 0;

    auto result = detector.detectPeaks(profile, lines, num_lines, peaks, count);
    ASSERT_RESULT_OK(result);
    ASSERT_EQ(count, 3);

    // CL: strong peak
    ASSERT_TRUE(peaks[0].valid);
    ASSERT_TRUE(peaks[0].height > 50.0f);
    ASSERT_TRUE(abs(static_cast<int>(peaks[0].center_idx) - 47) <= 3);

    // TL1: medium peak
    ASSERT_TRUE(peaks[1].valid);
    ASSERT_TRUE(peaks[1].height > 10.0f);
    ASSERT_TRUE(abs(static_cast<int>(peaks[1].center_idx) - 138) <= 3);

    // TL2: weak peak (may or may not be valid depending on threshold)
    ASSERT_TRUE(abs(static_cast<int>(peaks[2].center_idx) - 247) <= 5);
}

TEST_SUITE(peak_tc_ratio) {
    LineProfile profile = makeMultiPeakProfile();

    AssayLine lines[3];
    uint8_t num_lines = 0;
    setupIgELines(lines, num_lines);

    Dx365PeakDetector detector;
    PeakDescriptor peaks[MAX_LINES];
    uint8_t count = 0;

    detector.detectPeaks(profile, lines, num_lines, peaks, count);

    // T/C ratio
    if (peaks[0].value > 0.1f) {
        float tc1 = Dx365PeakDetector::computeTCRatio(peaks[1].value, peaks[0].value);
        ASSERT_TRUE(tc1 > 0.0f);
        ASSERT_TRUE(tc1 < 1.0f);

        // computeTCRatio with 0 control
        float tc_zero = Dx365PeakDetector::computeTCRatio(10.0f, 0.0f);
        ASSERT_FLOAT_EQ(tc_zero, 0.0f, 0.001f);
    }
}

TEST_SUITE(peak_empty_profile_error) {
    LineProfile empty = {};
    empty.length = 0;

    AssayLine line = {"test", false, 50.0f, 20.0f, 0, "test"};
    Dx365PeakDetector detector;
    PeakDescriptor peaks[1];
    uint8_t count = 0;

    auto result = detector.detectPeaks(empty, &line, 1, peaks, count);
    ASSERT_RESULT_ERR(result);
    ASSERT_EQ(static_cast<int>(result.error().category),
              static_cast<int>(ErrorCategory::INVALID_PARAMETER));
}

TEST_SUITE(peak_flat_profile) {
    // Flat profile — no real peaks
    LineProfile flat = {};
    flat.length = 200;
    for (int i = 0; i < 200; i++) flat.data[i] = 165.0f;

    AssayLine line = {"test", false, 100.0f, 40.0f, 0, "test"};
    Dx365PeakDetector detector;
    PeakDescriptor peaks[1];
    uint8_t count = 0;

    auto result = detector.detectPeaks(flat, &line, 1, peaks, count);
    ASSERT_RESULT_OK(result);
    ASSERT_EQ(count, 1);
    // Height should be ~0 (no peak)
    ASSERT_TRUE(peaks[0].height < 1.0f);
    ASSERT_FALSE(peaks[0].valid);
}

TEST_SUITE(peak_narrow_gaussian) {
    // Very narrow peak (sigma=3)
    LineProfile profile = makeGaussProfile(165.0f, 80.0f, 100.0f, 3.0f, 200);

    AssayLine line = {"test", false, 100.0f, 20.0f, 0, "test"};
    Dx365PeakDetector detector;
    PeakDescriptor peaks[1];
    uint8_t count = 0;

    auto result = detector.detectPeaks(profile, &line, 1, peaks, count);
    ASSERT_RESULT_OK(result);
    ASSERT_TRUE(peaks[0].valid);
    ASSERT_TRUE(peaks[0].height > 40.0f);
}

TEST_SUITE(peak_wide_gaussian) {
    // Very wide peak (sigma=25)
    LineProfile profile = makeGaussProfile(165.0f, 60.0f, 150.0f, 25.0f, 350);

    AssayLine line = {"test", false, 150.0f, 80.0f, 0, "test"};
    Dx365PeakDetector detector;
    PeakDescriptor peaks[1];
    uint8_t count = 0;

    auto result = detector.detectPeaks(profile, &line, 1, peaks, count);
    ASSERT_RESULT_OK(result);
    ASSERT_TRUE(peaks[0].valid);
    ASSERT_TRUE(abs(static_cast<int>(peaks[0].center_idx) - 150) <= 5);
}

TEST_SUITE(peak_noisy_profile) {
    // Gaussian + deterministic pseudo-noise
    LineProfile profile = makeGaussProfile(165.0f, 80.0f, 100.0f, 10.0f, 200);

    // Add deterministic noise (±5 based on index)
    for (int i = 0; i < profile.length; i++) {
        float noise = static_cast<float>((i * 7 + 13) % 11) - 5.0f;
        profile.data[i] += noise;
    }

    AssayLine line = {"test", false, 100.0f, 40.0f, 0, "test"};
    Dx365PeakDetector detector;
    PeakDescriptor peaks[1];
    uint8_t count = 0;

    auto result = detector.detectPeaks(profile, &line, 1, peaks, count);
    ASSERT_RESULT_OK(result);
    ASSERT_TRUE(peaks[0].valid);
    // Center should still be near 100 despite noise
    ASSERT_TRUE(abs(static_cast<int>(peaks[0].center_idx) - 100) <= 5);
}

TEST_SUITE(peak_compute_peak_value) {
    LineProfile profile = makeGaussProfile(165.0f, 100.0f, 100.0f, 10.0f, 200);

    AssayLine line = {"test", false, 100.0f, 40.0f, 0, "test"};
    Dx365PeakDetector detector;
    PeakDescriptor peaks[1];
    uint8_t count = 0;

    detector.detectPeaks(profile, &line, 1, peaks, count);

    float value = Dx365PeakDetector::computePeakValue(profile, peaks[0]);
    ASSERT_TRUE(value > 0.0f);
    ASSERT_FLOAT_EQ(value, peaks[0].value, 0.001f);
}

TEST_SUITE(peak_7point_descriptor_ordering) {
    LineProfile profile = makeMultiPeakProfile();

    AssayLine lines[3];
    uint8_t num_lines = 0;
    setupIgELines(lines, num_lines);

    Dx365PeakDetector detector;
    PeakDescriptor peaks[MAX_LINES];
    uint8_t count = 0;

    detector.detectPeaks(profile, lines, num_lines, peaks, count);

    for (uint8_t i = 0; i < count; i++) {
        const auto& p = peaks[i];
        // 7-point ordering must be monotonic
        ASSERT_TRUE(p.left_expect_idx <= p.left_limit_idx);
        ASSERT_TRUE(p.left_limit_idx <= p.left_peak_idx);
        ASSERT_TRUE(p.left_peak_idx <= p.center_idx);
        ASSERT_TRUE(p.center_idx <= p.right_peak_idx);
        ASSERT_TRUE(p.right_peak_idx <= p.right_limit_idx);
        ASSERT_TRUE(p.right_limit_idx <= p.right_expect_idx);
    }
}
