#ifndef __utils_h
#define __utils_h

//#define IS_TRACKER

#include "../build/config/sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include <stdlib.h>
#include "../components/TinyGPS/TinyGPS++.h"

#define SS TF_CS
#define MAX_NUM_POIS 10

extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
extern const unsigned char index_html_start[] asm("_binary_index_html_start");
extern const unsigned char index_html_end[]   asm("_binary_index_html_end");
extern const unsigned char app_css_start[] asm("_binary_app_css_start");
extern const unsigned char app_css_end[]   asm("_binary_app_css_end");
extern const unsigned char app_js_start[] asm("_binary_app_js_start");
extern const unsigned char app_js_end[]   asm("_binary_app_js_end");


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

struct app_config_t {
  uint32_t devId;
  cfg_label_t devName;
  struct sdcard_config_t {
    cfg_gpio_t MisoPin;
    cfg_gpio_t MosiPin;
    cfg_gpio_t ClkPin;
    cfg_gpio_t Cspin;
  } sdcard_config;
  struct gps_config_t {
    cfg_gpio_t rxPin;
    cfg_gpio_t txPin;
    cfg_gpio_t enPin;
  } gps_config;
  enum purpose_t {
    UNKNOWN = 0,
    TRACKER = BIT0,
    PULLER = BIT1
  } purpose;
  cfg_gpio_t wakePins[10];
  poiConfig_t pois[MAX_NUM_POIS];
};

enum item_state_t {
  UNKNOWN = BIT0,
  ACTIVE = BIT1,
  ERROR = BIT2,
  PAUSED = BIT3,
  INACTIVE = BIT4
};

struct app_state_t {
  item_state_t gps;
  item_state_t sdCard;
  double lattitude;
  double longitude;
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

struct trip
{
  uint32_t numNodes;
  uint32_t nodesAllocated;
  dataPoint **nodes;
  char fname[33];
  uint32_t lastExportedTs = 0;
};

bool startsWith(const char* str,const char* key);
uint8_t* loadImage(bool reset,uint32_t* iLen);
void sampleBatteryVoltage();
float getBatteryVoltage();
bool moveFile(char* src, char* dest);
static const char* getErrorMsg(uint32_t errCode);
bool initSPISDCard();
bool deinitSPISDCard();
bool initSDMMCSDCard();
char* indexOf(const char* str, const char* key);
bool endsWith(const char* str,const char* val) ;
bool stringContains(const char* str,const char* val) ;
FILE * fopen (const char * _name, const char * _type,bool createDir);
app_config_t* initConfig();
app_config_t* getAppConfig();
app_state_t* getAppState();
void saveConfig();
void commitTripToDisk(void* param);
trip* getActiveTrip();
void stopGps();
int64_t getSleepTime();
int64_t getUpTime();

#endif