set(COMPONENT_REQUIRES json bt )
set(COMPONENT_PRIV_REQUIRES eventmgr esp_littlefs )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "bt.cpp" )

register_component()