#include "logs.h"
#include "utils.h"
#include "esp_log.h"
#include "time.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

char* logBuff = NULL;
char* logfname = NULL;
uint32_t logBufPos;
static LogFunction_t callbacks[5];
static void* params[5];

char* getLogFName(){
    return logfname;
}

void dumpLogs(){
    if (initSPISDCard(false)) {
        FILE* fw = NULL;

        struct tm timeinfo;
        time_t now = ::time(NULL);

        localtime_r(&now, &timeinfo);
        
        if ((strlen(logfname) == 0) || (indexOf(logfname,"1970") && (timeinfo.tm_year > 1970))){
            char lpath[255];
            sprintf(lpath,"%s/logs/%s-%%Y-%%m-%%d_%%H-%%M-%%S.log",AppConfig::GetActiveStorage(),GetAppConfig()->IsAp()?"TRACKER":"PULLER");
            strftime(logfname, 254, lpath, &timeinfo);
        }

        if ((fw = fopen(logfname,"a",true,false)) != NULL) {
            if (fwrite((void*)logBuff,1,logBufPos,fw) == logBufPos) {
                if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG){
                    fprintf(stdout,"Written %d into %s",logBufPos,logfname);
                }
                *logBuff=0;
                logBufPos=0;
            } else if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG) {
                fprintf(stderr,"Logs not written to %s",logfname);
            }
            fClose(fw);
        } else if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG) {
            fprintf(stderr,"Failed to open %s",logfname);
        }
        deinitSPISDCard(false);
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

int loggit(const char *fmt, va_list args) {
    char* curLogLine = logBuff+logBufPos;
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

EventGroupHandle_t* getAppEG() {
  return &app_eg;
}
