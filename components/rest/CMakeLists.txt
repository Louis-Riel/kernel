set(COMPONENT_REQUIRES nvs_flash esp_http_server app_update)
set(COMPONENT_PRIV_REQUIRES )

set(COMPONENT_ADD_INCLUDEDIRS ".")
set(COMPONENT_SRCS "rest.cpp")

register_component()

