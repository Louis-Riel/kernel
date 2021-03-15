
#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>
#include "esp_http_server.h"
#include "esp_websocket_client.h"
#include "../../main/logs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"


#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static WebsocketManager* logsHandler = NULL;
static WebsocketManager* stateHandler = NULL;

void WebsocketManager::QueueHandler(void* instance){
  WebsocketManager* me = (WebsocketManager*)instance;
  char* buf = (char*)malloc(JSON_BUFFER_SIZE);
  httpd_ws_frame_t ws_pkt;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.final = false;
  esp_err_t ret;
  uint8_t* emptyString = (uint8_t*)malloc(1);
  *emptyString=0;
  while(me->isLive){
    if (xQueueReceive(me->rdySem,buf,3000/portTICK_PERIOD_MS)) {
      ws_pkt.payload = (uint8_t*)buf;
      ws_pkt.len = strlen(buf);
    } else {
      ws_pkt.payload = emptyString;
      ws_pkt.len = 0;
    }
    bool isLive = false;
    for (uint8_t idx = 0; idx < 5; idx++){
      if (me->clients[idx].isLive){
        isLive |= me->clients[idx].isLive = httpd_ws_send_frame_async(me->clients[idx].hd, me->clients[idx].fd, &ws_pkt) == ESP_OK;
      }
    }
    me->isLive = isLive;
  }
  ESP_LOGD(__FUNCTION__,"QueueHandler Done");
  vQueueDelete(me->rdySem);
  ws_pkt.final = true;
  for (uint8_t idx = 0; idx < 5; idx++){
    if (me->clients[idx].fd){
      httpd_ws_send_frame_async(me->clients[idx].hd, me->clients[idx].fd, &ws_pkt);
    }
  }
  free(buf);
  vTaskDelete(NULL);
}

WebsocketManager::WebsocketManager(char* name):
    rdySem(xQueueCreate(10, JSON_BUFFER_SIZE)),
    name((char*)malloc(strlen(name)+1)),
    isLive(true)
{
    strcpy(this->name,name);
    memset(clients,0,sizeof(clients));
    xTaskCreate(QueueHandler,name,4096,this,tskIDLE_PRIORITY,&queueTask);
};

bool WebsocketManager::RegisterClient(httpd_handle_t hd,int fd){
  for (uint8_t idx = 0; idx < 5; idx++){
    if ((clients[idx].hd == hd) && (clients[idx].fd == fd)){
      return clients[idx].isLive = true;
    }
  }
  for (uint8_t idx = 0; idx < 5; idx++){
    if (!clients[idx].isLive){
      clients[idx].hd = hd;
      clients[idx].fd = fd;
      return clients[idx].isLive = true;
    }
  }
  return false;
}

time_t GetTime() {
  return esp_timer_get_time() / 1000LL;
}

static bool logCallback(void* instance, char* logData){
  WebsocketManager* ws = (WebsocketManager*) instance;
  return ws == NULL || ws->isLive == false ? false : xQueueSend(ws->rdySem,logData,portMAX_DELAY);
};

static void statePoller(void *instance){
  WebsocketManager* ws = (WebsocketManager*) instance;
  EventGroupHandle_t stateEg = AppConfig::GetStateGroupHandle();
  httpd_ws_frame_t ws_pkt;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.final = false;

  EventBits_t bits = 0;
  while ((ws!=NULL) && (ws->isLive)) {
    EventBits_t lastBits = xEventGroupWaitBits(stateEg,0xff,pdFALSE,pdFALSE,10000/portTICK_RATE_MS);
    if (lastBits != bits){
      cJSON* state = NULL;
      char* buf = cJSON_Print((state=status_json()));
      xQueueSend(ws->rdySem,buf,portMAX_DELAY);
      free(buf);
      cJSON_free(state);
    }
    bits = lastBits;
  }
  ESP_LOGD(__FUNCTION__,"State Poller Done");
  stateHandler=NULL;
  vTaskDelete(NULL);
}


esp_err_t ws_handler(httpd_req_t *req){
    ESP_LOGD(__FUNCTION__, "WEBSOCKET Session");

    uint8_t buf[128] = { 0 };
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = buf;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 128);
    if (ret != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "httpd_ws_recv_frame failed with %d", ret);
        return ret;
    }
    ESP_LOGD(__FUNCTION__, "Got packet with message: %s", ws_pkt.payload);
    ESP_LOGD(__FUNCTION__, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT){
      if (strcmp((char*)ws_pkt.payload,"Logs") == 0) {
        if (logsHandler == NULL) {
          logsHandler = new WebsocketManager("LogsWebsocket");
          registerLogCallback(logCallback,logsHandler);
        }
        return logsHandler->RegisterClient(req->handle,httpd_req_to_sockfd(req)) ? ESP_OK : ESP_FAIL;
      }
      if (strcmp((char*)ws_pkt.payload,"State") == 0) {
        if (stateHandler == NULL) {
          stateHandler = new WebsocketManager("StateWebsocket");
          xTaskCreate(statePoller,"StatePoller",4096, stateHandler, tskIDLE_PRIORITY, NULL);
        }
        return stateHandler->RegisterClient(req->handle,httpd_req_to_sockfd(req)) ? ESP_OK : ESP_FAIL;
      }
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "httpd_ws_send_frame failed with %d", ret);
    }
    return ret;
}
