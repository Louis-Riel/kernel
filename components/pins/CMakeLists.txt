set(COMPONENT_REQUIRES nvs_flash fatfs lwip json )
set(COMPONENT_PRIV_REQUIRES rest wifi eventmgr )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "digital.cpp" )

register_component()