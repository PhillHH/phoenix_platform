// ==========================================================================
// FILE: src/Services/CalibrationService.cpp
// 5-Parameter Logistic curve fitting for LFA quantitation
// ==========================================================================

#include "phoenix/Services/CalibrationService.h"
#include "phoenix/Core/Logger.h"
#include <esp_timer.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <cstring>
#include <cmath>

namespace phoenix {

static const char* TAG = "CalSvc";

// ─── Workflow ─────────────────────────────────────────────────────────

Result<void> CalibrationService::startCalibrationWorkflow(const char* analyte) {
    workflow_active_ = true;
    workflow_count_  = 0;
    for (auto& pt : workflow_pts_) { pt = {}; }

    active_ = {};
    active_.analyte = analyte;
    active_.created_at = static_cast<uint32_t>(esp_timer_get_time() / 1000000ULL);

    PHOENIX_LOGI(TAG, "Calibration workflow started for: %s", analyte);
    return Ok();
}

Result<void> CalibrationService::addCalibrationPoint(
    float concentration, float signal)
{
    if (!workflow_active_) {
        return Err(ErrorCategory::INVALID_PARAMETER, "No workflow active");
    }
    if (workflow_count_ >= MAX_CAL_POINTS) {
        return Err(ErrorCategory::INVALID_PARAMETER, "Max points reached");
    }

    auto& pt       = workflow_pts_[workflow_count_];
    pt.concentration = concentration;
    pt.signal        = signal;
    pt.replicates    = 1;
    workflow_count_++;

    PHOENIX_LOGI(TAG, "Cal point %u: conc=%.3f sig=%.4f",
             workflow_count_,
             static_cast<double>(concentration),
             static_cast<double>(signal));
    return Ok();
}

Result<FivePLParams> CalibrationService::fitCurve() {
    if (workflow_count_ < 4) {
        return Err<FivePLParams>(ErrorCategory::CALIBRATION_ERROR,
                                 "Need at least 4 points for 5PL");
    }

    auto result = levenbergMarquardt(workflow_pts_, workflow_count_);
    if (result.is_ok()) {
        active_.params     = result.value();
        active_.num_points = workflow_count_;
        memcpy(active_.points, workflow_pts_, sizeof(workflow_pts_));

        PHOENIX_LOGI(TAG, "5PL fit: A=%.3f B=%.3f C=%.3f D=%.3f E=%.3f R²=%.4f",
                 static_cast<double>(active_.params.A),
                 static_cast<double>(active_.params.B),
                 static_cast<double>(active_.params.C),
                 static_cast<double>(active_.params.D),
                 static_cast<double>(active_.params.E),
                 static_cast<double>(active_.params.r_squared));
    }
    return result;
}

Result<void> CalibrationService::validateCalibration() {
    if (!active_.isValid()) {
        return Err(ErrorCategory::CALIBRATION_ERROR, "No valid calibration");
    }

    // Check R²
    if (active_.params.r_squared < 0.95f) {
        return Err(ErrorCategory::CALIBRATION_ERROR, "R² < 0.95, poor fit");
    }

    // Check residuals
    float max_residual = 0.0f;
    for (uint8_t i = 0; i < active_.num_points; ++i) {
        float predicted = active_.params.evaluate(active_.points[i].concentration);
        float residual  = fabsf(predicted - active_.points[i].signal);
        if (residual > max_residual) max_residual = residual;
    }

    // Max residual should be < 10% of range
    float range = fabsf(active_.params.D - active_.params.A);
    if (range > 0.0f && max_residual / range > 0.10f) {
        PHOENIX_LOGW(TAG, "Max residual %.4f exceeds 10%% of range %.4f",
                 static_cast<double>(max_residual),
                 static_cast<double>(range));
        return Err(ErrorCategory::CALIBRATION_ERROR, "Residuals too large");
    }

    PHOENIX_LOGI(TAG, "Calibration validated (R²=%.4f, max_res=%.4f)",
             static_cast<double>(active_.params.r_squared),
             static_cast<double>(max_residual));
    return Ok();
}

Result<void> CalibrationService::saveCalibration(const char* id) {
    if (!active_.isValid()) {
        return Err(ErrorCategory::CALIBRATION_ERROR, "Nothing to save");
    }

    active_.id    = id;
    active_.magic = CalibrationData::MAGIC;

    // Persist to NVS
    nvs_handle_t handle;
    esp_err_t err = nvs_open("cal_store", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "NVS open failed",
                   static_cast<uint32_t>(err));
    }

    err = nvs_set_blob(handle, id, &active_, sizeof(CalibrationData));
    if (err != ESP_OK) {
        nvs_close(handle);
        return Err(ErrorCategory::HARDWARE_FAILURE, "NVS write failed");
    }

    err = nvs_commit(handle);
    nvs_close(handle);

    if (err != ESP_OK) {
        return Err(ErrorCategory::HARDWARE_FAILURE, "NVS commit failed");
    }

    workflow_active_ = false;
    PHOENIX_LOGI(TAG, "Calibration '%s' saved (%u points)", id, active_.num_points);
    return Ok();
}

// ─── Retrieval ────────────────────────────────────────────────────────

Result<CalibrationData> CalibrationService::getActiveCalibration() const {
    if (!active_.isValid()) {
        return Err<CalibrationData>(ErrorCategory::NOT_FOUND,
                                    "No active calibration");
    }
    return Ok(active_);
}

Result<CalibrationData> CalibrationService::loadCalibration(const char* id) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open("cal_store", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return Err<CalibrationData>(ErrorCategory::NOT_FOUND, "NVS open failed");
    }

    CalibrationData cal;
    size_t blob_size = sizeof(CalibrationData);
    err = nvs_get_blob(handle, id, &cal, &blob_size);
    nvs_close(handle);

    if (err != ESP_OK || !cal.isValid()) {
        return Err<CalibrationData>(ErrorCategory::NOT_FOUND,
                                    "Calibration not found or invalid");
    }

    // Check version compatibility
    if (cal.version != CalibrationData::SERIAL_VERSION) {
        return Err<CalibrationData>(ErrorCategory::CALIBRATION_ERROR,
                                    "Incompatible calibration version");
    }

    active_ = cal;
    PHOENIX_LOGI(TAG, "Loaded calibration '%s' (%u points, R²=%.4f)",
             id, cal.num_points,
             static_cast<double>(cal.params.r_squared));
    return Ok(cal);
}

// ─── Apply ────────────────────────────────────────────────────────────

Result<float> CalibrationService::signalToConcentration(float signal) const {
    if (!active_.isValid()) {
        return Err<float>(ErrorCategory::CALIBRATION_ERROR, "No calibration");
    }

    float conc = active_.params.inverse(signal);

    // Range check
    if (conc < active_.range_low || conc > active_.range_high) {
        PHOENIX_LOGW(TAG, "Concentration %.3f outside range [%.3f, %.3f]",
                 static_cast<double>(conc),
                 static_cast<double>(active_.range_low),
                 static_cast<double>(active_.range_high));
    }

    return Ok(conc);
}

Result<float> CalibrationService::concentrationToSignal(float conc) const {
    if (!active_.isValid()) {
        return Err<float>(ErrorCategory::CALIBRATION_ERROR, "No calibration");
    }
    return Ok(active_.params.evaluate(conc));
}

// ─── Levenberg-Marquardt 5PL Fitting ──────────────────────────────────
// Simplified implementation suitable for embedded use (no Eigen needed)

Result<FivePLParams> CalibrationService::levenbergMarquardt(
    const CalibrationPoint* pts, uint8_t n, uint16_t max_iter)
{
    // Initial estimates
    FivePLParams p;

    // Sort by concentration to find min/max signals
    float min_sig = pts[0].signal, max_sig = pts[0].signal;
    float mid_conc = pts[0].concentration;
    for (uint8_t i = 1; i < n; ++i) {
        if (pts[i].signal < min_sig) min_sig = pts[i].signal;
        if (pts[i].signal > max_sig) max_sig = pts[i].signal;
        mid_conc += pts[i].concentration;
    }
    mid_conc /= static_cast<float>(n);

    p.A = min_sig;
    p.D = max_sig;
    p.C = mid_conc;
    p.B = 1.0f;
    p.E = 1.0f;

    float lambda = 0.01f;  // Damping factor

    float prev_residual = computeResidual(p, pts, n);

    for (uint16_t iter = 0; iter < max_iter; ++iter) {
        // Compute Jacobian numerically (5 parameters)
        float J[12][5] = {};  // max 12 points, 5 params
        float r[12]    = {};  // residuals

        const float delta = 1e-4f;

        for (uint8_t i = 0; i < n; ++i) {
            float y_pred = p.evaluate(pts[i].concentration);
            r[i] = pts[i].signal - y_pred;

            // Numerical partial derivatives
            FivePLParams p_tmp;
            float params[5] = {p.A, p.B, p.C, p.D, p.E};

            for (int j = 0; j < 5; ++j) {
                float saved = params[j];
                params[j] += delta;

                p_tmp.A = params[0]; p_tmp.B = params[1];
                p_tmp.C = params[2]; p_tmp.D = params[3];
                p_tmp.E = params[4];

                float y_pert = p_tmp.evaluate(pts[i].concentration);
                J[i][j] = (y_pert - y_pred) / delta;

                params[j] = saved;
            }
        }

        // Solve (J^T J + λI) δ = J^T r  using simple Gauss elimination
        // 5x5 system — small enough for direct solve
        float JTJ[5][5] = {};
        float JTr[5]    = {};

        for (int j = 0; j < 5; ++j) {
            for (int k = 0; k < 5; ++k) {
                for (uint8_t i = 0; i < n; ++i) {
                    JTJ[j][k] += J[i][j] * J[i][k];
                }
            }
            JTJ[j][j] += lambda;  // Damping

            for (uint8_t i = 0; i < n; ++i) {
                JTr[j] += J[i][j] * r[i];
            }
        }

        // Gaussian elimination with partial pivoting
        float aug[5][6] = {};
        for (int i = 0; i < 5; ++i) {
            for (int j = 0; j < 5; ++j) aug[i][j] = JTJ[i][j];
            aug[i][5] = JTr[i];
        }

        for (int col = 0; col < 5; ++col) {
            // Pivot
            int max_row = col;
            for (int row = col + 1; row < 5; ++row) {
                if (fabsf(aug[row][col]) > fabsf(aug[max_row][col])) {
                    max_row = row;
                }
            }
            if (max_row != col) {
                for (int j = 0; j < 6; ++j) {
                    float tmp = aug[col][j];
                    aug[col][j] = aug[max_row][j];
                    aug[max_row][j] = tmp;
                }
            }

            if (fabsf(aug[col][col]) < 1e-12f) continue;

            // Eliminate
            for (int row = col + 1; row < 5; ++row) {
                float factor = aug[row][col] / aug[col][col];
                for (int j = col; j < 6; ++j) {
                    aug[row][j] -= factor * aug[col][j];
                }
            }
        }

        // Back-substitution
        float delta_p[5] = {};
        for (int i = 4; i >= 0; --i) {
            delta_p[i] = aug[i][5];
            for (int j = i + 1; j < 5; ++j) {
                delta_p[i] -= aug[i][j] * delta_p[j];
            }
            if (fabsf(aug[i][i]) > 1e-12f) {
                delta_p[i] /= aug[i][i];
            }
        }

        // Trial update
        FivePLParams p_new;
        p_new.A = p.A + delta_p[0];
        p_new.B = p.B + delta_p[1];
        p_new.C = p.C + delta_p[2];
        p_new.D = p.D + delta_p[3];
        p_new.E = p.E + delta_p[4];

        // Constrain: B > 0, C > 0, E > 0
        if (p_new.B <= 0.0f) p_new.B = 0.01f;
        if (p_new.C <= 0.0f) p_new.C = 0.01f;
        if (p_new.E <= 0.0f) p_new.E = 0.01f;

        float new_residual = computeResidual(p_new, pts, n);

        if (new_residual < prev_residual) {
            p = p_new;
            prev_residual = new_residual;
            lambda *= 0.5f;  // Decrease damping
        } else {
            lambda *= 2.0f;  // Increase damping
        }

        // Convergence check
        if (prev_residual < 1e-8f) break;
    }

    // Compute R²
    float ss_res = 0.0f, ss_tot = 0.0f;
    float mean_sig = 0.0f;
    for (uint8_t i = 0; i < n; ++i) mean_sig += pts[i].signal;
    mean_sig /= static_cast<float>(n);

    for (uint8_t i = 0; i < n; ++i) {
        float pred = p.evaluate(pts[i].concentration);
        float res  = pts[i].signal - pred;
        float tot  = pts[i].signal - mean_sig;
        ss_res += res * res;
        ss_tot += tot * tot;
    }

    p.r_squared = (ss_tot > 1e-10f) ? 1.0f - ss_res / ss_tot : 0.0f;

    return Ok(p);
}

float CalibrationService::computeResidual(
    const FivePLParams& p, const CalibrationPoint* pts, uint8_t n)
{
    float sum = 0.0f;
    for (uint8_t i = 0; i < n; ++i) {
        float diff = pts[i].signal - p.evaluate(pts[i].concentration);
        sum += diff * diff;
    }
    return sum;
}

} // namespace phoenix
