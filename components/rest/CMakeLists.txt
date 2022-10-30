idf_component_register(
    REQUIRES nvs_flash esp_http_server esp_http_client esp_websocket_client app_update fatfs json wpa_supplicant esp_adc_cal
    INCLUDE_DIRS "."
    PRIV_REQUIRES "microtar mfile TinyGPS eventmgr pins eventmgr joltwallet__littlefs"
    EMBED_FILES "../../res/favicon.ico"
    EMBED_TXTFILES "../../res/index.html" "../../res/index-sd.html" "../../res/dist/app-min-min.js" "../../res/dist/app-min-min.css"
    SRCS "../../main/log.cpp" "../../main/utils.cpp" "handlers.cpp" "websocket.cpp" "route.cpp" "flash.cpp" "netjobs.cpp"
)
