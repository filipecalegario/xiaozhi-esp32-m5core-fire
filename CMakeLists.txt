# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "2.1.0")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)
# Disable format warnings as errors for external components (esp_emote_gfx has format issues)
add_compile_options(-Wno-error=format)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiaozhi)

# Apply to all components (including managed ones) after project() is called
idf_build_set_property(COMPILE_OPTIONS "-Wno-error=format" APPEND)

