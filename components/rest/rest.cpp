#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>
#include "../../main/utils.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

char* getPostField(const char* pname, const char* postData,char* dest) {
    char* param=strstr(postData,pname);
    if (param) {
        uint16_t plen=strlen(pname)+1;
        char* endPos=strstr(param,"&");
        if (endPos) {
            memcpy(dest,param+plen,endPos-param-plen);
            dest[endPos-param-plen]=0;
        } else {
            strcpy(dest,param+plen);
        }
    } else {
        return NULL;
    }
    return dest;
}

esp_err_t filedownload_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(__FUNCTION__, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_CONNECTED");
        xEventGroupClearBits(TheWifi::GetEventGroup(),DOWNLOAD_STARTED);
        xEventGroupClearBits(TheWifi::GetEventGroup(),DOWNLOAD_FINISHED);
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        if (strcmp(evt->header_key,"filename") == 0) {
            char* fname = NULL;
            if (evt->user_data != NULL) {
                if (*((char*)evt->user_data) == '/') {
                    fname = (char*)evt->user_data;
                    ESP_LOGD(__FUNCTION__,"Saving as override %s", fname);
                } else if (*((int*)evt->user_data) == 0) {
                    fname = evt->header_value;
                    ESP_LOGD(__FUNCTION__,"Saving as %s", fname);
                } else {
                    ESP_LOGW(__FUNCTION__, "We got dup headers");
                    break;
                }
            } else {
                ESP_LOGE(__FUNCTION__,"Downloader not properly setup");
            }
            FILE* dfile = fOpenCd(fname,"w",true);
            if (dfile == NULL) {
                ESP_LOGE(__FUNCTION__,"Error whiilst opening %s",evt->header_value);
                return ESP_FAIL;
            }
            *((FILE**)evt->user_data) = dfile;
            xEventGroupSetBits(TheWifi::GetEventGroup(),DOWNLOAD_STARTED);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if ((evt->user_data != NULL) && (*((char*)evt->user_data) != '/')) {
            ESP_LOGV(__FUNCTION__,"%d bytes",evt->data_len);
            fWrite(evt->data,1,evt->data_len,*((FILE**)evt->user_data));
        } else {
            ESP_LOGW(__FUNCTION__,"Data with no dest file %d bytes",evt->data_len);
            if (evt->data_len < 200) {
                ESP_LOGW(__FUNCTION__,"%s",(char*)evt->data);
            }
            return ESP_FAIL;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_FINISH ");
        xEventGroupSetBits(TheWifi::GetEventGroup(),DOWNLOAD_FINISHED);
        if ((evt->user_data != NULL) && (*((char*)evt->user_data) != '/')) {
            fClose(*(FILE**)evt->user_data);
        } else {
            ESP_LOGW(__FUNCTION__,"Close file with no dest file %d bytes",evt->data_len);
            return ESP_FAIL;
        }
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}

void restSallyForth(void *pvParameter) {
    if (TheRest::GetServer() == NULL) {
        TheRest::GetServer(pvParameter);
    }
    deviceId?deviceId:deviceId=AppConfig::GetAppConfig()->GetIntProperty("deviceid");
}
