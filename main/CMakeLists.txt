set(COMPONENT_REQUIRES esp_adc_cal )
set(COMPONENT_PRIV_REQUIRES wifi rest TinyGPS esp_littlefs )

set(COMPONENT_ADD_INCLUDEDIRS "." "../components/TinyGPS" )
set(COMPONENT_SRCS "log.cpp" "Blink.cpp" "utils.cpp" "configmgmt.cpp" )
set(COMPONENT_EMBED_TXTFILES "../res/trip.kml" )

register_component()

# spiffs_create_partition_image(storage ../lfs FLASH_IN_PROJECT)
