set(COMPONENT_REQUIRES nvs_flash fatfs lwip json )
set(COMPONENT_PRIV_REQUIRES rest wifi mfile esp_littlefs rest pins apa102 )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "eventmgr.cpp" "eventhandlerdescriptor.cpp" "eventinterpretor.cpp" "manageddevice.cpp" "managedthreads.cpp" )

register_component()