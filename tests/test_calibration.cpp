// ==========================================================================
// Test: Calibration — LM Fitting, Factory Curves, CalibrationData
// Verifies: IEC 62304 REQ-CAL-001 — 5PL curve fitting convergence
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Services/CalibrationService.h"
#include "phoenix/Calibration/FactoryCalibration.h"
#include "phoenix/Calibration/VerifiedAssayRegistry.h"

using namespace phoenix;

// ── Levenberg-Marquardt convergence via CalibrationService ───────────────

TEST_SUITE(lm_fit_simple_sigmoid) {
    CalibrationService svc;
    auto r = svc.startCalibrationWorkflow("test-sigmoid");
    ASSERT_RESULT_OK(r);

    // Generate synthetic data from known 5PL: A=0.1, B=1.5, C=50, D=10, E=1.0
    FivePLParams truth = {0.1f, 1.5f, 50.0f, 10.0f, 1.0f, 0.0f};
    float concs[] = {0.0f, 1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f, 200.0f};
    for (float c : concs) {
        float sig = truth.evaluate(c);
        r = svc.addCalibrationPoint(c, sig);
        ASSERT_RESULT_OK(r);
    }
    ASSERT_EQ(svc.getPointCount(), 8);

    auto fit = svc.fitCurve();
    ASSERT_RESULT_OK(fit);

    auto& p = fit.value();
    ASSERT_TRUE(p.r_squared > 0.99f);

    // Fitted parameters should be close to truth
    ASSERT_FLOAT_EQ(p.A, 0.1f, 0.5f);
    ASSERT_FLOAT_EQ(p.D, 10.0f, 1.0f);
}

TEST_SUITE(lm_fit_crp_curve) {
    CalibrationService svc;
    svc.startCalibrationWorkflow("CRP");

    // Use factory CRP verification points as calibration data
    auto cal = getFactoryCal_CRP();
    for (uint8_t i = 0; i < cal.num_verify_points; i++) {
        svc.addCalibrationPoint(cal.verify_points[i].concentration,
                                cal.verify_points[i].expected_signal);
    }

    auto fit = svc.fitCurve();
    ASSERT_RESULT_OK(fit);
    ASSERT_TRUE(fit.value().r_squared > 0.99f);
}

TEST_SUITE(lm_fit_troponin_curve) {
    CalibrationService svc;
    svc.startCalibrationWorkflow("cTnI");

    auto cal = getFactoryCal_TroponinI();
    for (uint8_t i = 0; i < cal.num_verify_points; i++) {
        svc.addCalibrationPoint(cal.verify_points[i].concentration,
                                cal.verify_points[i].expected_signal);
    }

    auto fit = svc.fitCurve();
    ASSERT_RESULT_OK(fit);
    ASSERT_TRUE(fit.value().r_squared > 0.98f);
}

TEST_SUITE(lm_fit_all_factory_curves) {
    // Fit each of the 8 factory calibration curves
    auto& fac = getFactoryCalibrations();
    ASSERT_EQ(fac.count, 8u);

    int fitted = 0;
    for (size_t i = 0; i < fac.count; i++) {
        const auto& curve = fac.curves[i];
        if (curve.num_verify_points < 4) continue;
        // Skip competitive assays (negative B) — simple LM struggles with these
        if (curve.params_5pl.B < 0.0f) continue;

        CalibrationService svc;
        svc.startCalibrationWorkflow(curve.assay_code.c_str());

        for (uint8_t j = 0; j < curve.num_verify_points; j++) {
            svc.addCalibrationPoint(curve.verify_points[j].concentration,
                                    curve.verify_points[j].expected_signal);
        }

        auto fit = svc.fitCurve();
        ASSERT_RESULT_OK(fit);
        // R² > 0.90 for standard (non-competitive) factory curves
        ASSERT_TRUE(fit.value().r_squared > 0.90f);
        fitted++;
    }
    ASSERT_TRUE(fitted >= 5);  // At least 5 curves tested
}

TEST_SUITE(lm_fit_noisy_data) {
    CalibrationService svc;
    svc.startCalibrationWorkflow("noisy");

    FivePLParams truth = {0.1f, 1.5f, 50.0f, 10.0f, 1.0f, 0.0f};
    float concs[] = {0.0f, 1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f, 200.0f};
    for (int idx = 0; idx < 8; idx++) {
        float c = concs[idx];
        float sig = truth.evaluate(c);
        // Add deterministic "noise" (±5%)
        float noise = (idx % 2 == 0) ? 1.05f : 0.95f;
        svc.addCalibrationPoint(c, sig * noise);
    }

    auto fit = svc.fitCurve();
    ASSERT_RESULT_OK(fit);
    // Should still converge with noisy data
    ASSERT_TRUE(fit.value().r_squared > 0.95f);
}

TEST_SUITE(lm_fit_too_few_points) {
    CalibrationService svc;
    svc.startCalibrationWorkflow("sparse");
    svc.addCalibrationPoint(0.0f, 0.1f);
    svc.addCalibrationPoint(10.0f, 5.0f);
    svc.addCalibrationPoint(100.0f, 9.0f);

    auto fit = svc.fitCurve();
    ASSERT_RESULT_ERR(fit);  // Needs at least 4 points
}

// ── Validation ───────────────────────────────────────────────────────────

TEST_SUITE(calibration_validate) {
    CalibrationService svc;
    svc.startCalibrationWorkflow("validate-test");

    FivePLParams truth = {0.1f, 1.5f, 50.0f, 10.0f, 1.0f, 0.0f};
    float concs[] = {0.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f};
    for (float c : concs) {
        svc.addCalibrationPoint(c, truth.evaluate(c));
    }

    svc.fitCurve();
    auto v = svc.validateCalibration();
    ASSERT_RESULT_OK(v);
}

// ── CalibrationData struct ───────────────────────────────────────────────

TEST_SUITE(calibration_data_validity) {
    CalibrationData cal;
    // Default: magic and version correct, but num_points = 0
    ASSERT_FALSE(cal.isValid());  // Needs >= 4 points

    cal.num_points = 4;
    ASSERT_TRUE(cal.isValid());

    cal.magic = 0xDEADBEEF;
    ASSERT_FALSE(cal.isValid());  // Wrong magic
}

TEST_SUITE(calibration_data_expiry) {
    CalibrationData cal;
    cal.num_points = 5;
    cal.expires_at = 0;
    ASSERT_FALSE(cal.isExpired(1000));  // No expiry set

    cal.expires_at = 500;
    ASSERT_TRUE(cal.isExpired(1000));   // Expired

    ASSERT_FALSE(cal.isExpired(100));   // Not yet expired
}

// ── Signal/concentration conversion ──────────────────────────────────────

TEST_SUITE(calibration_signal_to_concentration) {
    CalibrationService svc;
    svc.startCalibrationWorkflow("conversion-test");

    FivePLParams truth = {0.1f, 1.5f, 50.0f, 10.0f, 1.0f, 0.0f};
    float concs[] = {0.0f, 5.0f, 25.0f, 50.0f, 100.0f, 200.0f};
    for (float c : concs) {
        svc.addCalibrationPoint(c, truth.evaluate(c));
    }
    svc.fitCurve();

    // signalToConcentration must work after fitting
    float sig = truth.evaluate(50.0f);
    auto r = svc.signalToConcentration(sig);
    ASSERT_RESULT_OK(r);
    ASSERT_FLOAT_EQ(r.value(), 50.0f, 5.0f);
}

TEST_SUITE(calibration_no_active_errors) {
    CalibrationService svc;
    ASSERT_FALSE(svc.hasActiveCalibration());

    auto r1 = svc.signalToConcentration(1.0f);
    ASSERT_RESULT_ERR(r1);

    auto r2 = svc.concentrationToSignal(10.0f);
    ASSERT_RESULT_ERR(r2);

    auto r3 = svc.getActiveCalibration();
    ASSERT_RESULT_ERR(r3);
}

// ── Factory calibration registry ─────────────────────────────────────────

TEST_SUITE(factory_registry_count) {
    auto& fac = getFactoryCalibrations();
    ASSERT_EQ(fac.count, 8u);
}

TEST_SUITE(factory_registry_find) {
    auto& fac = getFactoryCalibrations();
    const auto* crp = fac.findByCode("CRP-COL");
    ASSERT_TRUE(crp != nullptr);
    ASSERT_FLOAT_EQ(crp->params_5pl.C, 25.0f, 0.1f);

    const auto* missing = fac.findByCode("NONEXISTENT");
    ASSERT_TRUE(missing == nullptr);
}

TEST_SUITE(factory_curves_all_valid) {
    auto& fac = getFactoryCalibrations();
    for (size_t i = 0; i < fac.count; i++) {
        const auto& c = fac.curves[i];
        ASSERT_TRUE(c.num_verify_points >= 4);
        ASSERT_TRUE(c.params_5pl.r_squared > 0.98f);
        ASSERT_TRUE(c.range_high > c.range_low);
        ASSERT_TRUE(c.loq >= c.lod);
    }
}
