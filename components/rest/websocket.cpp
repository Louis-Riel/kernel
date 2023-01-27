#include "camera.h"
#include "eventmgr.h"
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

static WebsocketManager* stateHandler = nullptr;
const char* emptySpace = "                              ";
const char* WebsocketManager::WEBSOCKET_BASE="WebsocketManager";

WebsocketManager::WebsocketManager():
    ManagedDevice(WEBSOCKET_BASE),
    logPos(0),
    msgQueue(xQueueCreate(25,JSON_BUFFER_SIZE)),
    emptyString(0),
    msgQueueBuf((ws_msg_t*)dmalloc(sizeof(ws_msg_t)*25)),
    msgQueueBufLen(25),
    msgQueueBufPos(0)
{
  memset(msgQueueBuf,0,sizeof(ws_msg_t)*25);
  stateHandler=this;
  memset(&clients,0,sizeof(clients));
  jClients = cJSON_HasObjectItem(status,"Clients")?cJSON_GetObjectItem(status,"Clients"):cJSON_AddArrayToObject(status,"Clients");
  jIsLive = cJSON_HasObjectItem(status,"IsLive") ? cJSON_GetObjectItem(status,"IsLive") : cJSON_AddNumberToObject(status,"IsLive",true);
  cJSON_SetNumberValue(jIsLive,true);
  ESP_LOGI(__PRETTY_FUNCTION__,"Created Websocket");
  CreateBackgroundTask(QueueHandler,"WebsocketQH",4096, nullptr, tskIDLE_PRIORITY, nullptr);
  ESP_LOGI(__PRETTY_FUNCTION__,"Created WebsocketQH");
  CreateBackgroundTask(StatePoller,"StatePoller",4096, stateHandler, tskIDLE_PRIORITY, nullptr);
  ESP_LOGI(__PRETTY_FUNCTION__,"Created StatePoller");
  registerLogCallback(LogCallback,stateHandler);
  ESP_LOGI(__PRETTY_FUNCTION__,"Log callback registered");
};

WebsocketManager::~WebsocketManager(){
  ESP_LOGI(__PRETTY_FUNCTION__,"State Poller Done");
  unregisterLogCallback(LogCallback);
  ESP_LOGI(__PRETTY_FUNCTION__,"Unregistered log callback");
  vQueueDelete(msgQueue);
  ldfree(msgQueueBuf);

  cJSON_SetIntValue(cJSON_GetObjectItem(status,"IsLive"),false);
  cJSON_DeleteItemFromObject(status,"Clients");
  stateHandler=nullptr;
}

void WebsocketManager::PostToClient(void* msg) {
  if (!msg) {
    ESP_LOGV(__PRETTY_FUNCTION__,"Missing message");
    return;
  }
  esp_err_t ret;
  struct tm timeinfo;

  ws_msg_t* wsMsg = (ws_msg_t*)msg;
  httpd_ws_frame_t ws_pkt = {
            .final = true,
            .fragmented = false,
            .type = wsMsg->msgType,
            .payload = (uint8_t *)wsMsg->buf,
            .len = msg == wsMsg->client->pingMsg ? 0 : wsMsg->len
        };

  if ((ret = httpd_ws_send_frame_async(wsMsg->client->hd,wsMsg->client->fd, &ws_pkt)) != ESP_OK) {
    cJSON_SetIntValue(wsMsg->client->jErrCount,wsMsg->client->jErrCount->valueint+1);
    if (wsMsg->client->jErrCount->valueint > 10){
      cJSON_SetIntValue(wsMsg->client->jIsLive,false);
      httpd_sess_trigger_close(wsMsg->client->hd, wsMsg->client->fd);
      wsMsg->client->fd=0;
      wsMsg->client->hd=nullptr;
      stateHandler->jIsLive->valueint = false;
      for (int idx = 0; idx < 5; idx++){
        if (stateHandler->clients[idx].jIsLive != nullptr) {
          stateHandler->jIsLive->valueint |= stateHandler->clients[idx].jIsLive->valueint;
        }
      }
      ESP_LOGW(__PRETTY_FUNCTION__,"Client disconnected: %s",esp_err_to_name(ret));
      AppConfig::SignalStateChange(state_change_t::MAIN);
    }
  } else {
    cJSON_SetIntValue(wsMsg->client->jBytesOut,wsMsg->client->jBytesOut->valueint+ws_pkt.len);
    time(&wsMsg->client->lastTs);
    localtime_r(&wsMsg->client->lastTs, &timeinfo);
    strftime(wsMsg->client->jLastTs->valuestring, 30, "%c", &timeinfo);
    ESP_LOGV(__PRETTY_FUNCTION__,"Client %s - %zu bytes:%s", wsMsg->client->jLastTs->valuestring, ws_pkt.len,(char*)ws_pkt.payload);
    if (wsMsg->postHook != nullptr) {
      wsMsg->postHook(wsMsg->postHookParam);
    }
  }
}

WebsocketManager::ws_msg_t* WebsocketManager::getNewMessage(){
  return &msgQueueBuf[msgQueueBufPos >= (msgQueueBufLen-1) ? (msgQueueBufPos=0) : msgQueueBufPos++];
}

void WebsocketManager::ProcessMessage(uint8_t* msg){
  esp_err_t ret;
  
  for (uint8_t idx = 0; idx < 5; idx++){
    if (stateHandler && clients[idx].jIsLive && clients[idx].jIsLive->valueint){
      ws_msg_t* wsMsg = msg ? getNewMessage() : clients[idx].pingMsg;
      if (msg) {
        if ((wsMsg->msgType != HTTPD_WS_TYPE_TEXT) || (wsMsg->bufLen < strlen((const char*)msg))) {
          if ((wsMsg->msgType == HTTPD_WS_TYPE_TEXT) && wsMsg->buf) {
            ldfree(wsMsg->buf);
          }
          wsMsg->msgType = HTTPD_WS_TYPE_TEXT;
          wsMsg->bufLen = strlen((const char*)msg)+255;
          wsMsg->buf = dmalloc(wsMsg->bufLen);
        }
        memset(wsMsg->buf,0,wsMsg->bufLen);
        strcpy((char*)wsMsg->buf,(const char*)msg);
        wsMsg->postHook = nullptr;
        wsMsg->postHookParam = nullptr;
        wsMsg->client=&clients[idx];
        wsMsg->len = strlen((const char*)msg);
      }
      if ((ret = httpd_queue_work(clients[idx].hd,PostToClient,wsMsg)) != ESP_OK) {
        cJSON_SetIntValue(clients[idx].jIsLive,false);
        httpd_sess_trigger_close(clients[idx].hd, clients[idx].fd);
        clients[idx].fd=0;
        clients[idx].hd=nullptr;
        for (int idx2 = 0; idx2 < 5; idx2++){
          stateHandler->jIsLive->valueint |= clients[idx2].jIsLive->valueint;
        }
        AppConfig::SignalStateChange(state_change_t::MAIN);
        ESP_LOGW(__PRETTY_FUNCTION__,"Client %d disconnected: %s",idx, esp_err_to_name(ret));
      } else if (msg) {
          ESP_LOGV(__PRETTY_FUNCTION__,"Client %d:%s",idx,(char*) msg);
      }
    }
  }
}

void WebsocketManager::QueueHandler(void* param){
  ESP_LOGI(__PRETTY_FUNCTION__,"QueueHandler Starting");
  auto* buf = (uint8_t*)dmalloc(JSON_BUFFER_SIZE);
  while(stateHandler && stateHandler->msgQueue && stateHandler->jIsLive->valueint){
    memset(buf,0,JSON_BUFFER_SIZE);
    if (xQueueReceive(stateHandler->msgQueue,buf,2800/portTICK_PERIOD_MS)) {
      stateHandler->ProcessMessage(buf);
    } else {
      stateHandler->ProcessMessage(nullptr);
    }
  }
  ESP_LOGI(__PRETTY_FUNCTION__,"QueueHandler Done");
  ldfree(buf);
}

bool WebsocketManager::HasOpenedWs(){
  return stateHandler ? stateHandler->jIsLive->valueint : false;
}

int WebsocketManager::RegisterClient(httpd_handle_t hd,int fd){
  cJSON_SetNumberValue(stateHandler->jIsLive, true);
  for (uint8_t idx = 0; idx < 5; idx++){
    if ((clients[idx].hd == hd) && (clients[idx].fd == fd)){
      ESP_LOGV(__PRETTY_FUNCTION__,"Client was at position %d",idx);
      cJSON_SetIntValue(clients[idx].jIsLive, true);
      cJSON_SetIntValue(clients[idx].jErrCount, 0);
      //AppConfig::SignalStateChange(state_change_t::MAIN);
      return idx;
    }
  }
  for (uint8_t idx = 0; idx < 5; idx++){
    if (!clients[idx].jIsLive || !clients[idx].jIsLive->valueint){
      clients[idx].hd = hd;
      clients[idx].fd = fd;
      if (!clients[idx].pingMsg) {
        clients[idx].pingMsg = (ws_msg_t*)dmalloc(sizeof(ws_msg_t));
        memset(clients[idx].pingMsg, 0, sizeof(ws_msg_t));
        clients[idx].pingMsg->buf=nullptr;
        clients[idx].pingMsg->bufLen=0;
        clients[idx].pingMsg->msgType = HTTPD_WS_TYPE_TEXT;
      }
      clients[idx].pingMsg->client=&clients[idx];

      if (clients[idx].jErrCount == nullptr) {
        ESP_LOGI(__PRETTY_FUNCTION__,"Client is inserted at position %d",idx);
        cJSON* client = cJSON_CreateObject();
        cJSON_AddItemToArray(jClients,client);
        clients[idx].jConnectTs = cJSON_AddStringToObject(client,"ConnectTs",emptySpace);
        clients[idx].jErrCount = cJSON_AddNumberToObject(client,"Errors",0);
        clients[idx].jBytesIn = cJSON_AddNumberToObject(client,"BytesIn",0);
        clients[idx].jBytesOut = cJSON_AddNumberToObject(client,"BytesOut",0);
        clients[idx].jIsLive = cJSON_AddNumberToObject(client,"IsLive",true);
        clients[idx].jAddr = cJSON_AddStringToObject(client,"Address",emptySpace);
        clients[idx].jLastTs = cJSON_AddStringToObject(client,"LastTs",emptySpace);
      } else {
        ESP_LOGI(__PRETTY_FUNCTION__,"Client took position %d",idx);
      }

      sockaddr_in6 addr;
      socklen_t addr_size = sizeof(addr);
      if (getpeername(fd, (struct sockaddr *)&addr, &addr_size) < 0) {
        ESP_LOGW(__PRETTY_FUNCTION__, "Error getting client IP");
      } else {
        inet_ntop(AF_INET6, &addr.sin6_addr, clients[idx].jAddr->valuestring, 30);
      }

      cJSON_SetNumberValue(clients[idx].jErrCount,0);
      cJSON_SetNumberValue(clients[idx].jBytesIn, 0);
      cJSON_SetNumberValue(clients[idx].jBytesOut, 0);
      cJSON_SetNumberValue(clients[idx].jIsLive, true);

      struct tm timeinfo;
      time(&clients[idx].lastTs);
      localtime_r(&clients[idx].lastTs, &timeinfo);
      strftime(clients[idx].jConnectTs->valuestring, 30, "%c", &timeinfo);

      AppConfig::SignalStateChange(state_change_t::MAIN);
      return idx;
    }
  }
  ESP_LOGI(__PRETTY_FUNCTION__,"Client could not be added");
  return -1;
}

time_t GetTime() {
  return esp_timer_get_time() / 1000LL;
}

bool WebsocketManager::LogCallback(void* instance, char* logData){
  if (stateHandler && stateHandler->jIsLive && stateHandler->jIsLive->valueint && logData) {
    return xQueueSend(stateHandler->msgQueue,logData,portMAX_DELAY);
  }
  return true;
};

bool WebsocketManager::EventCallback(char *event){
  if (stateHandler && stateHandler->jIsLive->valueint && event) {
    return xQueueSend(stateHandler->msgQueue,event,portMAX_DELAY);
  }
  return false;
}

void WebsocketManager::StatePoller(void* instance){
  EventGroupHandle_t stateEg = AppConfig::GetStateGroupHandle();
  ESP_LOGI(__PRETTY_FUNCTION__,"State poller started");
  EventBits_t bits = 0;
  cJSON* mainState = AppConfig::GetAppStatus()->GetJSONConfig(nullptr);
  cJSON* gpsState = AppConfig::GetAppStatus()->GetJSONConfig("gps");;
  cJSON* wifiState = AppConfig::GetAppStatus()->GetJSONConfig("wifi");
  cJSON* item;

  auto* stateBuffer = (char*)dmalloc(JSON_BUFFER_SIZE*2);
  while (stateHandler && stateHandler->jIsLive->valueint) {
    bits = xEventGroupWaitBits(stateEg,0xff,pdTRUE,pdFALSE,portMAX_DELAY);
    if (stateHandler && stateHandler->jIsLive->valueint){
      cJSON* state = cJSON_Duplicate(TheRest::status_json(),true);
      if (bits&state_change_t::GPS) {
        ESP_LOGV(__PRETTY_FUNCTION__,"gState Changed %d", bits);
        cJSON_AddItemReferenceToObject(state,"gps",gpsState);
      } else if (bits&state_change_t::THREADS) {
        cJSON_AddItemReferenceToObject(state,"tasks",TheRest::tasks_json());
      } else if (bits&state_change_t::WIFI) {
        cJSON_AddItemReferenceToObject(state,"wifi",wifiState);
      } else {
        cJSON_ArrayForEach(item,mainState) {
            cJSON_AddItemReferenceToObject(state,item->string,item);//AppConfig::GetAppStatus()->GetJSONConfig(item->string));
        }
      }
      char* jbuf = stateBuffer + (xPortGetCoreID()*JSON_BUFFER_SIZE);
      if (cJSON_PrintPreallocated(state,jbuf,JSON_BUFFER_SIZE,pdFALSE)){
        if (xQueueSend(stateHandler->msgQueue,jbuf,portMAX_DELAY) == pdTRUE) {
          ESP_LOGV(__PRETTY_FUNCTION__,"State sent %zu",strlen(jbuf));
        } else {
          ESP_LOGW(__PRETTY_FUNCTION__,"Failed to queue state");
        }
      } else {
        ESP_LOGV(__PRETTY_FUNCTION__,"Failed to stringify state buffer null:%d state null:%d",jbuf==nullptr,state==nullptr);
      }
      cJSON_free(state);
    }
  }
  ldfree(stateBuffer);
  ESP_LOGI(__PRETTY_FUNCTION__,"State poller done, waiting 10 seconds to die");
  vTaskDelay(10000/portTICK_RATE_MS);
  delete stateHandler;
}


esp_err_t TheRest::ws_handler(httpd_req_t *req) {
  ESP_LOGV(__PRETTY_FUNCTION__, "WEBSOCKET Session");

  if (stateHandler == nullptr) {
    ESP_LOGI(__PRETTY_FUNCTION__, "Staring Manager");
    stateHandler = new WebsocketManager();
    ESP_LOGI(__PRETTY_FUNCTION__, "Manager Started");
  }

  if (req->method == HTTP_GET) {
        ESP_LOGV(__PRETTY_FUNCTION__, "Websocket get request");
        return ESP_OK;
  }

  int clientIdx = -1;
  if ((clientIdx = stateHandler->RegisterClient(req->handle,httpd_req_to_sockfd(req))) != -1){
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
          printf("text msg(%zu):%s\n",ws_pkt.len,ws_pkt.payload ? (char*)ws_pkt.payload : "null");
          if (startsWith((char*)ws_pkt.payload, "STREAM:")) {
            Camera* cam = (Camera*)ManagedDevice::GetByName((char*)ws_pkt.payload+7);
            if (cam) {
              camera_ws_stream_t request;
              request.camera = cam;
              request.client = &stateHandler->clients[clientIdx];
              cam->streamVideoToWs(&request);
            } else {
              ESP_LOGE(__PRETTY_FUNCTION__,"Bad camera name");
            }
          }
          break;
        case HTTPD_WS_TYPE_CONTINUE:  printf("Packet type(%zu): CONTINUE\n: %s",ws_pkt.len, (char*)ws_pkt.payload); break;
        case HTTPD_WS_TYPE_BINARY:    printf("Packet type(%zu): BINARY\n",ws_pkt.len);   break;
        case HTTPD_WS_TYPE_CLOSE:     printf("Packet type(%zu): CLOSE\n",ws_pkt.len);    break;
        case HTTPD_WS_TYPE_PING:      printf("Packet type(%zu): PING\n",ws_pkt.len);     break;
        case HTTPD_WS_TYPE_PONG:      printf("Packet type(%zu): PONG\n",ws_pkt.len);     break;
        default:                      printf("Packet type(%zu): UNKNOWN\n",ws_pkt.len);  break;
      }
    } else {
      ESP_LOGE(__PRETTY_FUNCTION__, "httpd_ws_recv_frame failed: %s", esp_err_to_name(ret));
    }
    ldfree(ws_pkt.payload);
    return ESP_OK;
  }

  ESP_LOGW(__PRETTY_FUNCTION__,"Client cannot be connected");
  return ESP_FAIL;
}
