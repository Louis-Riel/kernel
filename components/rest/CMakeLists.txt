
idf_component_register(
    REQUIRES nvs_flash esp_http_server esp_http_client app_update fatfs json
    INCLUDE_DIRS "."
    PRIV_REQUIRES "microtar"
    EMBED_FILES "../../res/favicon.ico"
    EMBED_TXTFILES "../../res/index.html" "../../res/file_table.html" "../../res/settings.html"
    SRCS "rest.cpp" "handlers.cpp"
)


