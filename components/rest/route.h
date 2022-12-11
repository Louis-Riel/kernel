#ifndef __route_h
#define __route_h

#include "esp_http_client.h"
#include "esp_http_server.h"
#include "eventmgr.h"
#include "route.h"
#include "../wifi/station.h"
#include "esp_partition.h"
#include "esp_ota_ops.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

void restSallyForth(void *pvParameter);

typedef enum{
    TAR_BUFFER_FILLED = BIT0,
    TAR_BUFFER_SENT = BIT1,
    TAR_BUILD_DONE = BIT2,
    TAR_SEND_DONE = BIT3,
    HTTP_SERVING = BIT4,
    GETTING_TRIP_LIST = BIT5,
    GETTING_TRIPS = BIT6,
    DOWNLOAD_STARTED = BIT7,
    DOWNLOAD_FINISHED = BIT8
} restServerState_t;
class WebsocketManager : private ManagedDevice
{
public:
  WebsocketManager();
  ~WebsocketManager();

  struct ws_msg_t;
  struct ws_client_t
  {
    httpd_handle_t hd;
    int fd;
    time_t lastTs;
    cJSON* jConnectTs;
    cJSON* jBytesIn;
    cJSON* jBytesOut;
    cJSON* jLastTs;
    cJSON* jIsLive;
    cJSON* jAddr;
    cJSON* jErrCount;
    ws_msg_t* pingMsg;
  } clients[5];

  struct ws_msg_t {
    void* buf;
    ws_client_t* client;
    uint32_t bufLen;
  };

  bool RegisterClient(httpd_handle_t hd, int fd);
  void ProcessMessage(uint8_t *msg);
  static void QueueHandler(void *instance);
  static void StatePoller(void *instance);
  static bool EventCallback(char *event);
  static bool LogCallback(void *instance, char *logData);
  static bool HasOpenedWs();

  TaskHandle_t queueTask;
  cJSON* jIsLive;

protected:
  static void PostToClient(void* msg);
  static const char* WEBSOCKET_BASE;

  EventHandlerDescriptor *BuildHandlerDescriptors();

private:
  ws_msg_t* getNewMessage();
  uint32_t logPos;
  QueueHandle_t msgQueue;
  uint8_t emptyString;
  cJSON* jClients;
  ws_msg_t* msgQueueBuf;
  uint8_t msgQueueBufLen;
  uint8_t msgQueueBufPos;
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
  static esp_err_t SendConfig(char *addr, cJSON *cfg);
  static cJSON* status_json();
  static cJSON* tasks_json();
  cJSON* bake_status_json();
  static const char* REST_BASE;

protected:
  static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
  static bool HealthCheck(void *instance);
  static void ScanNetwork(void *instance);

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
  static esp_err_t eventDescriptor_handler(httpd_req_t *req);
  static esp_err_t ota_handler(httpd_req_t *req);
  static esp_err_t app_handler(httpd_req_t *req);
  static esp_err_t stat_handler(httpd_req_t *req);
  static esp_err_t config_handler(httpd_req_t *req);
  static esp_err_t config_template_handler(httpd_req_t *req);
  static esp_err_t ws_handler(httpd_req_t *req);
  static esp_err_t sendFile(httpd_req_t *req, const char *path);

  static esp_err_t HandleSystemCommand(httpd_req_t *req);
  static esp_err_t HandleStatusChange(httpd_req_t *req);
  static esp_err_t findFiles(httpd_req_t *req, const char *path, const char *ext, bool recursive, char *res, uint32_t resLen);
  static esp_err_t checkTheSum(httpd_req_t *req);

private:
  char* hcUrl;
  const cJSON* accessControlAllowOrigin;
  const cJSON* accessControlMaxAge;
  const cJSON* accessControlAllowMethods;
  const cJSON* accessControlAllowHeaders;
  char* postData;
  EventGroupHandle_t eventGroup;
  EventGroupHandle_t wifiEventGroup;
  httpd_config_t restConfig;
  httpd_handle_t server;
  char *gwAddr;
  char *ipAddr;
  EventGroupHandle_t app_eg;
  uint8_t storageFlags;
  TaskStatus_t * statuses;
  UBaseType_t statusesLen;

  cJSON* system_status;
  
  cJSON* jnumErrors;
  cJSON* jnumRequests;
  cJSON* jprocessingTime_us;
  cJSON* jBytesIn;
  cJSON* jBytesOut;
  cJSON* jScanning;

  httpd_uri_t restUris[12] =
      {
          {.uri = "/templates/config*",
           .method = HTTP_POST,
           .handler = config_template_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/config*",
           .method = HTTP_POST,
           .handler = config_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/config*",
           .method = HTTP_PUT,
           .handler = config_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/files/*",
           .method = HTTP_POST,
           .handler = list_files_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/status/*",
           .method = HTTP_POST,
           .handler = status_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/status/*",
           .method = HTTP_PUT,
           .handler = status_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/stat/*",
           .method = HTTP_POST,
           .handler = stat_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/stat/*",
           .method = HTTP_DELETE,
           .handler = stat_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/ota/*",
           .method = HTTP_POST,
           .handler = ota_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/sdcard/*",
           .method = HTTP_PUT,
           .handler = download_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/eventDescriptor/*",
           .method = HTTP_POST,
           .handler = eventDescriptor_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr},
          {.uri = "/lfs/*",
           .method = HTTP_PUT,
           .handler = download_handler,
           .user_ctx = nullptr,
           .is_websocket = false,
           .handle_ws_control_frames = false,
           .supported_subprotocol = nullptr}};

  httpd_uri_t appUri = {
      .uri = "*",
      .method = HTTP_GET,
      .handler = app_handler,
      .user_ctx = nullptr,
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = nullptr};

  httpd_uri_t restPostUri = {
      .uri = "*",
      .method = HTTP_POST,
      .handler = rest_handler,
      .user_ctx = nullptr,
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = nullptr};

  httpd_uri_t restPutUri = {
      .uri = "*",
      .method = HTTP_PUT,
      .handler = rest_handler,
      .user_ctx = nullptr,
      .is_websocket = false,
      .handle_ws_control_frames = false,
      .supported_subprotocol = nullptr};

  httpd_uri_t wsUri = {
      .uri = "/ws",
      .method = HTTP_GET,
      .handler = ws_handler,
      .user_ctx = nullptr,
      .is_websocket = true,
      .handle_ws_control_frames = false,
      .supported_subprotocol = nullptr};
};

#endif