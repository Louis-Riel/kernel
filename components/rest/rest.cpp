#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>
#include "../../main/utils.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static httpd_handle_t server = NULL;

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
        xEventGroupClearBits(eventGroup,DOWNLOAD_STARTED);
        xEventGroupClearBits(eventGroup,DOWNLOAD_FINISHED);
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
            FILE* dfile = fopen(fname,"w",true);
            if (dfile == NULL) {
                ESP_LOGE(__FUNCTION__,"Error whiilst opening %s",evt->header_value);
                return ESP_FAIL;
            }
            *((FILE**)evt->user_data) = dfile;
            xEventGroupSetBits(eventGroup,DOWNLOAD_STARTED);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if ((evt->user_data != NULL) && (*((char*)evt->user_data) != '/')) {
            ESP_LOGV(__FUNCTION__,"%d bytes",evt->data_len);
            fwrite(evt->data,1,evt->data_len,*((FILE**)evt->user_data));
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
        xEventGroupSetBits(eventGroup,DOWNLOAD_FINISHED);
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

bool routeHttpTraffic(const char *reference_uri, const char *uri_to_match, size_t match_upto){
    sampleBatteryVoltage();
    if ((strlen(reference_uri)==1) && (reference_uri[0]=='*')) {
        //ESP_LOGV(__FUNCTION__,"* match for %s",uri_to_match);
        return true;
    }

    size_t tLen=strlen(reference_uri);
    size_t sLen=strlen(uri_to_match);
    sLen=sLen>match_upto?match_upto:sLen;
    bool matches=true;
    uint8_t tc;
    uint8_t sc;
    int32_t sidx=0;
    int32_t tidx=0;
    bool eot=false;
    bool eos=false;

    //ESP_LOGV(__FUNCTION__,"routing ref:%s uri:%s",reference_uri,uri_to_match);
    while (matches && (sidx<sLen)) {
        tc=reference_uri[tidx];
        sc=uri_to_match[sidx];
        if (tidx >= 0){
            if (tc=='*') {
                //ESP_LOGV(__FUNCTION__,"Match on wildcard");
                break;
            }
            if (!eot && !eos && (tc != sc)) {
                //ESP_LOGV(__FUNCTION__,"Missmatch on tpos:%d spos:%d %c!=%c",tidx,sidx,tc,sc);
                matches=false;
                break;
            }
            if (tidx < tLen){
                tidx++;
            } else {
                eot=true;
                if (tc=='/') {
                    break;
                }
                //ESP_LOGV(__FUNCTION__,"Missmatch slen being longer at tpos:%d tlen:%d spos:%d slen:%d",tidx,tLen,sidx,sLen);
                matches=false;
                break;
            }
            if (sidx < (sLen-1)){
                sidx++;
            } else {
                eos=true;
                if ((tLen == sLen) ||
                    ((sLen == (tLen-1)) && (reference_uri[tLen-1] == '/')) ||
                    ((sLen == (tLen-1)) && (reference_uri[tLen-1] == '*')) ) {
                    break;
                }
                //ESP_LOGV(__FUNCTION__,"Missmatch slen being sorter at tpos:%d spos:%d",tidx,sidx);
                matches=false;
                break;
            }
        }
    }
    return matches;
}

esp_err_t rest_handler(httpd_req_t *req)
{
    ESP_LOGV(__FUNCTION__,"rest handling (%d)%s",req->method, req->uri);
    uint32_t idx=0;
    for (const httpd_uri_t &theUri : restUris) {
        idx++;
        if ((req->method == theUri.method) && (routeHttpTraffic(theUri.uri, req->uri, strlen(req->uri)))){
            ESP_LOGV(__FUNCTION__,"rest handled (%d)%s <- %s idx:%d",req->method,theUri.uri, req->uri, idx);
            return theUri.handler(req);
        }
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

void restSallyForth(void *pvParameter) {
    assert(pvParameter);
    EventGroupHandle_t weg = (EventGroupHandle_t*)pvParameter;
    if(xEventGroupGetBits(eventGroup) & HTTP_SERVING){
        ESP_LOGD(__FUNCTION__,"Not starting httpd, already serving");
        vTaskDelete( NULL );
    }
    xEventGroupWaitBits(weg,WIFI_CONNECTED_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = routeHttpTraffic;
    ESP_LOGI(__FUNCTION__, "Starting server on port %d", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGD(__FUNCTION__, "Registering URI handlers");
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wsUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restPostUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restPutUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &appUri));
        xEventGroupSetBits(eventGroup,HTTP_SERVING);
        xEventGroupWaitBits(weg,WIFI_DISCONNECTED_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
        xEventGroupClearBits(eventGroup,HTTP_SERVING);
        ESP_LOGI(__FUNCTION__, "Stopping server on port %d", config.server_port);
        TheWifi::GetInstance()->wifiStop(NULL);
    } else {
        ESP_LOGE(__FUNCTION__, "Error starting server!");
    }
    
    vTaskDelete( NULL );
}
