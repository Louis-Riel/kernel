#include "logs.h"
#include "utils.h"
#include "esp_log.h"
#include "time.h"
#include "eventmgr.h"
#include "mfile.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static LogFunction_t callbacks[5];
static void* params[5];
static SemaphoreHandle_t logMutex = xSemaphoreCreateMutex();
static BufferedFile* logFile = NULL;
static char* curLogLine = NULL;
static char* curLogBuf = NULL;
static TaskHandle_t dltask = NULL;
static EventGroupHandle_t app_eg = xEventGroupCreate();
static bool buildingLogf = false;
const char* EMPTY_STRING = "";

const char* getLogFName(){
    if (logFile) {
        return logFile->GetName();
    }
    return EMPTY_STRING;
}

void dumpTheLogs(void* params){
    if (xEventGroupGetBits(getAppEG())&app_bits_t::TRIPS_SYNCING) {
        printf("\nNot flushing whilst dumping'n");
        return;
    }

    if (!logFile) {
        printf("\nNot flushing whilst dumping'n");
        return;
    }

    xSemaphoreTake(logMutex,portMAX_DELAY);
    bool isAp = GetAppConfig()->IsAp();
    if (dltask == NULL) {
        dltask = (TaskHandle_t)1; //Just not null
    }
    logFile->Flush();
    dltask = NULL;
    xSemaphoreGive(logMutex);
}

void dumpLogs(){
    if (xEventGroupGetBits(getAppEG())&app_bits_t::TRIPS_SYNCING) {
        ESP_LOGD(__FUNCTION__,"Not flushing whilst dumping");
        return;
    }

    if (!logFile) {
        ESP_LOGW(__FUNCTION__,"No logs");
        return;
    }

    if (dltask == NULL) {
        CreateWokeBackgroundTask(dumpTheLogs, "dumpLogs", 4096, (void*)true, tskIDLE_PRIORITY, &dltask);
    }
}

void registerLogCallback( LogFunction_t callback, void* param) {
    for (int idx=0; idx < 5; idx++){
        if (callbacks[idx] == NULL) {
            callbacks[idx]=callback;
            params[idx]=param;
            break;
        }
    }
}

void unregisterLogCallback( LogFunction_t callback) {
    for (int idx=0; idx < 5; idx++){
        if (callbacks[idx] == callback) {
            callbacks[idx]=NULL;
            break;
        }
    }
}

int loggit(const char *fmt, va_list args) {
    if (*fmt != 'V'){
        xSemaphoreTake(logMutex,portMAX_DELAY);
        if (!logFile) {
            struct tm timeinfo = { 0 };
            time_t now;
            time(&now);
            localtime_r(&now, &timeinfo);

            if (!buildingLogf && ((timeinfo.tm_year > 70) || (curLogBuf && (strlen(curLogBuf) >= LOG_BUF_ULIMIT)))) {
                buildingLogf = true;
                char* lpath=(char*)dmalloc(255);
                char* logfname=(char*)dmalloc(355);
                sprintf(lpath,"%s/logs/%s/%%Y/%%m/%%d/%%H-%%M-%%S.log",AppConfig::GetActiveStorage(),AppConfig::GetAppConfig()->GetStringProperty("devName"));
                localtime_r(&now, &timeinfo);
                strftime(logfname, 254, lpath, &timeinfo);
                logFile = new BufferedFile(logfname);
                ldfree(lpath);
                ldfree(logfname);
            }
            if (!curLogBuf) {
                curLogBuf = (char*)dmalloc(LOG_BUF_SIZE);
                *curLogBuf=0;
                curLogLine = curLogBuf;
            }
        }

        if (logFile) {
            if (curLogBuf) {
                if (*curLogBuf != 0) {
                    logFile->Write((uint8_t*)curLogBuf,strlen(curLogBuf));
                }
                ldfree(curLogBuf);
                curLogBuf=NULL;
                curLogLine=(char*)dmalloc(LOG_LN_SIZE);
                *curLogLine=0;
            }
            vsprintf(curLogLine,fmt,args);
            logFile->Write((uint8_t*)curLogLine,strlen(curLogLine));
        } else {
            curLogLine+=vsprintf(curLogLine,fmt,args);
            *curLogLine=0;
        }

        for (int idx=0; idx < 5; idx++){
            if (callbacks[idx] != NULL) {
                if (!callbacks[idx](params[idx],curLogLine)){
                    callbacks[idx] = nullptr;
                }
            }
        }
        xSemaphoreGive(logMutex);
    }
    return vprintf(fmt, args);
}

void initLog() {
    curLogLine = NULL;
    curLogBuf = NULL;
    esp_log_set_vprintf(loggit);
}

EventGroupHandle_t getAppEG() {
  return app_eg;
}
