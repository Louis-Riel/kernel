#include "./route.h"
#include "esp_debug_helpers.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

static TheRest *restInstance = NULL;

void restSallyForth(void *pvParameter) {
    if (TheRest::GetServer() == NULL) {
        TheRest::GetServer(pvParameter);
    }
    deviceId?deviceId:deviceId=AppConfig::GetAppConfig()->GetIntProperty("deviceid");
}

TheRest::TheRest(AppConfig *config, EventGroupHandle_t evtGrp)
    : ManagedDevice("TheRest","TheRest",BuildStatus,HealthCheck),
      processingTime(0),
      numRequests(0),
      eventGroup(xEventGroupCreate()),
      wifiEventGroup(evtGrp),
      restConfig(HTTPD_DEFAULT_CONFIG()),
      gwAddr(NULL),
      ipAddr(NULL),
      app_eg(getAppEG())
{
    if (restInstance == NULL)
    {
        deviceId = AppConfig::GetAppConfig()->GetIntProperty("deviceid");
        ESP_LOGV(__FUNCTION__, "First Rest for %d", deviceId);
        restInstance = this;
    }
    if (xEventGroupGetBits(eventGroup) & HTTP_SERVING)
    {
        ESP_LOGV(__FUNCTION__, "Not starting httpd, already serving");
        return;
    }
    //xEventGroupWaitBits(wifiEventGroup, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (handlerDescriptors == NULL)
        EventManager::RegisterEventHandler((handlerDescriptors = BuildHandlerDescriptors()));

    ESP_LOGV(__FUNCTION__, "Getting Ip for %d", deviceId);

    if ((gwAddr == NULL) && (ipAddr == NULL))
    {
        TheWifi *theWifi = TheWifi::GetInstance();
        ipAddr = (char *)malloc(16);
        gwAddr = (char *)malloc(16);
        sprintf(ipAddr, IPSTR, IP2STR(&theWifi->staIp.ip));
        sprintf(gwAddr, IPSTR, IP2STR(&theWifi->staIp.gw));
        ESP_LOGD(__FUNCTION__, "Ip:%s Gw:%s", ipAddr, gwAddr);
    }

    restConfig.uri_match_fn = this->routeHttpTraffic;
    ESP_LOGI(__FUNCTION__, "Starting server on port %d ip%s gw:%s", restConfig.server_port, ipAddr, gwAddr);
    esp_err_t ret = ESP_FAIL;
    if ((ret = httpd_start(&server, &restConfig)) == ESP_OK)
    {
        ESP_LOGV(__FUNCTION__, "Registering URI handlers");
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wsUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restPostUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restPutUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &appUri));
        xEventGroupSetBits(eventGroup, HTTP_SERVING);
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Error starting server! %s", esp_err_to_name(ret));
    }
}

TheRest::~TheRest()
{
    vEventGroupDelete(eventGroup);
    if (handlerDescriptors != NULL)
        EventManager::UnRegisterEventHandler((handlerDescriptors = BuildHandlerDescriptors()));

    httpd_stop(server);
    xEventGroupClearBits(getAppEG(), app_bits_t::REST);
    restInstance=NULL;
}

bool TheRest::routeHttpTraffic(const char *reference_uri, const char *uri_to_match, size_t match_upto)
{
    sampleBatteryVoltage();
    if ((strlen(reference_uri) == 1) && (reference_uri[0] == '*'))
    {
        ESP_LOGV(__FUNCTION__,"* match for %s",uri_to_match);
        if (restInstance) {
            restInstance->numRequests++;
        }
        return true;
    }

    size_t tLen = strlen(reference_uri);
    size_t sLen = strlen(uri_to_match);
    sLen = sLen > match_upto ? match_upto : sLen;
    bool matches = true;
    uint8_t tc;
    uint8_t sc;
    int32_t sidx = 0;
    int32_t tidx = 0;
    bool eot = false;
    bool eos = false;

    while (matches && (sidx < sLen))
    {
        tc = reference_uri[tidx];
        sc = uri_to_match[sidx];
        if (tidx >= 0)
        {
            if (tc == '*')
            {
                //ESP_LOGV(__FUNCTION__,"Match on wildcard");
                break;
            }
            if (!eot && !eos && (tc != sc))
            {
                //ESP_LOGV(__FUNCTION__,"Missmatch on tpos:%d spos:%d %c!=%c",tidx,sidx,tc,sc);
                matches = false;
                break;
            }
            if (tidx < tLen)
            {
                tidx++;
            }
            else
            {
                eot = true;
                if (tc == '/')
                {
                    break;
                }
                //ESP_LOGV(__FUNCTION__,"Missmatch slen being longer at tpos:%d tlen:%d spos:%d slen:%d",tidx,tLen,sidx,sLen);
                matches = false;
                break;
            }
            if (sidx < (sLen - 1))
            {
                sidx++;
            }
            else
            {
                eos = true;
                if ((tLen == sLen) ||
                    ((sLen == (tLen - 1)) && (reference_uri[tLen - 1] == '/')) ||
                    ((sLen == (tLen - 1)) && (reference_uri[tLen - 1] == '*')))
                {
                    break;
                }
                //ESP_LOGV(__FUNCTION__,"Missmatch slen being sorter at tpos:%d spos:%d",tidx,sidx);
                matches = false;
                break;
            }
        }
    }
    ESP_LOGV(__FUNCTION__,"%s ref:%s uri:%s",matches ? "matches" : "no match",reference_uri,uri_to_match);
    if (matches && restInstance) {
        restInstance->numRequests++;
    }
    return matches;
}

TheRest *TheRest::GetServer()
{
    deviceId ? deviceId : deviceId = AppConfig::GetAppConfig()->GetIntProperty("deviceid");
    return restInstance;
}

void TheRest::GetServer(void *evtGrp)
{
    if (restInstance == NULL)
        restInstance = new TheRest(AppConfig::GetAppConfig(), (EventGroupHandle_t)evtGrp);
}

void TheRest::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    ESP_LOGV(__FUNCTION__, "Event %s-%d", base, id);

    switch (id)
    {
    case restEvents_t::CHECK_UPGRADE:
        break;
    case restEvents_t::SEND_STATUS:
        break;
    case restEvents_t::UPDATE_CONFIG:
        break;
    default:
        break;
    }
}

char *TheRest::PostRequest(const char *url, size_t *len)
{
    return SendRequest(url, esp_http_client_method_t::HTTP_METHOD_POST, len);
}

char *TheRest::GetRequest(const char *url, size_t *len)
{
    return SendRequest(url, esp_http_client_method_t::HTTP_METHOD_GET, len);
}

char *TheRest::SendRequest(const char *url, esp_http_client_method_t method, size_t *len)
{
    return SendRequest(url, method, len, NULL);
}

cJSON* TheRest::BuildStatus(void* instance){
    TheRest* theRest = (TheRest*)instance;

    cJSON* sjson = NULL;
    AppConfig* apin = new AppConfig(sjson=ManagedDevice::BuildStatus(instance),AppConfig::GetAppStatus());
    apin->SetIntProperty("numRequests",theRest->numRequests);
    apin->SetIntProperty("processingTime",theRest->processingTime);
    delete apin;
    return sjson;
}


char *TheRest::SendRequest(const char *url, esp_http_client_method_t method, size_t *len, char *charBuf)
{
    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t *config = NULL;
    bool isPreAllocated = charBuf != NULL;

    config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = url;
    config->method = method;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;
    ESP_LOGD(__FUNCTION__, "Getting %s", config->url);
    esp_err_t err = ESP_ERR_HW_CRYPTO_BASE;
    int hlen = 0;
    int retCode = -1;
    size_t totLen = isPreAllocated ? *len : 0;
    
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    if ((client = esp_http_client_init(config)) &&
        ((err = esp_http_client_open(client, 0)) == ESP_OK) &&
        ((hlen = esp_http_client_fetch_headers(client)) >= 0) &&
        ((retCode = esp_http_client_get_status_code(client)) == 200))
    {
        if (!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(__FUNCTION__,"bcaps integrity error");
        }
        int bufLen = isPreAllocated ? *len : hlen > 0 ? hlen + 1
                                                      : JSON_BUFFER_SIZE;
        char *retVal = isPreAllocated ? charBuf : (char *)dmalloc(bufLen);

        int chunckLen = isPreAllocated ? min(JSON_BUFFER_SIZE, *len) : hlen ? hlen
                                                                            : JSON_BUFFER_SIZE;
        *len = 0;
        memset(retVal, 0, bufLen);
        ESP_LOGV(__FUNCTION__, "Downloading isPreAllocated:%d hlen:%d chunckLen:%d buflen:%d len: %d bytes of data for %s", isPreAllocated, hlen, chunckLen, bufLen, (int)*len, url);
        if (!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(__FUNCTION__,"bcaps integrity error");
        }
        while ((chunckLen = esp_http_client_read(client, retVal + *len, chunckLen)) > 0)
        {
            ESP_LOGV(__FUNCTION__, "Chunck %d bytes of data for %s. Totlen:%d", chunckLen, url, *len);
            *len += chunckLen;
            if (!isPreAllocated || (isPreAllocated && (*len < totLen)))
                memset(retVal + *len, 0, 1);
            if (!heap_caps_check_integrity_all(true)) {
                ESP_LOGE(__FUNCTION__,"bcaps integrity error");
            }
        }
        if (!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(__FUNCTION__,"bcaps integrity error");
        }
        ESP_LOGV(__FUNCTION__, "Downloaded %d bytes of data from %s", *len, url);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        ldfree((void *)config);
        if (!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(__FUNCTION__,"bcaps integrity error");
        }
        return retVal;
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Failed sending request(%s). client is %snull, err:%s, hlen:%d, retCode:%d, len:%d", config->url, client ? "not " : "", esp_err_to_name(err), hlen, retCode, *len);
    }

    if (client){
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    ldfree((void *)config);
    return NULL;
}

cJSON *TheRest::PostJSonRequest(const char *url)
{
    cJSON *ret = NULL;
    size_t len = 0;
    ESP_LOGV(__FUNCTION__, "Posting (%s).", url);
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    char *sjson = PostRequest(url, &len);
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    if (len)
    {
        ESP_LOGV(__FUNCTION__, "Parsing (%s).", sjson);
        DisplayMemInfo();
        if (len && sjson && (ret = cJSON_ParseWithLength(sjson,len)))
        {
            ESP_LOGV(__FUNCTION__, "Got back from %d from (%s).", len, url);
        }
        else
        {
            ESP_LOGW(__FUNCTION__, "Got %d of nobueno bits for %s:%s.", len, url, sjson ? sjson : "*null*");
        }
        if (!heap_caps_check_integrity_all(true)) {
            ESP_LOGE(__FUNCTION__,"bcaps integrity error");
        }
        ldfree(sjson);
    }

    return ret;
}

cJSON *TheRest::GetDeviceConfig(esp_ip4_addr_t *ipInfo, uint32_t deviceId)
{
    char *url = (char *)dmalloc(100);
    cJSON *jret = NULL;

    if (deviceId)
        sprintf((char *)url, "http://" IPSTR "/config/%d", IP2STR(ipInfo), deviceId);
    else
        sprintf((char *)url, "http://" IPSTR "/config", IP2STR(ipInfo));
    ESP_LOGV(__FUNCTION__, "Getting config from %s", url);
    DisplayMemInfo();
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    jret = PostJSonRequest(url);
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    DisplayMemInfo();
    ldfree(url);
    return jret;
}

esp_err_t TheRest::SendConfig(char *addr, cJSON *cfg)
{
    esp_err_t ret = ESP_FAIL;
    esp_http_client_config_t *config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = (char *)dmalloc(60);
    sprintf((char *)config->url, "http://%s/config/%d", addr, deviceId);
    config->method = HTTP_METHOD_PUT;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;
    esp_http_client_handle_t client = esp_http_client_init(config);

    char *sjson = cJSON_PrintUnformatted(cfg);

    if ((ret = esp_http_client_open(client, strlen(sjson))) == ESP_OK)
    {
        if (esp_http_client_write(client, sjson, strlen(sjson)) != strlen(sjson))
        {
            ESP_LOGE(__FUNCTION__, "Config update failed: %s", esp_err_to_name(ret));
            ret = ESP_FAIL;
        }
        else
        {
            ESP_LOGV(__FUNCTION__, "Config sent %d bytes to %s", strlen(sjson), config->url);
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    else
    {
        ESP_LOGW(__FUNCTION__, "Send wifi off request failed: %s", esp_err_to_name(ret));
    }

    ldfree(sjson);
    free((void *)config->url);
    free(config);
    return ret;
}

void TheRest::MergeConfig(void *param)
{
    TheRest *rest = NULL;
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    while ((rest = restInstance) == NULL)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    xEventGroupWaitBits(rest->eventGroup, HTTP_SERVING, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGD(__FUNCTION__, "Merging Config");
    uint32_t addr = ipaddr_addr(rest->gwAddr);
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    cJSON *newCfg = rest->GetDeviceConfig((esp_ip4_addr *)&addr, deviceId);
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    cJSON *curCfg = AppConfig::GetAppConfig()->GetJSONConfig(NULL);
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
    if (newCfg)
    {
        if (!cJSON_Compare(newCfg, curCfg, true))
        {
            AppConfig::GetAppConfig()->SetAppConfig(newCfg);
            ESP_LOGD(__FUNCTION__, "Updated config from server");
            TheRest::SendConfig(rest->gwAddr, newCfg);
        }
        else
        {
            cJSON_free(newCfg);
            ESP_LOGD(__FUNCTION__, "No config update needed from server");
        }
    }
    else
    {
        ESP_LOGW(__FUNCTION__, "Cannot get config from " IPSTR " for devid %d", IP2STR((esp_ip4_addr *)&addr), deviceId);
        TheRest::SendConfig(rest->gwAddr, curCfg);
    }
    if (!heap_caps_check_integrity_all(true)) {
        ESP_LOGE(__FUNCTION__,"bcaps integrity error");
    }
}

void TheRest::SendStatus(void *param)
{
    TheRest *rest = NULL;
    while ((rest = restInstance) == NULL)
    {
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    xEventGroupWaitBits(rest->eventGroup, HTTP_SERVING, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGV(__FUNCTION__, "Sending status");
    esp_err_t ret = ESP_FAIL;
    uint32_t addr = ipaddr_addr(rest->gwAddr);
    esp_http_client_config_t *config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = (char *)dmalloc(60);
    sprintf((char *)config->url, "http://" IPSTR "/status/%d", IP2STR((esp_ip4_addr *)&addr), deviceId);
    config->method = HTTP_METHOD_PUT;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;
    esp_http_client_handle_t client = esp_http_client_init(config);

    cJSON *cstats = status_json();
    cJSON *jstats = tasks_json();
    cJSON *astats = AppConfig::GetAppStatus()->GetJSONConfig(NULL);
    cJSON *jtmp;
    cJSON_ArrayForEach(jtmp, cstats)
    {
        cJSON_AddItemReferenceToObject(astats, jtmp->string, jtmp);
    }
    cJSON_AddItemReferenceToObject(astats, "tasks", jstats);

    char *sjson = cJSON_PrintUnformatted(astats);
    cJSON_Delete(astats);
    cJSON_Delete(jstats);
    cJSON_Delete(cstats);

    if ((ret = esp_http_client_open(client, strlen(sjson))) == ESP_OK)
    {
        if (esp_http_client_write(client, sjson, strlen(sjson)) != strlen(sjson))
        {
            ESP_LOGE(__FUNCTION__, "Status update failed: %s", esp_err_to_name(ret));
        }
        else
        {
            ESP_LOGV(__FUNCTION__, "Status sent to %s", config->url);
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    else
    {
        ESP_LOGW(__FUNCTION__, "Send wifi off request failed: %s", esp_err_to_name(ret));
    }

    ldfree((void*)config->url);
    ldfree(config);
    ldfree(sjson);
}

EventHandlerDescriptor *TheRest::BuildHandlerDescriptors()
{
    ESP_LOGV(__FUNCTION__, "TheRest: BuildHandlerDescriptors");
    EventHandlerDescriptor *handler = ManagedDevice::BuildHandlerDescriptors();
    handler->AddEventDescriptor(restEvents_t::CHECK_UPGRADE, "CHECK_UPGRADE");
    handler->AddEventDescriptor(restEvents_t::SEND_STATUS, "SEND_STATUS");
    handler->AddEventDescriptor(restEvents_t::UPDATE_CONFIG, "UPDATE_CONFIG");
    return handler;
}

esp_err_t TheRest::rest_handler(httpd_req_t *req)
{
    ESP_LOGV(__FUNCTION__, "rest handling (%d)%s", req->method, req->uri);
    uint32_t idx = 0;

    for (const httpd_uri_t &theUri : restInstance->restUris)
    {
        idx++;
        if ((req->method == theUri.method) && (routeHttpTraffic(theUri.uri, req->uri, strlen(req->uri))))
        {
            ESP_LOGV(__FUNCTION__, "rest handled (%d)%s <- %s idx:%d", req->method, theUri.uri, req->uri, idx);
            time_t start = esp_timer_get_time();
            esp_err_t ret = theUri.handler(req);
            restInstance->processingTime+=(esp_timer_get_time()-start);
            return ret;
        }
    }
    ESP_LOGW(__FUNCTION__,"Cannot route %d %s",req->method,req->uri);
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

EventGroupHandle_t TheRest::GetEventGroup()
{
    return restInstance->eventGroup;
}

bool TheRest::HealthCheck(void* instance){
    TheRest* theRest = (TheRest*)instance;
    char* url = (char*)dmalloc(50);
    sprintf(url,"http://%s/status/",theRest->ipAddr);
    size_t len=0;
    char* resp = theRest->SendRequest(url,HTTP_METHOD_POST,&len);
    if ((len > 0)){
        cJSON* jtmp = cJSON_Parse(resp);
        AppConfig* atmp = new AppConfig(jtmp,NULL);

        if (atmp->HasProperty("freeram")){
            if ( atmp->GetIntProperty("freeram") < 500000) {
                ESP_LOGE(theRest->name,"Running low on memory");
                return false;
            }
        } else {
            ESP_LOGE(theRest->name,"Unexpected response from healthcheck");
            return false;
        }
        ESP_LOGV(theRest->name,"Req validated");
        ldfree(resp);
        ldfree(atmp);
        cJSON_Delete(jtmp);
        return ManagedDevice::HealthCheck(instance);
    }
    return false;
}