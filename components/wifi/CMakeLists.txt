set(COMPONENT_REQUIRES nvs_flash fatfs lwip json eventmgr )
set(COMPONENT_PRIV_REQUIRES rest wifi esp_littlefs )

set(COMPONENT_ADD_INCLUDEDIRS "." "../rest")
set(COMPONENT_SRCS "station.cpp" )

register_component()