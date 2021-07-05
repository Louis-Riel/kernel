set(COMPONENT_REQUIRES esp_adc_cal json )
set(COMPONENT_PRIV_REQUIRES wifi rest TinyGPS esp_littlefs eventmgr pins mfile )

set(COMPONENT_ADD_INCLUDEDIRS "." "../components/TinyGPS" )
set(COMPONENT_SRCS "log.cpp" "Blink.cpp" "utils.cpp" "configmgmt.cpp" "mallocdbg.cpp")
set(COMPONENT_EMBED_TXTFILES "../res/trip.kml" "../res/defaultconfig.json" )

register_component()

# spiffs_create_partition_image(storage ../lfs FLASH_IN_PROJECT)
