set(COMPONENT_REQUIRES nvs_flash fatfs lwip )
set(COMPONENT_PRIV_REQUIRES rest wifi )

set(COMPONENT_ADD_INCLUDEDIRS "." "../rest")
set(COMPONENT_SRCS "station.cpp" )

register_component()