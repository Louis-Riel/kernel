#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>
#include "math.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

esp_err_t json_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(__FUNCTION__, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        if (xEventGroupGetBits(eventGroup) & GETTING_TRIP_LIST)
        {
            if (evt->data_len < KML_BUFFER_SIZE)
            {
                assert(evt->user_data);
                char *kmlFiles = (char *)evt->user_data;
                ESP_LOGV(__FUNCTION__, "kmllist data len:%d", evt->data_len);
                memcpy(kmlFiles + strlen(kmlFiles), evt->data, evt->data_len);
                ESP_LOGV(__FUNCTION__, "%s", kmlFiles);
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "Cannot fit %d in the ram", evt->data_len);
            }
        }
        else
        {
            ESP_LOGV(__FUNCTION__, "Got data with no dest");
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        if (xEventGroupGetBits(eventGroup) & GETTING_TRIP_LIST)
        {
            xEventGroupClearBits(eventGroup, GETTING_TRIP_LIST);
            xEventGroupSetBits(eventGroup, GETTING_TRIPS);
        }
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}

bool moveFolder(char *folderName, char *toFolderName)
{
    if ((folderName == NULL) || (toFolderName == NULL))
    {
        ESP_LOGE(__FUNCTION__, "Empty params passed: foldername:%s, toFolderName:%s", folderName == NULL ? "null" : "not nukll", toFolderName == NULL ? "null" : "not nukll");
        return false;
    }
    if ((strlen(folderName) == 0) || (strlen(toFolderName) == 0))
    {
        ESP_LOGE(__FUNCTION__, "Empty params passed: foldername len:%d, toFolderName len:%d", strlen(folderName), strlen(toFolderName));
        return false;
    }
    FF_DIR theFolder;
    char *fName = (char *)dmalloc(270);
    char *destfName = (char *)dmalloc(270);
    void *buf = dmalloc(F_BUF_SIZE);
    FILINFO *fi = (FILINFO *)dmalloc(sizeof(FILINFO));
    bool retval = true;

    if (f_opendir(&theFolder, folderName) == FR_OK)
    {
        ESP_LOGD(__FUNCTION__, "reading files in %s", folderName);
        while (f_readdir(&theFolder, fi) == FR_OK)
        {
            if (strlen(fi->fname) == 0)
            {
                break;
            }
            if (!(fi->fattrib & AM_DIR))
            {
                sprintf(fName, "/sdcard%s/%s", folderName, fi->fname);
                sprintf(destfName, "/sdcard%s%s/%s", toFolderName, folderName, fi->fname);
                ESP_LOGD(__FUNCTION__, "Moving %s - %d to %s", fName, fi->fsize, destfName);
                if (moveFile(fName, destfName))
                {
                    ESP_LOGD(__FUNCTION__, "%s deleted", fName);
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Cannot move %s", fName);
                    retval = false;
                    break;
                }
            }
            else
            {
                sprintf(fName, "%s/%s", folderName, fi->fname);
                ESP_LOGD(__FUNCTION__, "Moving sub folder %s of %s as %s", fi->fname, folderName, fName);
                moveFolder(fName, "/sent");
            }
        }
        f_closedir(&theFolder);
        if (f_unlink(folderName) != 0)
        {
            ESP_LOGE(__FUNCTION__, "Cannot delete folder %s", folderName);
            retval = false;
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot read dir %s", folderName);
        retval = false;
    }
    ldfree(fi);
    ldfree(fName);
    ldfree(destfName);
    ldfree(buf);
    return retval;
}

bool CheckOTA(esp_ip4_addr_t *ipInfo)
{
    esp_err_t err = ESP_OK;
    bool retCode = true;
    esp_http_client_config_t *config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = (char *)dmalloc(255);
    sprintf((char *)config->url, "http://" IPSTR "/ota/getmd5", IP2STR(ipInfo));
    config->method = HTTP_METHOD_POST;
    config->timeout_ms = 9000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->timeout_ms = 3000;
    config->port = 80;
    esp_http_client_handle_t client = NULL;
    char dmd5[37];
    esp_err_t ret = ESP_OK;
    ESP_LOGD(__FUNCTION__, "Checking Version");
    int len=-1;
    int hrc = 202;
    if ((client = esp_http_client_init(config)) &&
        ((err = esp_http_client_open(client,0)) == ESP_OK) && 
        ((len=esp_http_client_fetch_headers(client))>=0) &&
        ((hrc=esp_http_client_get_status_code(client)) == 200))
    {
        if ((len = esp_http_client_read(client, &dmd5[0], 33)) >= 0)
        {
            if (len == 0){
                dmd5[0]=0;
            }
            ESP_LOGD(__FUNCTION__, "Got back (%s)%d char of md5", len ? dmd5 : "null", len);
            char ch;
            bool isValid = (len > 0) && (strcmp(dmd5, "BADMD5") != 0);
            if (!isValid && (len > 0))
            {
                for (int idx = 0; idx < len; idx++)
                {
                    ch = *(dmd5 + idx);
                    isValid = (((ch >= 'a') && (ch <= 'z')) ||
                               ((ch >= '0') && (ch <= '9')));
                    if (!isValid)
                    {
                        ESP_LOGD(__FUNCTION__, "Invalid char for %s at pos %d(%c)", dmd5, idx, ch);
                        break;
                    }
                }
            }
            FILE *fw = NULL;
            if ((fw = fopen("/lfs/firmware/current.bin.md5", "r")) != NULL)
            {
                char ccmd5[33];
                if ((len = fread((void *)ccmd5, 1, 32, fw)) >= 0)
                {
                    fClose(fw);
                    ccmd5[32] = 0;
                    dmd5[32] = 0;
                    if (isValid && strcmp(ccmd5, dmd5) == 0)
                    {
                        ESP_LOGD(__FUNCTION__, "No firmware update needed, is up to date");
                    }
                    else
                    {
                        ESP_LOGI(__FUNCTION__, "firmware update needed %d, %s!=%s", len, dmd5[0]?dmd5:"N/A", ccmd5);
                        esp_http_client_cleanup(client);
                        memset(config, 0, sizeof(esp_http_client_config_t));
                        fClose(fw);
                        FILE* fw;

                        struct stat st;

                        ldfree((void *)config->url);

                        if ((fw = fopen("/lfs/firmware/current.bin", "r", true)) != NULL)
                        {
                            stat("/lfs/firmware/current.bin", &st);
                            memset(config, 0, sizeof(esp_http_client_config_t));
                            config->url = (char *)dmalloc(255);
                            uint8_t* img = (uint8_t*)dmalloc(st.st_size);
                            memset((void *)config->url, 0, 255);
                            config->method = HTTP_METHOD_POST;
                            config->timeout_ms = 9000;
                            config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                            config->max_redirection_count = 0;
                            config->port = 80;

                            len=0;
                            while (!feof(fw))
                            {
                                if (((len += fread(img+len, 1, st.st_size, fw)) == 0) || (len >= st.st_size))
                                {
                                    ESP_LOGD(__FUNCTION__,"Read %d bytes from current bin", len);
                                    break;
                                }
                            }
                            fclose(fw);

                            sprintf((char *)config->url, "http://" IPSTR "/ota/flash?md5=%s&len=%d", IP2STR(ipInfo), ccmd5, (int)st.st_size);
                            if ((client = esp_http_client_init(config)) &&
                                ((ret = esp_http_client_open(client, st.st_size)) == ESP_OK)) {
                                ESP_LOGD(__FUNCTION__,"Sending fw of %d/%d bytes",len,(int)st.st_size);
                                if ((len = esp_http_client_write(client, (const char*)img, st.st_size)) > 0)
                                {                                
                                    ESP_LOGD(__FUNCTION__, "firmware sent %d bytes", len);
                                    esp_http_client_fetch_headers(client);
                                    if ((len = esp_http_client_get_status_code(client)) != 200)
                                    {
                                        ESP_LOGE(__FUNCTION__, "Status code:%d", len);
                                    }
                                    else
                                    {
                                        if ((len = esp_http_client_read(client, dmd5, 36)) > 0)
                                        {
                                            if (strcmp(dmd5, "Flashing") == 0)
                                            {
                                                ESP_LOGD(__FUNCTION__, "Station will be updated:%s", dmd5);
                                                retCode = false;
                                            }
                                            else
                                            {
                                                ESP_LOGE(__FUNCTION__, "Station will not be updated:%s", dmd5);
                                            }
                                        }
                                        else
                                        {
                                            ESP_LOGE(__FUNCTION__, "Got empty response on flash.");
                                        }
                                    }
                                } else {
                                    ESP_LOGE(__FUNCTION__,"Cannot write image:%s",esp_err_to_name(ret));
                                }
                                esp_http_client_close(client);
                            } else {
                                ESP_LOGE(__FUNCTION__,"Cannot open client:%s",esp_err_to_name(ret));
                            }
                            esp_http_client_cleanup(client);
                        } else {
                            ESP_LOGE(__FUNCTION__,"Cannot open image: /lfs/firmware/current.bin");
                        }
                    }
                }
                else
                {
                    fClose(fw);
                    ESP_LOGE(__FUNCTION__, "Error with weird md5 len %d", len);
                }
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "Failed in opeing md5");
            }
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "Unexpected output from get md5 len:%d", len);
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Version check request failed: %s", esp_err_to_name(err));
    }
    return retCode;
}

cJSON *GetStatus(ip4_addr_t *ipInfo, uint32_t devId)
{
    char *kmlFiles = (char *)dmalloc(KML_BUFFER_SIZE);
    esp_http_client_config_t *config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(kmlFiles, 0, KML_BUFFER_SIZE);
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = (char *)dmalloc(255);
    memset((void *)config->url, 0, 255);
    sprintf((char *)config->url, "http://" IPSTR "/status/", IP2STR(ipInfo));
    config->method = HTTP_METHOD_POST;
    config->timeout_ms = 9000;
    config->event_handler = json_event_handler;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->timeout_ms = 3000;
    config->port = 80;
    config->user_data = kmlFiles;
    ESP_LOGD(__FUNCTION__, "Getting %s", config->url);
    esp_http_client_handle_t client = esp_http_client_init(config);
    xEventGroupSetBits(eventGroup, GETTING_TRIP_LIST);
    esp_err_t err;
    err = esp_http_client_perform(client);
    esp_http_client_cleanup(client);
    ldfree((void *)config->url);
    ldfree((void *)config);

    if (err != ESP_OK)
    {
        ESP_LOGD(__FUNCTION__, "Probably not a tracker but a lurker %s", esp_err_to_name(err));
        //ldfree(kmlFiles);
        //deinitSPISDCard();
        //vTaskDelete(NULL);
    }

    xEventGroupWaitBits(eventGroup, GETTING_TRIPS, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGV(__FUNCTION__, "Got %s", kmlFiles);
    cJSON *json = cJSON_Parse(kmlFiles);
    if (json != NULL)
    {
        char *fname = (char *)dmalloc(255);
        sprintf(fname, "/lfs/status/%d.json", devId);
        ESP_LOGV(__FUNCTION__, "Writing %s", fname);
        FILE *destF = fopen(fname, "w", true);
        if (destF != NULL)
        {
            fputs(kmlFiles, destF);
            fClose(destF);
            ESP_LOGV(__FUNCTION__, "Wrote %s", fname);
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "Cannot open dest %s", fname);
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Error whilst parsing config");
    }
    return json;
}

void extractClientTar(char *tarFName)
{
    ESP_LOGD(__FUNCTION__, "Parsing File %s", tarFName);
    mtar_t tar;
    mtar_header_t header;
    int ret = mtar_open(&tar, tarFName, "r");
    char *buf = (char *)dmalloc(8192);
    uint32_t len = 0;
    uint32_t chunkLen = 0;
    char fname[255];
    FILE *fw = NULL;
    if (ret == MTAR_ESUCCESS)
    {
        cJSON *msg = NULL;
        cJSON *devid = NULL;
        while ((ret = mtar_read_header(&tar, &header)) != MTAR_ENULLRECORD)
        {
            if ((header.type == MTAR_TREG) && (header.size > 0))
            {
                ESP_LOGD(__FUNCTION__, "File %s (%d bytes)", header.name, header.size);
                len = 0;
                if (endsWith(header.name, ".json"))
                {
                    ret = mtar_read_data(&tar, buf, header.size);
                    if ((ret == MTAR_ESUCCESS) && (buf[0] == '{'))
                    {
                        buf[header.size] = 0;
                        if (devid == NULL){
                            msg = cJSON_Parse(buf);
                            devid = cJSON_GetObjectItemCaseSensitive(msg, "deviceid");
                        }
                        if (devid != NULL)
                        {
                            sprintf(fname, "/lfs/%s/%d.json", indexOf(header.name,"config") ? "config" : "status", devid->valueint);
                            ESP_LOGD(__FUNCTION__, "Saved as %s (%d bytes)\n", fname, header.size);
                            fw = fopen(fname, "w", true);
                            fwrite(buf, 1, header.size, fw);
                            fClose(fw);
                        }
                    }
                }
                else
                {
                    if (endsWith(header.name, ".log"))
                    {
                        if (devid != NULL) {
                            sprintf(fname, "/sdcard/logs/%d/%s", devid->valueint, lastIndexOf(header.name, "/") + 1);
                        } else {
                            sprintf(fname, "/sdcard/logs/%s", lastIndexOf(header.name, "/") + 1);
                        }
                        for (char *fchar = lastIndexOf(fname, "/"); *fchar != 0; fchar++)
                        {
                            if (*fchar == '-')
                            {
                                *fchar = '/';
                            }
                            if (*fchar == '_')
                            {
                                *fchar = '/';
                                break;
                            }
                        }
                    }
                    if (endsWith(header.name, ".csv"))
                    {
                        if (devid != NULL) {
                            sprintf(fname, "/sdcard/csv/%d/%s", devid->valueint, lastIndexOf(header.name, "/") + 1);
                        } else {
                            sprintf(fname, "/sdcard/csv/%s", lastIndexOf(header.name, "/") + 1);
                        }
                        for (char *fchar = lastIndexOf(fname, "/"); *fchar != 0; fchar++)
                        {
                            if (*fchar == '-')
                            {
                                *fchar = '/';
                            }
                            if (*fchar == '_')
                            {
                                *fchar = '/';
                                break;
                            }
                        }
                    }
                    ESP_LOGD(__FUNCTION__, "Saved as %s (%d bytes)\n", fname, header.size);
                    fw = fopen(fname, "w", true);
                    if (fw != NULL)
                    {
                        while (len < header.size)
                        {
                            chunkLen = fmin(header.size - len, 8192);
                            mtar_read_data(&tar, buf, chunkLen);
                            fwrite(buf, 1, chunkLen, fw);
                            len += chunkLen;
                        }
                        fClose(fw);
                        ESP_LOGV(__FUNCTION__, "end %s (%d bytes)", header.name, len);
                    }
                    else
                    {
                        ESP_LOGE(__FUNCTION__, "Cannot write %s", fname);
                    }
                }
            }
            if ((ret = mtar_next(&tar)) != MTAR_ESUCCESS)
            {
                ESP_LOGE(__FUNCTION__, "Error reading %s %s", header.name, mtar_strerror(ret));
                break;
            }
        }
        if (ret != MTAR_ENULLRECORD)
        {
            ESP_LOGE(__FUNCTION__, "Error parsing %s %s", header.name, mtar_strerror(ret));
        }
        mtar_close(&tar);
        if (msg) {
            cJSON_Delete(msg);
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot unter the tar %s:%s", tarFName, mtar_strerror(ret));
    }
    ldfree(buf);
}

cJSON* GetDeviceConfig(esp_ip4_addr_t *ipInfo)
{
    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t *config = NULL;
    cJSON* ret = NULL;

    config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = (char *)dmalloc(30);
    sprintf((char *)config->url, "http://" IPSTR "/config", IP2STR(ipInfo));
    config->method = HTTP_METHOD_POST;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;
    ESP_LOGV(__FUNCTION__, "Getting %s", config->url);
    esp_err_t err=ESP_ERR_HW_CRYPTO_BASE;
    int len=0;
    int retCode=-1;
    if ((client = esp_http_client_init(config)) &&
        ((err = esp_http_client_open(client,0)) == ESP_OK) && 
        ((len=esp_http_client_fetch_headers(client))>=0) &&
        ((retCode=esp_http_client_get_status_code(client)) == 200))
    {
        ESP_LOGD(__FUNCTION__,"Getting %d bytes of config",len+1);
        char* sjson = (char*)dmalloc(len+1);
        if ((len=esp_http_client_read(client,sjson,len+1))>0) {
            if ((ret = cJSON_Parse(sjson)) == NULL) {
                ESP_LOGD(__FUNCTION__,"Failed to parse %s(%d)",len?sjson:"null",len);
            }
        } else {
            ESP_LOGE(__FUNCTION__,"Failed reading config response %d", len);
        }
        ldfree(sjson);
    } else {
        ESP_LOGE(__FUNCTION__,"Failed sending config request. client is %snull, err:%s, len:%d",client?"not ":"",esp_err_to_name(err),len);
    }
    esp_http_client_cleanup(client);
    ldfree((void *)config->url);
    ldfree((void *)config);
    return ret;
}

static bool isPulling = false;
void pullStation(void *pvParameter)
{
    if (isPulling)
    {
        ESP_LOGW(__FUNCTION__, "Not repulling");
        vTaskDelete(NULL);
    }
    isPulling = true;
    esp_ip4_addr_t *ipInfo = (esp_ip4_addr_t *)pvParameter;
    int retryCtn = 10;
    cJSON* jcfg = NULL;
    char tarFName[255];
    bool isAllGood = false;
    while (((jcfg = GetDeviceConfig(ipInfo)) == NULL) && (retryCtn-- >= 0))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    char strftime_buf[64];
    if (jcfg)
    {
        ESP_LOGD(__FUNCTION__,"Pulling from " IPSTR , IP2STR(ipInfo));
        esp_http_client_config_t *config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
        if (initSPISDCard())
        {
            memset(config, 0, sizeof(esp_http_client_config_t));
            config->url = (char *)dmalloc(30);
            sprintf((char *)config->url, "http://" IPSTR "/trips", IP2STR(ipInfo));
            config->method = HTTP_METHOD_POST;
            config->timeout_ms = 30000;
            config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
            config->max_redirection_count = 0;
            config->port = 80;

            struct tm timeinfo;
            time_t now = 0;
            time(&now);
            localtime_r(&now, &timeinfo);
            strftime(strftime_buf, sizeof(strftime_buf), "%Y/%m/%d/%H-%M-%S", &timeinfo);
            sprintf(tarFName, "/sdcard/tars/" IPSTR, IP2STR(ipInfo));
            sprintf(tarFName + strlen(tarFName), "/%s.tar", strftime_buf);
            ESP_LOGD(__FUNCTION__, "Saving as:%s", tarFName);
            config->user_data = tarFName;
            config->event_handler = filedownload_event_handler;
            ESP_LOGD(__FUNCTION__, "Getting %s", config->url);
            esp_http_client_handle_t client = esp_http_client_init(config);
            esp_err_t err;
            err = esp_http_client_perform(client);

            if (esp_http_client_get_status_code(client) == 200)
            {
                isAllGood=true;
                esp_http_client_cleanup(client);
                ldfree((void *)config->url);

                CheckOTA(ipInfo);
                AppConfig* acfg = new AppConfig(jcfg,NULL);

                struct stat st;
                esp_err_t ret = ESP_FAIL;
                char fname[255];
                sprintf(fname,"/lfs/config/%d.json",acfg->GetIntProperty("deviceid"));

                if ((ret = stat(fname, &st)) == 0)
                {
                    char* sjson = (char*)dmalloc(st.st_size+1);
                    FILE* fcfg = fopen(fname,"r");
                    if (fcfg) {
                        ESP_LOGD(__FUNCTION__,"Reading %d bytes from %s",(int)st.st_size,fname);
                        if (fread(sjson,sizeof(char),st.st_size+1,fcfg) > 0) {
                            cJSON* joldcfg = cJSON_Parse(sjson);
                            if (sjson && cJSON_Compare(jcfg,joldcfg,true)){
                                memset(config, 0, sizeof(esp_http_client_config_t));
                                config->url = (char *)dmalloc(255);
                                sprintf((char *)config->url, "http://" IPSTR "/config/%d", IP2STR(ipInfo), acfg->GetIntProperty("deviceid"));
                                config->method = HTTP_METHOD_POST;
                                config->timeout_ms = 9000;
                                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                                config->max_redirection_count = 0;
                                config->port = 80;
                                client = esp_http_client_init(config);
                                
                                ESP_LOGD(__FUNCTION__, "Updating config for %d", acfg->GetIntProperty("deviceid"));
                                if ((ret = esp_http_client_open(client, strlen(sjson))) == ESP_OK)
                                {
                                    if (esp_http_client_write(client, sjson, strlen(sjson)) != strlen(sjson))
                                    {
                                        ESP_LOGE(__FUNCTION__, "Failed in updating %d's cfg", acfg->GetIntProperty("deviceid"));
                                    }
                                    else
                                    {
                                        ESP_LOGD(__FUNCTION__, "updated %d's cfg", acfg->GetIntProperty("deviceid"));
                                    }
                                    esp_http_client_cleanup(client);
                                } else {
                                    ESP_LOGE(__FUNCTION__,"Cannot open json in %s",fname);
                                }
                            }
                            else if (sjson)
                            {
                                ESP_LOGD(__FUNCTION__, "No cfg updated needed for %d", acfg->GetIntProperty("deviceid"));
                            } 
                            else 
                            {
                                ESP_LOGE(__FUNCTION__,"Cannot parse new cfg for %d",acfg->GetIntProperty("deviceid"));
                            }
                        } else {
                            ESP_LOGE(__FUNCTION__,"Cannot read bits for new cfg %d",acfg->GetIntProperty("deviceid"));
                        }
                        fclose(fcfg);
                    } else {
                        ESP_LOGE(__FUNCTION__,"Cannot read new cfg for %d",acfg->GetIntProperty("deviceid"));
                    }
                } else {
                    ESP_LOGE(__FUNCTION__,"Cannot open new cfg for %s",fname);
                }

                memset(config, 0, sizeof(esp_http_client_config_t));
                config->url = (char *)dmalloc(255);
                sprintf((char *)config->url, "http://" IPSTR "/status/wifi", IP2STR(ipInfo));
                config->method = HTTP_METHOD_PUT;
                config->timeout_ms = 9000;
                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                config->max_redirection_count = 0;
                config->port = 80;
                esp_http_client_handle_t client = esp_http_client_init(config);
                char *postData = "{\"enabled\":\"no\"}";
                ESP_LOGD(__FUNCTION__, "Sending wifi off %s to %s", postData, config->url);
                if ((ret = esp_http_client_open(client, strlen(postData))) == ESP_OK)
                {
                    if (esp_http_client_write(client, postData, strlen(postData)) != strlen(postData))
                    {
                        ESP_LOGD(__FUNCTION__, "Turn off wifi failed, but that is to be expected: %s", esp_err_to_name(ret));
                    }
                    else
                    {
                        ESP_LOGD(__FUNCTION__, "Sent wifi off %s to %s", postData, config->url);
                    }
                    esp_http_client_cleanup(client);
                }
                else
                {
                    ESP_LOGW(__FUNCTION__, "Send wifi off request failed: %s", esp_err_to_name(ret));
                }
                free(acfg);
            }
            else
            {
                ESP_LOGW(__FUNCTION__, "Cannot pull: %s", esp_err_to_name(err));
            }

            ldfree((void *)config->url);
        }

        ldfree((void *)config);
        ldfree(pvParameter);
        deinitSPISDCard();
    }
    else
    {
        ESP_LOGW(__FUNCTION__, "Cannot pull from " IPSTR, IP2STR(ipInfo));
    }
    isPulling = false;
    if (isAllGood)
    {
        tarFName[0] = '/';
        tarFName[1] = 's';
        tarFName[2] = 'd';
        tarFName[3] = 'c';
        extractClientTar(tarFName);
        xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void *)(BIT2 | BIT3), tskIDLE_PRIORITY, NULL);
    }

    vTaskDelete(NULL);
}

typedef struct
{
    mtar_t *tar;
    uint8_t *buf;
    uint32_t bufsize;
} tarDownloading_t;

tarDownloading_t tarDownloading;

esp_err_t http_tar_download_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(__FUNCTION__, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        if (*(FILE **)evt->user_data != NULL)
        {
            fwrite(evt->data, 1, evt->data_len, (FILE *)evt->user_data);
        }
        else
        {
            ESP_LOGW(__FUNCTION__, "Data with no dest file %d bytes", evt->data_len);
            if (evt->data_len < 200)
            {
                ESP_LOGW(__FUNCTION__, "%s", (char *)evt->data);
            }
            return ESP_FAIL;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        fClose((FILE *)evt->user_data);
        ESP_LOGD(__FUNCTION__, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(__FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}