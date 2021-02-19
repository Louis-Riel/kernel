
#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>
#include "esp_http_server.h"
#include "esp_websocket_client.h"
#include "../../main/logs.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static void websocket_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(__FUNCTION__, "WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(__FUNCTION__, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(__FUNCTION__, "WEBSOCKET_EVENT_DATA");
        ESP_LOGI(__FUNCTION__, "Received opcode=%d", data->op_code);
        ESP_LOGW(__FUNCTION__, "Received=%.*s", data->data_len, (char *)data->data_ptr);
        ESP_LOGW(__FUNCTION__, "Total payload length=%d, data_len=%d, current payload offset=%d\r\n", data->payload_len, data->data_len, data->payload_offset);

        //xTimerReset(shutdown_signal_timer, portMAX_DELAY);
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(__FUNCTION__, "WEBSOCKET_EVENT_ERROR");
        break;
    }
}

class ws_resp_arg {
  public:
    ws_resp_arg(httpd_handle_t hd,int fd){
      this->hd = hd;
      this->fd = fd;
      this->rdySem=xQueueCreate(10, JSON_BUFFER_SIZE);
      xTaskCreate(ws_resp_arg::logPumper,"logws",4096,this,tskIDLE_PRIORITY,&this->pumperTask);
    };
    static bool logCallback(void* instance, char* logData){
      ws_resp_arg* me = (ws_resp_arg*)instance;
      if (me->isLive){
        me->lastTs = GetTime();
        xQueueSend(me->rdySem,logData,portMAX_DELAY);
      }
      return me->isLive;
    };

  private:
    time_t lastTs;
    bool isLive = false;
    httpd_handle_t hd;
    TaskHandle_t pumperTask;
    int fd;
    uint32_t logPos = 0;
    QueueHandle_t rdySem;
    static time_t GetTime(){
      return esp_timer_get_time() / 1000LL;
    }
    static void logPumper(void* instance){
      ws_resp_arg* me = (ws_resp_arg*)instance;
      me->isLive = true;
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
        me->isLive = httpd_ws_send_frame_async(me->hd, me->fd, &ws_pkt) == ESP_OK;
      }
      ESP_LOGD(__FUNCTION__,"Logpumper Done");
      vQueueDelete(me->rdySem);
      ws_pkt.final = true;
      httpd_ws_send_frame_async(me->hd, me->fd, &ws_pkt);
      free(buf);
      vTaskDelete(NULL);
    }
};

void wsLogsSession(void* param) {
    ws_resp_arg* me = (ws_resp_arg*)param;
    registerLogCallback(me->logCallback,me);
}

int logsSessionManager(httpd_handle_t hd, httpd_req_t* req){
    return httpd_queue_work(hd, wsLogsSession, new ws_resp_arg(req->handle,httpd_req_to_sockfd(req)));
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
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Logs") == 0) {
        return logsSessionManager(req->handle, req);
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "httpd_ws_send_frame failed with %d", ret);
    }
    return ret;
}
