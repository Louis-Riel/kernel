
#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>
#include "esp_http_server.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

void websocket_callback(uint8_t num,WEBSOCKET_TYPE_t type,char* msg,uint64_t len) {
  const static char* TAG = "websocket_callback";
  int value;

  switch(type) {
    case WEBSOCKET_CONNECT:
      ESP_LOGI(TAG,"client %i connected!",num);
      break;
    case WEBSOCKET_DISCONNECT_EXTERNAL:
      ESP_LOGI(TAG,"client %i sent a disconnect message",num);
      break;
    case WEBSOCKET_DISCONNECT_INTERNAL:
      ESP_LOGI(TAG,"client %i was disconnected",num);
      break;
    case WEBSOCKET_DISCONNECT_ERROR:
      ESP_LOGI(TAG,"client %i was disconnected due to an error",num);
      break;
    case WEBSOCKET_TEXT:
      if(len) { // if the message length was greater than zero
        switch(msg[0]) {
          case 'L':
            if(sscanf(msg,"L%i",&value)) {
              ESP_LOGI(TAG,"LED value: %i",value);
              ws_server_send_text_all_from_callback(msg,len); // broadcast it!
            }
            break;
          case 'M':
            ESP_LOGI(TAG, "got message length %i: %s", (int)len-1, &(msg[1]));
            break;
          default:
	          ESP_LOGI(TAG, "got an unknown message with length %i", (int)len);
	          break;
        }
      }
      break;
    case WEBSOCKET_BIN:
      ESP_LOGI(TAG,"client %i sent binary message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PING:
      ESP_LOGI(TAG,"client %i pinged us with message of size %i:\n%s",num,(uint32_t)len,msg);
      break;
    case WEBSOCKET_PONG:
      ESP_LOGI(TAG,"client %i responded to the ping",num);
      break;
  }
}

void wsSession(void* param) {
    httpd_req_t *req = (httpd_req_t *)param;
    char* charbuf = (char*)malloc(JSON_BUFFER_SIZE);
    *charbuf=0;
    uint32_t slen;
    esp_err_t ret;
    if (req) {
        while ((slen=httpd_req_recv(req,charbuf,JSON_BUFFER_SIZE)) != 0) {
            ESP_LOGD(__FUNCTION__,"%s",charbuf);
            if (strcmp(charbuf,"exit")==0) {
                break;
            }
            if ((ret=httpd_resp_send_chunk(req,charbuf,slen)) != ESP_OK){
                ESP_LOGE(__FUNCTION__,"Error sending chunk %s",charbuf);
            }
        }
    }
    free(req);
    if ((ret=httpd_resp_send_chunk(req,NULL,0)) != ESP_OK){
        ESP_LOGE(__FUNCTION__,"Error sending final chunk %s",esp_err_to_name(ret));
    }
}

int sessionManager(httpd_handle_t hd, int sockfd){
    return 500;
}


esp_err_t ws_handler(httpd_req_t *req){
    ESP_LOGD(__FUNCTION__, "WEBSOCKET Session");
    char* charbuf = (char*)malloc(JSON_BUFFER_SIZE);
    *charbuf=0;
    uint32_t slen;
    esp_err_t ret;
    if (req) {
        httpd_ws_frame_t ws_pkt;
        //ws_server_add_client(req->conn,charbuf,JSON_BUFFER_SIZE,req->uri,websocket_callback);
    }
    if ((ret=httpd_resp_send_chunk(req,NULL,0)) != ESP_OK){
        ESP_LOGE(__FUNCTION__,"Error sending final chunk %s",esp_err_to_name(ret));
    }
    return ESP_OK;
}
