set(HAL_SOURCES
    "src/HAL/DisplayDriver_ST7701S.cpp"
    "src/HAL/TouchDriver_CTP.cpp"
)

set(ALL_SOURCES
    "src/main.cpp"
    "src/Core/Result.cpp"
    "src/Core/ServiceLocator.cpp"
    ${HAL_SOURCES}
    "src/UI/PhoenixUI.cpp"
    "src/UI/CircularMenu.cpp"
    "src/UI/Accessibility.cpp"
    "src/UI/Screens/HomeScreen.cpp"
    "src/UI/Screens/MeasurementScreen.cpp"
    "src/UI/Screens/ResultScreen.cpp"
    "src/UI/Screens/CalibrationScreen.cpp"
    "src/UI/Screens/PatientScreen.cpp"
    "src/UI/Screens/SettingsScreen.cpp"
    "src/Connectivity/WiFiManager.cpp"
    "src/Connectivity/HttpLimsClient.cpp"
    "src/IPC/UartBridge.cpp"
    "src/IPC/MeasurementProxy.cpp"
    "src/Services/PatientManager.cpp"
    "src/Services/ResultStorage.cpp"
    "src/Services/LocalizationManager.cpp"
    "src/Services/DemoMeasurementEngine.cpp"
    "src/UI/Screens/TestSelectionScreen.cpp"
)
idf_component_register(
    SRCS ${ALL_SOURCES}
    INCLUDE_DIRS "include" "include/phoenix"
    REQUIRES driver freertos nvs_flash esp_wifi esp_http_client esp_timer lvgl esp_lcd
    PRIV_REQUIRES esp_pm mbedtls
)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    PHOENIX_VERSION="108.0" PHOENIX_TARGET_UI_MCU=1)
