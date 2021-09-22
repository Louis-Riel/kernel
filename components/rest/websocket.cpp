
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

WebsocketManager::WebsocketManager():
    ManagedDevice("WebsocketManager","WebsocketManager",BuildStatus,ManagedDevice::HealthCheck),
    isLive(true),
    logPos(0),
    msgQueue(xQueueCreate(25,JSON_BUFFER_SIZE)),
    logBuffer((char*)dmalloc(JSON_BUFFER_SIZE)),
    stateBuffer((char*)dmalloc(JSON_BUFFER_SIZE)),
    emptyString(0)
{
  stateHandler=this;
  memset(clients,0,sizeof(clients));
  ESP_LOGV(__FUNCTION__,"Created Websocket");
  CreateBackgroundTask(QueueHandler,"WebsocketQH",4096, NULL, tskIDLE_PRIORITY, NULL);
  CreateBackgroundTask(statePoller,"StatePoller",4096, stateHandler, tskIDLE_PRIORITY, NULL);
  registerLogCallback(logCallback,stateHandler);
  BuildStatus(this);
};

WebsocketManager::~WebsocketManager(){
  ESP_LOGD(__FUNCTION__,"State Poller Done");
  unregisterLogCallback(logCallback);
  ESP_LOGD(__FUNCTION__,"Unregistered log callback");
  vQueueDelete(msgQueue);
  ldfree(logBuffer);
  ldfree(stateBuffer);
  AppConfig* stats = new AppConfig(AppConfig::GetAppStatus()->GetJSONConfig("WebsocketManager"),NULL);
  stats->SetBoolProperty("IsLive",false);
  cJSON_DeleteItemFromObject(stats->GetJSONConfig(NULL),"clients");
  delete stats;
  stateHandler=NULL;
}

cJSON* WebsocketManager::BuildStatus(void* instance){
  if (instance == NULL) {
    return NULL;
  }
  WebsocketManager* ws = (WebsocketManager*)instance;
  cJSON* sjson = NULL;
  AppConfig* acfg = new AppConfig(sjson=ManagedDevice::BuildStatus(instance),AppConfig::GetAppStatus());
  if (instance == NULL) {
    acfg->SetBoolProperty("IsLive",false);
    cJSON_DeleteItemFromObject(sjson,"clients");
    return sjson;
  }

  if (!cJSON_HasObjectItem(sjson,"clients")) {
    ESP_LOGV(__FUNCTION__,"First Status, creating array");
    cJSON_AddArrayToObject(sjson,"clients");
  } else {
    ESP_LOGV(__FUNCTION__,"Refresh Status");
    cJSON* clients = cJSON_GetObjectItem(sjson,"clients");
    while (cJSON_GetArraySize(clients)) {
      cJSON_DeleteItemFromArray(clients,0);
    }
  }
  cJSON* jClients = cJSON_GetObjectItem(sjson,"clients");

  AppConfig* client=NULL;
  struct tm timeinfo;
  char strftime_buf[30];
  char ipstr[INET6_ADDRSTRLEN];
  for (uint8_t idx = 0; idx < 5; idx++){
    if (ws->clients[idx].isLive){
      ESP_LOGV(__FUNCTION__,"Client at idx %d is live",idx);
      client = new AppConfig(cJSON_CreateObject(),AppConfig::GetAppStatus());
      localtime_r(&ws->clients[idx].lastTs, &timeinfo);
      strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

      socklen_t addr_size = sizeof(ws->clients[idx].addr);
      if (getpeername(ws->clients[idx].fd, (struct sockaddr *)&ws->clients[idx].addr, &addr_size) < 0) {
          ESP_LOGE(__FUNCTION__, "Error getting client IP");
      } else {
        inet_ntop(AF_INET6, &ws->clients[idx].addr.sin6_addr, ipstr, sizeof(ipstr));
        client->SetStringProperty("Address",ipstr);
      }

      client->SetStringProperty("LastTs",strftime_buf);
      client->SetLongProperty("Bytes",ws->clients[idx].bytesOut);
      client->SetBoolProperty("IsLive",ws->clients[idx].isLive);
      cJSON_AddItemToArray(jClients,client->GetJSONConfig(NULL));
      delete client;
    } else {
      ESP_LOGV(__FUNCTION__,"Client at idx %d is not live",idx);
    }
  }

  acfg->SetBoolProperty("IsLive",ws->isLive);
  AppConfig::SignalStateChange(state_change_t::MAIN);

  delete acfg;
  return sjson;
}

void WebsocketManager::ProcessMessage(uint8_t* msg){
  httpd_ws_frame_t ws_pkt;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.final = false;
  ws_pkt.payload = msg == NULL ? &emptyString : msg;
  ws_pkt.len = msg == NULL ? 0 : strlen((char*)msg);
  ws_pkt.final = true;
  esp_err_t ret;

  bool hasLiveClients = false;
  bool hadClients = false;
  bool stateChange=false;
  for (uint8_t idx = 0; idx < 5; idx++){
    if (clients[idx].isLive){
      hadClients = true;
      if ((ret = httpd_ws_send_frame_async(clients[idx].hd, clients[idx].fd, &ws_pkt)) != ESP_OK) {
        clients[idx].isLive=false;
        httpd_sess_trigger_close(clients[idx].hd, clients[idx].fd);
        stateChange=true;
        ESP_LOGV(__FUNCTION__,"Client %d disconnected",idx);
      } else {
        clients[idx].bytesOut+=ws_pkt.len;
        time(&clients[idx].lastTs);
      }
      hasLiveClients |= clients[idx].isLive;
    }
  }
  if (hadClients){
    if (isLive != hasLiveClients){
      isLive = hasLiveClients;
      stateChange=true;
    }
  }
  if (stateChange){
    BuildStatus(this);
  }
}

void WebsocketManager::QueueHandler(void* param){
  ESP_LOGD(__FUNCTION__,"QueueHandler Starting");
  uint8_t* buf = (uint8_t*)dmalloc(JSON_BUFFER_SIZE);
  while(stateHandler->isLive){
    if (xQueueReceive(stateHandler->msgQueue,buf,3000/portTICK_PERIOD_MS)) {
      stateHandler->ProcessMessage(buf);
    } else {
      stateHandler->ProcessMessage(NULL);
    }
  }
  ESP_LOGD(__FUNCTION__,"QueueHandler Done");
  ldfree(buf);
}

bool WebsocketManager::HasOpenedWs(){
  return stateHandler ? stateHandler->isLive : false;
}

bool WebsocketManager::RegisterClient(httpd_handle_t hd,int fd){
  for (uint8_t idx = 0; idx < 5; idx++){
    if ((clients[idx].hd == hd) && (clients[idx].fd == fd)){
      ESP_LOGV(__FUNCTION__,"Client was at position %d",idx);
      isLive=true;
      clients[idx].isLive = true;
      BuildStatus(this);
      return true;
    }
  }
  for (uint8_t idx = 0; idx < 5; idx++){
    if (!clients[idx].isLive){
      clients[idx].hd = hd;
      clients[idx].fd = fd;
      socklen_t addr_size = sizeof(clients[idx].addr);
      
      if (getpeername(fd, (struct sockaddr *)&clients[idx].addr, &addr_size) < 0) {
        ESP_LOGW(__FUNCTION__, "Error getting client IP");
      }
      ESP_LOGD(__FUNCTION__,"Client is inserted at position %d",idx);
      isLive=true;
      clients[idx].isLive = true;
      BuildStatus(this);
      return true;
    }
  }
  ESP_LOGD(__FUNCTION__,"Client could not be added");
  return false;
}

time_t GetTime() {
  return esp_timer_get_time() / 1000LL;
}

bool WebsocketManager::logCallback(void* instance, char* logData){
  if (stateHandler && stateHandler->isLive) {
    if (logData) {
      strcpy(stateHandler->logBuffer,logData);
      return xQueueSend(stateHandler->msgQueue,stateHandler->logBuffer,portMAX_DELAY);
    }
  }
  return false;
};

void WebsocketManager::statePoller(void *instance){
  EventGroupHandle_t stateEg = AppConfig::GetStateGroupHandle();
  ESP_LOGD(__FUNCTION__,"State poller started");
  EventBits_t bits = 0;
  cJSON* gpsState = NULL;
  cJSON* mainState = AppConfig::GetAppStatus()->GetJSONConfig(NULL);
  while (stateHandler && stateHandler->isLive) {
    bits = xEventGroupWaitBits(stateEg,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
    if (stateHandler && stateHandler->isLive){
      cJSON* state = NULL;
      state = status_json();
      if (bits&state_change_t::GPS) {
        ESP_LOGV(__FUNCTION__,"gState Changed %d", bits);
        if (gpsState == NULL) {
          gpsState = AppConfig::GetAppStatus()->GetJSONConfig("gps");
        }
        cJSON_AddItemReferenceToObject(state,"gps",gpsState);
      } else if (bits&state_change_t::THREADS) {
        cJSON_AddItemToObject(state,"tasks",tasks_json());
      } else {
        ESP_LOGV(__FUNCTION__,"mState Changed %d", bits);
        cJSON* item;
        cJSON_ArrayForEach(item,mainState) {
          if (state!= NULL){
            cJSON_AddItemReferenceToObject(state,item->string,AppConfig::GetAppStatus()->GetJSONConfig(item->string));
          } else {
            ESP_LOGW(__FUNCTION__, "Missing state");
          }
        }
      }
      if (cJSON_PrintPreallocated(state,stateHandler->logBuffer,JSON_BUFFER_SIZE,pdFALSE)){
        if (xQueueSend(stateHandler->msgQueue,stateHandler->logBuffer,portMAX_DELAY) == pdTRUE) {
          ESP_LOGV(__FUNCTION__,"State sent %d",strlen(stateHandler->logBuffer));
        } else {
          ESP_LOGW(__FUNCTION__,"Failed to queue state");
        }
      } else {
        ESP_LOGW(__FUNCTION__,"Failed to stringify state");
      }
      cJSON_free(state);
    }
  }
  ESP_LOGD(__FUNCTION__,"State poller done");
  delete stateHandler;
}


esp_err_t TheRest::ws_handler(httpd_req_t *req) {
  ESP_LOGV(__FUNCTION__, "WEBSOCKET Session");
  if (stateHandler == NULL) {
    ESP_LOGD(__FUNCTION__, "Staring Manager");
    stateHandler = new WebsocketManager();
  }

  httpd_ws_frame_t ws_pkt;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.payload = (uint8_t*)dmalloc(128);
  memset(ws_pkt.payload,0,128);
  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 128);
  if (ret == ESP_OK) 
  {
    GetServer()->jBytesOut->valuedouble = GetServer()->jBytesOut->valueint+=ws_pkt.len;
    switch (ws_pkt.type)
    {
    case HTTPD_WS_TYPE_TEXT:
      if (strcmp((char*)ws_pkt.payload,"Connect")==0){
        if (stateHandler->RegisterClient(req->handle,httpd_req_to_sockfd(req))){
          ESP_LOGV(__FUNCTION__,"Client Connected");
        } else {
          ESP_LOGW(__FUNCTION__,"Client cannot be connected");
        }
      } else {
        ESP_LOGW(__FUNCTION__,"Unknown Command(%d):%s",ws_pkt.len, (char*)ws_pkt.payload);
      }
    break;
    case HTTPD_WS_TYPE_CONTINUE:  ESP_LOGD(__FUNCTION__,"Packet type(%d): CONTINUE: %s",ws_pkt.len, (char*)ws_pkt.payload); break;
    case HTTPD_WS_TYPE_BINARY:    ESP_LOGD(__FUNCTION__,"Packet type(%d): BINARY",ws_pkt.len);   break;
    case HTTPD_WS_TYPE_CLOSE:     ESP_LOGD(__FUNCTION__,"Packet type(%d): CLOSE",ws_pkt.len);    break;
    case HTTPD_WS_TYPE_PING:      ESP_LOGD(__FUNCTION__,"Packet type(%d): PING",ws_pkt.len);     break;
    case HTTPD_WS_TYPE_PONG:      ESP_LOGD(__FUNCTION__,"Packet type(%d): PONG",ws_pkt.len);     break;
    default:                      ESP_LOGW(__FUNCTION__,"Packet type(%d): UNKNOWN",ws_pkt.len);  break;
    }
  } else {
    ESP_LOGE(__FUNCTION__, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
  }
  ldfree(ws_pkt.payload);
  return ret;
}
