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
static BufferedFile* logFile = nullptr;
static char* logBuf = nullptr;
static TaskHandle_t dltask = nullptr;
static EventGroupHandle_t app_eg = xEventGroupCreate();
const char* EMPTY_STRING = "";
static bool logInitializing = false;
static bool logInitialized = false;

const char* getLogFName(){
    if (logFile) {
        return logFile->GetFilename();
    }
    return EMPTY_STRING;
}

void dumpTheLogs(void* params){
    if (dltask == nullptr) {
        dltask = (TaskHandle_t)1; //Just not null
    }

    if (logFile)
        logFile->Flush();

    dltask = nullptr;
}

void dumpLogs(){
    if (xEventGroupGetBits(getAppEG())&app_bits_t::TRIPS_SYNCING) {
        ESP_LOGI(__FUNCTION__,"Not flushing whilst dumping");
        return;
    }

    if (dltask == nullptr) {
        CreateWokeBackgroundTask(dumpTheLogs, "dumpLogs", 8192, (void*)true, tskIDLE_PRIORITY+10, &dltask);
    }
}

void registerLogCallback( LogFunction_t callback, void* param) {
    for (int idx=0; idx < 5; idx++){
        if (callbacks[idx] == nullptr) {
            callbacks[idx]=callback;
            params[idx]=param;
            break;
        }
    }
}

void unregisterLogCallback( LogFunction_t callback) {
    for (int idx=0; idx < 5; idx++){
        if (callbacks[idx] == callback) {
            callbacks[idx]=nullptr;
            break;
        }
    }
}

int loggit(const char *fmt, va_list args) {
    if (*fmt != 'V' && (((logFile == nullptr) && !logInitializing) ||
                        ((logFile != nullptr) && logInitialized))) {
        xSemaphoreTake(logMutex,portMAX_DELAY);
        if (!logBuf) {
            logBuf = (char*)dmalloc(LOG_LN_SIZE);
        }

        uint32_t lineLen=vsprintf(logBuf,fmt,args);

        if (((*fmt == 'E') || (*fmt == 'W') || AppConfig::HasSDCard()) && ((logFile==nullptr) && !logInitializing)) {
            logInitializing = true;
            struct tm timeinfo = { 0 };
            time_t now;
            time(&now);
            localtime_r(&now, &timeinfo);

            auto* lpath=(char*)dmalloc(255);
            auto* logfname=(char*)dmalloc(355);
            sprintf(lpath,"/sdcard/logs/%s/%%Y/%%m/%%d/%%H-%%M-%%S.log",AppConfig::GetAppConfig()->GetStringProperty("devName"));
            localtime_r(&now, &timeinfo);
            strftime(logfname, 254, lpath, &timeinfo);
            printf("\nlogfname:%s\n",logfname);
            logFile = new BufferedFile(logfname);
            ldfree(lpath);
            ldfree(logfname);
            logInitializing = false;
            logInitialized = true;
        }
        if (logFile && logInitialized) {
            logFile->Write((uint8_t*)logBuf,lineLen);
        }
        for (int idx=0; idx < 5; idx++){
            if ((callbacks[idx] != nullptr) && (!callbacks[idx](params[idx],logBuf))){
                callbacks[idx] = nullptr;
            }
        }
        xSemaphoreGive(logMutex);
    }
    return vprintf(fmt, args);
}

void initLog() {
    esp_log_set_vprintf(loggit);
}

EventGroupHandle_t getAppEG() {
  return app_eg;
}
