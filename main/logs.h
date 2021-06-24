#ifndef __logs_h
#define __logs_h

//#define IS_TRACKER

#define LOG_BUF_SIZE 102400
#define LOG_BUF_ULIMIT 102300
#define LOG_FNAME_LEN 100

#include "../build/config/sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "cJSON.h"
#include "freertos/event_groups.h"

static EventGroupHandle_t app_eg = xEventGroupCreate();
EventGroupHandle_t* getAppEG();

typedef enum {
 BUMP_BIT = BIT0,
 COMMITTING_TRIPS = BIT1,
 TRIPS_COMMITTED = BIT2,
 TRIPS_SYNCED = BIT3
} app_bits_t;

void initLog();
void dumpLogs();
char* getLogFName();
typedef bool (*LogFunction_t)( void * ,char * );
void registerLogCallback( LogFunction_t callback, void* param);

#endif