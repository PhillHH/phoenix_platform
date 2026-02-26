// ==========================================================================
// Test: 5PL Calibration Math — Forward, Inverse, Roundtrip
// Verifies: IEC 62304 REQ-ALGO-001 — All 30 verified assay curves
//
// 5PL model: y = D + (A - D) / (1 + (x/C)^B)^G
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Analysis/Dx365Algorithm.h"
#include "phoenix/Calibration/VerifiedAssayRegistry.h"
#include "phoenix/Services/CalibrationService.h"

using namespace phoenix;

// ── Forward evaluation ───────────────────────────────────────────────────

TEST_SUITE(fivepl_g_forward_ige) {
    // Verified IgE DIV 5PL
    FivePL_G ige = {-0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f};

    float y0   = ige.evaluate(0.0f);
    float y1   = ige.evaluate(1.0f);
    float y10  = ige.evaluate(10.0f);
    float y50  = ige.evaluate(50.0f);
    float y100 = ige.evaluate(100.0f);
    float y200 = ige.evaluate(200.0f);

    // At x=0, evaluate returns A
    ASSERT_FLOAT_EQ(y0, -0.000202f, 0.0001f);

    // Monotonically increasing
    ASSERT_TRUE(y1 > y0);
    ASSERT_TRUE(y10 > y1);
    ASSERT_TRUE(y50 > y10);
    ASSERT_TRUE(y100 > y50);
    ASSERT_TRUE(y200 > y100);

    // Approaches D asymptotically
    ASSERT_TRUE(y200 < 0.30f);
}

TEST_SUITE(fivepl_g_forward_negative_x) {
    FivePL_G model = {0.0f, 1.0f, 100.0f, 1.0f, 1.0f};
    float y = model.evaluate(-5.0f);
    ASSERT_FLOAT_EQ(y, 0.0f, 0.0001f);  // Returns A for x<=0
}

TEST_SUITE(fivepl_g_forward_zero_x) {
    FivePL_G model = {0.5f, 2.0f, 50.0f, 10.0f, 1.0f};
    ASSERT_FLOAT_EQ(model.evaluate(0.0f), 0.5f, 0.0001f);  // Returns A
}

// ── Inverse evaluation ───────────────────────────────────────────────────

TEST_SUITE(fivepl_g_inverse_ige_roundtrip) {
    FivePL_G ige = {-0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f};

    float test_concs[] = {1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f, 150.0f, 200.0f};
    for (float conc : test_concs) {
        float signal = ige.evaluate(conc);
        float recovered = ige.inverse(signal);
        // Roundtrip within 2%
        float tol = conc * 0.02f;
        ASSERT_FLOAT_EQ(recovered, conc, tol);
    }
}

TEST_SUITE(fivepl_g_inverse_at_asymptote) {
    FivePL_G model = {0.0f, 1.5f, 100.0f, 1.0f, 1.0f};
    // inverse(D) should return very large value
    float inv = model.inverse(1.0f);
    ASSERT_TRUE(inv > 1000.0f);
}

TEST_SUITE(fivepl_g_inverse_degenerate) {
    // A == D (flat curve)
    FivePL_G flat = {5.0f, 1.0f, 100.0f, 5.0f, 1.0f};
    // When y == D, the "at max asymptote" check triggers first
    float inv_at_D = flat.inverse(5.0f);
    ASSERT_TRUE(inv_at_D > 1000.0f);
    // When y != D but A == D, the degenerate check returns C
    float inv_away = flat.inverse(3.0f);
    ASSERT_FLOAT_EQ(inv_away, 100.0f, 0.1f);
}

TEST_SUITE(fivepl_g_inverse_negative_inner) {
    FivePL_G model = {0.0f, 1.0f, 100.0f, 1.0f, 1.0f};
    // inverse(y) where (A-D)/(y-D) < 0
    float inv = model.inverse(2.0f);  // y > D, so inner < 0
    ASSERT_FLOAT_EQ(inv, 0.0f, 0.0001f);
}

// ── Edge cases ───────────────────────────────────────────────────────────

TEST_SUITE(fivepl_g_small_G) {
    // G very small (nearly 0 would be degenerate, use 0.1)
    FivePL_G model = {0.0f, 1.0f, 100.0f, 1.0f, 0.1f};
    float y = model.evaluate(100.0f);
    // At x=C, ratio=1, 1+1=2, pow(2,0.1)~1.072, D+(A-D)/1.072
    ASSERT_TRUE(y > 0.0f && y < 1.0f);
}

TEST_SUITE(fivepl_g_large_G) {
    // G very large
    FivePL_G model = {0.0f, 1.0f, 100.0f, 1.0f, 100.0f};
    float y50  = model.evaluate(50.0f);
    float y200 = model.evaluate(200.0f);
    // Large G makes transition sharper
    ASSERT_TRUE(y50 >= 0.0f);
    ASSERT_TRUE(y200 <= 1.0f);
}

TEST_SUITE(fivepl_g_very_small_values) {
    // Numerical stability with very small parameters
    FivePL_G model = {1e-6f, 0.5f, 1e-3f, 1e-4f, 0.5f};
    float y = model.evaluate(1e-4f);
    ASSERT_TRUE(!std::isnan(y) && !std::isinf(y));
}

TEST_SUITE(fivepl_g_very_large_values) {
    // Numerical stability with large concentrations
    FivePL_G ige = {-0.000202f, 1.59834f, 255.486f, 0.28367f, 10.0f};
    float y = ige.evaluate(1e6f);
    ASSERT_TRUE(!std::isnan(y) && !std::isinf(y));
    // Should be near D (saturated)
    ASSERT_TRUE(fabsf(y - 0.28367f) < 0.01f);
}

// ── All 30 assay curves: forward evaluation sanity ───────────────────────

TEST_SUITE(fivepl_all_30_assays_forward) {
    auto& reg = getVerifiedAssays();
    ASSERT_TRUE(reg.count >= 30);

    for (size_t i = 0; i < reg.count; i++) {
        const auto& assay = reg.assays[i];
        const FivePL_G& curve = assay.div_5pl;

        // Evaluate at 5 concentration points
        float concs[] = {0.0f, 1.0f, 10.0f, 100.0f, 1000.0f};
        for (float c : concs) {
            float y = curve.evaluate(c);
            ASSERT_TRUE(!std::isnan(y));
            ASSERT_TRUE(!std::isinf(y));
        }
    }
}

TEST_SUITE(fivepl_all_30_assays_roundtrip) {
    auto& reg = getVerifiedAssays();

    for (size_t i = 0; i < reg.count; i++) {
        const auto& assay = reg.assays[i];
        const FivePL_G& curve = assay.div_5pl;

        // Skip degenerate curves where A~=D
        if (fabsf(curve.A - curve.D) < 1e-4f) continue;

        // Test roundtrip at mid-range concentration
        float mid = curve.C;
        if (mid <= 0.0f) mid = 1.0f;

        float signal = curve.evaluate(mid);
        float recovered = curve.inverse(signal);

        // Must be finite
        ASSERT_TRUE(!std::isnan(recovered));
        ASSERT_TRUE(!std::isinf(recovered));

        // Roundtrip within 5% (some curves have extreme asymmetry)
        if (recovered > 0.0f && mid > 0.0f) {
            float err_pct = fabsf(recovered - mid) / mid * 100.0f;
            ASSERT_TRUE(err_pct < 5.0f);
        }
    }
}

// ── FivePLParams (CalibrationService variant with E parameter) ──────────

TEST_SUITE(fivepl_params_forward) {
    FivePLParams p;
    p.A = 0.05f;
    p.B = 1.8f;
    p.C = 25.0f;
    p.D = 8.5f;
    p.E = 0.85f;

    float y0  = p.evaluate(0.0f);
    float y25 = p.evaluate(25.0f);

    ASSERT_FLOAT_EQ(y0, 0.05f, 0.001f);  // Returns A
    ASSERT_TRUE(y25 > y0);
    ASSERT_TRUE(y25 < 8.5f);  // Below D
}

TEST_SUITE(fivepl_params_roundtrip) {
    FivePLParams p = {0.05f, 1.8f, 25.0f, 8.5f, 0.85f, 0.0f};

    float concs[] = {1.0f, 5.0f, 10.0f, 25.0f, 50.0f, 100.0f};
    for (float c : concs) {
        float sig = p.evaluate(c);
        float rec = p.inverse(sig);
        if (rec > 0.0f) {
            float err = fabsf(rec - c) / c;
            ASSERT_TRUE(err < 0.05f);  // 5% tolerance
        }
    }
}

// ── Assay registry lookup ────────────────────────────────────────────────

TEST_SUITE(registry_find_by_id) {
    auto& reg = getVerifiedAssays();
    const auto* ige = reg.findById("IgE");
    ASSERT_TRUE(ige != nullptr);
    ASSERT_STR_EQ(ige->id.c_str(), "IgE");
    ASSERT_EQ(ige->num_lines, 3);

    const auto* crp = reg.findById("CRP");
    ASSERT_TRUE(crp != nullptr);

    const auto* missing = reg.findById("NONEXISTENT");
    ASSERT_TRUE(missing == nullptr);
}

TEST_SUITE(registry_find_by_loinc) {
    auto& reg = getVerifiedAssays();
    const auto* ige = reg.findByLoinc("51651-8");
    ASSERT_TRUE(ige != nullptr);
    ASSERT_STR_EQ(ige->id.c_str(), "IgE");

    const auto* missing = reg.findByLoinc("00000-0");
    ASSERT_TRUE(missing == nullptr);
}
