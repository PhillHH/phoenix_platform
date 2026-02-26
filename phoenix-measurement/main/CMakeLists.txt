##############################################################################
# Main Component — Measurement MCU Sources
##############################################################################

set(CORE_SOURCES
    "src/Core/Result.cpp"
    "src/Core/ServiceLocator.cpp"
    "src/Core/SafetyManager.cpp"
    "src/Core/AuditTrail.cpp"
)

set(HAL_SOURCES
    "src/HAL/CameraController_OV2686.cpp"
    "src/HAL/LEDController.cpp"
)

set(ANALYSIS_SOURCES
    "src/Analysis/ImageProcessor.cpp"
    "src/Analysis/CurveCorrection.cpp"
    "src/Analysis/MeasurementPipeline.cpp"
    "src/Analysis/Dx365Algorithm.cpp"
)

set(CALIBRATION_SOURCES
    "src/Calibration/ColorChartCalibration.cpp"
)

set(SERVICES_SOURCES
    "src/Services/CalibrationService.cpp"
    "src/Services/BenchmarkValidator.cpp"
)

set(IPC_SOURCES
    "src/IPC/UartBridge.cpp"
    "src/IPC/CommandProtocol.cpp"
)

set(ALL_SOURCES
    "src/main.cpp"
    ${CORE_SOURCES}
    ${HAL_SOURCES}
    ${ANALYSIS_SOURCES}
    ${CALIBRATION_SOURCES}
    ${SERVICES_SOURCES}
    ${IPC_SOURCES}
)

idf_component_register(
    SRCS ${ALL_SOURCES}
    INCLUDE_DIRS
        "include"
        "include/phoenix"
        "include/phoenix/Core"
        "include/phoenix/HAL"
        "include/phoenix/Analysis"
        "include/phoenix/Services"
        "include/phoenix/Calibration"
        "include/phoenix/IPC"
    REQUIRES
        driver freertos nvs_flash esp_timer
    PRIV_REQUIRES
        esp_pm
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wpedantic
    -Wshadow
    -fno-exceptions -fno-rtti
    -Werror -Wconversion
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    PHOENIX_VERSION="108.0"
    PHOENIX_TARGET_MEASUREMENT_MCU=1
    $<$<BOOL:${PHOENIX_AI_ENABLED}>:PHOENIX_AI_ENABLED=1>
    $<$<BOOL:${PHOENIX_TRF_ENABLED}>:PHOENIX_TRF_ENABLED=1>
    $<$<BOOL:${PHOENIX_BENCHMARK_ENABLED}>:PHOENIX_BENCHMARK_ENABLED=1>
    $<$<BOOL:${PHOENIX_COLOR_CAL_ENABLED}>:PHOENIX_COLOR_CAL_ENABLED=1>
)
