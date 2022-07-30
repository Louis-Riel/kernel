set(COMPONENT_REQUIRES json )
set(COMPONENT_PRIV_REQUIRES eventmgr esp_littlefs esp_adc_cal )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "analog.cpp" "digital.cpp" "analog.cpp")

register_component()