set(COMPONENT_REQUIRES esp_adc_cal json pthread esp_rom )
set(COMPONENT_PRIV_REQUIRES wifi rest TinyGPS esp_littlefs eventmgr pins mfile IR bluetooth )

set(COMPONENT_ADD_INCLUDEDIRS "." "../components/TinyGPS" "../components/esp_littlefs/include" )
set(COMPONENT_SRCS "log.cpp" "Blink.cpp" "utils.cpp" "configmgmt.cpp" "mallocdbg.cpp")
set(COMPONENT_EMBED_TXTFILES "../res/defaultconfig.json" )

register_component()

# spiffs_create_partition_image(storage ../lfs FLASH_IN_PROJECT)
