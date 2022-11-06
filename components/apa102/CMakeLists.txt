set(COMPONENT_REQUIRES json )
set(COMPONENT_PRIV_REQUIRES eventmgr joltwallet__littlefs )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "apa102.cpp")

register_component()