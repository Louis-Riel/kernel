#ifndef __logs_h
#define __logs_h

//#define IS_TRACKER

#define LOG_BUF_SIZE 102400
#define LOG_BUF_ULIMIT 102300
#define LOG_FNAME_LEN 100

#include "../build/config/sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"

void initLog();
void dumpLogs();
char* getLogFName();

#endif