#ifndef __route_h
#define __route_h

#include "esp_http_client.h"
static char mounts[3][6] = {"/kml","/sent","/"};
esp_err_t trips_handler(httpd_req_t *req);
esp_err_t list_files_handler(httpd_req_t *req);
esp_err_t status_handler(httpd_req_t *req);
esp_err_t ota_handler(httpd_req_t *req);
esp_err_t app_handler(httpd_req_t *req);
//esp_err_t flashmode_handler(httpd_req_t *req);
esp_err_t list_entity_handler(httpd_req_t *req);
esp_err_t rest_handler(httpd_req_t *req);
esp_err_t stat_handler(httpd_req_t *req);
//esp_err_t ws_handler(httpd_req_t *req);
esp_err_t config_handler(httpd_req_t *req);
//esp_err_t kmllist_event_handler(httpd_req_t *req);


bool routeHttpTraffic(const char *reference_uri, const char *uri_to_match, size_t match_upto);

static const httpd_uri_t restUris[] = {
    {
    .uri       = "/listentities/*",
    .method    = HTTP_POST,
    .handler   = list_entity_handler,
    .user_ctx  = NULL
    },
    {
    .uri       = "/trips",
    .method    = HTTP_POST,
    .handler   = trips_handler,
    .user_ctx  = NULL
    },
    {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = config_handler,
    .user_ctx  = NULL
    },
    {
    .uri       = "/files/*",
    .method    = HTTP_POST,
    .handler   = list_files_handler,
    .user_ctx  = NULL
    },
    {
    .uri       = "/status/*",
    .method    = HTTP_POST,
    .handler   = status_handler,
    .user_ctx  = NULL
    },
    {
    .uri       = "/status/*",
    .method    = HTTP_PUT,
    .handler   = status_handler,
    .user_ctx  = NULL
    },
    {
    .uri       = "/stat/*",
    .method    = HTTP_POST,
    .handler   = stat_handler,
    .user_ctx  = NULL
    },
    {
    .uri       = "/ota/*",
    .method    = HTTP_POST,
    .handler   = ota_handler,
    .user_ctx  = NULL
    }
};

static const httpd_uri_t appUri = {
    .uri       = "*",
    .method    = HTTP_GET,
    .handler   = app_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t restPostUri = {
    .uri       = "*",
    .method    = HTTP_POST,
    .handler   = rest_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t restPutUri = {
    .uri       = "*",
    .method    = HTTP_PUT,
    .handler   = rest_handler,
    .user_ctx  = NULL
};

//static const httpd_uri_t wsUri = {
//    .uri       = "/ws/*",
//    .method    = HTTP_POST,
//    .handler   = ws_handler,
//    .user_ctx  = NULL
//};

#endif