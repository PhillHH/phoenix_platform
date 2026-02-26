// ==========================================================================
// Test: NVS Storage — CalibrationData persistence roundtrip
// Verifies: IEC 62304 REQ-DATA-001 — Data integrity in non-volatile storage
//
// Uses host NVS stubs (in-memory map) to verify serialization correctness.
// ==========================================================================
#include "test_framework.h"
#include "phoenix/Services/CalibrationService.h"
#include <nvs.h>
#include <cstring>

using namespace phoenix;

static CalibrationData makeTestCalData() {
    CalibrationData cal;
    cal.magic = CalibrationData::MAGIC;
    cal.version = CalibrationData::SERIAL_VERSION;
    cal.id = "TEST-CAL-01";
    cal.analyte = "C-Reactive Protein";
    cal.params.A = 0.05f;
    cal.params.B = 1.8f;
    cal.params.C = 25.0f;
    cal.params.D = 8.5f;
    cal.params.E = 0.85f;
    cal.params.r_squared = 0.994f;
    cal.num_points = 6;
    cal.points[0] = {0.0f, 0.050f, 3, 2.1f};
    cal.points[1] = {5.0f, 0.427f, 3, 3.5f};
    cal.points[2] = {10.0f, 1.223f, 3, 2.8f};
    cal.points[3] = {25.0f, 3.500f, 3, 1.9f};
    cal.points[4] = {50.0f, 6.139f, 3, 2.2f};
    cal.points[5] = {100.0f, 7.553f, 3, 3.1f};
    cal.created_at = 1740000000;
    cal.expires_at = 1742592000;
    cal.lod = 0.5f;
    cal.loq = 1.0f;
    cal.range_low = 0.0f;
    cal.range_high = 200.0f;
    return cal;
}

TEST_SUITE(nvs_blob_roundtrip) {
    nvs_stub::reset();

    CalibrationData original = makeTestCalData();
    nvs_handle_t handle;

    esp_err_t err = nvs_open("test_ns", NVS_READWRITE, &handle);
    ASSERT_EQ(err, ESP_OK);

    err = nvs_set_blob(handle, "cal_crp", &original, sizeof(original));
    ASSERT_EQ(err, ESP_OK);

    err = nvs_commit(handle);
    ASSERT_EQ(err, ESP_OK);

    // Read back
    CalibrationData loaded = {};
    size_t blob_size = sizeof(loaded);
    err = nvs_get_blob(handle, "cal_crp", &loaded, &blob_size);
    ASSERT_EQ(err, ESP_OK);
    ASSERT_EQ(blob_size, sizeof(CalibrationData));

    // Verify all fields
    ASSERT_TRUE(loaded.isValid());
    ASSERT_EQ(loaded.magic, CalibrationData::MAGIC);
    ASSERT_EQ(loaded.version, CalibrationData::SERIAL_VERSION);
    ASSERT_STR_EQ(loaded.id.c_str(), "TEST-CAL-01");
    ASSERT_STR_EQ(loaded.analyte.c_str(), "C-Reactive Protein");
    ASSERT_FLOAT_EQ(loaded.params.A, 0.05f, 0.001f);
    ASSERT_FLOAT_EQ(loaded.params.B, 1.8f, 0.001f);
    ASSERT_FLOAT_EQ(loaded.params.C, 25.0f, 0.01f);
    ASSERT_FLOAT_EQ(loaded.params.D, 8.5f, 0.01f);
    ASSERT_FLOAT_EQ(loaded.params.E, 0.85f, 0.001f);
    ASSERT_FLOAT_EQ(loaded.params.r_squared, 0.994f, 0.001f);
    ASSERT_EQ(loaded.num_points, 6);
    ASSERT_FLOAT_EQ(loaded.points[0].concentration, 0.0f, 0.001f);
    ASSERT_FLOAT_EQ(loaded.points[3].signal, 3.500f, 0.001f);
    ASSERT_EQ(loaded.created_at, 1740000000u);
    ASSERT_EQ(loaded.expires_at, 1742592000u);
    ASSERT_FLOAT_EQ(loaded.lod, 0.5f, 0.01f);
    ASSERT_FLOAT_EQ(loaded.range_high, 200.0f, 0.1f);

    nvs_close(handle);
}

TEST_SUITE(nvs_u32_roundtrip) {
    nvs_stub::reset();
    nvs_handle_t handle;
    nvs_open("test_ns", NVS_READWRITE, &handle);

    uint32_t date = 20260226;
    nvs_set_u32(handle, "cal_date", date);
    nvs_commit(handle);

    uint32_t loaded = 0;
    esp_err_t err = nvs_get_u32(handle, "cal_date", &loaded);
    ASSERT_EQ(err, ESP_OK);
    ASSERT_EQ(loaded, 20260226u);

    nvs_close(handle);
}

TEST_SUITE(nvs_missing_key_error) {
    nvs_stub::reset();
    nvs_handle_t handle;
    nvs_open("test_ns", NVS_READWRITE, &handle);

    uint32_t val = 0;
    esp_err_t err = nvs_get_u32(handle, "nonexistent", &val);
    ASSERT_TRUE(err != ESP_OK);

    CalibrationData cal = {};
    size_t blob_size = sizeof(cal);
    err = nvs_get_blob(handle, "nonexistent", &cal, &blob_size);
    ASSERT_TRUE(err != ESP_OK);

    nvs_close(handle);
}

TEST_SUITE(nvs_corrupt_magic_detected) {
    nvs_stub::reset();
    nvs_handle_t handle;
    nvs_open("test_ns", NVS_READWRITE, &handle);

    CalibrationData original = makeTestCalData();
    original.magic = 0xDEADBEEF;  // Corrupt magic

    nvs_set_blob(handle, "corrupt_cal", &original, sizeof(original));
    nvs_commit(handle);

    CalibrationData loaded = {};
    size_t blob_size = sizeof(loaded);
    nvs_get_blob(handle, "corrupt_cal", &loaded, &blob_size);

    // isValid() should catch corrupt magic
    ASSERT_FALSE(loaded.isValid());

    nvs_close(handle);
}

TEST_SUITE(nvs_multiple_keys) {
    nvs_stub::reset();
    nvs_handle_t handle;
    nvs_open("test_ns", NVS_READWRITE, &handle);

    CalibrationData cal1 = makeTestCalData();
    cal1.id = "CRP-01";

    CalibrationData cal2 = makeTestCalData();
    cal2.id = "IGE-01";
    cal2.params.C = 255.0f;

    nvs_set_blob(handle, "cal_crp", &cal1, sizeof(cal1));
    nvs_set_blob(handle, "cal_ige", &cal2, sizeof(cal2));
    nvs_commit(handle);

    CalibrationData loaded1 = {}, loaded2 = {};
    size_t size1 = sizeof(loaded1), size2 = sizeof(loaded2);
    nvs_get_blob(handle, "cal_crp", &loaded1, &size1);
    nvs_get_blob(handle, "cal_ige", &loaded2, &size2);

    ASSERT_STR_EQ(loaded1.id.c_str(), "CRP-01");
    ASSERT_STR_EQ(loaded2.id.c_str(), "IGE-01");
    ASSERT_FLOAT_EQ(loaded2.params.C, 255.0f, 0.1f);

    nvs_close(handle);
}
