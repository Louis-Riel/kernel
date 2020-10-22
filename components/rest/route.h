#ifndef __route_h
#define __route_h

#include "esp_http_client.h"
static char mounts[3][6] = {"/kml","/sent","/"};
esp_err_t list_trips_handler(httpd_req_t *req);
esp_err_t trips_handler(httpd_req_t *req);
esp_err_t config_get_handler(httpd_req_t *req);
esp_err_t ota_handler(httpd_req_t *req);
esp_err_t root_handler(httpd_req_t *req);
esp_err_t flashmode_handler(httpd_req_t *req);
esp_err_t rest_handler(httpd_req_t *req);
esp_err_t kmllist_event_handler(httpd_req_t *req);

static const httpd_uri_t listTripsUri = {
    .uri       = "/listtrips/*",
    .method    = HTTP_GET,
    .handler   = list_trips_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t rootUri = {
    .uri       = "*",
    .method    = HTTP_GET,
    .handler   = root_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t tripUri = {
    .uri       = "/trips",
    .method    = HTTP_GET,
    .handler   = trips_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t configGetUri = {
    .uri       = "/rest/config",
    .method    = HTTP_GET,
    .handler   = config_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t otaUri = {
    .uri       = "/ota/*",
    .method    = HTTP_POST,
    .handler   = ota_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t getRestUri = {
    .uri       = "/rest/*",
    .method    = HTTP_GET,
    .handler   = rest_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t postReqUri = {
    .uri       = "/rest/*",
    .method    = HTTP_POST,
    .handler   = rest_handler,
    .user_ctx  = NULL
};
#endif