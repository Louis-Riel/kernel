set(COMPONENT_REQUIRES nvs_flash fatfs lwip json  )
set(COMPONENT_PRIV_REQUIRES rest wifi eventmgr joltwallet__littlefs )

set(COMPONENT_ADD_INCLUDEDIRS "." "../rest")
set(COMPONENT_SRCS "station.cpp" )

register_component()