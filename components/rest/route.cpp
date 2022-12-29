#include "./route.h"
#include "esp_debug_helpers.h"
#include "eventmgr.h"
#include "pwdmgr.h"
#include <mbedtls/sha256.h>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

const char* TheRest::REST_BASE="Rest";

static TheRest *restInstance = nullptr;
static uint32_t deviceId = 0;

void restSallyForth(void *pvParameter) {
    if (TheRest::GetServer() == nullptr) {
        TheRest::GetServer(pvParameter);
    }
    deviceId?deviceId:deviceId=AppConfig::GetAppConfig()->GetIntProperty("deviceid");
}

TheRest::TheRest(AppConfig *config, EventGroupHandle_t evtGrp)
    :ManagedDevice(REST_BASE),
      postData((char*)dmalloc(JSON_BUFFER_SIZE)),
      eventGroup(xEventGroupCreate()),
      wifiEventGroup(evtGrp),
      restConfig(HTTPD_DEFAULT_CONFIG()),
      gwAddr(nullptr),
      ipAddr(nullptr),
      app_eg(getAppEG()),
      storageFlags(initStorage(SDCARD_FLAG+SPIFF_FLAG)),
      statuses(nullptr),
      statusesLen(0),
      system_status(nullptr)
{
    auto* apin = new AppConfig(status,AppConfig::GetAppStatus());
    if (restInstance == nullptr)
    {
        deviceId = AppConfig::GetAppConfig()->GetIntProperty("deviceid");
        ESP_LOGI(__FUNCTION__, "First Rest for %d", deviceId);
        restInstance = this;
        ESP_LOGI(__FUNCTION__, "Getting Config for %d", deviceId);
        apin->SetIntProperty("numRequests",0);
        apin->SetIntProperty("processingTime_us",0);
        apin->SetIntProperty("BytesIn",0);
        apin->SetIntProperty("BytesOut",0);
        apin->SetIntProperty("Failures",0);
        jnumRequests = apin->GetPropertyHolder("numRequests");
        jprocessingTime_us = apin->GetPropertyHolder("processingTime_us");
        jScanning = cJSON_HasObjectItem(status,"Scanning") ? cJSON_GetObjectItem(status,"Scanning") : cJSON_AddFalseToObject(status,"Scanning");
        jBytesIn = apin->GetPropertyHolder("BytesIn");
        jBytesOut = apin->GetPropertyHolder("BytesOut");
        jnumErrors = apin->GetPropertyHolder("Failures");

        accessControlAllowOrigin=config && config->HasProperty("Access-Control-Allow-Origin") ? config->GetPropertyHolder("Access-Control-Allow-Origin") : nullptr;
        accessControlMaxAge=config && config->HasProperty("Access-Control-Max-Age") ? config->GetPropertyHolder("Access-Control-Max-Age") : nullptr;
        accessControlAllowMethods=config && config->HasProperty("Access-Control-Allow-Methods") ? config->GetPropertyHolder("Access-Control-Allow-Methods") : nullptr;
        accessControlAllowHeaders=config && config->HasProperty("Access-Control-Allow-Headers") ? config->GetPropertyHolder("Access-Control-Allow-Headers") : nullptr;
    }
    if (xEventGroupGetBits(eventGroup) & HTTP_SERVING)
    {
        ESP_LOGI(__FUNCTION__, "Not starting httpd, already serving");
        deinitStorage(storageFlags);
        return;
    }
    ESP_LOGI(__FUNCTION__, "Waiting for wifi");
    xEventGroupWaitBits(wifiEventGroup, WIFI_CONNECTED_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    ESP_LOGV(__FUNCTION__, "Registering RegisterEventHandler");
    if (handlerDescriptors == nullptr){
        handlerDescriptors = BuildHandlerDescriptors();
        EventManager::RegisterEventHandler(handlerDescriptors);
    }

    ESP_LOGI(__FUNCTION__, "Getting Ip for %d", deviceId);

    if ((gwAddr == nullptr) && (ipAddr == nullptr))
    {
        TheWifi *theWifi = TheWifi::GetInstance();
        ipAddr = (char *)dmalloc(16);
        gwAddr = (char *)dmalloc(16);
        sprintf(ipAddr, IPSTR, IP2STR(&theWifi->staIp.ip));
        sprintf(gwAddr, IPSTR, IP2STR(&theWifi->staIp.gw));
        ESP_LOGI(__FUNCTION__, "Ip:%s Gw:%s", ipAddr, gwAddr);
    }

    hcUrl = (char*)dmalloc(30);
    sprintf(hcUrl,"http://%s/status/",ipAddr);
    restConfig.uri_match_fn = TheRest::routeHttpTraffic;
    restConfig.lru_purge_enable = true;

    ESP_LOGI(__FUNCTION__, "Starting server on port %d ip%s gw:%s", restConfig.server_port, ipAddr, restInstance->gwAddr);
    esp_err_t ret = ESP_FAIL;
    if ((ret = httpd_start(&server, &restConfig)) == ESP_OK)
    {
        ESP_LOGV(__FUNCTION__, "Registering URI handlers");

        if (config && config->HasProperty("KeyServer")) {
            auto* uris = cJSON_AddObjectToObject(status,"Keys");
            auto* urlName = (char*)dmalloc(255);
            for (auto& uri : restUris) {
                uri.user_ctx = dmalloc(sizeof(PasswordManager));
                memset(uri.user_ctx,0,sizeof(PasswordManager));
                sprintf(urlName,"%s-%s",PasswordEntry::GetMethodName(uri.method),uri.uri);
                new(uri.user_ctx) PasswordManager(config, apin, new AppConfig(cJSON_AddObjectToObject(uris,urlName),AppConfig::GetAppStatus()),&uri);
            }
            wsUri.user_ctx = dmalloc(sizeof(PasswordManager));
            memset(wsUri.user_ctx,0,sizeof(PasswordManager));
            sprintf(urlName,"%s-%s",PasswordEntry::GetMethodName(wsUri.method),wsUri.uri);
            new(wsUri.user_ctx) PasswordManager(config, apin, new AppConfig(cJSON_AddObjectToObject(uris,urlName),AppConfig::GetAppStatus()), &wsUri);

            appUri.user_ctx = dmalloc(sizeof(PasswordManager));
            memset(appUri.user_ctx,0,sizeof(PasswordManager));
            sprintf(urlName,"%s-%s",PasswordEntry::GetMethodName(appUri.method),appUri.uri);
            new(appUri.user_ctx) PasswordManager(config, apin, new AppConfig(cJSON_AddObjectToObject(uris,urlName),AppConfig::GetAppStatus()),  &appUri);
            PasswordManager::InitializePasswordRefresher();
            ldfree(urlName);
        } else {
            ESP_LOGW(__FUNCTION__,"This WebServer is not using key validation");
        }
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &wsUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restPostUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &restPutUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &appUri));
        xEventGroupSetBits(eventGroup, HTTP_SERVING);
        xEventGroupSetBits(app_eg,app_bits_t::REST);
        xEventGroupClearBits(app_eg,app_bits_t::REST_OFF);
        PostEvent(nullptr,0,HTTP_SERVING);
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Error starting server! %s", esp_err_to_name(ret));
    }
    delete apin;
}

TheRest::~TheRest()
{
    ESP_LOGI(__FUNCTION__,"Rest resting");
    deinitStorage(storageFlags);
    vEventGroupDelete(eventGroup);
    if (handlerDescriptors != nullptr)
        EventManager::UnRegisterEventHandler((handlerDescriptors = BuildHandlerDescriptors()));

    httpd_stop(server);
    xEventGroupClearBits(getAppEG(), app_bits_t::REST);
    xEventGroupSetBits(getAppEG(), app_bits_t::REST_OFF);
    ldfree(ipAddr);
    ldfree(gwAddr);
    ldfree(postData);
    restInstance=nullptr;
    ldfree(wsUri.user_ctx);
    ldfree(appUri.user_ctx);
    for (auto const& uri : restUris) {
        ldfree(uri.user_ctx);
    }
}

bool TheRest::routeHttpTraffic(const char *reference_uri, const char *uri_to_match, size_t match_upto)
{
    sampleBatteryVoltage();
    if ((strlen(reference_uri) == 1) && (reference_uri[0] == '*'))
    {
        ESP_LOGV(__FUNCTION__,"* match for %s",uri_to_match);
        if (restInstance) {
            restInstance->jnumRequests->valuedouble = restInstance->jnumRequests->valueint++;
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
        restInstance->jnumRequests->valuedouble = restInstance->jnumRequests->valueint++;
    }
    return matches;
}

TheRest *TheRest::GetServer()
{
    if (restInstance && !deviceId)
        deviceId ? deviceId : deviceId = AppConfig::GetAppConfig()->GetIntProperty("deviceid");

    return restInstance;
}

void TheRest::GetServer(void *evtGrp)
{
    if (restInstance == nullptr)
        restInstance = new TheRest(AppConfig::GetAppConfig()->GetConfig("Rest"), (EventGroupHandle_t)evtGrp);
}

void TheRest::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    ESP_LOGV(__FUNCTION__, "Event %s-%d", base, id);
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
    return SendRequest(url, method, len, nullptr);
}

char *TheRest::SendRequest(const char *url, esp_http_client_method_t method, size_t *len, char *charBuf)
{
    esp_http_client_handle_t client = nullptr;
    esp_http_client_config_t *config = nullptr;
    bool isPreAllocated = charBuf != nullptr;

    config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = url;
    config->method = method;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;
    ESP_LOGI(__FUNCTION__, "Getting %s", config->url);
    esp_err_t err = ESP_OK;
    int hlen = 0;
    int retCode = -1;
    size_t totLen = isPreAllocated ? *len : 0;
    char *retVal = nullptr;
    
    if ((client = esp_http_client_init(config)) &&
        ((err = esp_http_client_open(client, 0)) == ESP_OK) &&
        ((hlen = esp_http_client_fetch_headers(client)) >= 0) &&
        ((retCode = esp_http_client_get_status_code(client)) == 200))
    {
        int bufLen = isPreAllocated ? *len : hlen > 0 ? hlen + 1 : JSON_BUFFER_SIZE;
        retVal = isPreAllocated ? charBuf : (char *)dmalloc(bufLen);

        int chunckLen = isPreAllocated ? min(JSON_BUFFER_SIZE, *len) : hlen ? hlen
                                                                            : JSON_BUFFER_SIZE;
        *len = 0;
        memset(retVal, 0, bufLen);
        ESP_LOGI(__FUNCTION__, "Downloading isPreAllocated:%d hlen:%d chunckLen:%d buflen:%d len: %d bytes of data for %s", isPreAllocated, hlen, chunckLen, bufLen, (int)*len, url);
        while ((chunckLen = esp_http_client_read(client, retVal + *len, chunckLen)) > 0)
        {
            ESP_LOGV(__FUNCTION__, "Chunck %d bytes of data for %s. Totlen:%d", chunckLen, url, *len);
            *len += chunckLen;
            if (!isPreAllocated || (isPreAllocated && (*len < totLen)))
                memset(retVal + *len, 0, 1);
        }
        ESP_LOGV(__FUNCTION__, "Downloaded %d bytes of data from %s", *len, url);
        jBytesIn->valuedouble = jBytesIn->valueint+=*len;
    }
    else
    {
        jnumErrors->valueint = jnumErrors->valuedouble++;
        ESP_LOGE(__FUNCTION__, "Failed sending request(%s). client is %snull, err:%s, hlen:%d, retCode:%d, len:%d", config->url, client ? "not " : "", esp_err_to_name(err), hlen, retCode, *len);
    }

    if (client){
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
    ldfree((void *)config);
    return retVal;
}

cJSON *TheRest::PostJSonRequest(const char *url)
{
    cJSON *ret = nullptr;
    size_t len = 0;
    ESP_LOGV(__FUNCTION__, "Posting (%s).", url);
    char *sjson = PostRequest(url, &len);
    if (len)
    {
        ESP_LOGV(__FUNCTION__, "Parsing (%s).", sjson);
        //DisplayMemInfo();
        if (len && sjson && (ret = cJSON_ParseWithLength(sjson,len)))
        {
            ESP_LOGV(__FUNCTION__, "Got back from %d from (%s).", len, url);
        }
        else
        {
            ESP_LOGW(__FUNCTION__, "Got %d of nobueno bits for %s:%s.", len, url, sjson ? sjson : "*null*");
        }
        ldfree(sjson);
    }

    return ret;
}

cJSON *TheRest::GetDeviceConfig(esp_ip4_addr_t *ipInfo, uint32_t deviceId)
{
    char *url = (char *)dmalloc(100);
    cJSON *jret = nullptr;

    if (deviceId)
        sprintf((char *)url, "http://" IPSTR "/config/%d", IP2STR(ipInfo), deviceId);
    else
        sprintf((char *)url, "http://" IPSTR "/config", IP2STR(ipInfo));
    ESP_LOGV(__FUNCTION__, "Getting config from %s", url);
    //DisplayMemInfo();
    jret = PostJSonRequest(url);
    //DisplayMemInfo();
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
    ldfree((void *)config->url);
    ldfree(config);
    return ret;
}

void TheRest::MergeConfig(void *param)
{
    ESP_LOGI(__FUNCTION__, "Merging Config %d %d",restInstance == nullptr, restInstance == nullptr ? 0 : restInstance->gwAddr==nullptr);
    uint32_t addr = ipaddr_addr(restInstance->gwAddr);
    cJSON *newCfg = restInstance->GetDeviceConfig((esp_ip4_addr *)&addr, deviceId);
    cJSON *curCfg = AppConfig::GetAppConfig()->GetJSONConfig(nullptr);
    if (newCfg)
    {
        if (!cJSON_Compare(newCfg, curCfg, true))
        {
            AppConfig::GetAppConfig()->SetAppConfig(newCfg);
            ESP_LOGI(__FUNCTION__, "Updated config from server");
            TheRest::SendConfig(restInstance->gwAddr, newCfg);
            vTaskDelay(1000/portTICK_PERIOD_MS);
            esp_restart();
        }
        else
        {
            cJSON_free(newCfg);
            ESP_LOGI(__FUNCTION__, "No config update needed from server");
        }
    }
    else
    {
        ESP_LOGW(__FUNCTION__, "Cannot get config from " IPSTR " for devid %d", IP2STR((esp_ip4_addr *)&addr), deviceId);
        TheRest::SendConfig(restInstance->gwAddr, curCfg);
    }
}

void TheRest::SendStatus(void *param)
{
    ESP_LOGV(__FUNCTION__, "Sending status");
    esp_err_t ret = ESP_FAIL;
    uint32_t addr = ipaddr_addr(restInstance->gwAddr);
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
    cJSON *astats = AppConfig::GetAppStatus()->GetJSONConfig(nullptr);
    cJSON *jtmp;
    cJSON_ArrayForEach(jtmp, cstats)
    {
        cJSON_AddItemReferenceToObject(astats, jtmp->string, jtmp);
    }
    cJSON_AddItemReferenceToObject(astats, "tasks", jstats);

    char *sjson = cJSON_PrintUnformatted(astats);
    cJSON_Delete(jstats);

    if (sjson){
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
        ldfree(sjson);
    } else {
        ESP_LOGE(__FUNCTION__, "Cannot printf the json");
    }

    ldfree((void*)config->url);
    ldfree(config);
}

EventHandlerDescriptor *TheRest::BuildHandlerDescriptors()
{
    ESP_LOGV(__FUNCTION__, "TheRest: BuildHandlerDescriptors");
    EventHandlerDescriptor *handler = ManagedDevice::BuildHandlerDescriptors();
    ESP_LOGV(__FUNCTION__, "TheRest: Done BuildHandlerDescriptors");
    return handler;
}

void get_client_ip(httpd_req_t *req, char* ipstr)
{
    int sockfd = httpd_req_to_sockfd(req);
    struct sockaddr_in6 addr;   // esp_http_server uses IPv6 addressing
    socklen_t addr_size = sizeof(addr);
    
    if (getpeername(sockfd, (struct sockaddr *)&addr, &addr_size) < 0) {
        ESP_LOGE(__FUNCTION__, "Error getting client IP");
        return;
    }
    
    inet_ntop(AF_INET, &addr.sin6_addr.un.u32_addr, ipstr, sizeof(ipstr));
    //inet_ntop(AF_INET, &addr.sin6_addr, ipstr, sizeof(ipstr));
    //sprintf(ipstr,"%d.%d.%d.%d",IP2STR(&addr.sin6_addr.un.u32_addr[3]));
    ESP_LOGV(__FUNCTION__, "Client IP => %s", ipstr);
}

esp_err_t TheRest::rest_handler(httpd_req_t *req)
{
    ESP_LOGV(__FUNCTION__, "rest handling (%d)%s", req->method, req->uri);

    uint32_t idx = 0;
    char ipstr[INET6_ADDRSTRLEN] = {0};

    if (restInstance->accessControlAllowOrigin && restInstance->accessControlAllowOrigin->valuestring) {
        if (strcmp(restInstance->accessControlAllowOrigin->valuestring,"*") == 0) {
            get_client_ip(req,ipstr);
            ESP_ERROR_CHECK(httpd_resp_set_hdr(req,"Access-Control-Allow-Origin", ipstr));
        } else {
            ESP_ERROR_CHECK(httpd_resp_set_hdr(req,"Access-Control-Allow-Origin", restInstance->accessControlAllowOrigin->valuestring));
        }
    }

    if (restInstance->accessControlMaxAge && restInstance->accessControlMaxAge->valuestring) {
        ESP_ERROR_CHECK(httpd_resp_set_hdr(req,"Access-Control-Max-Age", restInstance->accessControlMaxAge->valuestring));
    }
    if (restInstance->accessControlAllowMethods && restInstance->accessControlAllowMethods->valuestring) {
        ESP_ERROR_CHECK(httpd_resp_set_hdr(req,"Access-Control-Allow-Methods", restInstance->accessControlAllowMethods->valuestring));
    }
    if (restInstance->accessControlAllowHeaders && restInstance->accessControlAllowHeaders->valuestring) {
        ESP_ERROR_CHECK(httpd_resp_set_hdr(req,"Access-Control-Allow-Headers", restInstance->accessControlAllowHeaders->valuestring));
    }

    for (const httpd_uri_t &theUri : restInstance->restUris)
    {
        idx++;
        if ((req->method == theUri.method) && (routeHttpTraffic(theUri.uri, req->uri, strlen(req->uri))))
        {
            ESP_LOGV(__FUNCTION__, "rest handled (%d)%s <- %s idx:%d", req->method, theUri.uri, req->uri, idx);
            time_t start = esp_timer_get_time();
            req->user_ctx = theUri.user_ctx;
            if (checkTheSum(req) != ESP_OK) {
                return ESP_FAIL;
            }

            esp_err_t ret = theUri.handler(req);
            restInstance->jprocessingTime_us->valuedouble = restInstance->jprocessingTime_us->valueint+=(esp_timer_get_time()-start);
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
    if (ManagedDevice::HealthCheck(instance)) {
        TheRest* theRest = (TheRest*)instance;
        size_t len=0;
        char* resp = theRest->SendRequest(theRest->hcUrl,HTTP_METHOD_POST,&len);
        if (resp != nullptr){
            ldfree(resp);
            return true;
        }
    }
    return false;
}