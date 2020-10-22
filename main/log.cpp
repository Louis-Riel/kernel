#include "utils.h"
#include "esp_log.h"
#include "time.h"
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

char* logBuff = NULL;
char* logfname = NULL;
uint32_t logBufPos;

char* getLogFName(){
    return logfname;
}

void dumpLogs(){
    if (initSPISDCard()) {
        FILE* fw = NULL;

        if (strlen(logfname) == 0){
            struct tm timeinfo;
            time_t now = ::time(NULL);

            localtime_r(&now, &timeinfo);
            if (getAppConfig()->purpose == app_config_t::purpose_t::TRACKER)
                strftime(logfname, 100, "/sdcard/logs/TRACKER-%Y-%m-%d_%H-%M-%S.log", &timeinfo);
            else
                strftime(logfname, 100, "/sdcard/logs/PULLER-%Y-%m-%d_%H-%M-%S.log", &timeinfo);
        }

        if ((fw = fopen(logfname,"a",true)) != NULL) {
            if (fwrite((void*)logBuff,1,logBufPos,fw) == logBufPos) {
                if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG){
                    fprintf(stdout,"Written %d into %s",logBufPos,logfname);
                }
                *logBuff=0;
                logBufPos=0;
            } else if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG) {
                fprintf(stderr,"Logs not written to %s",logfname);
            }
            fclose(fw);
        } else if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG) {
            fprintf(stderr,"Failed to open %s",logfname);
        }
        deinitSPISDCard();
    }
}

int loggit(const char *fmt, va_list args) {
    static bool static_fatal_error = false;
    static const uint32_t WRITE_CACHE_CYCLE = 5;
    static uint32_t counter_write = 0;
    int iresult;
    logBufPos+=vsprintf(logBuff+logBufPos,fmt,args);
    if (logBufPos >= LOG_BUF_ULIMIT) {
        dumpLogs();
    }

    return vprintf(fmt, args);
}

void initLog() {
    logBuff = (char*)malloc(LOG_BUF_SIZE);
    logfname = (char*)malloc(LOG_FNAME_LEN);
    memset(logBuff,0,LOG_BUF_SIZE);
    memset(logfname,0,LOG_FNAME_LEN);
    logBufPos=0;
    esp_log_set_vprintf(loggit);
}