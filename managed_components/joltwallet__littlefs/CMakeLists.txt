cmake_minimum_required(VERSION 3.10)

file(GLOB SOURCES src/littlefs/*.c)
list(APPEND SOURCES src/esp_littlefs.c src/littlefs_api.c src/lfs_config.c)
idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS src include
    REQUIRES spi_flash vfs
    PRIV_REQUIRES esptool_py
    )

set_source_files_properties(
    ${SOURCES}
    PROPERTIES COMPILE_FLAGS "-DLFS_CONFIG=lfs_config.h"
)
