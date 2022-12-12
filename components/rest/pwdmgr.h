#include "route.h"

#define NUM_KEYS 5
#define KEY_SERVER_LEN 50
#define KEY_PATH_LEN 255
#define KEY_ID_LEN 50
#define KEY_DEFAULT_TTL_SEC 600
#define PASSWORD_LEN 400
#define KEY_DYING_TIME_SEC 5
#define HASH_LEN 65
#define HEADER_MAX_LEN 1024
#define KEYS_URL_QUERY_PARAMS "path=%s &method=%s&ttl=%d&name=%s"
#define KEYS_URL_DELETE_PARAMS "name=%s"

class PasswordEntry {
public:
    explicit PasswordEntry(AppConfig*,AppConfig*,AppConfig*,AppConfig*,httpd_uri_t*);
    ~PasswordEntry();
    bool RefreshKey(uint32_t ttl);
    int64_t GetExpiresAt() const;
    bool IsExpired(int64_t) const;
    const char* GetPassword() const;
    const char* GetKey() const;
    const char* GetPath() const;
    const char* GetMethod() const;
    static const char* GetMethodName(httpd_method_t method);
    esp_err_t CheckTheSum(httpd_req_t *req);

private:

    static esp_err_t HttpEventHandler(esp_http_client_event_t *evt);
    char* keyServerUrl;
    
    httpd_uri_t* uri;
    char keyid[KEY_ID_LEN];
    char pwd[PASSWORD_LEN];
    int64_t expires_at;
    uint32_t blen;
    cJSON* jBytesOut;
    cJSON* jBytesIn;
    cJSON* jUriBytesOut;
    cJSON* jUriBytesIn;
    cJSON* jLastRefresh;
    cJSON* jNextRefresh;
    cJSON* jNumRefreshes;
    cJSON* jUriNumRefreshes;
    cJSON* jNumValid;
    cJSON* jNumInvalid;
    AppConfig* pwdState;
};

class PasswordManager {
public:
    explicit PasswordManager(AppConfig*, AppConfig*, AppConfig*, httpd_uri_t*);
    ~PasswordManager();
    static void InitializePasswordRefresher();
    esp_err_t CheckTheSum(httpd_req_t *req);

private:
    bool ResetKeys(AppConfig* config);
    uint32_t GetNextTTL(uint32_t ttl) const;
    static void RefreshKeys(void* instance);
    PasswordEntry* GetPassword(char const *) ;
    static uint8_t refreshKeyTaskId;
    static uint64_t nextRefreshTs;
    static PasswordManager* passwordManagers[255];
    static uint8_t numPasswordManagers;
    AppConfig* urlState;
    cJSON* jNumValid;
    cJSON* jNumInvalid;

    PasswordEntry* passwords[NUM_KEYS];
};