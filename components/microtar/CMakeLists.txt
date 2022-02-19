set(COMPONENT_REQUIRES json )
set(COMPONENT_PRIV_REQUIRES esp_littlefs )

set(COMPONENT_ADD_INCLUDEDIRS "./src")
set(COMPONENT_SRCS "./src/microtar.c")

register_component()