set(COMPONENT_REQUIRES nvs_flash fatfs lwip json )
set(COMPONENT_PRIV_REQUIRES rest wifi mfile rest pins apa102 joltwallet__littlefs )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "eventmgr.cpp" "eventhandlerdescriptor.cpp" "eventinterpretor.cpp" "manageddevice.cpp" "managedthreads.cpp" )

register_component()