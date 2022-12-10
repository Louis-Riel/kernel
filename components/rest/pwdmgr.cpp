#include "pwdmgr.h"

uint8_t PasswordManager::refreshKeyTaskId=0;
uint64_t PasswordManager::nextRefreshTs=INT64_MAX;
PasswordManager* PasswordManager::passwordManagers[255];
uint8_t PasswordManager::numPasswordManagers=0;
static bool initialized = false;

PasswordManager::~PasswordManager(){
    delete urlState;
    for (PasswordEntry* pwd : passwords) {
        pwd->~PasswordEntry();
        ldfree(pwd);
    }
}

PasswordManager::PasswordManager(AppConfig* config, AppConfig* restState, AppConfig* purlState, httpd_uri_t* uri)
{
    if (uri && uri->uri) {
        ESP_LOGV(__FUNCTION__,"Initing:(%s) with %d*%d=%d",uri->uri,NUM_KEYS,sizeof(PasswordEntry),sizeof(PasswordEntry)*NUM_KEYS);
        if (numPasswordManagers==0) {
            memset(passwordManagers,0,sizeof(void*)*255);
        }
        passwordManagers[numPasswordManagers++]=this;
        urlState = purlState;
        urlState->SetStringProperty("path",uri->uri);
        urlState->SetStringProperty("method",PasswordEntry::GetMethodName(uri->method));
        urlState->SetIntProperty("NumValid",0);
        urlState->SetIntProperty("NumInvalid",0);
        jNumInvalid = urlState->GetPropertyHolder("NumInvalid");
        jNumValid = urlState->GetPropertyHolder("NumValid");

        auto* uris = cJSON_AddArrayToObject(urlState->GetJSONConfig(nullptr),"keys");

        if (!initialized) {
            if (!ResetKeys(config)) {
                ESP_LOGE(__FUNCTION__,"Failed to reset keys");
            }
            initialized = true;
        }

        for (uint8_t idx = 0; idx < NUM_KEYS; idx++) {
            cJSON* url = cJSON_CreateObject();
            cJSON_AddNumberToObject(url,"ID",idx);
            cJSON_AddItemToArray(uris,url);
            passwords[idx] = (PasswordEntry *)dmalloc(sizeof(PasswordEntry));
            new(passwords[idx]) PasswordEntry(config,restState,urlState,new AppConfig(url,AppConfig::GetAppStatus()), uri);
        }
    } else {
        ESP_LOGE(__FUNCTION__,"No URI %d %d", uri==nullptr, uri->uri==nullptr);
    }
}

bool PasswordManager::ResetKeys(AppConfig* config) {
    char query[255];
    sprintf(query,KEYS_URL_DELETE_PARAMS,AppConfig::GetAppConfig()->GetStringProperty("devName"));
    ESP_LOGV(__FUNCTION__,"url(%s)",query);
    auto* serverCfg = new AppConfig(config->GetJSONConfig("KeyServer",config));

    esp_http_client_config_t cconfig = {
        .host = serverCfg->GetStringProperty("keyServer"),
        .port = serverCfg->GetIntProperty("keyServerPort"),
        .path = serverCfg->GetStringProperty("keyServerPath"),
        .query = query,
        .cert_pem = serverCfg->GetStringProperty("serverCert"),
        .method = HTTP_METHOD_DELETE,
        .timeout_ms = 2000,
        .disable_auto_redirect = true,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = (void*)this,
        .is_async = false,
        .skip_cert_common_name_check = true
    };
    esp_http_client_handle_t client = esp_http_client_init(&cconfig);
    bool ret = true;
    if (esp_http_client_perform(client) != ESP_OK)
    {
        ESP_LOGE(__FUNCTION__,"Error deleting key for %s",cconfig.query);
        ret = false;
    }
    esp_http_client_cleanup(client);
    delete serverCfg;
    return ret;
}

void PasswordManager::InitializePasswordRefresher() {
    refreshKeyTaskId = CreateRepeatingTask(RefreshKeys,"RefreshKeys",nullptr,KEY_DEFAULT_TTL_SEC);
}


PasswordEntry* PasswordManager::GetPassword(const char* keyId){
    int64_t curTime = esp_timer_get_time();
    for (uint8_t idx = 0; idx < NUM_KEYS; idx++) {
        if (!passwords[idx]->IsExpired(curTime) &&
           (strcmp(passwords[idx]->GetKey(),keyId)==0)) {
            return passwords[idx];
        } else {
            ESP_LOGV(__FUNCTION__,"Expired:%d keyid:%s!=%s",passwords[idx]->IsExpired(curTime),passwords[idx]->GetKey(),keyId);
        }
    }
    return nullptr;
}

uint32_t GetRandomTTL(uint32_t time) {
    uint32_t ret = time* (esp_random()/((double)UINT32_MAX));
    ESP_LOGV(__FUNCTION__,"rnd:%d:%d",time,ret);
    return ret;
}

uint32_t PasswordManager::GetNextTTL(uint32_t ttl) const {
    uint64_t curTime = esp_timer_get_time();
    uint8_t numRetries=10;

    for (int64_t thePlan = curTime + (ttl*1000000); numRetries--; thePlan = GetRandomTTL(ttl)) {
        int64_t nextRefresh = 0;
        for (uint8_t idx = 0; idx < NUM_KEYS; idx++) {
            if (passwords[idx]->GetExpiresAt() > nextRefresh) {
                nextRefresh = passwords[idx]->GetExpiresAt();
            }
        }
        if ((nextRefresh == 0) || ((thePlan - nextRefresh) > 60000000)) {
            ESP_LOGV(__FUNCTION__,"%lli,%lli",(thePlan - curTime) / 1000000,thePlan - nextRefresh);
            return (thePlan - curTime) / 1000000;
        }
    }

    uint32_t ret = GetRandomTTL(ttl);
    ESP_LOGV(__FUNCTION__,"Picking random TTL after 10 retries:%d",ret);
    return ret;
}

void PasswordManager::RefreshKeys(void* instance) {    
    int64_t curTime = esp_timer_get_time();
    bool initialRun = nextRefreshTs==INT64_MAX;
    ESP_LOGV(__FUNCTION__,"Cur:%lld",esp_timer_get_time());
    int64_t refreshTs = INT64_MAX;
    uint32_t numKeys = 0;
    for (uint8_t idx = 0; idx < NUM_KEYS; idx++) {
        for (int i=0; i < 255; i++) {
            if (passwordManagers[i]) {
                auto const* that = passwordManagers[i];
                if (that->passwords[idx]->IsExpired(curTime)) {
                    numKeys++;
                    if (that->passwords[idx]->RefreshKey(that->GetNextTTL(KEY_DEFAULT_TTL_SEC))) {
                        ESP_LOGV(__FUNCTION__,"Id:%s(%s %s)(%lld)",that->passwords[idx]->GetKey(), that->passwords[idx]->GetMethod(), that->passwords[idx]->GetPath(), that->passwords[idx]->GetExpiresAt());
                    } else {
                        ESP_LOGE(__FUNCTION__,"Refresh failed for %s %s",that->passwords[idx]->GetMethod(), that->passwords[idx]->GetPath());
                    }
                }
                if (refreshTs > that->passwords[idx]->GetExpiresAt()) {
                    refreshTs = that->passwords[idx]->GetExpiresAt();
                    ESP_LOGV(__FUNCTION__,"Next Key Refresh now at %lld", refreshTs);
                    ESP_LOGV(__FUNCTION__,"Next Key Refresh s:%lld", (refreshTs-esp_timer_get_time())/1000000);
                }
            }
        }
    }
    if (initialRun) {
        ESP_LOGI(__FUNCTION__,"Baked %d keys", numKeys);
    }
    nextRefreshTs=refreshTs;
    int32_t nextRefresh = (refreshTs-esp_timer_get_time())/1000;
    if (nextRefresh <= 5000) {
        nextRefresh = 5000;
    }
    if (nextRefresh > (KEY_DEFAULT_TTL_SEC * 1000)) {
        nextRefresh = KEY_DEFAULT_TTL_SEC * 1000;
    }
    ESP_LOGV(__FUNCTION__,"Next Key Refresh in %dms", nextRefresh);
    UpdateRepeatingTaskPeriod(refreshKeyTaskId, nextRefresh);
}

esp_err_t PasswordManager::CheckTheSum(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
#if CONFIG_ENABLE_VALIDATED_REQUESTS == 1
    auto* theHash = (char*)dmalloc(255);
    if (!theHash) {
        ESP_LOGE(__FUNCTION__,"Can't allocate hash id");
        return ESP_FAIL;
    }
    if (httpd_req_get_hdr_value_str(req, "TheHashKey", theHash, 255) == ESP_OK) {
        ESP_LOGV(__FUNCTION__,"Hash:%s(%s)",req->uri, theHash);
        PasswordEntry* entry = GetPassword(theHash);
        if (entry) {
            ret=entry->CheckTheSum(req);
            if (ret == ESP_OK) {
                ESP_LOGV(__FUNCTION__,"%s got matching hashid:%s",req->uri, theHash);
                cJSON_SetIntValue(jNumValid,jNumValid->valueint+1);
            } else {
                ESP_LOGW(__FUNCTION__,"%s got invalid hashid:%s",req->uri, theHash);
                cJSON_SetIntValue(jNumInvalid,jNumInvalid->valueint+1);
            }
        } else {
            ESP_LOGE(__FUNCTION__,"%s got weird hash id",theHash);
            cJSON_SetIntValue(jNumInvalid,jNumInvalid->valueint+1);
            httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"You are not worthy with this sillyness");
            ret=ESP_FAIL;
        }
    } else {
        ESP_LOGE(__FUNCTION__,"%s got bad hash id",req->uri);
        cJSON_SetIntValue(jNumInvalid,jNumInvalid->valueint+1);
        httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"You are not worthy with this sillyness");
        ret=ESP_FAIL;
    }
    ldfree(theHash);
#endif
    return ret;

}