
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
  memset(&clients,0,sizeof(clients));
  ESP_LOGV(__FUNCTION__,"Created Websocket");
  CreateBackgroundTask(QueueHandler,"WebsocketQH",4096, NULL, tskIDLE_PRIORITY, NULL);
  CreateBackgroundTask(StatePoller,"StatePoller",4096, stateHandler, tskIDLE_PRIORITY, NULL);
  registerLogCallback(LogCallback,stateHandler);
  BuildStatus(this);
  cJSON* jState = ManagedDevice::BuildStatus(this);
  jClients = cJSON_HasObjectItem(jState,"Clients")?cJSON_GetObjectItem(jState,"Clients"):cJSON_AddArrayToObject(jState,"Clients");
  cJSON_AddNumberToObject(jState,"IsLive",true);
};

WebsocketManager::~WebsocketManager(){
  ESP_LOGD(__FUNCTION__,"State Poller Done");
  unregisterLogCallback(LogCallback);
  ESP_LOGD(__FUNCTION__,"Unregistered log callback");
  vQueueDelete(msgQueue);
  ldfree(logBuffer);
  ldfree(stateBuffer);
  cJSON* jState = ManagedDevice::BuildStatus(this);
  cJSON_SetIntValue(cJSON_GetObjectItem(jState,"IsLive"),false);
  cJSON_DeleteItemFromObject(jState,"Clients");
  stateHandler=NULL;
}

void WebsocketManager::ProcessMessage(uint8_t* msg){
  httpd_ws_frame_t ws_pkt;
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;
  ws_pkt.payload = msg == NULL ? &emptyString : msg;
  ws_pkt.len = msg == NULL ? 0 : strlen((char*)msg);
  ws_pkt.final = true;
  esp_err_t ret;
  struct tm timeinfo;

  bool hasLiveClients = false;
  bool hadClients = false;
  bool stateChange=false;
  for (uint8_t idx = 0; idx < 5; idx++){
    if (clients[idx].jIsLive && clients[idx].jIsLive->valueint){
      hadClients = true;
      if ((ret = httpd_ws_send_frame_async(clients[idx].hd, clients[idx].fd, &ws_pkt)) != ESP_OK) {
        cJSON_SetIntValue(clients[idx].jIsLive,false);
        httpd_sess_trigger_close(clients[idx].hd, clients[idx].fd);
        stateChange=true;
        ESP_LOGV(__FUNCTION__,"Client %d disconnected",idx);
      } else {
        cJSON_SetIntValue(clients[idx].jBytesOut,clients[idx].jBytesOut->valueint+ws_pkt.len);
        time(&clients[idx].lastTs);
        localtime_r(&clients[idx].lastTs, &timeinfo);
        strftime(clients[idx].jLastTs->valuestring, 30, "%c", &timeinfo);
        ESP_LOGD(__FUNCTION__,"Client %d %s",idx, clients[idx].jLastTs->valuestring);
      }
      hasLiveClients |= clients[idx].jIsLive->valueint;
    }
  }
  if (hadClients){
    if (isLive != hasLiveClients){
      isLive = hasLiveClients;
      stateChange=true;
    }
  }
  if (stateChange){
    AppConfig::SignalStateChange(state_change_t::MAIN);
  }
}

void WebsocketManager::QueueHandler(void* param){
  ESP_LOGD(__FUNCTION__,"QueueHandler Starting");
  uint8_t* buf = (uint8_t*)dmalloc(JSON_BUFFER_SIZE);
  while(stateHandler->isLive){
    memset(buf,0,JSON_BUFFER_SIZE);
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
  isLive=true;
  for (uint8_t idx = 0; idx < 5; idx++){
    if ((clients[idx].hd == hd) && (clients[idx].fd == fd)){
      ESP_LOGD(__FUNCTION__,"Client was at position %d",idx);
      isLive=true;
      cJSON_SetIntValue(clients[idx].jIsLive, true);
      AppConfig::SignalStateChange(state_change_t::MAIN);
      return true;
    }
  }
  for (uint8_t idx = 0; idx < 5; idx++){
    if (!clients[idx].jIsLive || !clients[idx].jIsLive->valueint){
      clients[idx].hd = hd;
      clients[idx].fd = fd;

      if (!clients[idx].jIsLive) {
        ESP_LOGD(__FUNCTION__,"Client is inserted at position %d",idx);
        cJSON* client = cJSON_CreateObject();
        cJSON_AddItemToArray(jClients,client);
        clients[idx].jBytesIn = cJSON_AddNumberToObject(client,"BytesIn",0);
        clients[idx].jBytesOut = cJSON_AddNumberToObject(client,"BytesOut",0);
        clients[idx].jIsLive = cJSON_AddNumberToObject(client,"IsLive",true);
        clients[idx].jAddr = cJSON_AddStringToObject(client,"Address","                              ");
        clients[idx].jLastTs = cJSON_AddStringToObject(client,"LastTs","                              ");
      }
      cJSON_SetIntValue(clients[idx].jBytesIn,0);
      cJSON_SetIntValue(clients[idx].jBytesOut,0);
      cJSON_SetValuestring(clients[idx].jLastTs,"                              ");
      cJSON_SetIntValue(clients[idx].jIsLive,true);

      sockaddr_in6 addr;
      socklen_t addr_size = sizeof(addr);
      if (getpeername(fd, (struct sockaddr *)&addr, &addr_size) < 0) {
        ESP_LOGW(__FUNCTION__, "Error getting client IP");
      } else {
        inet_ntop(AF_INET6, &addr.sin6_addr, clients[idx].jAddr->valuestring, 30);
      }
      AppConfig::SignalStateChange(state_change_t::MAIN);
      return true;
    }
  }
  ESP_LOGD(__FUNCTION__,"Client could not be added");
  return false;
}

time_t GetTime() {
  return esp_timer_get_time() / 1000LL;
}

bool WebsocketManager::LogCallback(void* instance, char* logData){
  if (stateHandler && stateHandler->isLive && logData) {
    strcpy(stateHandler->logBuffer,logData);
    return xQueueSend(stateHandler->msgQueue,stateHandler->logBuffer,portMAX_DELAY);
  }
  return false;
};

bool WebsocketManager::EventCallback(char *event){
  if (stateHandler && stateHandler->isLive && event) {
    return xQueueSend(stateHandler->msgQueue,event,portMAX_DELAY);
  }
  return false;
}


void WebsocketManager::StatePoller(void *instance){
  EventGroupHandle_t stateEg = AppConfig::GetStateGroupHandle();
  ESP_LOGD(__FUNCTION__,"State poller started");
  EventBits_t bits = 0;
  cJSON* gpsState = NULL;
  cJSON* mainState = AppConfig::GetAppStatus()->GetJSONConfig(NULL);
  while (stateHandler && stateHandler->isLive) {
    bits = xEventGroupWaitBits(stateEg,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
    if (stateHandler && stateHandler->isLive){
      cJSON* state = TheRest::status_json();
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
          cJSON_AddItemReferenceToObject(state,item->string,AppConfig::GetAppStatus()->GetJSONConfig(item->string));
        }
      }
      if (cJSON_PrintPreallocated(state,stateHandler->stateBuffer,JSON_BUFFER_SIZE,pdFALSE)){
        if (xQueueSend(stateHandler->msgQueue,stateHandler->stateBuffer,portMAX_DELAY) == pdTRUE) {
          ESP_LOGV(__FUNCTION__,"State sent %d",strlen(stateHandler->stateBuffer));
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
  ESP_LOGD(__FUNCTION__, "WEBSOCKET Session");
  if (stateHandler == NULL) {
    ESP_LOGD(__FUNCTION__, "Staring Manager");
    stateHandler = new WebsocketManager();
  }

  if (stateHandler->RegisterClient(req->handle,httpd_req_to_sockfd(req))){
    ESP_LOGD(__FUNCTION__,"Client Connected");
    return ESP_OK;
  }
  ESP_LOGW(__FUNCTION__,"Client cannot be connected");
  return ESP_FAIL;
}
