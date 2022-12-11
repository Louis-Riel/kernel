#include "pwdmgr.h"
#include "mbedtls/md.h"

char* buf = (char*)dmalloc(JSON_BUFFER_SIZE);
const char* keyServer = nullptr;
const char* keyServerPath = nullptr;
const char* keyServerCert = nullptr;
const char* hostName = nullptr;
char* theHashes = (char*)dmalloc(HASH_LEN * 2);
char* theHeaders = (char*)dmalloc(HEADER_MAX_LEN * 2);
int keyServerPort = 0;

PasswordEntry::PasswordEntry(AppConfig* config, AppConfig* restState, AppConfig* urlState, AppConfig* ppwdState, httpd_uri_t* curi) {
    if (keyServer == nullptr) {
        auto* serverCfg = new AppConfig(config->GetJSONConfig("KeyServer",config));
        keyServer=serverCfg->GetStringProperty("keyServer");
        keyServerPath=serverCfg->GetStringProperty("keyServerPath");
        keyServerCert=serverCfg->GetStringProperty("serverCert");
        hostName=AppConfig::GetAppConfig()->GetStringProperty("devName");
        keyServerPort=serverCfg->GetIntProperty("keyServerPort");
        delete serverCfg;
        ESP_LOGV(__FUNCTION__,"Initial Config Completed server:%s path:%s hostname:%s port:%d cert:%s",keyServer,keyServerPath,hostName,keyServerPort,keyServerCert);
    }
    keyServerUrl = (char*)dmalloc(1024);
    pwdState = ppwdState;

    if (!restState->HasProperty("BytesOut")) {
        restState->SetIntProperty("BytesOut",0);
    }
    if (!restState->HasProperty("BytesIn")) {
        restState->SetIntProperty("BytesIn",0);
    }
    if (!urlState->HasProperty("BytesOut")) {
        urlState->SetIntProperty("BytesOut",0);
    }
    if (!urlState->HasProperty("BytesIn")) {
        urlState->SetIntProperty("BytesIn",0);
    }
    if (!urlState->HasProperty("NumRefreshes")) {
        urlState->SetIntProperty("NumRefreshes",0);
    }

    pwdState->SetIntProperty("NumRefreshes",0);
    pwdState->SetIntProperty("NumValid",0);
    pwdState->SetIntProperty("NumInvalid",0);
    pwdState->SetStringProperty("LastRefresh","                                 ");
    pwdState->SetStringProperty("NextRefresh","                                 ");

    jBytesOut = restState->GetPropertyHolder("BytesOut");
    jBytesIn = restState->GetPropertyHolder("BytesIn");

    jNumRefreshes = pwdState->GetPropertyHolder("NumRefreshes");
    jLastRefresh = pwdState->GetPropertyHolder("LastRefresh");
    jNextRefresh = pwdState->GetPropertyHolder("NextRefresh");
    jNumValid = pwdState->GetPropertyHolder("NumValid");
    jNumInvalid = pwdState->GetPropertyHolder("NumInvalid");

    jUriBytesOut = urlState->GetPropertyHolder("BytesOut");
    jUriBytesIn = urlState->GetPropertyHolder("BytesIn");
    jUriNumRefreshes = urlState->GetPropertyHolder("NumRefreshes");

    uri=curi;
    expires_at=0;
    blen=0;
}

PasswordEntry::~PasswordEntry()
{
    delete pwdState;
    ESP_LOGV(__FUNCTION__,"Destructing");
}

bool PasswordEntry::IsExpired(int64_t curTime) const {
    return expires_at <= curTime;
}

int64_t PasswordEntry::GetExpiresAt() const {
    return expires_at;
}

const char* PasswordEntry::GetPassword() const{
    return pwd;
}
const char* PasswordEntry::GetKey() const{
    return keyid;
}

esp_err_t PasswordEntry::HttpEventHandler(esp_http_client_event_t *evt) {
    auto* pEntry=((PasswordEntry*)evt->user_data);
    cJSON* json;

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_CONNECTED");
            cJSON_SetIntValue(pEntry->jBytesOut,pEntry->jBytesOut->valueint + strlen(keyServer) + strlen(keyServerPath) + 16 + strlen(pEntry->uri->uri));
            cJSON_SetIntValue(pEntry->jUriBytesOut,pEntry->jUriBytesOut->valueint + strlen(keyServer) + strlen(keyServerPath) + 16 + strlen(pEntry->uri->uri));
            cJSON_SetIntValue(pEntry->jNumRefreshes,pEntry->jNumRefreshes->valueint + 1);
            cJSON_SetIntValue(pEntry->jUriNumRefreshes,pEntry->jUriNumRefreshes->valueint + 1);
            pEntry->blen=0;
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGV(__FUNCTION__, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            memcpy(buf + pEntry->blen, evt->data, evt->data_len);
            pEntry->blen+=evt->data_len;
            cJSON_SetIntValue(pEntry->jBytesIn,pEntry->jBytesIn->valueint+evt->data_len);
            cJSON_SetIntValue(pEntry->jUriBytesIn,pEntry->jUriBytesIn->valueint+evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_FINISH");
            if (pEntry->blen) {
                *(buf+pEntry->blen)=0;
                ESP_LOGV(__FUNCTION__,"Parsing(%s)",buf);
                json = cJSON_ParseWithLength(buf,pEntry->blen);
                if (json && 
                    (cJSON_HasObjectItem(json,"keyid") &&
                     cJSON_HasObjectItem(json,"password") &&
                     cJSON_HasObjectItem(json,"ttl"))) {

                    memset(pEntry->keyid,0,KEY_ID_LEN);
                    memset(pEntry->pwd,0,PASSWORD_LEN);
                    pEntry->expires_at=0;

                    strncpy(pEntry->keyid,cJSON_GetStringValue(cJSON_GetObjectItem(json,"keyid")),KEY_ID_LEN);
                    ESP_LOGV(__FUNCTION__,"%s-%s KeyId:%s ",pEntry->GetPath(),pEntry->GetMethod(), pEntry->keyid);

                    strncpy(pEntry->pwd,cJSON_GetStringValue(cJSON_GetObjectItem(json,"password")),PASSWORD_LEN);
                    ESP_LOGV(__FUNCTION__,"Password:%s ",pEntry->pwd);

                    pEntry->expires_at=(cJSON_GetNumberValue(cJSON_GetObjectItem(json,"ttl")) * 1000000) + esp_timer_get_time();
                    ESP_LOGV(__FUNCTION__,"Ttl:%lld ",pEntry->GetExpiresAt());

                    if (strlen(pEntry->keyid) > KEY_ID_LEN) {
                        ESP_LOGE(__FUNCTION__,"Corrupted key id:(%s) pwd:(%s) ttl:%lld",pEntry->keyid,pEntry->pwd,pEntry->expires_at);
                        pEntry->expires_at=0;
                        pEntry->keyid[0]=0;
                        pEntry->pwd[0]=0;
                    }
                } else if (json) {
                    ESP_LOGE(__FUNCTION__,"Invalid json from output:%s",buf);
                } else {
                    ESP_LOGE(__FUNCTION__,"Cannot parse json from output:%s",buf);
                }
                if (json) {
                    cJSON_Delete(json);
                }
            } else {
                ESP_LOGE(__FUNCTION__,"No output");
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGV(__FUNCTION__, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}

const char* PasswordEntry::GetMethodName(httpd_method_t method) {
    static const char* mPOST = "POST";
    static const char* mPUT = "PUT";
    static const char* mGET = "GET";
    static const char* mDELETE = "DELETE";
    static const char* mUNKNOWN = "Unknown";

    switch (method)
    {
    case HTTP_POST:
        return mPOST;
        break;
    case HTTP_PUT:
        return mPUT;
        break;
    case HTTP_GET:
        return mGET;
        break;
    case HTTP_DELETE:
        return mDELETE;
        break;
    
    default:
        ESP_LOGV(__FUNCTION__,"Unknown method:%d",method);
        break;
    }
    return mUNKNOWN;
}

bool PasswordEntry::RefreshKey(uint32_t ttl) {
    sprintf(keyServerUrl,KEYS_URL_QUERY_PARAMS,uri->uri,GetMethodName(uri->method),ttl,hostName);
    ESP_LOGV(__FUNCTION__,"url(%s)",keyServerUrl);
    char* wc = indexOf(keyServerUrl," ");
    if (wc) {
        if (*(wc-1) == '*') {
            *(wc-1) = '.';
            *(wc) = '*';
        } else {
            while (*wc++ != 0) {
                *(wc-1)=*wc;
            }
            *(wc-1)=0;
        }
        ESP_LOGV(__FUNCTION__,"url adjusted to (%s) for (%s) on (%s/%s):%d %d",keyServerUrl,hostName,keyServer,keyServerPath,keyServerPort,keyServer==nullptr);
    } else {
        ESP_LOGW(__FUNCTION__,"Weird URL format, let's just see what happens");
    }

    ESP_LOGV(__FUNCTION__,"Cert(%s)",keyServerCert);

    esp_http_client_config_t config = {
        .host = keyServer,
        .port = keyServerPort,
        .path = keyServerPath,
        .query = keyServerUrl,
        .cert_pem = keyServerCert,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 2000,
        .disable_auto_redirect = true,
        .event_handler = HttpEventHandler,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
        .user_data = (void*)this,
        .is_async = false,
        .skip_cert_common_name_check = true
    };
    ESP_LOGV(__FUNCTION__,"getting key for %s %s from http://%s:%d%s?%s", GetMethodName(uri->method) ,uri->uri, config.host, config.port, config.path, config.query);
    esp_http_client_handle_t client = esp_http_client_init(&config);
    bool ret = true;
    if (esp_http_client_perform(client) != ESP_OK)
    {
        ESP_LOGE(__FUNCTION__,"Error getting key for %s %s from http://%s:%d%s?%s", GetMethodName(uri->method) ,uri->uri, config.host, config.port, config.path, config.query);
        ret = false;
    } else {
        struct tm timeinfo;
        time_t now;
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(jLastRefresh->valuestring, 30, "%c", &timeinfo);
        now += ttl;
        localtime_r(&now, &timeinfo);
        strftime(jNextRefresh->valuestring, 30, "%c", &timeinfo);
    }
    esp_http_client_cleanup(client);
    return ret || (expires_at != 0);
}

const char* PasswordEntry::GetPath() const {
    return uri->uri;
}
const char* PasswordEntry::GetMethod() const {
    return GetMethodName(uri->method);    
}

esp_err_t PasswordEntry::CheckTheSum(httpd_req_t *req) {
    esp_err_t ret = ESP_OK;
#if CONFIG_ENABLE_VALIDATED_REQUESTS == 1
    if (!pwd || !strlen(pwd)) {
        ESP_LOGW(__FUNCTION__,"Missing password");
        return ESP_FAIL;
    }
    if (!keyid || !strlen(keyid)) {
        ESP_LOGW(__FUNCTION__,"Missing keyid on %s", req->uri);
        return ESP_FAIL;
    }
    size_t hlen = httpd_req_get_hdr_value_len(req, "TheHash");
    if (hlen > 1) {
        auto* theHash = xPortGetCoreID() ? theHashes : theHashes + HASH_LEN;
        if (!theHash) {
            ESP_LOGE(__FUNCTION__,"Can't allocate hash");
            return ESP_FAIL;
        }
        if (httpd_req_get_hdr_value_str(req, "TheHash", theHash, 65) == ESP_OK) {
            ESP_LOGV(__FUNCTION__,"Hash:%s(%s)",req->uri, theHash);
            mbedtls_md_context_t ctx;
            mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

            mbedtls_md_init(&ctx);
            mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
            mbedtls_md_starts(&ctx);

            const char* val = GetMethodName((httpd_method_t)req->method);

            mbedtls_md_update(&ctx, (const unsigned char*)val, strlen(val));
            ESP_LOGV(__FUNCTION__,"method(%s)",val);
            mbedtls_md_update(&ctx, (const unsigned char*)"/", 1);
            mbedtls_md_update(&ctx, (const unsigned char*)hostName, strlen(hostName));
            mbedtls_md_update(&ctx, (const unsigned char*)req->uri, strlen(req->uri));
            ESP_LOGV(__FUNCTION__,"path(/%s%s)",hostName,req->uri);

            auto* theHeader = xPortGetCoreID() ? theHeaders : theHeaders + HEADER_MAX_LEN;
            
            for (const char* headerName : {"TheHashTime","TheHashRandom","TheHashKey"}) {
                hlen = httpd_req_get_hdr_value_len(req, headerName);
                if (hlen && (hlen < 254) && (httpd_req_get_hdr_value_str(req, headerName, theHeader, HEADER_MAX_LEN) == ESP_OK)) {
                    mbedtls_md_update(&ctx, (const unsigned char*)theHeader, strlen(theHeader));
                    ESP_LOGV(__FUNCTION__,"%s(%s)",headerName,theHeader);
                } else {
                    ESP_LOGE(__FUNCTION__,"Can't get header for %s",headerName);
                    ret=ESP_FAIL;
                }
            }
            mbedtls_md_update(&ctx, (const unsigned char*)GetPassword(), strlen(GetPassword()));
            ESP_LOGV(__FUNCTION__,"password(%s)",GetPassword());

            uint8_t shaResult[32];
            mbedtls_md_finish(&ctx, shaResult);
            mbedtls_md_free(&ctx);
            for (uint8_t i = 0; i < sizeof(shaResult); ++i)
            {
                sprintf(theHeader+(i*2),"%02x", shaResult[i]);
            }

            if (strcmp(theHeader,theHash) == 0) {
                ESP_LOGV(__FUNCTION__,"The hash %s matches",theHash);
                cJSON_SetIntValue(jNumValid,jNumValid->valueint+1);
            } else {
                cJSON_SetIntValue(jNumInvalid,jNumInvalid->valueint+1);
                ESP_LOGW(__FUNCTION__,"Mismatch hash %s!=%s",theHash,theHeader);
                ret=ESP_FAIL;
            }
        } else {
            ret=ESP_FAIL;
            ESP_LOGE(__FUNCTION__,"%s got bad hash:%d",req->uri, hlen);
            httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"You are not worthy with this sillyness");
        }
    } else {
        ret=ESP_FAIL;
        ESP_LOGW(__FUNCTION__,"%s got no hash",req->uri);
        httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"You are not worthy with this sillyness");
    }
    
#endif
    return ret;
}
