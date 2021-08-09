set(COMPONENT_REQUIRES nvs_flash fatfs lwip json )
set(COMPONENT_PRIV_REQUIRES rest wifi mfile esp_littlefs rest )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "eventmgr.cpp" "eventhandlerdescriptor.cpp" "eventinterpretor.cpp" "manageddevice.cpp" )

register_component()