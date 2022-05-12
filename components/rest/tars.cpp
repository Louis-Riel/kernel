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

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG


typedef struct
{
    uint8_t memBuf[JSON_BUFFER_SIZE*2];
    uint8_t *tarBuf;
    uint8_t *sendBuf;
    uint32_t bufLen;
    uint32_t sendLen;
    httpd_req_t *req;
    uint32_t len;
    EventGroupHandle_t eventGroup;
} sendTarParams;

//sendTarParams sp;

esp_err_t tarString(mtar_t *tar, const char *path, const char *data)
{
    mtar_write_file_header(tar, path, strlen(data));
    return mtar_write_data(tar, data, strlen(data));
}

esp_err_t tarFiles(mtar_t *tar, const char *path, const char *ext, bool recursive, const char *excludeList, uint32_t maxSize, bool removeSrc, char* filesToDelete)
{
    if ((path == NULL) || (strlen(path) == 0))
    {
        return ESP_FAIL;
    }
    if (!initSDCard())
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
    uint32_t totlen = 0;
    uint32_t tarStat = MTAR_ESUCCESS;

    char *theFolders = (char *)dmalloc(1024);
    char *theFName = (char *)dmalloc(300);
    char *buf = (char *)dmalloc(sizeof(char) * F_BUF_SIZE);
    char *kmlFileName = (char *)dmalloc(300);

    memset(kmlFileName, 0, 300);
    memset(theFolders, 0, 1024);
    memset(theFName, 0, 300);
    EventGroupHandle_t eventGroup = TheRest::GetEventGroup();

    uint32_t fdpos = filesToDelete == NULL ? 0 : strlen(filesToDelete);
    ESP_LOGI(__FUNCTION__, "Parsing %s", path);
    struct timeval tv_start, tv_end, tv_open, tv_stat, tv_rstart, tv_rend, tv_wstart, tv_wend;
    bool folderAdded=false;

    if ((theFolder = openDir(path)) != NULL)
    {
        ESP_LOGV(__FUNCTION__, "Reading folder %s", path);
        while (!(xEventGroupGetBits(eventGroup) & TAR_SEND_DONE) && (tarStat == MTAR_ESUCCESS) && ((fi = readDir(theFolder)) != NULL))
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
                        continue;
                    }
                    gettimeofday(&tv_start, NULL);
                    if ((theFile = fOpen(theFName, "r")) &&
                        (gettimeofday(&tv_open, NULL) == 0))
                    {
                        uint32_t startPos = tar->pos;
                        fcnt++;
                        if (!folderAdded) {
                            sprintf(kmlFileName, "%s/", path+1);
                            tarStat = mtar_write_dir_header(tar, kmlFileName);
                            folderAdded=true;
                        }
                        ESP_LOGV(__FUNCTION__, "Adding %s %s/%s", fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name);
                        tarStat = mtar_write_file_header(tar, theFName + 1, fileStat.st_size);
                        ESP_LOGV(__FUNCTION__, "stat %s: %d files. file %s, ram %d len: %li", path, fcnt, fi->d_name, heap_caps_get_free_size(MALLOC_CAP_DEFAULT), fileStat.st_size);
                        while (!(xEventGroupGetBits(eventGroup) & TAR_SEND_DONE) && (tarStat == MTAR_ESUCCESS) && !feof(theFile))
                        {
                            gettimeofday(&tv_rstart, NULL);
                            if ((len = fRead(buf, 1, F_BUF_SIZE, theFile)) > 0)
                            {
                                totlen+=len;
                                gettimeofday(&tv_rend, NULL);
                                ESP_LOGV(__FUNCTION__, "%d read", len);

                                tarStat = mtar_write_data(tar, buf, len);
                                gettimeofday(&tv_wend, NULL);
                            }
                            else
                            {
                                ESP_LOGW(__FUNCTION__, "Error reading %s totlen:%d len:%d.", theFName, totlen, len);
                            }
                        }
                        gettimeofday(&tv_end, NULL);
                        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
                        {
                            int64_t start_time_ms = (int64_t)tv_start.tv_sec * 1000L + ((int64_t)tv_start.tv_usec / 1000);
                            int64_t end_time_ms = (int64_t)tv_end.tv_sec * 1000L + ((int64_t)tv_end.tv_usec / 1000);
                            int64_t oend_time_ms = (int64_t)tv_open.tv_sec * 1000L + ((int64_t)tv_open.tv_usec / 1000);
                            int64_t rstart_time_ms = (int64_t)tv_rstart.tv_sec * 1000L + ((int64_t)tv_rstart.tv_usec / 1000);
                            int64_t rend_time_ms = (int64_t)tv_rend.tv_sec * 1000L + ((int64_t)tv_rend.tv_usec / 1000);
                            int64_t wend_time_ms = (int64_t)tv_wend.tv_sec * 1000L + ((int64_t)tv_wend.tv_usec / 1000);
                            ESP_LOGV(__FUNCTION__, "%s: Total Time: %f,Open Time: %f,Read Time: %f, Len: %d, Rate %f/s ram %d, ",
                                        path,
                                        (end_time_ms - start_time_ms) / 1000.0,
                                        (oend_time_ms - start_time_ms) / 1000.0,
                                        (rend_time_ms - rstart_time_ms) / 1000.0,
                                        tar->pos - startPos,
                                        (tar->pos - startPos) / ((end_time_ms - start_time_ms) / 1000.0),
                                        heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
                        }
                        fClose(theFile);
                        ESP_LOGV(__FUNCTION__, "Closing %s", theFName);
                        if ((tarStat == MTAR_ESUCCESS) && 
                            !(xEventGroupGetBits(eventGroup) & TAR_SEND_DONE) &&
                            removeSrc && 
                            (BufferedFile::GetOpenedFile(theFName) == NULL) &&
                            !endsWith(theFName, ".json") && 
                            !endsWith(theFName, ".md5") &&
                            !endsWith(theFName,getLogFName())){
                            if ((strlen(theFName)+strlen(filesToDelete)) > JSON_BUFFER_SIZE){
                                ESP_LOGW(__FUNCTION__,"%s cannot be added to the deleted files this go round", theFName);
                            } else {
                                ESP_LOGI(__FUNCTION__,"Flagging %s for deletion",theFName);
                                fdpos+=sprintf(filesToDelete+fdpos,"%s,",theFName);
                                ESP_LOGV(__FUNCTION__,"Files to delete(%d):%s",fdpos, filesToDelete);
                            }
                        } else {
                            ESP_LOGV(__FUNCTION__,"Not deleting %s",theFName);
                        }
                        ESP_LOGI(__FUNCTION__, "Added %d bytes from %s %s/%s", len, fi->d_type == DT_DIR ? "folder" : "file", path, fi->d_name);
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
                if ((ret = tarFiles(tar, theFolders + ctpos, ext, recursive, excludeList, maxSize, removeSrc, filesToDelete)) != ESP_OK)
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
    deinitSDCard();
    ESP_LOGV(__FUNCTION__,"Done reading %s",path);
    return ret;
}

int tarRead(mtar_t *tar, void *data, unsigned size)
{
    ESP_LOGE(__FUNCTION__, "Cannot read");
    return ESP_FAIL;
}

int tarCount(mtar_t *tar, const void *data, unsigned size)
{
    sendTarParams* sp = (sendTarParams*)tar->stream;
    if (size == 0)
    {
        ESP_LOGW(__FUNCTION__, "empty set");
        return ESP_OK;
    }
    sp->len+=size;
    return ESP_OK;
}

int tarWrite(mtar_t *tar, const void *data, unsigned size)
{
    sendTarParams* sp = (sendTarParams*)tar->stream;
    if (size == 0)
    {
        ESP_LOGW(__FUNCTION__, "empty set");
        return ESP_OK;
    }

    if (sp->bufLen + size >= JSON_BUFFER_SIZE)
    {
        if (xEventGroupGetBits(sp->eventGroup) & TAR_SEND_DONE)
        {
            ESP_LOGE(__FUNCTION__, "SendeR Died");
            return ESP_FAIL;
        }

        ESP_LOGV(__FUNCTION__, "Preparing chunck of %d", sp->bufLen);

        if (sp->sendBuf != NULL){
            ESP_LOGV(__FUNCTION__, "Waiting for TAR_BUFFER_SENT");
            xEventGroupWaitBits(sp->eventGroup, TAR_BUFFER_SENT, pdTRUE, pdTRUE, portMAX_DELAY);
        }

        sp->sendBuf = sp->tarBuf;
        sp->sendLen = sp->bufLen;

        xEventGroupSetBits(sp->eventGroup, TAR_BUFFER_FILLED);

        sp->tarBuf = sp->tarBuf == sp->memBuf ? &sp->memBuf[JSON_BUFFER_SIZE] : sp->memBuf;
        sp->bufLen = 0;
    }
    //ESP_LOGV(__FUNCTION__, "chunck: %d buflen:%d tot:%d", size, sp->bufLen, sp->len);
    memcpy(sp->tarBuf + sp->bufLen, data, size);
    sp->bufLen += size;
    sp->len += size;
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
    sendTarParams* sp = (sendTarParams*)tar->stream;

    if (sp->sendBuf && (xEventGroupGetBits(sp->eventGroup) & TAR_SEND_DONE) && (sp->bufLen > 0)) {
        ESP_LOGE(__FUNCTION__, "Closing Tar, Cannot send final chunck of %d as sender died", sp->bufLen);
        xEventGroupSetBits(sp->eventGroup, TAR_BUILD_DONE);
        return ESP_FAIL;
    }

    if (sp->bufLen > 0){
        ESP_LOGV(__FUNCTION__, "Closing Tar, waiting for final chunck of %d", sp->bufLen);

        if (sp->sendBuf)
            xEventGroupWaitBits(sp->eventGroup, TAR_BUFFER_SENT, pdFALSE, pdFALSE, portMAX_DELAY);

        ESP_LOGV(__FUNCTION__, "Closing Tar, sending final chunck of %d", sp->bufLen);
        sp->sendBuf = sp->tarBuf;
        sp->sendLen = sp->bufLen;
        xEventGroupSetBits(sp->eventGroup, TAR_BUFFER_FILLED);
    }

    ESP_LOGI(__FUNCTION__, "Wrote %d bytes", sp->len);
    xEventGroupSetBits(sp->eventGroup, TAR_BUILD_DONE);

    return ESP_OK;
}

void DeleteTarFiles(void* param){
    char* filesToDelete = (char*) param;
    ESP_LOGV(__FUNCTION__,"Deleting %s",filesToDelete);

    struct tm timeinfo;
    time_t now = 0;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[65]; 
    strftime(strftime_buf, 64, "%Y-%m-%d", &timeinfo);

    char* nextFile = lastIndexOf(filesToDelete,",");
    if (nextFile){
        *nextFile=0; //Remove trailng ,
        nextFile = lastIndexOf(filesToDelete,",");
        ESP_LOGV(__FUNCTION__,"Files to delete:%s",filesToDelete);
        if (nextFile == NULL) {
            nextFile = filesToDelete; //Assume there was only one file
        } else {
            nextFile++;
        }
    }
    while (nextFile) {
        ESP_LOGV(__FUNCTION__,"nextFile:%s",filesToDelete);
        if (!indexOf(nextFile,strftime_buf)){
            deleteFile(nextFile);
        } else {
            ESP_LOGI(__FUNCTION__,"Not deleting %s",nextFile);
        }
        if (nextFile == filesToDelete) {
            ESP_LOGV(__FUNCTION__,"Done deleting");
            break;
        } else {
            *(nextFile-1)=0; // remove comma of processed file
            ESP_LOGV(__FUNCTION__,"FTD:%s",filesToDelete);
            nextFile = lastIndexOf(filesToDelete,",");
            if (nextFile == NULL) {
                nextFile = filesToDelete; // last file to delete
            } else {
                nextFile++;
            }
        }
    }
    ldfree(filesToDelete);
}

void BuildTar(void* param){
    mtar_t* tar = (mtar_t*)param;
    char* filesToDelete = (char*)dmalloc(JSON_BUFFER_SIZE);
    memset(filesToDelete,0,JSON_BUFFER_SIZE);
    tarFiles(tar,"/lfs",NULL,true,"current.bin",1024000,true,filesToDelete);
    if (mtar_close(tar) == MTAR_ESUCCESS){
        ESP_LOGI(__FUNCTION__,"Deleting %s",filesToDelete);
        CreateWokeBackgroundTask(DeleteTarFiles, "DeleteTarFiles", 4096, filesToDelete, tskIDLE_PRIORITY, NULL);
    } else {
        ldfree(filesToDelete);
        ESP_LOGE(__FUNCTION__,"Failed to close tar");
    }
}

void MeasureTar(void* param){
    mtar_t tar;
    memset(&tar, 0, sizeof(tar));
    tar.read = tarRead;
    tar.close = tarCountClose;
    tar.seek = tarSeek;
    tar.write = tarCount;
    tarFiles(&tar,"/lfs",NULL,true,"current.bin",1024000,false,NULL);
    mtar_close(&tar);
}

void TheRest::SendTar(void* param)
{
    BufferedFile::FlushAll();

    TheRest* rest = GetServer();

    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t *config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    char* url = (char*)dmalloc(255);

    sprintf(url, "http://%s/sdcard/tars/%s/", rest->gwAddr, rest->ipAddr);

    struct tm timeinfo;
    time_t now = 0;
    esp_err_t err = 0;
    uint32_t hlen=0;
    uint32_t sentLen=0;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(url+strlen(url), 255-strlen(url), "%Y/%m/%d/%H-%M-%S.tar", &timeinfo);

    config->method = esp_http_client_method_t::HTTP_METHOD_PUT;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;
    config->url = url;

    uint32_t len=0;

    sendTarParams* sp = (sendTarParams*)dmalloc(sizeof(sendTarParams));
    memset(sp,0,sizeof(sendTarParams));
    sp->eventGroup = TheRest::GetEventGroup();
    
    xEventGroupSetBits(sp->eventGroup,app_bits_t::TRIPS_SYNCING);

    sp->sendBuf = NULL;
    sp->tarBuf = sp->memBuf;
    sp->bufLen = 0;
    sp->len = 0;
    xEventGroupClearBits(sp->eventGroup, TAR_BUFFER_FILLED);
    xEventGroupClearBits(sp->eventGroup, TAR_BUFFER_SENT);
    xEventGroupClearBits(sp->eventGroup, TAR_BUILD_DONE);
    xEventGroupClearBits(sp->eventGroup, TAR_SEND_DONE);
    uint32_t totLen = sp->len;
    sp->bufLen = 0;
    sp->len = 0;

    mtar_t tar;
    tar.read = tarRead;
    tar.close = tarClose;
    tar.seek = tarSeek;
    tar.write = tarWrite;
    tar.stream = sp;

    CreateBackgroundTask(BuildTar,"BuildTar",8192, &tar, tskIDLE_PRIORITY,NULL);
    
    ESP_LOGV(__FUNCTION__, "Connecting to %s", url);
    if ((client = esp_http_client_init(config)) &&
        ((err = esp_http_client_set_header(client, "Content-Type","application/x-tar")) == ESP_OK) &&
        ((err = esp_http_client_open(client, 20000000) == ESP_OK))) {
        
        ESP_LOGI(__FUNCTION__, "Sending to %s", url);
        EventBits_t theBits = 0;

        while ((theBits=xEventGroupWaitBits(sp->eventGroup, TAR_BUFFER_FILLED, pdTRUE, pdTRUE, portMAX_DELAY)))
        {
            if (sp->sendLen > 0)
            {
                //printf(".");
                ESP_LOGV(__FUNCTION__, "Sending chunk of %d", sp->sendLen);
                len += sp->sendLen;
                sentLen=0;
                while((sentLen = esp_http_client_write(client,(const char *)sp->sendBuf, sp->sendLen)) != -1)
                {
                    if (sentLen > 0) {
                        sp->sendBuf+=sentLen;
                        sp->sendLen-=sentLen;
                        if (sp->sendLen <= 0 ) {
                            ESP_LOGV(__FUNCTION__, "Sent chunk of %d", sp->sendLen);
                            break;
                        }
                    } else {
                        ESP_LOGW(__FUNCTION__,"Failed sending %d",sp->sendLen);
                        break;
                    }
                }
                if (sentLen <= 0) {
                    ESP_LOGE(__FUNCTION__, "Client disconnected:%d",sentLen);
                    break;
                }

                if (sentLen == sp->sendLen)
                {
                }
                xEventGroupSetBits(sp->eventGroup, TAR_BUFFER_SENT);
            } else {
                ESP_LOGW(__FUNCTION__,"Got an empty chunck");
            }
            if (xEventGroupGetBits(sp->eventGroup) & TAR_BUILD_DONE)
            {
                break;
            }
        }
        char buf[20];
        esp_http_client_read(client,buf,20);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        xEventGroupSetBits(sp->eventGroup, TAR_SEND_DONE);
        ESP_LOGI(__FUNCTION__, "Sent %d to %s", len, url);
    } else {
        ESP_LOGE(__FUNCTION__,"Error whilst sending tar:%s hlen:%d",esp_err_to_name(err), hlen);
    }
    ESP_LOGI(__FUNCTION__, "Done sending to %s", url);
    xEventGroupClearBits(getAppEG(),app_bits_t::TRIPS_SYNCING);
    ldfree((void*)config->url);
    ldfree(config);
    ldfree(sp);
}

