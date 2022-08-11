set(COMPONENT_REQUIRES nvs_flash fatfs lwip json joltwallet__littlefs )
set(COMPONENT_PRIV_REQUIRES eventmgr )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "mfile.cpp" )

register_component()