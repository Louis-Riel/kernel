#ifndef __utils_h
#define __utils_h

//#define IS_TRACKER

#include "../build/config/sdkconfig.h"
#include "mallocdbg.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdlib.h>
#include "cJSON.h"
#include "esp_littlefs.h"

#define SS TF_CS
#define MAX_NUM_POIS 10
#define MAX_NUM_PINS 25
#define MAX_CONFIG_INSTANCES 20
#define CFG_PATH "/lfs/config/current.json"

#define MESSAGE_BUFFER_SIZE 4096
#define NUM_IMG_BUFFERS     512
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define F_BUF_SIZE 8192
#define HTTP_BUF_SIZE 8192
#define HTTP_CHUNK_SIZE 8192
#define JSON_BUFFER_SIZE 8192
#define KML_BUFFER_SIZE 204600

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define HTTP_BUF_SIZE 8192
#define HTTP_CHUNK_SIZE 8192
#define HTTP_RECEIVE_BUFFER_SIZE 2048
#define HTTP_MAX_RECEIVE_BUFFER_SIZE 3145728
#define HTTP_MAX_NUM_BUFFER_CHUNCK HTTP_MAX_RECEIVE_BUFFER_SIZE/HTTP_RECEIVE_BUFFER_SIZE
#define JSON_BUFFER_SIZE 8192
#define KML_BUFFER_SIZE 204600

extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[]   asm("_binary_index_html_end");
extern const unsigned char app_css_start[] asm("_binary_app_css_start");
extern const unsigned char app_css_end[]   asm("_binary_app_css_end");
extern const unsigned char app_js_start[] asm("_binary_app_js_start");
extern const unsigned char app_js_end[]   asm("_binary_app_js_end");
extern const unsigned char jsonschema_start[] asm("_binary_configschema_json_start");
extern const unsigned char jsonschema_end[]   asm("_binary_configschema_json_end");

struct poiConfig_t {
  float lat;
  float lng;
  uint16_t minDistance;
  uint16_t statusBits;
};

struct cfg_label_t
{
  char value[32];
  uint32_t version;
};

struct cfg_gpio_t
{
  gpio_num_t value;
  uint32_t version;
};

struct sdcard_config_t {
    cfg_gpio_t MisoPin;
    cfg_gpio_t MosiPin;
    cfg_gpio_t ClkPin;
    cfg_gpio_t Cspin;
};

struct gps_config_t {
  cfg_gpio_t rxPin;
  cfg_gpio_t txPin;
  cfg_gpio_t enPin;
};  

enum purpose_t {
  UNKNOWN_PURPOSE = 0,
  TRACKER = BIT0,
  PULLER = BIT1
};

enum state_change_t {
  MAIN = BIT0,
  GPS = BIT1,
  WIFI = BIT2
};

struct gpio_driver_t {
  cfg_gpio_t pinNo;
  cfg_label_t pinName;
  bool isActive;
  enum driver_type_t {
    digital_in = BIT0,
    digital_out = BIT1,
    pullup = BIT2,
    pulldown = BIT3,
    touch = BIT4
  } driverFlags;
};

enum item_state_t {
  UNKNOWN = BIT0,
  ACTIVE = BIT1,
  ERROR = BIT2,
  PAUSED = BIT3,
  INACTIVE = BIT4
};

struct dataPoint
{
  uint32_t ts;
  double lng;
  double lat;
  uint16_t speed;
  uint16_t course;
  float bat;
  uint32_t freeRam;
  uint32_t bumpCnt;
  uint32_t altitude;
};

class AppConfig {
public:
  AppConfig(char* filePath);
  AppConfig(cJSON* json, AppConfig* root);

  static AppConfig* GetAppConfig();
  static AppConfig* GetAppStatus();
  static EventGroupHandle_t GetStateGroupHandle();
  static void ResetAppConfig(bool save);
  void SaveAppConfig();
  static const char* GetActiveStorage();

  bool isValid();
  bool isItemObject(const char* path);
  AppConfig* GetConfig(const char* path);
  cJSON* GetJSONConfig(const char* path);
  cJSON* GetJSONConfig(const char *path, bool createWhenMissing);
  void SetAppConfig(cJSON* config);
  static void MergeJSon(cJSON* curConfig, cJSON *newConfig);

  bool HasProperty(const char* path);

  char* GetStringProperty(const char* path);
  int32_t GetIntProperty(const char* path);
  gpio_num_t GetPinNoProperty(const char* path);
  item_state_t GetStateProperty(const char* path);
  double GetDoubleProperty(const char* path);
  bool GetBoolProperty(const char* path);
  static void SignalStateChange(state_change_t state);
  uint32_t version;

  void SetStringProperty(const char* path,const char* value);
  void SetIntProperty(const char* path,int32_t value);
  void SetPinNoProperty(const char* path,gpio_num_t value);
  void SetStateProperty(const char* path,item_state_t value);
  void SetDoubleProperty(const char* path,double value);
  void SetBoolProperty(const char* path,bool value);
  bool IsAp();
  bool IsSta();
  static cJSON* GetPropertyHolder(cJSON* prop);
protected:
  const char *SDPATH = "/sdcard";
  const char *SPIFFPATH = "/lfs";

  cJSON* GetJSONProperty(cJSON* json,const char* path, bool createWhenMissing);
  cJSON* GetJSONProperty(const char* path);
  cJSON* GetJSONConfig(cJSON* json, const char* path,bool createWhenMissing);
  void SaveAppConfig(bool skipMount);
  int32_t GetIntProperty(const char *path, int32_t defaultValue);
  cJSON* json;
  static AppConfig* configInstance;
  static AppConfig* statusInstance;
  EventGroupHandle_t eg;
  char* filePath;
  AppConfig* root = NULL;
  char* activeStorage;
};

void UpgradeFirmware();
AppConfig* GetAppConfig();
bool startsWith(const char* str,const char* key);
void sampleBatteryVoltage();
float getBatteryVoltage();
bool moveFile(const char* src, const char* dest);
bool initSPISDCard();
bool initSPISDCard(bool);
bool deinitSPISDCard();
bool deinitSPISDCard(bool);
bool initSDMMCSDCard();
char* indexOf(const char* str, const char* key);
char* lastIndexOf(const char* str, const char* key);
bool endsWith(const char* str,const char* val) ;
bool stringContains(const char* str,const char* val) ;

void commitTripToDisk(void* param);
void stopGps();
int64_t getSleepTime();
int64_t getUpTime();
void cJSON_AddVersionedStringToObject(cfg_label_t* itemToAdd, char* name, cJSON* dest);
void cJSON_AddVersionedGpioToObject(cfg_gpio_t* itemToAdd, char* name,  cJSON* dest);
uint32_t GetNumOpenFiles();
bool rmDashFR(char* folderName);
bool deleteFile(char* fileName);

#endif