
idf_component_register(
    REQUIRES nvs_flash esp_http_server esp_http_client app_update fatfs json
    INCLUDE_DIRS "."
    PRIV_REQUIRES "microtar esp_websocket_client"
    EMBED_FILES "../../res/favicon.ico"
    EMBED_TXTFILES "../../res/index.html" "../../res/app.js" "../../res/app.css"
    SRCS "rest.cpp" "../../main/log.cpp" "handlers.cpp" "websocket.cpp" "puller.cpp"
)


