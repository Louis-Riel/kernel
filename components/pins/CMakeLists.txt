set(COMPONENT_REQUIRES json )
set(COMPONENT_PRIV_REQUIRES eventmgr esp_adc_cal joltwallet__littlefs )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "analog.cpp" "digital.cpp" "analog.cpp")

register_component()