set(COMPONENT_REQUIRES nvs_flash fatfs lwip json )
set(COMPONENT_PRIV_REQUIRES rest wifi eventmgr esp_littlefs )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "digital.cpp" )

register_component()