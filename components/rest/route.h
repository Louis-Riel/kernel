#ifndef __route_h
#define __route_h

#include "esp_http_client.h"
#include "esp_http_server.h"
#include "eventmgr.h"
#include "rest.h"
#include "../wifi/station.h"

static uint32_t deviceId = 0;

class WebsocketManager : ManagedDevice
{
public:
  WebsocketManager();
  ~WebsocketManager();
  bool RegisterClient(httpd_handle_t hd, int fd);
  void ProcessMessage(uint8_t *msg);
  static void QueueHandler(void *instance);
  static void StatePoller(void *instance);
  static bool EventCallback(char *event);
  static bool LogCallback(void *instance, char *logData);
  static bool HasOpenedWs();

  TaskHandle_t queueTask;
  bool isLive;
  struct ws_client_t
  {
    httpd_handle_t hd;
    int fd;
    bool isLive = false;
    uint8_t errorCount = 0;
    uint64_t bytesOut = 0;
    time_t lastTs = 0;
    sockaddr_in6 addr;
  } clients[5];

protected:
  static bool HealthCheck(void *instance);
  static cJSON *BuildStatus(void *instance);

  EventHandlerDescriptor *BuildHandlerDescriptors();

private:
  uint32_t logPos;
  QueueHandle_t msgQueue;
  char *logBuffer;
  char *stateBuffer;
  uint8_t emptyString;
};

class TheRest : ManagedDevice
{
public:
  TheRest(AppConfig *config, EventGroupHandle_t evtGrp);
  ~TheRest();

  static void GetServer(void *evtGrp);
  static TheRest *GetServer();
  static EventGroupHandle_t GetEventGroup();
  static void CheckUpgrade(void *param);
  static void MergeConfig(void *param);
  static void SendStatus(void *param);
  static void SendTar(void *params);
  static esp_err_t SendConfig(char *addr, cJSON *cfg);
  static esp_err_t HandleWifiCommand(httpd_req_t *req);

  enum restEvents_t
  {
    UPDATE_CONFIG,
    SEND_STATUS,
    CHECK_UPGRADE
  };

protected:
  static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
  static bool HealthCheck(void *instance);

  EventHandlerDescriptor *BuildHandlerDescriptors();
  
  char *SendRequest(const char *url, esp_http_client_method_t method, size_t *len, char *charBuf);
  char *SendRequest(const char *url, esp_http_client_method_t method, size_t *len);
  char *PostRequest(const char *url, size_t *len);
  char *GetRequest(const char *url, size_t *len);
  cJSON *PostJSonRequest(const char *url);
  cJSON *GetDeviceConfig(esp_ip4_addr_t *ipInfo, uint32_t deviceId);
  bool DownloadFirmware(char *srvMd5);
  bool GetLocalMD5(char *ccmd5);

  static bool routeHttpTraffic(const char *reference_uri, const char *uri_to_match, size_t match_upto);

  static esp_err_t rest_handler(httpd_req_t *req);
  static esp_err_t list_files_handler(httpd_req_t *req);
  static esp_err_t status_handler(httpd_req_t *req);
  static esp_err_t download_handler(httpd_req_t *req);
  static esp_err_t ota_handler(httpd_req_t *req);
  static esp_err_t app_handler(httpd_req_t *req);
  static esp_err_t list_entity_handler(httpd_req_t *req);
  static esp_err_t stat_handler(httpd_req_t *req);
  static esp_err_t config_handler(httpd_req_t *req);
  static esp_err_t ws_handler(httpd_req_t *req);
  static esp_err_t sendFile(httpd_req_t *req, const char *path);

  static esp_err_t HandleSystemCommand(httpd_req_t *req);
  static esp_err_t findFiles(httpd_req_t *req, const char *path, const char *ext, bool recursive, char *res, uint32_t resLen);

private:
  EventGroupHandle_t eventGroup;
  EventGroupHandle_t wifiEventGroup;
  httpd_config_t restConfig;
  httpd_handle_t server;
  char *gwAddr;
  char *ipAddr;
  EventGroupHandle_t app_eg;
  uint32_t healthCheckCount;

  cJSON* jnumRequests;
  cJSON* jprocessingTime_us;
  cJSON* jBytesIn;
  cJSON* jBytesOut;

  httpd_uri_t const restUris[10] =
      {
          {.uri = "/config*",
           .method = HTTP_POST,
           .handler = config_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/config*",
           .method = HTTP_PUT,
           .handler = config_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/files/*",
           .method = HTTP_POST,
           .handler = list_files_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/status/*",
           .method = HTTP_POST,
           .handler = status_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/status/*",
           .method = HTTP_PUT,
           .handler = status_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/stat/*",
           .method = HTTP_POST,
           .handler = stat_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/stat/*",
           .method = HTTP_DELETE,
           .handler = stat_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/ota/*",
           .method = HTTP_POST,
           .handler = ota_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/sdcard/*",
           .method = HTTP_PUT,
           .handler = download_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL},
          {.uri = "/lfs/*",
           .method = HTTP_PUT,
           .handler = download_handler,
           .user_ctx = NULL,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = NULL}};

  const httpd_uri_t appUri = {
      .uri = "*",
      .method = HTTP_GET,
      .handler = app_handler,
      .user_ctx = NULL,
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = NULL};

  const httpd_uri_t restPostUri = {
      .uri = "*",
      .method = HTTP_POST,
      .handler = rest_handler,
      .user_ctx = NULL,
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = NULL};

  const httpd_uri_t restPutUri = {
      .uri = "*",
      .method = HTTP_PUT,
      .handler = rest_handler,
      .user_ctx = NULL,
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = NULL};

  const httpd_uri_t wsUri = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = ws_handler,
      .user_ctx = NULL,
      .is_websocket = true,
      .handle_ws_control_frames = false,
      .supported_subprotocol = NULL};
};

#endif