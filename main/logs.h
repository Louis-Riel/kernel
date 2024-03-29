#ifndef __logs_h
#define __logs_h

//#define IS_TRACKER

//#define LOG_BUF_SIZE 10240
//#define LOG_BUF_ULIMIT 8000
#define LOG_BUF_SIZE   102400
#define LOG_BUF_ULIMIT 100000
#define LOG_LN_SIZE 16384

#include "../build/config/sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/event_groups.h"

EventGroupHandle_t getAppEG();

typedef enum {
 BUMP_BIT = BIT0,
 COMMITTING_TRIPS = BIT1,
 TRIPS_COMMITTED = BIT2,
 TRIPS_SYNCING = BIT3,
 WIFI_ON = BIT4,
 SPIFF_MOUNTED = BIT5,
 SDCARD_MOUNTED = BIT6,
 SDCARD_ERROR = BIT7,
 REST = BIT8,
 GPS_ON = BIT9,
 WIFI_OFF = BIT10,
 GPS_OFF = BIT11,
 HIBERNATE = BIT12,
 IR = BIT13,
 REST_OFF = BIT14,
 SDCARD_WORKING = BIT15,
 MAX_APP_BITS = 16
} app_bits_t;

#define APP_SERVICE_BITS (app_bits_t::WIFI_ON|app_bits_t::WIFI_OFF|app_bits_t::REST|app_bits_t::REST_OFF|app_bits_t::GPS_ON|app_bits_t::HIBERNATE|app_bits_t::IR)

void initLog();
void dumpLogs();
void dumpTheLogs(void* params);
const char* getLogFName();
typedef bool (*LogFunction_t)( void * ,char * );
void registerLogCallback( LogFunction_t callback, void* param);
void unregisterLogCallback( LogFunction_t callback);

#endif