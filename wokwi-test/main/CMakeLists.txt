idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver freertos nvs_flash esp_timer
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra)
