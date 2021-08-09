
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

static WebsocketManager* stateHandler = NULL;

void WebsocketManager::QueueHandler(void* instance){
  ESP_LOGD(__FUNCTION__,"QueueHandler Starting");
  WebsocketManager* me = (WebsocketManager*)instance;
  char* buf = (char*)dmalloc(JSON_BUFFER_SIZE);
  httpd_ws_frame_t ws_pkt;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.final = false;
  esp_err_t ret;
  uint8_t* emptyString = (uint8_t*)dmalloc(1);
  *emptyString=0;
  while(me->isLive){
    if (xQueueReceive(me->rdySem,buf,3000/portTICK_PERIOD_MS)) {
      ws_pkt.payload = (uint8_t*)buf;
      ws_pkt.len = strlen(buf);
      //printf("\nGot a %s(%d) msg",ws_pkt.payload[0] == '{' ? "State" : "Logs\n", ws_pkt.len);
    } else {
      ws_pkt.payload = emptyString;
      ws_pkt.len = 0;
    }
    bool isLive = false;
    for (uint8_t idx = 0; idx < 5; idx++){
      if (me->clients[idx].isLive){
        if ((ret = httpd_ws_send_frame_async(me->clients[idx].hd, me->clients[idx].fd, &ws_pkt)) != ESP_OK) {
          if (me->clients[idx].errorCount++ > 4){
            me->clients[idx].isLive=false;
            if (ret == ESP_ERR_INVALID_ARG) {
              ESP_LOGD(__FUNCTION__,"Client %d disconnected",idx);
            } else {
              ESP_LOGW(__FUNCTION__,"Client %d Error %s",idx, esp_err_to_name(ret));
            }
          } else {
              ESP_LOGD(__FUNCTION__,"Client %d not responding, retry %d",idx, me->clients[idx].errorCount);
              vTaskDelay(1000/portTICK_PERIOD_MS);
          }
        }
        isLive |= me->clients[idx].isLive;
        break;
      }
    }
    me->isLive = isLive;
  }
  ESP_LOGD(__FUNCTION__,"QueueHandler Done");
  ws_pkt.final = true;
  for (uint8_t idx = 0; idx < 5; idx++){
    if (me->clients[idx].fd){
      httpd_ws_send_frame_async(me->clients[idx].hd, me->clients[idx].fd, &ws_pkt);
    }
  }
  ldfree(buf);
  ldfree(emptyString);
  ldfree(me->name);
  vQueueDelete(me->rdySem);
}

WebsocketManager::WebsocketManager(const char* name):
    rdySem(xQueueCreate(10, JSON_BUFFER_SIZE)),
    name((char*)dmalloc(strlen(name)+1)),
    isLive(true)
{
    strcpy(this->name,name);
    memset(clients,0,sizeof(clients));
    ESP_LOGV(__FUNCTION__,"Created Websocket %s",name);
    CreateBackgroundTask(QueueHandler,name,4096,this,tskIDLE_PRIORITY,&queueTask);
};

bool WebsocketManager::RegisterClient(httpd_handle_t hd,int fd){
  for (uint8_t idx = 0; idx < 5; idx++){
    if ((clients[idx].hd == hd) && (clients[idx].fd == fd)){
      ESP_LOGV(__FUNCTION__,"Client was at position %d",idx);
      isLive=true;
      return clients[idx].isLive = true;
    }
  }
  for (uint8_t idx = 0; idx < 5; idx++){
    if (!clients[idx].isLive){
      clients[idx].hd = hd;
      clients[idx].fd = fd;
      ESP_LOGV(__FUNCTION__,"Client is inserted at position %d",idx);
      isLive=true;
      return clients[idx].isLive = true;
    }
  }
  ESP_LOGV(__FUNCTION__,"Client could not be added");
  return false;
}

time_t GetTime() {
  return esp_timer_get_time() / 1000LL;
}

static bool logCallback(void* instance, char* logData){
  WebsocketManager* ws = (WebsocketManager*) instance;
  //printf("\nSending log(%d)ws null:%d live:%d", strlen(logData), ws == NULL, ws != NULL && ws->isLive);

  return ws == NULL || ws->isLive == false ? false : xQueueSend(ws->rdySem,logData,portMAX_DELAY);
};

static void statePoller(void *instance){
  WebsocketManager* ws = (WebsocketManager*) instance;
  EventGroupHandle_t stateEg = AppConfig::GetStateGroupHandle();

  EventBits_t bits = 0;
  while ((ws!=NULL) && (ws->isLive)) {
    bits = xEventGroupWaitBits(stateEg,0xff,pdTRUE,pdFALSE,1000/portTICK_RATE_MS);
    if (bits){
      cJSON* state = status_json();
      if (bits&state_change_t::GPS) {
        ESP_LOGV(__FUNCTION__,"gState Changed %d", bits);
        cJSON_AddItemReferenceToObject(state,"gps",AppConfig::GetAppStatus()->GetJSONConfig("gps"));
      } 
      if (bits&state_change_t::WIFI) {
        ESP_LOGV(__FUNCTION__,"wState Changed %d", bits);
        cJSON* item;
        cJSON_ArrayForEach(item,AppConfig::GetAppStatus()->GetJSONConfig(NULL)) {
          if (state!= NULL){
            cJSON_AddItemReferenceToObject(state,item->string,AppConfig::GetAppStatus()->GetJSONConfig(item->string));
          } else {
            ESP_LOGW(__FUNCTION__, "missing state");
          }
        }
      } else {
        ESP_LOGV(__FUNCTION__,"State Changed %d", bits);
      }
      char* buf = cJSON_PrintUnformatted(state);
      if (buf){
        xQueueSend(ws->rdySem,buf,portMAX_DELAY);
        ldfree(buf);
      }
      cJSON_free(state);
    }
  }
  ESP_LOGD(__FUNCTION__,"State Poller Done");
  stateHandler=NULL;
}


esp_err_t TheRest::ws_handler(httpd_req_t *req){
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
      if (stateHandler == NULL) {
        stateHandler = new WebsocketManager("StateWebsocket");
        CreateBackgroundTask(statePoller,"StatePoller",4096, stateHandler, tskIDLE_PRIORITY, NULL);
        registerLogCallback(logCallback,stateHandler);
      }
      if (strcmp((char*)ws_pkt.payload,"Logs") == 0) {
        return stateHandler->RegisterClient(req->handle,httpd_req_to_sockfd(req)) ? ESP_OK : ESP_FAIL;
      }
      if (strcmp((char*)ws_pkt.payload,"State") == 0) {
        return stateHandler->RegisterClient(req->handle,httpd_req_to_sockfd(req)) ? ESP_OK : ESP_FAIL;
      }
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "httpd_ws_send_frame failed with %d", ret);
    }
    return ret;
}
