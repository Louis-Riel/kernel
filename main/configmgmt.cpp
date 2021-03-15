#include "utils.h"
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static app_state_t app_state;

extern const uint8_t defaultconfig_json_start[] asm("_binary_defaultconfig_json_start");
extern const uint8_t defaultconfig_json_end[] asm("_binary_defaultconfig_json_end");

app_state_t *getAppState()
{
  return &app_state;
}

void cJSON_AddVersionedStringToObject(cfg_label_t *itemToAdd, char *name, cJSON *dest)
{
  cJSON *item = cJSON_CreateObject();
  cJSON_AddItemToObject(item, "value", cJSON_CreateString(itemToAdd->value));
  cJSON_AddItemToObject(item, "version", cJSON_CreateNumber(itemToAdd->version));
  cJSON_AddItemToObject(dest, name, item);
}

void cJSON_AddVersionedGpioToObject(cfg_gpio_t *itemToAdd, char *name, cJSON *dest)
{
  cJSON *item = cJSON_CreateObject();
  cJSON_AddItemToObject(item, "value", cJSON_CreateNumber(itemToAdd->value));
  cJSON_AddItemToObject(item, "version", cJSON_CreateNumber(itemToAdd->version));
  cJSON_AddItemToObject(dest, name, item);
}

AppConfig *GetAppConfig()
{
  return AppConfig::GetAppConfig();
}

AppConfig *AppConfig::configInstance = NULL;
AppConfig *AppConfig::statusInstance = NULL;

AppConfig::AppConfig(char *filePath)
{
  this->filePath=filePath;
  if ((configInstance == NULL) && (filePath != NULL))
  {
    ESP_LOGV(__FUNCTION__, "Setting global config instance");
    configInstance = this;
    FILE *currentCfg = fOpen(filePath, "r");
    if (currentCfg == NULL)
    {
      ESP_LOGD(__FUNCTION__, "Getting default config for %s", filePath);
      json = cJSON_ParseWithLength((const char *)defaultconfig_json_start, defaultconfig_json_end - defaultconfig_json_start);
      AppConfig::SaveAppConfig(true);
    }
    else
    {
      ESP_LOGD(__FUNCTION__, "Reading config at %s", filePath);
      struct stat fileStat;
      fstat(fileno(currentCfg), &fileStat);
      char *sjson = (char *)malloc(fileStat.st_size);
      fread(sjson, 1, fileStat.st_size, currentCfg);
      fClose(currentCfg);
      cJSON* toBeCfg = cJSON_ParseWithLength(sjson, fileStat.st_size);
      if ((toBeCfg != NULL) && (cJSON_GetObjectItem(toBeCfg,"type") != NULL)) {
        json = toBeCfg;
      } else {
        ESP_LOGE(__FUNCTION__,"Corrupted configuration, not applying:%s",sjson);
      }
      free(sjson);
    }
  }
}

EventGroupHandle_t AppConfig::GetStateGroupHandle(){
  return GetAppStatus()->eg;
}


AppConfig::AppConfig(cJSON *config)
{
  json = config;
}

AppConfig *AppConfig::GetAppConfig()
{
  return AppConfig::configInstance;
}

AppConfig *AppConfig::GetAppStatus()
{
  if (statusInstance == NULL)
  {
    statusInstance = new AppConfig((char*)NULL);
    statusInstance->eg = xEventGroupCreate();
  }
  return statusInstance;
}

void AppConfig::SetAppConfig(cJSON *config)
{
  if (config == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Save with empty set");
    return;
  }
  char *c1, *c2;
  if (strcmp((c1 = cJSON_PrintUnformatted(json)), (c2 = cJSON_PrintUnformatted(config))) != 0)
  {
    cJSON_Delete(json);
    json = config;
    SaveAppConfig();
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "No changes to save");
  }
  free(c1);
  free(c2);
}

void AppConfig::ResetAppConfig(bool save)
{
  configInstance->json = cJSON_ParseWithLength((const char *)defaultconfig_json_start, defaultconfig_json_end - defaultconfig_json_start);
  if (save) {
    AppConfig::SaveAppConfig(false);
    esp_restart();
  }
}

void AppConfig::SignalStateChange(){
    xEventGroupSetBits(GetAppStatus()->eg,state_change_t::CHANGED);
    xEventGroupClearBits(GetAppStatus()->eg,state_change_t::CHANGED);
}

void AppConfig::SaveAppConfig()
{
  AppConfig::SaveAppConfig(false);
}

void AppConfig::SaveAppConfig(bool skipMount)
{
  AppConfig *config = AppConfig::GetAppConfig();
  if (config->filePath == NULL)
  {
    return;
  }
  ESP_LOGD(__FUNCTION__, "Saving config");
  if (!skipMount)
  {
    initSPISDCard();
  }
  FILE *currentCfg = fOpen(config->filePath, "w");
  if (currentCfg != NULL)
  {
    char *sjson = cJSON_Print(config->json);
    fwrite(sjson, 1, strlen(sjson), currentCfg);
    fClose(currentCfg);
    free(sjson);
  }
  else
  {
    ESP_LOGE(__FUNCTION__, "Cannot save config at %s", config->filePath);
  }
  if (!skipMount)
  {
    deinitSPISDCard();
  }
}

AppConfig *AppConfig::GetConfig(char *path)
{
  if ((path == NULL) || (strlen(path) == 0) || (strcmp(path, "/") == 0))
  {
    return this;
  }
  return new AppConfig(GetJSONConfig(path));
}

cJSON *AppConfig::GetJSONConfig(char *path)
{
  return GetJSONConfig(json, path, false);
}

cJSON *AppConfig::GetJSONConfig(cJSON *json, char *path, bool createWhenMissing)
{
  if ((path == NULL) || (strlen(path) == 0) || (strcmp(path, "/") == 0))
  {
    return json;
  }
  ESP_LOGV(__FUNCTION__, "Getting JSON at path %s", path);

  if (path[0] == '/')
  {
    path++;
    ESP_LOGV(__FUNCTION__, "Removed heading / from JSON, path:%s", path);
  }

  char *slash = 0;
  if ((slash = indexOf(path, "/")) > 0)
  {
    ESP_LOGV(__FUNCTION__, "Getting Child JSON as:%s", slash);
    char *name = (char *)malloc((slash - path) + 1);
    memcpy(name, path, slash - path);
    cJSON *parJson = cJSON_GetObjectItem(json, slash + 1);
    if (createWhenMissing && (parJson == NULL))
    {
      ESP_LOGV(__FUNCTION__, "Creating missing level at path %s", path);
      parJson = cJSON_AddObjectToObject(json, name);
    }
    free(name);
    return GetJSONConfig(parJson, slash + 1, createWhenMissing);
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Getting JSON as:%s", path);
    if (createWhenMissing && !cJSON_HasObjectItem(json, path))
    {
      ESP_LOGV(__FUNCTION__, "%s was missing", path);
      return cJSON_AddObjectToObject(json, path);
    }
    else
    {
      return cJSON_GetObjectItem(json, path);
    }
  }
}

cJSON *AppConfig::GetPropertyHolder(cJSON *prop)
{
  if (prop == NULL)
  {
    return NULL;
  }

  if (cJSON_IsObject(prop))
  {
    if (cJSON_HasObjectItem(prop, "version"))
    {
      return cJSON_GetObjectItem(prop, "value");
    }
    else
    {
      char *sjson = cJSON_Print(prop);
      ESP_LOGE(__FUNCTION__, "JSon is an object but missing version:%s", sjson);
      free(sjson);
      return NULL;
    }
  }
  return prop;
}

cJSON *AppConfig::GetJSONProperty(char *path)
{
  return GetJSONProperty(json, path, false);
}

cJSON *AppConfig::GetJSONProperty(cJSON *json, char *path, bool createWhenMissing)
{
  if ((path == NULL) || (strlen(path) == 0) || (strcmp(path, "/") == 0))
  {
    ESP_LOGW(__FUNCTION__, "Invalid os Missing path:%s", path == NULL ? "*null*" : path);
    return NULL;
  }
  ESP_LOGV(__FUNCTION__, "Getting JSON at %s", path);

  if (path[0] == '/')
  {
    path++;
    ESP_LOGV(__FUNCTION__, "Path adjusted to %s", path);
  }

  char *lastSlash = lastIndexOf(path, "/");
  if (lastSlash > 0)
  {
    char *propPath = (char *)malloc(strlen(path));
    memcpy(propPath, path, lastSlash - path);
    propPath[lastSlash - path] = 0;
    ESP_LOGV(__FUNCTION__, "Pathed value prop at %s,%s,%s", path == NULL ? "*null*" : path, lastSlash, propPath);
    cJSON *holder = GetJSONConfig(json, propPath, createWhenMissing);
    if (holder != NULL)
    {
      free(propPath);
      return cJSON_GetObjectItem(holder, lastSlash + 1);
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "Cannot get property holder for %s", propPath);
      free(propPath);
    }
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Value prop at %s(%d)", path == NULL ? "*null*" : path, createWhenMissing);
    cJSON *prop = cJSON_GetObjectItem(json, path);
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
    }
    return prop;
  }

  return NULL;
}

bool AppConfig::HasProperty(char *path)
{
  return GetPropertyHolder(GetJSONProperty(path)) != NULL;
}

char *AppConfig::GetStringProperty(char *path)
{
  ESP_LOGV(__FUNCTION__, "Getting int value at %s", path == NULL ? "*null*" : path);
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valuestring;
  }
  ESP_LOGW(__FUNCTION__, "Nothing to get at %s", path);
  return NULL;
}

void AppConfig::SetStringProperty(char *path, char *value)
{
  cJSON *holder = GetJSONConfig(json, path, true);
  if (holder == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s", path);
    return;
  }

  cJSON *val = cJSON_GetObjectItem(holder, "value");
  cJSON *version = cJSON_GetObjectItem(holder, "version");

  if (cJSON_IsObject(holder))
  {
    if (filePath != NULL)
    {
      if (version == NULL)
      {
        ESP_LOGV(__FUNCTION__, "Creating versioned %s to %s", path, value);
        val = cJSON_AddStringToObject(holder, "value", value);
        version = cJSON_AddNumberToObject(holder, "version", 0);
        AppConfig::SaveAppConfig();
      }
      else if (strcmp(val->valuestring, value) != 0)
      {
        ESP_LOGV(__FUNCTION__, "Setting versioned %s to %s", path, value);
        cJSON_SetValuestring(holder, value);
        cJSON_SetIntValue(version, version->valueint + 1);
        AppConfig::SaveAppConfig();
      }
    }
    else
    {
      cJSON_SetValuestring(holder, value);
    }
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Setting %s to %s", path, value);
    cJSON_SetValuestring(holder, value);
    AppConfig::SaveAppConfig();
  }
}

int32_t AppConfig::GetIntProperty(char *path)
{
  ESP_LOGV(__FUNCTION__, "Getting int value at %s", path == NULL ? "*null*" : path);
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valueint;
  }
  ESP_LOGW(__FUNCTION__, "Nothing to get at %s", path);
  return -1;
}

void AppConfig::SetIntProperty(char *path, int value)
{
  cJSON *holder = GetJSONConfig(json, path, true);
  if (holder == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s", path);
    return;
  }
  if (cJSON_IsObject(holder))
  {
    cJSON *val = cJSON_GetObjectItem(holder, "value");
    cJSON *version = cJSON_GetObjectItem(holder, "version");

    if (version == NULL)
    {
      ESP_LOGV(__FUNCTION__, "Creating versioned %s to %d", path, value);
      val = cJSON_AddNumberToObject(holder, "value", value);
      version = cJSON_AddNumberToObject(holder, "version", 0);
      AppConfig::SaveAppConfig();
    }
    else if (val->valueint != value)
    {
      ESP_LOGV(__FUNCTION__, "Setting versioned %s to %d", path, value);
      cJSON_SetIntValue(val, value);
      cJSON_SetIntValue(version, version->valueint + 1);
      AppConfig::SaveAppConfig();
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "No change at path %s", path);
    }
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Setting %s to %d", path, value);
    cJSON_SetIntValue(holder, value);
    AppConfig::SaveAppConfig();
  }
}

double AppConfig::GetDoubleProperty(char *path)
{
  ESP_LOGV(__FUNCTION__, "Getting int value at %s", path == NULL ? "*null*" : path);
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valuedouble;
  }
  ESP_LOGW(__FUNCTION__, "Nothing to get at %s", path);
  return -1;
}

void AppConfig::SetDoubleProperty(char *path, double value)
{
  cJSON *holder = GetJSONConfig(json, path, true);
  if (holder == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s", path);
    return;
  }
  if (cJSON_IsObject(holder))
  {
    cJSON *val = cJSON_GetObjectItem(holder, "value");
    cJSON *version = cJSON_GetObjectItem(holder, "version");

    if (version == NULL)
    {
      ESP_LOGV(__FUNCTION__, "Creating versioned %s to %f", path, value);
      val = cJSON_AddNumberToObject(holder, "value", value);
      version = cJSON_AddNumberToObject(holder, "version", 0);
      AppConfig::SaveAppConfig();
    }
    else if (val->valuedouble != value)
    {
      ESP_LOGV(__FUNCTION__, "Setting versioned %s to %f", path, value);
      cJSON_SetNumberValue(val, value);
      cJSON_SetIntValue(version, version->valueint + 1);
      AppConfig::SaveAppConfig();
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "No change at path %s", path);
    }
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Setting %s to %f", path, value);
    cJSON_SetIntValue(holder, value);
    AppConfig::SaveAppConfig();
  }
}

bool AppConfig::GetBoolProperty(char *path)
{
  cJSON *prop = GetPropertyHolder(GetJSONProperty(path));
  if (prop != NULL)
  {
    return prop->valuestring != NULL ? strcmp(prop->valuestring, "true") == 0 : prop->valueint;
  }
  ESP_LOGW(__FUNCTION__, "Nothing to get at %s", path);
  return false;
}

void AppConfig::SetBoolProperty(char *path, bool value)
{
  cJSON *holder = GetJSONConfig(json, path, true);
  if (holder == NULL)
  {
    ESP_LOGE(__FUNCTION__, "Cannot set property at path:%s", path);
    return;
  }
  if (cJSON_IsObject(holder))
  {
    cJSON *val = cJSON_GetObjectItem(holder, "value");
    cJSON *version = cJSON_GetObjectItem(holder, "version");

    if (version == NULL)
    {
      ESP_LOGV(__FUNCTION__, "Creating versioned %s to %d", path, value);
      val = cJSON_AddNumberToObject(holder, "value", value);
      version = cJSON_AddNumberToObject(holder, "version", 0);
      AppConfig::SaveAppConfig();
    }
    else if (val->valueint != value)
    {
      ESP_LOGV(__FUNCTION__, "Setting versioned %s to %d", path, value);
      cJSON_SetIntValue(holder, value);
      cJSON_SetIntValue(version, version->valueint + 1);
      AppConfig::SaveAppConfig();
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "No change at path %s", path);
    }
  }
  else
  {
    ESP_LOGV(__FUNCTION__, "Setting %s to %d", path, value);
    cJSON_SetIntValue(holder, value);
    AppConfig::SaveAppConfig();
  }
}

gpio_num_t AppConfig::GetPinNoProperty(char *path)
{
  return (gpio_num_t)GetIntProperty(path);
}

void AppConfig::SetPinNoProperty(char *path, gpio_num_t value)
{
  SetIntProperty(path, (int)value);
}

bool AppConfig::IsAp()
{
  return indexOf(GetStringProperty("type"), "AP") != NULL;
}

bool AppConfig::IsSta()
{
  return indexOf(GetStringProperty("type"), "STA") != NULL;
}