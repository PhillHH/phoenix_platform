// ==========================================================================
// FILE: include/phoenix/UI/PhoenixUI.h
// LVGL-based UI Manager — screen navigation, status bar, theme
// ==========================================================================
#pragma once
#include "phoenix/Core/Result.h"
#include "phoenix/Core/MeasurementTypes.h"
#include <lvgl.h>

namespace phoenix {

class MeasurementProxy;

enum class ScreenID : uint8_t {
    HOME, MEASUREMENT, RESULT, CALIBRATION,
    PATIENT, SETTINGS, SYSTEM_MENU
};

class PhoenixUI {
public:
    Result<void> initialize(lv_disp_t* disp);
    void         runTick();   // Call from LVGL tick task
    void         navigateTo(ScreenID screen);

    // Update from Measurement MCU callbacks
    void onMeasurementProgress(uint8_t pct, const char* msg);
    void onMeasurementResult(const MeasurementResult& result);
    void onMeasurementError(ErrorCategory cat, const char* msg);

    void setProxy(MeasurementProxy* proxy) { proxy_ = proxy; }

    lv_obj_t* getScreen() const { return screen_; }

private:
    lv_obj_t*         screen_      = nullptr;
    lv_obj_t*         status_bar_  = nullptr;
    lv_obj_t*         content_     = nullptr;
    MeasurementProxy* proxy_       = nullptr;
    ScreenID          current_     = ScreenID::HOME;

    void createStatusBar();
    void createTheme();

    // Screen builders
    void showHome();
    void showMeasurement();
    void showResult(const MeasurementResult& result);
    void showCalibration();
    void showPatient();
    void showSettings();
    void showSystemMenu();
};

} // namespace phoenix
