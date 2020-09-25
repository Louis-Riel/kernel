set(COMPONENT_REQUIRES nvs_flash)
set(COMPONENT_PRIV_REQUIRES "rest")

set(COMPONENT_ADD_INCLUDEDIRS "." "../rest")
set(COMPONENT_SRCS "station.cpp")

register_component()