set(COMPONENT_REQUIRES eventmgr )
set(COMPONENT_PRIV_REQUIRES json esp_littlefs eventmgr )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "TinyGPS++.cpp")

register_component()