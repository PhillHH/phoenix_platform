// ==========================================================================
// FILE: src/Analysis/CurveCorrection.cpp
// Phoenix v108.0 — Polynomial background correction
// Iterative weighted least-squares polynomial fit to baseline regions
// ==========================================================================
#include "phoenix/Analysis/Interfaces.h"
#include <esp_log.h>
#include <cmath>

namespace phoenix {

static const char* TAG = "CurveCorr";

class PolynomialCorrection : public ICurveCorrection {
public:
    explicit PolynomialCorrection(uint8_t order = 3, uint8_t iterations = 3)
        : order_(order > 5 ? 5 : order), iterations_(iterations) {}

    Result<Profile1D> correct(const Profile1D& raw) override {
        if (raw.length < 20) {
            return Err<Profile1D>(ErrorCategory::INVALID_PARAMETER,
                                   "Profile too short for correction");
        }

        Profile1D corrected = raw;

        // Iterative approach: fit polynomial, subtract, identify baseline,
        // refit on baseline-only points
        float weights[MAX_PROFILE_LENGTH];
        for (uint16_t i = 0; i < raw.length; ++i) weights[i] = 1.0f;

        float coeffs[6] = {}; // max order 5

        for (uint8_t iter = 0; iter < iterations_; ++iter) {
            // Fit weighted polynomial
            fitPolynomial(raw.values, weights, raw.length, coeffs, order_);

            // Compute residuals and update weights
            // Points far above the polynomial are likely peaks → lower weight
            float residual_sum = 0.0f;
            float residual_sq_sum = 0.0f;

            for (uint16_t i = 0; i < raw.length; ++i) {
                float x_norm = static_cast<float>(i) /
                               static_cast<float>(raw.length);
                float baseline_val = evalPolynomial(x_norm, coeffs, order_);
                float residual = raw.values[i] - baseline_val;
                residual_sum += residual;
                residual_sq_sum += residual * residual;
            }

            float mean_res = residual_sum / static_cast<float>(raw.length);
            float std_res  = sqrtf(residual_sq_sum /
                                    static_cast<float>(raw.length) -
                                    mean_res * mean_res);
            if (std_res < 1e-8f) std_res = 1e-8f;

            // Asymmetric weighting: penalize points above baseline (likely peaks)
            for (uint16_t i = 0; i < raw.length; ++i) {
                float x_norm = static_cast<float>(i) /
                               static_cast<float>(raw.length);
                float baseline_val = evalPolynomial(x_norm, coeffs, order_);
                float residual = raw.values[i] - baseline_val;

                if (residual > 0.5f * std_res) {
                    // Above baseline → likely peak, reduce weight
                    weights[i] *= 0.1f;
                } else {
                    // At or below baseline → good baseline point
                    weights[i] = fminf(weights[i] * 1.5f, 1.0f);
                }
            }
        }

        // Final subtraction
        for (uint16_t i = 0; i < raw.length; ++i) {
            float x_norm = static_cast<float>(i) /
                           static_cast<float>(raw.length);
            float baseline = evalPolynomial(x_norm, coeffs, order_);
            corrected.values[i] = raw.values[i] - baseline;
            if (corrected.values[i] < 0.0f) corrected.values[i] = 0.0f;
        }

        ESP_LOGD(TAG, "Polynomial correction (order=%u, iter=%u)",
                 order_, iterations_);
        return Ok(corrected);
    }

private:
    uint8_t order_;
    uint8_t iterations_;

    // Weighted least-squares polynomial fit
    // Solves normal equations: (X^T W X) a = X^T W y
    void fitPolynomial(const float* y, const float* w, uint16_t n,
                        float* coeffs, uint8_t order)
    {
        uint8_t p = order + 1;

        // Build normal equations (p x p system)
        float A[6][7] = {}; // max 6x7 augmented matrix

        for (uint16_t i = 0; i < n; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(n);
            float wi = w[i];

            // Powers of x
            float xpow[12] = {};
            xpow[0] = 1.0f;
            for (uint8_t j = 1; j < 2 * p; ++j) {
                xpow[j] = xpow[j - 1] * x;
            }

            // Accumulate X^T W X
            for (uint8_t r = 0; r < p; ++r) {
                for (uint8_t c = 0; c < p; ++c) {
                    A[r][c] += wi * xpow[r + c];
                }
                // X^T W y (augmented column)
                A[r][p] += wi * xpow[r] * y[i];
            }
        }

        // Gauss elimination with partial pivoting
        for (uint8_t col = 0; col < p; ++col) {
            // Pivot
            uint8_t max_row = col;
            for (uint8_t row = col + 1; row < p; ++row) {
                if (fabsf(A[row][col]) > fabsf(A[max_row][col]))
                    max_row = row;
            }
            if (max_row != col) {
                for (uint8_t j = 0; j <= p; ++j) {
                    float tmp = A[col][j];
                    A[col][j] = A[max_row][j];
                    A[max_row][j] = tmp;
                }
            }
            if (fabsf(A[col][col]) < 1e-12f) continue;

            for (uint8_t row = col + 1; row < p; ++row) {
                float factor = A[row][col] / A[col][col];
                for (uint8_t j = col; j <= p; ++j) {
                    A[row][j] -= factor * A[col][j];
                }
            }
        }

        // Back-substitution
        for (int i = p - 1; i >= 0; --i) {
            coeffs[i] = A[i][p];
            for (uint8_t j = i + 1; j < p; ++j) {
                coeffs[i] -= A[i][j] * coeffs[j];
            }
            if (fabsf(A[i][i]) > 1e-12f)
                coeffs[i] /= A[i][i];
            else
                coeffs[i] = 0.0f;
        }
    }

    float evalPolynomial(float x, const float* coeffs, uint8_t order) {
        float result = coeffs[0];
        float xpow = x;
        for (uint8_t i = 1; i <= order; ++i) {
            result += coeffs[i] * xpow;
            xpow *= x;
        }
        return result;
    }
};

static PolynomialCorrection s_correction(3, 3);
ICurveCorrection* getDefaultCurveCorrection() { return &s_correction; }

} // namespace phoenix
