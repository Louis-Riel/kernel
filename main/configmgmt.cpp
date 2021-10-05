#include <sys/types.h>
#include <sys/stat.h>
#include "utils.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

#if LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE
#include "esp_debug_helpers.h" |
#endif

extern const uint8_t defaultconfig_json_start[] asm("_binary_defaultconfig_json_start");
extern const uint8_t defaultconfig_json_end[] asm("_binary_defaultconfig_json_end");
const char* emptyString="";

AppConfig *GetAppConfig()
{
  return AppConfig::GetAppConfig();
}

AppConfig *AppConfig::configInstance = NULL;
AppConfig *AppConfig::statusInstance = NULL;

AppConfig::AppConfig(const char *filePath)
    : version(0)
    , json(NULL)
    , filePath(filePath)
    , root(this)
    , sema(xSemaphoreCreateRecursiveMutex())
{
  activeStorage=SPIFFPATH;

  if ((configInstance == NULL) && (filePath != NULL))
  {
    ESP_LOGV(__FUNCTION__, "Setting global config instance");
    configInstance = this;
    FILE *currentCfg = fOpen(filePath, "r");
    if (currentCfg == NULL)
    {
      ESP_LOGD(__FUNCTION__, "Getting default config for %s", filePath);
      json = cJSON_ParseWithLength((const char *)defaultconfig_json_start, defaultconfig_json_end - defaultconfig_json_start);
      SaveAppConfig(true);
    }
    else
    {
      ESP_LOGD(__FUNCTION__, "Reading config at %s", filePath);
      struct stat fileStat;
      fstat(fileno(currentCfg), &fileStat);
      char *sjson = (char *)dmalloc(fileStat.st_size);
      fRead(sjson, 1, fileStat.st_size, currentCfg);
      fClose(currentCfg);
      if (sjson && (strlen(sjson) > 0))
      {
        cJSON *toBeCfg = cJSON_ParseWithLength(sjson, fileStat.st_size);
        if ((toBeCfg != NULL) && (cJSON_GetObjectItem(toBeCfg, "wifitype") != NULL))
        {
          json = toBeCfg;
        }
        else if (sjson != NULL)
        {
          ESP_LOGE(__FUNCTION__, "Corrupted configuration, not applying:%s", sjson);
        }
        ldfree(sjson);
      }
      else
      {
        ESP_LOGE(__FUNCTION__, "Empty configuration, not applying");
      }
    }
  }
}

AppConfig::AppConfig(cJSON *json, AppConfig *root)
    : version(0)
    , json(json)
    , filePath(root == NULL ? NULL : root->filePath)
    , root(root == NULL ? this : root)
    , sema(root == NULL ? xSemaphoreCreateRecursiveMutex() : root->sema)
{
}

AppConfig::~AppConfig() {
  if (root == this)
    vSemaphoreDelete(sema);
}

EventGroupHandle_t AppConfig::GetStateGroupHandle()
{
  return GetAppStatus()->eg;
}

AppConfig *AppConfig::GetAppConfig()
{
  return AppConfig::configInstance;
}

AppConfig *AppConfig::GetAppStatus()
{
  if (statusInstance == NULL)
  {
    statusInstance = new AppConfig(cJSON_CreateObject(), NULL);
    statusInstance->eg = xEventGroupCreate();
    ESP_LOGV(__FUNCTION__,"Initializing Status");
  }
  return statusInstance;
}

const char *AppConfig::GetActiveStorage()
{
  AppConfig* stat = GetAppStatus();
  if (stat->activeStorage == NULL)
    return stat->SPIFFPATH;
  return stat->activeStorage == NULL ? stat->SPIFFPATH:stat->activeStorage;
}

bool AppConfig::isValid()
{
  return (json != NULL) && !cJSON_IsInvalid(json);
}

void AppConfig::MergeJSon(cJSON *curConfig, cJSON *newConfig)
{
  cJSON *curCfgItem = NULL;
  cJSON *newCfgItem = NULL;
  cJSON *curCfgValItem = NULL;
  cJSON *newCfgVerItem = NULL;
  cJSON *curCfgVerItem = NULL;
  cJSON *newCfgValItem = NULL;
  cJSON *curArrayItem = NULL;
  cJSON *newArrayItem = NULL;
  bool foundIt = false;
  uint8_t newIdx = 0, curIdx = 0;
  ESP_LOGD(__FUNCTION__, "Parsing src:%d dest:%d", curConfig == NULL, newConfig == NULL);

  xSemaphoreTakeRecursive(sema,portMAX_DELAY);
  cJSON_ArrayForEach(curCfgItem, curConfig)
  {
    if (curCfgItem && curCfgItem->string)
    {
      ESP_LOGD(__FUNCTION__, "Parsing %s item id:%s", cJSON_IsArray(curCfgItem) ? "Array" : cJSON_IsObject(curCfgItem) ? "Object"
                                                                                                                      : "Value",
              curCfgItem->string ? curCfgItem->string : "?");

      newCfgItem = cJSON_GetObjectItem(newConfig, curCfgItem->string);

      if ((curCfgVerItem = cJSON_GetObjectItem(curCfgItem, "version")) &&
          (curCfgValItem = cJSON_GetObjectItem(curCfgItem, "value")))
      {
        ESP_LOGD(__FUNCTION__, "Parsing %s item id:%s", "versioned field", curCfgItem->string ? curCfgItem->string : "?");
        int curVer = cJSON_GetNumberValue(curCfgVerItem);
        int newVer = -1;
        if ((newCfgItem != NULL) &&
            (newCfgVerItem = cJSON_GetObjectItem(newCfgItem, "version")) &&
            (newCfgValItem = cJSON_GetObjectItem(newCfgItem, "value")))
        {
          ESP_LOGD(__FUNCTION__, "New %s item id:%s", "versioned field", curCfgItem->string ? curCfgItem->string : "?");
          newVer = cJSON_GetNumberValue(newCfgVerItem);
          if (newVer > curVer)
          {
            cJSON_SetNumberValue(curCfgVerItem, newVer);
            cJSON_ReplaceItemInObject(curCfgValItem, newCfgValItem->string, cJSON_Duplicate(newCfgValItem, true));
          }
          else if (curVer > newVer)
          {
            cJSON_SetNumberValue(newCfgVerItem, curVer);
            cJSON_ReplaceItemInObject(newCfgValItem, curCfgValItem->string, cJSON_Duplicate(curCfgValItem, true));
          }
        }
        else
        {
          ESP_LOGD(__FUNCTION__, "Missing from new %s item id:%s", "versioned field", curCfgItem->string ? curCfgItem->string : "?");
          cJSON_AddItemToObject(newCfgValItem, curCfgValItem->string, cJSON_Duplicate(curCfgValItem, true));
          cJSON_DeleteItemFromObject(curConfig, curCfgItem->string);
        }
      }
      else if (cJSON_IsArray(curCfgItem))
      {
        ESP_LOGD(__FUNCTION__, "Parsing %s item id:%s", "Array field", curCfgItem->string ? curCfgItem->string : "?");
        if (newCfgItem && cJSON_IsArray(newCfgItem))
        {
          curIdx = 0;
          cJSON_ArrayForEach(curArrayItem, curCfgItem)
          {
            newIdx = 0;
            foundIt = false;
            cJSON_ArrayForEach(newArrayItem, newCfgItem)
            {
              if (cJSON_Compare(curArrayItem, newArrayItem, true))
              {
                foundIt = true;
                break;
              }
              newIdx++;
            }
            if (foundIt)
            {
              ESP_LOGD(__FUNCTION__, "****New %s item", "Array item");
              MergeJSon(curArrayItem, newArrayItem);
            }
            else
            {
              ESP_LOGD(__FUNCTION__, "Missing Array item from new %s item id:%d", "versioned field", curIdx);
              cJSON_AddItemToArray(newCfgItem, cJSON_Duplicate(curArrayItem, true));
              cJSON_DeleteItemFromArray(curCfgItem, curIdx);
            }
            curIdx++;
          }
        }
        else
        {
          ESP_LOGD(__FUNCTION__, "Missing Array field from new %s item id:%d", "versioned field", newIdx);
          cJSON_AddItemToObject(newConfig, curCfgItem->string, cJSON_Duplicate(curCfgItem, true));
          cJSON_DeleteItemFromObject(curConfig, curCfgItem->string);
        }
      }
      else if (cJSON_IsObject(curCfgItem))
      {
        if (newCfgItem != NULL)
        {
          ESP_LOGD(__FUNCTION__, "******Parsing %s item id:%s", "object field", curCfgItem->string ? curCfgItem->string : "?");
          MergeJSon(curCfgItem, newCfgItem);
        }
        else
        {
          ESP_LOGD(__FUNCTION__, "Missing object field from new %s item id:%s", "versioned field", curCfgItem->string);
          cJSON_AddItemToObject(newConfig, curCfgItem->string, cJSON_Duplicate(curCfgItem, true));
          cJSON_DeleteItemFromObject(curConfig, curCfgItem->string);
        }
      }
      else
      {
        ESP_LOGD(__FUNCTION__, "Parsing %s item id:%s", "value field", curCfgItem->string ? curCfgItem->string : "?");
        if (newCfgItem != NULL)
        {
          cJSON_ReplaceItemInObject(curConfig, newCfgItem->string, cJSON_Duplicate(newCfgItem,true));
        }
        else
        {
          cJSON_AddItemToObject(newConfig, curCfgItem->string, cJSON_Duplicate(curCfgItem, true));
          cJSON_DeleteItemFromObject(curConfig, curCfgItem->string);
        }
      }
    };
  }
  xSemaphoreGiveRecursive(sema);
}

void AppConfig::SetAppConfig(cJSON *config)
{
  if (config == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Save with empty set");
    return;
  }
  xSemaphoreTakeRecursive(sema,portMAX_DELAY);
  char *c1, *c2;
  if (strcmp((c1 = cJSON_PrintUnformatted(json)), (c2 = cJSON_PrintUnformatted(config))) != 0)
  {
    cJSON_Delete(json);
    json = config;
    version++;
    SaveAppConfig();
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "No changes to save");
  }
  ldfree(c1);
  ldfree(c2);
  xSemaphoreGiveRecursive(sema);
}

void AppConfig::ResetAppConfig(bool save)
{
  configInstance->json = cJSON_ParseWithLength((const char *)defaultconfig_json_start, defaultconfig_json_end - defaultconfig_json_start);
  if (save)
  {
    GetAppConfig()->SaveAppConfig(false);
    esp_restart();
  }
}

void AppConfig::SignalStateChange(state_change_t state)
{
  xEventGroupSetBits(GetAppStatus()->eg, state);
}

void AppConfig::SaveAppConfig()
{
  SaveAppConfig(false);
}

void AppConfig::SaveAppConfig(bool skipMount)
{
  AppConfig *config = AppConfig::GetAppConfig();
  if ((config == NULL) || (config->filePath == NULL) || (root != config))
  {
    return;
  }
  xSemaphoreTakeRecursive(sema,portMAX_DELAY);
  ESP_LOGD(__FUNCTION__, "Saving config %s",config->filePath);
  version++;
  if (!skipMount)
  {
    initSPISDCard();
  }
  FILE *currentCfg = fOpen(config->filePath, "w");
  if (currentCfg != NULL)
  {
    char *sjson = cJSON_PrintUnformatted(config->json);
    ESP_LOGV(__FUNCTION__, "Config(%d):%s", strlen(sjson),sjson);
    size_t wlen = fWrite(sjson, 1, strlen(sjson), currentCfg);
    if (fClose(currentCfg) != 0)
    {
      ESP_LOGE(__FUNCTION__, "Failed wo close config");
    } else {
      if (wlen != strlen(sjson)) {
        ESP_LOGE(__FUNCTION__,"Cannot write %d bytes, wrote %d bytes",strlen(sjson),wlen);
        esp_littlefs_format("storage");
        esp_restart();
      } else {
        ESP_LOGD(__FUNCTION__, "Wrote %d config bytes", wlen);
      }
    }
    ldfree(sjson);
  }
  else
  {
    ESP_LOGE(__FUNCTION__, "Cannot save config at %s", config->filePath);
  }
  if (!skipMount)
  {
    deinitSPISDCard();
  }
  xSemaphoreGiveRecursive(sema);
}

AppConfig *AppConfig::GetConfig(const char *path)
{
  if ((path == NULL) || (strlen(path) == 0) || (strcmp(path, "/") == 0))
  {
    return this;
  }
  return new AppConfig(GetJSONConfig(path), root);
}

cJSON *AppConfig::GetJSONConfig(const char *path, bool createWhenMissing)
{
  return GetJSONConfig(json, path, createWhenMissing);
}

cJSON *AppConfig::GetJSONConfig(const char *path)
{
  return GetJSONConfig(json, path, root != GetAppConfig());
}

cJSON *AppConfig::GetJSONConfig(cJSON *json, const char *path, bool createWhenMissing)
{
  if ((json == NULL) || (path == NULL) || (strlen(path) == 0) || (strcmp(path, "/") == 0))
  {
    return json;
  }
  ESP_LOGV(__FUNCTION__, "Getting JSON at path %s cwm:%d", path, createWhenMissing);
  xSemaphoreTakeRecursive(sema,portMAX_DELAY);

  if (path[0] == '/')
  {
    path++;
    ESP_LOGV(__FUNCTION__, "Removed heading / from JSON, path:%s", path);
  }

  char *slash = 0;
  cJSON* parJson;
  char* parPath = (char*) dmalloc(strlen(path)+1);
  strcpy(parPath,path);
  char* ctmp1 = NULL;
  if ((slash = indexOf(path, "/")) != NULL)
  {
    ctmp1 = lastIndexOf(parPath,"/");
    if (ctmp1){
      *ctmp1=0;
      parJson = GetJSONConfig(parPath);
    } else {
      parJson = cJSON_GetObjectItem(json,parPath);
    }
    ESP_LOGV(__FUNCTION__, "Parented by:%s", parPath);
  } else {
    parJson = json;
  }
  if (ctmp1) {
    if (parJson == NULL) {
      ESP_LOGE(__FUNCTION__,"Missing parent json");
      xSemaphoreGiveRecursive(sema);
      ldfree(parPath);
      return NULL;
    }
  } else {
    if (json == NULL) {
      ESP_LOGE(__FUNCTION__,"Missing parent json.");
      xSemaphoreGiveRecursive(sema);
      ldfree(parPath);
      return NULL;
    }
  }
  ESP_LOGV(__FUNCTION__, "Getting JSON at:%s from %s", ctmp1?ctmp1+1:parPath,parPath);
  cJSON* ret = NULL;
  if (!cJSON_HasObjectItem(ctmp1?parJson:json, ctmp1?ctmp1+1:parPath))
  {
    if (createWhenMissing){
      ESP_LOGV(__FUNCTION__, "%s was missing, creating as object", ctmp1?ctmp1+1:parPath);
      ret = cJSON_AddObjectToObject(ctmp1?parJson:json, ctmp1?ctmp1+1:parPath);
    } else {
      ESP_LOGV(__FUNCTION__, "%s was missing", ctmp1?ctmp1+1:parPath);
    }
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Got JSON at:%s", path);
    ret = cJSON_GetObjectItem(parJson, ctmp1?ctmp1+1:parPath);
  }
  xSemaphoreGiveRecursive(sema);
  ldfree(parPath);
  return ret;
}

bool AppConfig::isItemObject(const char *path)
{
  return GetPropertyHolder(GetJSONConfig(path)) == NULL;
}

cJSON *AppConfig::GetPropertyHolder(const char* path){
  return GetPropertyHolder(GetJSONProperty(json,path,false));
}

cJSON *AppConfig::GetPropertyHolder(cJSON *prop)
{
  if (prop == NULL)
  {
    return NULL;
  }
  xSemaphoreTakeRecursive(sema,portMAX_DELAY);

  cJSON* ret = NULL;
  if (cJSON_IsObject(prop))
  {
    if (cJSON_HasObjectItem(prop, "version"))
    {
      ESP_LOGV(__FUNCTION__, "JSon is a versioned object");
      ret = cJSON_GetObjectItem(prop, "value");
    }
    else
    {
      ESP_LOGV(__FUNCTION__, "JSon is an object but missing version");
      ret = prop;
    }
  } else {
    ESP_LOGV(__FUNCTION__, "JSon is not an object");
    ret = prop;
  }
  xSemaphoreGiveRecursive(sema);
  return ret;
}

cJSON *AppConfig::GetJSONProperty(const char *path)
{
  return GetJSONProperty(json, path, this == GetAppStatus());
}

cJSON *AppConfig::GetJSONProperty(cJSON *json, const char *path, bool createWhenMissing)
{
  if (!isValid())
  {
    ESP_LOGE(__FUNCTION__, "Cannot Get property at path:%s from null config", path);
    return NULL;
  }
  if ((path == NULL) || (strlen(path) == 0) || (strcmp(path, "/") == 0))
  {
    ESP_LOGW(__FUNCTION__, "Invalid os Missing path:%s", path == NULL ? "*null*" : path);
    return NULL;
  }
  if (json == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot Get property at path:%s from null json", path);
    return NULL;
  }
  if (path == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot Get property Missing path");
    return NULL;
  }

  if (path[0] == '/')
  {
    path++;
    ESP_LOGV(__FUNCTION__, "Path adjusted to %s", path);
  }

  ESP_LOGV(__FUNCTION__, "Getting JSON at %s", path);

  xSemaphoreTakeRecursive(sema,portMAX_DELAY);
  cJSON *prop = NULL;
  char *lastSlash = lastIndexOf(path, "/");
  if (lastSlash != NULL)
  {
    char *propPath = (char *)dmalloc(strlen(path));
    memcpy(propPath, path, lastSlash - path);
    propPath[lastSlash - path] = 0;
    ESP_LOGV(__FUNCTION__, "Pathed value prop at %s,%s,%s", path == NULL ? "*null*" : path, lastSlash, propPath);
    cJSON *holder = GetJSONConfig(json, propPath, createWhenMissing);
    if (holder != NULL)
    {
      ESP_LOGV(__FUNCTION__,"%s found through recursion",path);
      prop = cJSON_GetObjectItem(holder, lastSlash + 1);
    }
    else
    {
      char* ctmp = cJSON_Print(json);
      if (ctmp){
        ESP_LOGV(__FUNCTION__, "Cannot get property holder for %s in %s", propPath, ctmp);
      } else {
        ESP_LOGV(__FUNCTION__, "Cannot get property holder for %s unparsable json", propPath);
      }
      ldfree(ctmp);
    }
    ldfree(propPath);
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Value prop at %s(%d)", path == NULL ? "*null*" : path, createWhenMissing);
    if ((path == NULL) || (json == NULL))
    {
      ESP_LOGW(__FUNCTION__, "(path == NULL)%d (json == null)%d", (path == NULL), (json == NULL));
    }
    prop = cJSON_GetObjectItem(json, path);
    if (createWhenMissing && (prop == NULL))
    {
      if (filePath != NULL)
      {
        ESP_LOGV(__FUNCTION__, "Creating versioned prop at %s", path == NULL ? "*null*" : path);
        prop = cJSON_AddObjectToObject(json, path);
        cJSON_AddObjectToObject(prop, "value");
        cJSON_AddObjectToObject(prop, "version");
      }
      else
      {
        ESP_LOGV(__FUNCTION__, "Creating prop at %s", path == NULL ? "*null*" : path);
        prop = cJSON_CreateObject();
        cJSON_AddItemToObject(json, path, prop);
      }
    }
    else
    {
      if (prop == NULL)
        ESP_LOGV(__FUNCTION__, "Missing prop at %s", path == NULL ? "*null*" : path);
      else
      {
        ESP_LOGV(__FUNCTION__,"%s found",path);
      }
    }
  }

  xSemaphoreGiveRecursive(sema);
  return prop;
}

bool AppConfig::HasProperty(const char *path)
{
  return GetPropertyHolder(GetJSONProperty(json,path,false)) != NULL;
}

const char *AppConfig::GetStringProperty(const char *path)
{
  if (!isValid())
  {
    ESP_LOGW(__FUNCTION__,"%s is invalid",path);
    return NULL;
  }
  ESP_LOGV(__FUNCTION__, "Getting string value at %s", path == NULL ? "*null*" : path);
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if ((prop != NULL) && (prop->valuestring != NULL))
  {
    return prop->valuestring;
  }
  return emptyString;
}

void AppConfig::SetStringProperty(const char *path, const char *value)
{
  if (!isValid())
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s from null config", path);
    return;
  }
  if (value == NULL) {
    ESP_LOGW(__FUNCTION__, "Cannot set property at path:%s from null value", path);
    return;
  }

  cJSON *holder = GetJSONConfig(json, path, false);
  bool hasChanges = false;
  char* parPath = (char*) dmalloc(strlen(path)+1);
  strcpy(parPath,*path=='/'?path+1:path);
  char* ctmp1 = lastIndexOf(parPath,"/");
  if (ctmp1) {
    *ctmp1=0;
  }

  xSemaphoreTakeRecursive(sema,portMAX_DELAY);

  if (!holder)
  {
    holder = GetJSONConfig(parPath,true);
    if (filePath) {
      ESP_LOGV(__FUNCTION__, "Added versioned string for %s to %s", path, value);
      cJSON_AddStringToObject(holder, "value",value);
      cJSON_AddNumberToObject(holder, "version",0);
      hasChanges = true;
    } else {
      ESP_LOGV(__FUNCTION__, "Added straight up string for %s to %s(%s) parPath:%s", path, value, ctmp1?ctmp1+1:path, parPath);
      hasChanges = true;
      cJSON_DeleteItemFromObject(ctmp1?holder:json,ctmp1?ctmp1+1:path);
      cJSON_AddStringToObject(ctmp1?GetJSONConfig(parPath):json,ctmp1?ctmp1+1:path,value);
    }
    ldfree(parPath);
  } else if (cJSON_IsObject(holder))
  {
    ldfree(parPath);
    cJSON *val = cJSON_GetObjectItem(holder, "value");
    cJSON *version = cJSON_GetObjectItem(holder, "version");
    if (!val || !version) {
      ESP_LOGW(__FUNCTION__,"Weirdness at versioned path %s",path);
    } else {
      ESP_LOGV(__FUNCTION__, "holder exists for %s file:%s", path, filePath == NULL?"*NULL*":filePath);
      if (strcmp(val->valuestring, value) != 0)
      {
        ESP_LOGV(__FUNCTION__, "Setting versioned %s to %s", path, value);
        cJSON_SetValuestring(holder, value);
        cJSON_SetIntValue(version, version->valueint + 1);
        hasChanges = true;
      } else {
        ESP_LOGV(__FUNCTION__, "No change for %s to %s", path, value);
      }
    }
  }
  else if (strcmp(holder->valuestring, value) != 0)
  {
    cJSON_SetValuestring(holder, value);
    hasChanges = true;
    ESP_LOGV(__FUNCTION__, "Update straight up string for %s to %s", path, value);
  } else {
    ESP_LOGV(__FUNCTION__, "No change for %s to %s", path, value);
  }
  if (hasChanges)
    SaveAppConfig();
  xSemaphoreGiveRecursive(sema);
}

int32_t AppConfig::GetIntProperty(const char *path, int32_t defaultValue)
{
  if (!isValid())
  {
    return defaultValue;
  }

  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valueint;
  }
  return defaultValue;
}

int32_t AppConfig::GetIntProperty(const char *path)
{
  if (!isValid())
  {
    ESP_LOGE(__FUNCTION__,"Cannot get %s as json is invalid",path);
    return -1;
  }
  //ESP_LOGV(__FUNCTION__, "Getting int value at %s", path == NULL ? "*null*" : path);
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valueint;
  }
  return -1;
}

void AppConfig::SetIntProperty(const char *path, int value)
{
  if (!isValid())
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s from null(%d) or invalid(%d) config", path, json==NULL,cJSON_IsInvalid(json));
    return;
  }
  ESP_LOGV(__FUNCTION__, "Setting int value at %s=%d", path == NULL ? "*null*" : path,value);

  cJSON *holder = GetJSONConfig(json, path, false);
  char* parPath = (char*) dmalloc(strlen(path)+1);
  strcpy(parPath,*path=='/'?path+1:path);
  char* ctmp1 = lastIndexOf(parPath,"/");
  if (ctmp1) {
    *ctmp1=0;
  }

  xSemaphoreTakeRecursive(sema,portMAX_DELAY);
  if (!holder)
  {
    holder = GetJSONConfig(path,true);
    if (filePath) {
      ESP_LOGV(__FUNCTION__, "Added versioned int for %s to %d", path, value);
      cJSON_AddNumberToObject(holder, "value",value);
      cJSON_AddNumberToObject(holder, "version",0);
    } else {
      ESP_LOGV(__FUNCTION__, "Added straight up int for %s to %d", path, value);
      cJSON_DeleteItemFromObject(ctmp1?holder:json,ctmp1?ctmp1+1:path);
      cJSON_AddNumberToObject(ctmp1?holder:json,ctmp1?ctmp1+1:path,value);
    }
    SaveAppConfig();
    xSemaphoreGiveRecursive(sema);
    ldfree(parPath);
    return;
  }
  ldfree(parPath);

  if (cJSON_IsObject(holder))
  {
    cJSON *val = cJSON_GetObjectItem(holder, "value");
    cJSON *version = cJSON_GetObjectItem(holder, "version");
    if (!val || !version) {
      ESP_LOGW(__FUNCTION__,"Weirdness at versioned path %s",path);
      xSemaphoreGiveRecursive(sema);
      return;
    }
    ESP_LOGV(__FUNCTION__, "holder exists for %s file:%s", path, filePath == NULL?"*NULL*":filePath);
    if (val->valueint != value)
    {
      ESP_LOGV(__FUNCTION__, "Setting versioned %s to %d", path, value);
      cJSON_SetIntValue(holder, value);
      cJSON_SetIntValue(version, version->valueint + 1);
    } else {
      ESP_LOGV(__FUNCTION__, "No change for %s to %d", path, value);
    }
  }
  else if (holder->valueint != value)
  {
    cJSON_SetIntValue(holder, value);
    ESP_LOGV(__FUNCTION__, "Update straight up int for %s to %d", path, value);
  } else {
    ESP_LOGV(__FUNCTION__, "No change for %s to %d", path, value);
    xSemaphoreGiveRecursive(sema);
    return;
  }
  SaveAppConfig();
  xSemaphoreGiveRecursive(sema);
}

void AppConfig::SetLongProperty(const char *path, uint64_t value)
{
  if (!isValid())
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s from null config", path);
    return;
  }
  const char* cval = GetStringProperty(path);
  char tval[20];
  sprintf(tval,"%lld",value);

  if ((strlen(cval) == 0) || (std::atoll(cval) != value)) {
    SetStringProperty(path,tval);
    SaveAppConfig();
  } else {
    ESP_LOGV(__FUNCTION__,"No change for long %s",path);
  }
}

double AppConfig::GetDoubleProperty(const char *path)
{
  if (!isValid())
  {
    return -1;
  }
  //ESP_LOGV(__FUNCTION__, "Getting int value at %s", path == NULL ? "*null*" : path);
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valuedouble;
  }
  return -1;
}

void AppConfig::SetDoubleProperty(const char *path, double value)
{
  if (!isValid())
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s from null(%d) or invalid(%d) config", path, json==NULL,cJSON_IsInvalid(json));
    return;
  }
  ESP_LOGV(__FUNCTION__, "Setting int value at %s=%f", path == NULL ? "*null*" : path,value);

  cJSON *holder = GetJSONConfig(json, path, false);
  char* parPath = (char*) dmalloc(strlen(path)+1);
  strcpy(parPath,*path=='/'?path+1:path);
  char* ctmp1 = lastIndexOf(parPath,"/");
  if (ctmp1) {
    *ctmp1=0;
  }

  xSemaphoreTakeRecursive(sema,portMAX_DELAY);
  if (!holder)
  {
    holder = GetJSONConfig(path,true);
    if (filePath) {
      ESP_LOGV(__FUNCTION__, "Added versioned int for %s to %f", path, value);
      cJSON_AddNumberToObject(holder, "value",value);
      cJSON_AddNumberToObject(holder, "version",0);
    } else {
      ESP_LOGV(__FUNCTION__, "Added straight up int for %s to %f", path, value);
      cJSON_DeleteItemFromObject(ctmp1?holder:json,ctmp1?ctmp1+1:path);
      cJSON_AddNumberToObject(ctmp1?holder:json,ctmp1?ctmp1+1:path,value);
    }
    SaveAppConfig();
    xSemaphoreGiveRecursive(sema);
    ldfree(parPath);
    return;
  }
  ldfree(parPath);

  if (cJSON_IsObject(holder))
  {
    cJSON *val = cJSON_GetObjectItem(holder, "value");
    cJSON *version = cJSON_GetObjectItem(holder, "version");
    if (!val || !version) {
      ESP_LOGW(__FUNCTION__,"Weirdness at versioned path %s",path);
      xSemaphoreGiveRecursive(sema);
      return;
    }
    ESP_LOGV(__FUNCTION__, "holder exists for %s file:%s", path, filePath == NULL?"*NULL*":filePath);
    if (val->valueint != value)
    {
      ESP_LOGV(__FUNCTION__, "Setting versioned %s to %f", path, value);
      cJSON_SetIntValue(holder, value);
      cJSON_SetIntValue(version, version->valueint + 1);
    } else {
      ESP_LOGV(__FUNCTION__, "No change for %s to %f", path, value);
    }
  }
  else if (holder->valueint != value)
  {
    cJSON_SetIntValue(holder, value);
    ESP_LOGV(__FUNCTION__, "Update straight up int for %s to %f", path, value);
  } else {
    ESP_LOGV(__FUNCTION__, "No change for %s to %f", path, value);
    xSemaphoreGiveRecursive(sema);
    return;
  }
  SaveAppConfig();
  xSemaphoreGiveRecursive(sema);
}

bool AppConfig::GetBoolProperty(const char *path)
{
  if (!isValid())
  {
    return false;
  }
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valuestring != NULL ? strcmp(prop->valuestring, "true") == 0 : prop->valueint;
  }
  return false;
}

void AppConfig::SetBoolProperty(const char *path, bool value)
{
  SetIntProperty(path, (int)value);
}

gpio_num_t AppConfig::GetPinNoProperty(const char *path)
{
  return (gpio_num_t)GetIntProperty(path);
}

void AppConfig::SetPinNoProperty(const char *path, gpio_num_t value)
{
  SetIntProperty(path, (int)value);
}

void AppConfig::SetStateProperty(const char *path, item_state_t value)
{
  if ((strcmp("/sdcard/state",path)==0) && (value == item_state_t::ACTIVE)){
    if (value == item_state_t::ACTIVE){
      activeStorage = SDPATH;
      ESP_LOGD(__FUNCTION__,"Defined storage as %s", activeStorage);
    }
  }
  SetIntProperty(path, (int)value);
}

item_state_t AppConfig::GetStateProperty(const char *path)
{
  return (item_state_t)GetIntProperty(path,0);
}

bool AppConfig::IsAp()
{
  return indexOf(GetStringProperty("wifitype"), "AP") != NULL;
}

bool AppConfig::IsSta()
{
  return indexOf(GetStringProperty("wifitype"), "STA") != NULL;
}