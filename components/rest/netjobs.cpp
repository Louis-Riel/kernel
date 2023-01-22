#include "route.h"

#define DATA_BUFFER_SIZE JSON_BUFFER_SIZE

struct netJobSession
{
    esp_http_client_config_t* config;
    char* sIpInfo;
    cJSON* neighbours;
    char* data;
    uint32_t len;
    uint32_t bufLen;
};

esp_err_t event_handler(esp_http_client_event_t *evt)
{
    netJobSession* job = (netJobSession*)evt->user_data;
    cJSON* jneighbor = NULL;
    cJSON* jState = NULL;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGV(__PRETTY_FUNCTION__, "HTTP_EVENT_ON_FINISH");
        jneighbor = cJSON_HasObjectItem(job->neighbours,job->sIpInfo) ? cJSON_GetObjectItem(job->neighbours,job->sIpInfo) : NULL;
        if (jneighbor) {
            ESP_LOGI(__PRETTY_FUNCTION__, "%s error",job->sIpInfo);
            if (cJSON_HasObjectItem(jneighbor,"reachable")) {
                cJSON_SetBoolValue(cJSON_GetObjectItem(jneighbor,"reachable"),cJSON_False);
            } else {
                cJSON_AddBoolToObject(jneighbor,"reachable",cJSON_False);
            }
        }
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGV(__PRETTY_FUNCTION__, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGV(__PRETTY_FUNCTION__, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGV(__PRETTY_FUNCTION__, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        if ((job->len + evt->data_len) < job->bufLen ){
            memcpy(job->data+job->len,evt->data,evt->data_len);
            job->len+=evt->data_len;
        } else {
            ESP_LOGE(__PRETTY_FUNCTION__,"Cannot fit %d in the ram. len: %d capacity:%d",evt->data_len,job->len,job->bufLen);
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        if (job->len > 0){
            jState = cJSON_ParseWithLength(job->data,job->len);
            if (jState) {
                jneighbor = cJSON_HasObjectItem(job->neighbours,job->sIpInfo) ? cJSON_GetObjectItem(job->neighbours,job->sIpInfo) : cJSON_AddObjectToObject(job->neighbours,job->sIpInfo);
                if (cJSON_HasObjectItem(jneighbor,"state")) {
                    cJSON_DeleteItemFromObject(jneighbor,"state");
                    ESP_LOGI(__PRETTY_FUNCTION__, "%s was found",job->sIpInfo);
                } else {
                    ESP_LOGI(__PRETTY_FUNCTION__, "%s is new",job->sIpInfo);
                }
                cJSON_AddItemToObject(jneighbor,"state",jState);
                if (cJSON_HasObjectItem(jneighbor,"reachable")) {
                    cJSON_SetBoolValue(cJSON_GetObjectItem(jneighbor,"reachable"),cJSON_True);
                } else {
                    cJSON_AddBoolToObject(jneighbor,"reachable",cJSON_True);
                }
                AppConfig::SignalStateChange(state_change_t::MAIN);
            }
        }
        ESP_LOGV(__PRETTY_FUNCTION__, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGV(__PRETTY_FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}

void GetStatus(netJobSession* job){
    sprintf((char*)job->config->url,"http://%s/status/",job->sIpInfo);
    job->config->method=HTTP_METHOD_POST;
    job->config->timeout_ms = 2000;
    job->config->event_handler = event_handler;
    job->config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    job->config->max_redirection_count=0;
    job->config->port=80;
    job->config->user_data=job;
    ESP_LOGV(__PRETTY_FUNCTION__,"Scanning %s",job->config->url);
    esp_http_client_handle_t client = esp_http_client_init(job->config);
    esp_err_t err;
    if ((err = esp_http_client_perform(client)) != ESP_OK)
    {
        ESP_LOGI(__PRETTY_FUNCTION__,"%s not there",job->config->url);
    }
    //h(client);
}

void TheRest::ScanNetwork(void *instance){
    if (cJSON_IsFalse(GetServer()->jScanning)){
        ESP_LOGI(__PRETTY_FUNCTION__,"Scanning Network");
        cJSON_SetBoolValue(GetServer()->jScanning,cJSON_True);
        TheWifi *theWifi = TheWifi::GetInstance();
        cJSON* status = GetServer()->status;

        netJobSession job;
        char* data = (char*)dmalloc(DATA_BUFFER_SIZE);
        char* sIpInfo = (char*)dmalloc(16);
        cJSON* neighbours = cJSON_HasObjectItem(status,"neighbours") ? cJSON_GetObjectItem(status,"neighbours") : cJSON_AddObjectToObject(status,"neighbours");

        esp_http_client_config_t config;
        char* url = (char*)dmalloc(255);

        memset(&job,0,sizeof(netJobSession));
        job.data=data;
        job.sIpInfo=sIpInfo;
        job.bufLen=DATA_BUFFER_SIZE;
        job.neighbours = neighbours;
        config.url=url;

        for (uint8_t idx = 100; idx < 255; idx++) {
            if (idx != esp_ip4_addr4_16(&theWifi->staIp.ip)) {
                memset(&config,0,sizeof(esp_http_client_config_t));
                memset(&job,0,sizeof(netJobSession));
                memset((void*)url,0,255);
                memset((void*)data,0,DATA_BUFFER_SIZE);
                sprintf(sIpInfo, "%d.%d.%d.%d", 
                                    esp_ip4_addr1_16(&theWifi->staIp.ip),
                                    esp_ip4_addr2_16(&theWifi->staIp.ip),
                                    esp_ip4_addr3_16(&theWifi->staIp.ip),
                                    idx);

                job.data=data;
                job.sIpInfo=sIpInfo;
                job.bufLen=DATA_BUFFER_SIZE;
                job.neighbours = neighbours;
                job.config=&config;
                config.url=url;
                GetStatus(&job);
            }
        }
        for (uint8_t idx = 1; idx < 100; idx++) {
            if (idx != esp_ip4_addr4_16(&theWifi->staIp.ip)) {
                memset(&config,0,sizeof(esp_http_client_config_t));
                memset(&job,0,sizeof(netJobSession));
                memset((void*)url,0,255);
                memset((void*)data,0,DATA_BUFFER_SIZE);
                sprintf(sIpInfo, "%d.%d.%d.%d", 
                                    esp_ip4_addr1_16(&theWifi->staIp.ip),
                                    esp_ip4_addr2_16(&theWifi->staIp.ip),
                                    esp_ip4_addr3_16(&theWifi->staIp.ip),
                                    idx);

                job.data=data;
                job.sIpInfo=sIpInfo;
                job.bufLen=DATA_BUFFER_SIZE;
                job.neighbours = neighbours;
                job.config=&config;
                config.url=url;
                GetStatus(&job);
            }
        }
        ESP_LOGI(__PRETTY_FUNCTION__,"Done Scanning Network");
        ldfree(data);
        ldfree(sIpInfo);
        ldfree(url);
    } else {
        ESP_LOGW(__PRETTY_FUNCTION__,"Already Scanning");
    }
}
