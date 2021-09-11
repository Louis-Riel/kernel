#include "logs.h"
#include "utils.h"
#include "esp_log.h"
#include "time.h"
#include "eventmgr.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

char* logBuff = NULL;
char* logfname = NULL;
uint32_t logBufPos;
static LogFunction_t callbacks[5];
static void* params[5];
static SemaphoreHandle_t logMutex = xSemaphoreCreateMutex();

char* getLogFName(){
    return logfname;
}

static TaskHandle_t dltask = NULL;

void dumpTheLogs(void* params){
    bool isAp = GetAppConfig()->IsAp();
    if (logBufPos == 0) {
        ESP_LOGD(__FUNCTION__,"No logs to dump");
        dltask=NULL;
        return;
    }
    if (xEventGroupGetBits(getAppEG())&app_bits_t::TRIPS_SYNCING) {
        ESP_LOGD(__FUNCTION__,"Not flushing whilst dumping");
        dltask=NULL;
        *logBuff=0;
        logBufPos=0;
        return;
    }
    if (dltask == NULL) {
        dltask = (TaskHandle_t)1; //Just not null
    }

    xSemaphoreTake(logMutex,portMAX_DELAY);
    uint8_t* db = (uint8_t*)dmalloc(logBufPos);
    uint32_t len = logBufPos;
    if (db) {
        memccpy(db,logBuff,sizeof(uint8_t),logBufPos);
        *logBuff=0;
        logBufPos=0;
    }
    xSemaphoreGive(logMutex);

    if (len && initSPISDCard(true)) {
        FILE* fw = NULL;

        struct tm timeinfo;
        time_t now = ::time(NULL);

        localtime_r(&now, &timeinfo);
        
        if ((strlen(logfname) == 0) || (indexOf(logfname,"1970") && (timeinfo.tm_year > 1970))){
            char lpath[255];
            sprintf(lpath,"%s/logs/%s-%%Y-%%m-%%d_%%H-%%M-%%S.log",AppConfig::GetActiveStorage(),isAp?"TRACKER":"PULLER");
            strftime(logfname, 254, lpath, &timeinfo);
        }

        size_t bc = __UINT32_MAX__;
        if (((fw = fOpenCdL(logfname,"a",true,false)) != NULL) &&
            ((bc=fWrite((void*)db,1,len,fw)) == len)) {
            if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG){
                fprintf(stdout,"\nWritten %d into %s\n",len,logfname);
            }
        } else if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG) {
            fprintf(stderr,"\nLogs not written to %s fw:%d len:%d",logfname,fw!=NULL,bc);
        }
        fClose(fw);
        deinitSPISDCard(false);
    } else {
        ESP_LOGD(__FUNCTION__,"No logs to dump I think %d",len);
    }
    dltask = NULL;
    ldfree(db);
}

void dumpLogs(){
    if (dltask == NULL)
        CreateWokeBackgroundTask(dumpTheLogs, "dumpLogs", 4096, (void*)true, tskIDLE_PRIORITY, &dltask);
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
    char* curLogLine = logBuff+logBufPos;
    xSemaphoreTake(logMutex,portMAX_DELAY);
    logBufPos+=vsprintf(logBuff+logBufPos,fmt,args);
    for (int idx=0; idx < 5; idx++){
        if (callbacks[idx] != NULL) {
            if (!callbacks[idx](params[idx],curLogLine)){
                callbacks[idx] = nullptr;
            }
        }
    }
    if (logBufPos >= LOG_BUF_ULIMIT) {
        dumpLogs();
    }
    xSemaphoreGive(logMutex);
    return vprintf(fmt, args);
}

void initLog() {
    logBuff = (char*)dmalloc(LOG_BUF_SIZE);
    logfname = (char*)dmalloc(LOG_FNAME_LEN);
    memset(logBuff,0,LOG_BUF_SIZE);
    memset(logfname,0,LOG_FNAME_LEN);
    logBufPos=0;
    esp_log_set_vprintf(loggit);
}
static EventGroupHandle_t app_eg = xEventGroupCreate();

EventGroupHandle_t getAppEG() {
  return app_eg;
}
