set(COMPONENT_REQUIRES json esp_http_server esp_http_client eventmgr )
set(COMPONENT_PRIV_REQUIRES joltwallet__littlefs espressif__esp32-camera )

set(COMPONENT_ADD_INCLUDEDIRS "include")
set(COMPONENT_SRCS "camera.cpp")

register_component()