set(COMPONENT_REQUIRES fatfs esp_adc_cal)
set(COMPONENT_PRIV_REQUIRES "TinyGPS"
                            "wifi")

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "Blink.cpp" "utils.cpp")
set(COMPONENT_EMBED_TXTFILES "../res/trip.kml")

register_component()