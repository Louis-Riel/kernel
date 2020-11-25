
#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>
#include "esp_websocket_client.h"

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
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(__FUNCTION__, "WEBSOCKET_EVENT_ERROR");
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
    ESP_LOGD(__FUNCTION__, "WEBSOCKET_EVENT_DATA");
    char* charbuf = (char*)malloc(JSON_BUFFER_SIZE);
    *charbuf=0;
    uint32_t slen;
    esp_err_t ret;
    if (req) {
        while ((slen=httpd_req_recv(req,charbuf,JSON_BUFFER_SIZE)) >= 0) {
            if (slen > 0){
                *(charbuf+slen)=0;
                ESP_LOGD(__FUNCTION__,"%s",charbuf);
                if ((ret=httpd_resp_send_chunk(req,charbuf,slen)) != ESP_OK){
                    ESP_LOGE(__FUNCTION__,"Error sending chunk %s",charbuf);
                }
                if (strcmp(charbuf,"exit")==0) {
                    break;
                }
            } else {
                vTaskDelay(100);
            }
        }
        ESP_LOGD(__FUNCTION__,"All done");
    }
    free(charbuf);
    if ((ret=httpd_resp_send_chunk(req,NULL,0)) != ESP_OK){
        ESP_LOGE(__FUNCTION__,"Error sending final chunk %s",esp_err_to_name(ret));
    }
    return ESP_OK;
}
