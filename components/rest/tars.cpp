#include "rest.h"
#include "route.h"
#include "math.h"
#include "time.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "../esp_littlefs/include/esp_littlefs.h"
#include "rom/ets_sys.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/adc.h"
#include "mfile.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE


typedef struct
{
    uint8_t *tarBuf;
    uint8_t *sendBuf;
    uint32_t bufLen;
    uint32_t sendLen;
    httpd_req_t *req;
    uint32_t len;
} sendTarParams;

sendTarParams sp;

esp_err_t tarString(mtar_t *tar, const char *path, const char *data)
{
    mtar_write_file_header(tar, path, strlen(data));
    return mtar_write_data(tar, data, strlen(data));
}

esp_err_t tarFiles(mtar_t *tar, char *path, const char *ext, bool recursive, const char *excludeList, uint32_t maxSize, bool removeSrc)
{
    if ((path == NULL) || (strlen(path) == 0))
    {
        return ESP_FAIL;
    }
    if (!initSPISDCard())
    {
        return ESP_FAIL;
    }

    DIR *theFolder;
    FILE *theFile;
    struct dirent *fi;
    struct stat fileStat;

    esp_err_t ret = ESP_OK;
    uint32_t fpos = 0;
    uint32_t fcnt = 0;
    uint32_t dcnt = 0;
    uint32_t len = 0;
    uint32_t tarStat = MTAR_ESUCCESS;

    char *theFolders = (char *)dmalloc(1024);
    char *theFName = (char *)dmalloc(300);
    char *buf = (char *)dmalloc(sizeof(char) * F_BUF_SIZE);
    char *kmlFileName = (char *)dmalloc(300);

    memset(kmlFileName, 0, 300);
    memset(theFolders, 0, 1024);
    memset(theFName, 0, 300);

    ESP_LOGV(__FUNCTION__, "Parsing %s", path);
    struct timeval tv_start, tv_end, tv_open, tv_stat, tv_rstart, tv_rend, tv_wstart, tv_wend;

    if ((theFolder = openDir(path)) != NULL)
    {
        sprintf(kmlFileName, "%s/", path+1);
        tarStat = mtar_write_dir_header(tar, kmlFileName);
        ESP_LOGV(__FUNCTION__, "Reading folder %s", path);
        while ((tarStat == MTAR_ESUCCESS) && ((fi = readDir(theFolder)) != NULL))
        {
            ESP_LOGV(__FUNCTION__, "At %s %s/%s", fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name);
            if (strlen(fi->d_name) == 0)
            {
                break;
            }
            if (fi->d_type == DT_DIR)
            {
                if (recursive)
                {
                    ESP_LOGV(__FUNCTION__, "Parsing %s %s/%s", fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name);
                    dcnt++;
                    sprintf(kmlFileName, "%s/%s", path, fi->d_name);
                    sprintf(theFolders + fpos, "%s", kmlFileName);
                    fpos += strlen(kmlFileName) + 1;
                    ESP_LOGV(__FUNCTION__, "%s currently has %d files and %d folders subfolder len:%d. Adding dir (%s)%d", path, fcnt, dcnt, fpos, kmlFileName, strlen(kmlFileName));
                }
            }
            else if ((ext == NULL) || (strlen(ext) == 0) || endsWith(fi->d_name, ext))
            {
                ESP_LOGV(__FUNCTION__, "Parsing %s %s/%s", fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name);
                sprintf(theFName, "%s/%s", path, fi->d_name);
                if ((excludeList == NULL) || (indexOf(excludeList, fi->d_name) == NULL))
                {
                    stat(theFName, &fileStat);
                    if (fileStat.st_size == 0) {
                        ESP_LOGW(__FUNCTION__,"Skipping empty file %s",theFName);
                        unlink(theFName);
                        continue;
                    }
                    gettimeofday(&tv_start, NULL);
                    if ((theFile = fOpen(theFName, "r")) &&
                        (gettimeofday(&tv_open, NULL) == 0))
                    {
                        uint32_t startPos = tar->pos;
                        fcnt++;
                        bool headerWritten = false;
                        bool allDone = false;
                        ESP_LOGV(__FUNCTION__, "Reading %s %s/%s", fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name);
                        while ((tarStat == MTAR_ESUCCESS) && !allDone)
                        {
                            gettimeofday(&tv_rstart, NULL);
                            if ((len = fRead(buf, 1, F_BUF_SIZE, theFile)) > 0)
                            {
                                gettimeofday(&tv_rend, NULL);
                                ESP_LOGV(__FUNCTION__, "%d read", len);

                                if (!headerWritten)
                                {
                                    headerWritten = true;
                                    if (len == F_BUF_SIZE)
                                    {
                                        fstat(fileno(theFile), &fileStat);
                                        gettimeofday(&tv_stat, NULL);
                                        tarStat = mtar_write_file_header(tar, theFName + 1, fileStat.st_size);
                                        ESP_LOGV(__FUNCTION__, "stat %s: %d files. file %s, ram %d len: %li", path, fcnt, fi->d_name, heap_caps_get_free_size(MALLOC_CAP_DEFAULT), fileStat.st_size);
                                    }
                                    else
                                    {
                                        tv_stat = tv_end;
                                        tarStat = mtar_write_file_header(tar, theFName + 1, len);
                                        ESP_LOGV(__FUNCTION__, "full %s: %d files. file %s, ram %d len: %d", path, fcnt, fi->d_name, heap_caps_get_free_size(MALLOC_CAP_DEFAULT), len);
                                    }
                                    gettimeofday(&tv_wstart, NULL);
                                }

                                tarStat = mtar_write_data(tar, buf, len);
                                gettimeofday(&tv_wend, NULL);
                                allDone = feof(theFile);
                                if (allDone)
                                {
                                    fClose(theFile);
                                    ESP_LOGV(__FUNCTION__, "Closing %s", theFName);
                                    if (removeSrc && !endsWith(theFName, ".json") && !endsWith(theFName, ".md5")){
                                        deleteFile(theFName);
                                        ESP_LOGV(__FUNCTION__, "Deleted %s", theFName);
                                    }
                                }
                            }
                            else
                            {

                                fClose(theFile);
                                allDone = true;
                                ESP_LOGV(__FUNCTION__, "Closing %s.", theFName);
                            }
                            gettimeofday(&tv_end, NULL);
                            if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
                            {
                                int64_t start_time_ms = (int64_t)tv_start.tv_sec * 1000L + ((int64_t)tv_start.tv_usec / 1000);
                                int64_t end_time_ms = (int64_t)tv_end.tv_sec * 1000L + ((int64_t)tv_end.tv_usec / 1000);
                                int64_t oend_time_ms = (int64_t)tv_open.tv_sec * 1000L + ((int64_t)tv_open.tv_usec / 1000);
                                int64_t ostat_time_ms = (int64_t)tv_stat.tv_sec * 1000L + ((int64_t)tv_stat.tv_usec / 1000);
                                int64_t rstart_time_ms = (int64_t)tv_rstart.tv_sec * 1000L + ((int64_t)tv_rstart.tv_usec / 1000);
                                int64_t rend_time_ms = (int64_t)tv_rend.tv_sec * 1000L + ((int64_t)tv_rend.tv_usec / 1000);
                                int64_t wstart_time_ms = (int64_t)tv_wstart.tv_sec * 1000L + ((int64_t)tv_wstart.tv_usec / 1000);
                                int64_t wend_time_ms = (int64_t)tv_wend.tv_sec * 1000L + ((int64_t)tv_wend.tv_usec / 1000);
                                ESP_LOGV(__FUNCTION__, "%s: Total Time: %f,Open Time: %f,Stat Time: %f,Read Time: %f,Write Time: %f, Len: %d, Rate %f/s ram %d, ",
                                         path,
                                         (end_time_ms - start_time_ms) / 1000.0,
                                         (oend_time_ms - start_time_ms) / 1000.0,
                                         (ostat_time_ms - oend_time_ms) / 1000.0,
                                         (rend_time_ms - rstart_time_ms) / 1000.0,
                                         (wend_time_ms - wstart_time_ms) / 1000.0,
                                         tar->pos - startPos,
                                         (tar->pos - startPos) / ((end_time_ms - start_time_ms) / 1000.0),
                                         heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
                            }
                        }
                        ESP_LOGV(__FUNCTION__, "Read %d bytes from %s %s/%s", len, fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name);
                    }
                    else
                    {
                        ESP_LOGE(__FUNCTION__, "Cannot read %s", theFName);
                        ret = ESP_FAIL;
                        break;
                    }
                }
                else
                {
                    ESP_LOGV(__FUNCTION__, "Skipping %s %s/%s excludeList:%s Match:%s", fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name, excludeList, indexOf(excludeList, fi->d_name));
                }
            }
            else
            {
                ESP_LOGV(__FUNCTION__, "Skipping %s %s", fi->d_type == DT_DIR ? "folder" : "file", fi->d_name);
            }
            if (tar->pos > maxSize)
            {
                //break;
            }
        }
        closeDir(theFolder);
        if (tarStat == MTAR_ESUCCESS)
        {
            ESP_LOGV(__FUNCTION__, "%s had %d files and %d folders subfolder len:%d", path, fcnt, dcnt, fpos);
            uint32_t ctpos = 0;
            while ((tarStat == MTAR_ESUCCESS)&&(dcnt-- > 0))
            {
                ESP_LOGV(__FUNCTION__, "%d-%s: Getting sub-folder(%d) %s", dcnt, path, ctpos, theFolders + ctpos);
                if ((ret = tarFiles(tar, theFolders + ctpos, ext, recursive, excludeList, maxSize, removeSrc)) != ESP_OK)
                {
                    ESP_LOGW(__FUNCTION__, "Error invoking getSdFiles for %s", kmlFileName);
                }
                ctpos += strlen(theFolders + ctpos) + 1;
            }
        } 
        else 
        {
            ESP_LOGW(__FUNCTION__, "Tar error opening %s:%d", path, tarStat);
            ret = ESP_FAIL;
        }
    } 
    else
    {
        ESP_LOGW(__FUNCTION__, "Error opening %s:%s", path, esp_err_to_name(errno));
        ret = ESP_FAIL;
    }

    ldfree(theFName);
    ldfree(theFolders);
    ldfree(buf);
    ldfree(kmlFileName);
    deinitSPISDCard();
    return ret;
}

int tarRead(mtar_t *tar, void *data, unsigned size)
{
    ESP_LOGE(__FUNCTION__, "Cannot read");
    return ESP_FAIL;
}

int tarCount(mtar_t *tar, const void *data, unsigned size)
{
    if (size == 0)
    {
        ESP_LOGW(__FUNCTION__, "empty set");
        return ESP_OK;
    }
    sp.len+=size;
    return ESP_OK;
}

int tarWrite(mtar_t *tar, const void *data, unsigned size)
{
    if (size == 0)
    {
        ESP_LOGW(__FUNCTION__, "empty set");
        return ESP_OK;
    }

    if (sp.bufLen + size >= HTTP_CHUNK_SIZE)
    {
        EventGroupHandle_t eventGroup = TheRest::GetEventGroup();
        if (xEventGroupGetBits(eventGroup) & TAR_SEND_DONE)
        {
            ESP_LOGE(__FUNCTION__, "SendeR Died");
            return ESP_FAIL;
        }

        ESP_LOGV(__FUNCTION__, "Preparing chunck of %d", sp.bufLen);
        sp.sendLen = sp.bufLen;
        if (sp.sendBuf == NULL)
        {
            sp.sendBuf = (uint8_t *)dmalloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf, (const void *)sp.tarBuf, sp.bufLen);
        sp.bufLen = 0;
        xEventGroupSetBits(eventGroup, TAR_BUFFER_FILLED);
        ESP_LOGV(__FUNCTION__, "Waiting for TAR_BUFFER_SENT");
        xEventGroupWaitBits(eventGroup, TAR_BUFFER_SENT, pdTRUE, pdTRUE, portMAX_DELAY);
    }
    if (size > 1)
    {
        ESP_LOGV(__FUNCTION__, "chunck: %d buflen:%d tot:%d", size, sp.bufLen, sp.len);
        memcpy(sp.tarBuf + sp.bufLen, data, size);
        sp.bufLen += size;
        sp.len += size;
    }
    else
    {
        *(sp.tarBuf + sp.bufLen++) = *((uint8_t *)data);
        sp.len++;
    }
    return ESP_OK;
}

int tarSeek(mtar_t *tar, unsigned pos)
{
    ESP_LOGE(__FUNCTION__, "Cannot seek");
    return ESP_FAIL;
}

int tarCountClose(mtar_t *tar){
    return ESP_OK;
}

int tarClose(mtar_t *tar)
{
    ESP_LOGV(__FUNCTION__, "Closing Tar, waiting for final chunck");
    EventGroupHandle_t eventGroup = TheRest::GetEventGroup();
    xEventGroupWaitBits(eventGroup, TAR_BUFFER_SENT, pdTRUE, pdTRUE, portMAX_DELAY);
    xEventGroupSetBits(eventGroup, TAR_BUILD_DONE);
    ESP_LOGV(__FUNCTION__, "tar done");
    sp.sendLen = sp.bufLen;
    if (sp.sendLen > 0)
    {
        ESP_LOGV(__FUNCTION__, "Sending final chunck of %d",sp.sendLen);
        if (sp.sendBuf == NULL)
        {
            sp.sendBuf = (uint8_t *)dmalloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf, (const void *)sp.tarBuf, sp.bufLen);
        ESP_LOGV(__FUNCTION__, "Sending final chunk of %d", sp.bufLen);
    }
    xEventGroupSetBits(eventGroup, TAR_BUFFER_FILLED);
    ESP_LOGV(__FUNCTION__, "Closing Tar, waiting for final chunck to be sent");
    xEventGroupWaitBits(eventGroup, TAR_SEND_DONE, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGD(__FUNCTION__, "Wrote %d bytes", sp.len);
    if (sp.sendBuf != NULL)
        ldfree(sp.sendBuf);
    if (sp.tarBuf != NULL)
        ldfree(sp.tarBuf);
    return ESP_OK;
}

void BuildTar(void* param){
    mtar_t tar;
    memset(&tar, 0, sizeof(tar));
    tar.read = tarRead;
    tar.close = tarClose;
    tar.seek = tarSeek;
    tar.write = tarWrite;
    tarFiles(&tar,"/lfs",NULL,true,"current.bin",1024000,true);
    xEventGroupSetBits(TheRest::GetEventGroup(), TAR_BUILD_DONE);
}

void MeasureTar(void* param){
    mtar_t tar;
    memset(&tar, 0, sizeof(tar));
    tar.read = tarRead;
    tar.close = tarCountClose;
    tar.seek = tarSeek;
    tar.write = tarCount;
    tarFiles(&tar,"/lfs",NULL,true,"current.bin",1024000,false);
}

void TheRest::SendTar(void* param)
{
    dumpTheLogs(NULL);
    BufferedFile::FlushAll();
    xEventGroupSetBits(getAppEG(),app_bits_t::TRIPS_SYNCING);
    TheRest* rest = GetServer();

    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t *config = NULL;

    char* url = (char*)dmalloc(255);
    sprintf(url, "http://%s/sdcard/tars/%s/", rest->gwAddr, rest->ipAddr);

    struct tm timeinfo;
    time_t now = 0;
    esp_err_t err = 0;
    uint32_t hlen=0;
    uint32_t retCode=0;
    uint32_t sentLen=0;
    uint32_t postLen=0;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(url+strlen(url), 255-strlen(url), "%Y/%m/%d/%H-%M-%S.tar", &timeinfo);

    config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = url;
    config->method = esp_http_client_method_t::HTTP_METHOD_PUT;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;

    EventGroupHandle_t eventGroup = TheRest::GetEventGroup();
    uint32_t len=0;
    sp.tarBuf = (uint8_t *)dmalloc(HTTP_BUF_SIZE);
    sp.sendBuf = NULL;
    sp.bufLen = 0;
    sp.len = 0;
    xEventGroupClearBits(eventGroup, TAR_BUFFER_FILLED);
    xEventGroupClearBits(eventGroup, TAR_BUFFER_SENT);
    xEventGroupClearBits(eventGroup, TAR_BUILD_DONE);
    //CreateWokeInlineTask(MeasureTar,"MeasureTar",4096, NULL);
    uint32_t totLen = sp.len;
    sp.bufLen = 0;
    sp.len = 0;
    CreateBackgroundTask(BuildTar,"BuildTar",4096, NULL, tskIDLE_PRIORITY,NULL);
    xEventGroupWaitBits(eventGroup, TAR_BUFFER_FILLED, pdFALSE, pdFALSE, portMAX_DELAY);
    
    ESP_LOGD(__FUNCTION__, "Sending %d bytes tar to %s", totLen, url);
    if ((client = esp_http_client_init(config)) &&
        ((err = esp_http_client_set_header(client, "Content-Type","application/x-tar")) == ESP_OK) &&
        ((err = esp_http_client_open(client, 20000000) == ESP_OK))) {
        
        ESP_LOGD(__FUNCTION__, "Connected to %s", url);

        while (xEventGroupWaitBits(eventGroup, TAR_BUFFER_FILLED|TAR_BUILD_DONE, pdFALSE, pdFALSE, portMAX_DELAY))
        {
            if (sp.sendLen > 0)
            {
                //printf(".");
                ESP_LOGV(__FUNCTION__, "Sending chunk of %d", sp.sendLen);
                len += sp.sendLen;
                sentLen = esp_http_client_write(client,(const char *)sp.sendBuf, sp.sendLen);

                if (sentLen == sp.sendLen)
                {
                    ESP_LOGV(__FUNCTION__, "Sent chunk of %d", sp.sendLen);
                    sp.sendLen = 0;
                    xEventGroupClearBits(eventGroup, TAR_BUFFER_FILLED);
                    xEventGroupSetBits(eventGroup, TAR_BUFFER_SENT);
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Chunk len %d won't go, sent: %d", sp.sendLen, sentLen);
                    xEventGroupSetBits(eventGroup, TAR_SEND_DONE);
                    break;
                }
            } else {
                ESP_LOGW(__FUNCTION__,"Got an empty chunck");
            }
            if (xEventGroupGetBits(eventGroup) & TAR_BUILD_DONE)
            {
                break;
            }
        }
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        ldfree(config);
        ESP_LOGD(__FUNCTION__, "Sent %d to %s", len, url);
        ldfree(url);
    } else {
        ESP_LOGE(__FUNCTION__,"Error whilst sending tar:%s hlen:%d",esp_err_to_name(err), hlen);
    }
    xEventGroupClearBits(getAppEG(),app_bits_t::TRIPS_SYNCING);
}

