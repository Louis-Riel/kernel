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
        if (xEventGroupGetBits(TheWifi::GetEventGroup()) & GETTING_TRIP_LIST)
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
        if (xEventGroupGetBits(TheWifi::GetEventGroup()) & GETTING_TRIP_LIST)
        {
            xEventGroupClearBits(TheWifi::GetEventGroup(), GETTING_TRIP_LIST);
            xEventGroupSetBits(TheWifi::GetEventGroup(), GETTING_TRIPS);
        }
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}

bool moveFolder(const char *folderName,const char *toFolderName)
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
    DIR* theFolder;
    char *fName = (char *)dmalloc(270);
    char *destfName = (char *)dmalloc(270);
    void *buf = dmalloc(F_BUF_SIZE);
    struct dirent *fi;
    bool retval = true;

    if ((theFolder = openDir(folderName)) != NULL)
    {
        ESP_LOGD(__FUNCTION__, "reading files in %s", folderName);
        while ((fi = readDir(theFolder)) != NULL)
        {
            if (strlen(fi->d_name) == 0)
            {
                break;
            }
            if (!(fi->d_type & DT_DIR))
            {
                sprintf(fName, "/sdcard%s/%s", folderName, fi->d_name);
                sprintf(destfName, "/sdcard%s%s/%s", toFolderName, folderName, fi->d_name);
                ESP_LOGD(__FUNCTION__, "Moving %s - to %s", fName, destfName);
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
                sprintf(fName, "%s/%s", folderName, fi->d_name);
                ESP_LOGD(__FUNCTION__, "Moving sub folder %s of %s as %s", fi->d_name, folderName, fName);
                moveFolder(fName, "/sent");
            }
        }
        closeDir(theFolder);
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
            if ((fw = fOpen("/lfs/firmware/current.bin.md5", "r")) != NULL)
            {
                char ccmd5[33];
                if ((len = fRead((void *)ccmd5, 1, 32, fw)) >= 0)
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
                        esp_http_client_close(client);
                        esp_http_client_cleanup(client);
                        ldfree((void*)config->url);
                        memset(config, 0, sizeof(esp_http_client_config_t));
                        fClose(fw);
                        FILE* fw;

                        struct stat st;

                        ldfree((void *)config->url);

                        if ((fw = fOpenCd("/lfs/firmware/current.bin", "r", true)) != NULL)
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
                                if (((len += fRead(img+len, 1, st.st_size, fw)) == 0) || (len >= st.st_size))
                                {
                                    ESP_LOGD(__FUNCTION__,"Read %d bytes from current bin", len);
                                    break;
                                }
                            }
                            fClose(fw);

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
                            esp_http_client_close(client);
                            esp_http_client_cleanup(client);
                            ldfree((void*)config->url);
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
    xEventGroupSetBits(TheWifi::GetEventGroup(), GETTING_TRIP_LIST);
    esp_err_t err;
    err = esp_http_client_perform(client);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    ldfree((void *)config->url);
    ldfree((void *)config);

    if (err != ESP_OK)
    {
        ESP_LOGD(__FUNCTION__, "Probably not a tracker but a lurker %s", esp_err_to_name(err));
        //ldfree(kmlFiles);
        //deinitSPISDCard();
    }

    xEventGroupWaitBits(TheWifi::GetEventGroup(), GETTING_TRIPS, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGV(__FUNCTION__, "Got %s", kmlFiles);
    cJSON *json = cJSON_ParseWithLength(kmlFiles,KML_BUFFER_SIZE);
    if (json != NULL)
    {
        char *fname = (char *)dmalloc(255);
        sprintf(fname, "/lfs/status/%d.json", devId);
        ESP_LOGV(__FUNCTION__, "Writing %s", fname);
        FILE *destF = fOpenCd(fname, "w", true);
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
        cJSON_Delete(json);
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Error whilst parsing config");
    }
    return json;
}

bool extractClientTar(char *tarFName)
{
    ESP_LOGD(__FUNCTION__, "Parsing File %s", tarFName);
    bool retVal = true;
    mtar_t tar;
    mtar_header_t* header = (mtar_header_t*)dmalloc(sizeof(mtar_header_t));
    int ret = mtar_open(&tar, tarFName, "r");
    char *buf = (char *)dmalloc(JSON_BUFFER_SIZE);
    uint32_t len = 0;
    uint32_t chunkLen = 0;
    char* fname = (char *)dmalloc(255);
    FILE *fw = NULL;
    struct stat st;
    char* prevName=(char *)dmalloc(255);
    memset(prevName, 0, 255);
    if (ret == MTAR_ESUCCESS)
    {
        cJSON *msg = NULL;
        cJSON *devid = NULL;
        while ((ret = mtar_read_header(&tar, header)) != MTAR_ENULLRECORD)
        {
            *buf=0;
            if (strcmp(header->name, prevName)!=0) {
                ESP_LOGV(__FUNCTION__,"Different header name %s",header->name);
                strcpy(prevName,header->name);
            } else {
                ESP_LOGD(__FUNCTION__, "This is the end..%s",mtar_strerror(ret));
                ret=MTAR_ENULLRECORD;
                break; //Weirdness ensues with tars. When the same header is sent twice, lets just assume we are done and good.
            }
            if ((header->type == MTAR_TREG) && (header->size > 0))
            {
                ESP_LOGV(__FUNCTION__, "File %s (%d bytes)", header->name, header->size);
                len = 0;
                if (endsWith(header->name, "current.json"))
                {
                    ret = mtar_read_data(&tar, buf, header->size);
                    if ((ret == MTAR_ESUCCESS) && (buf[0] == '{'))
                    {
                        buf[header->size] = 0;
                        if (devid == NULL) {
                            msg = cJSON_ParseWithLength(buf,header->size);
                            if (msg) {
                                devid = cJSON_GetObjectItemCaseSensitive(msg, "deviceid");
                                if (cJSON_HasObjectItem(devid,"value")) {
                                    devid = cJSON_GetObjectItem(devid,"value");
                                }
                                cJSON_Delete(msg);
                            }
                        }
                        if (devid != NULL)
                        {
                            sprintf(fname, "/sdcard/%s/%d.json", indexOf(header->name,"config") ? "config" : "status", devid->valueint);
                        } else{
                            sprintf(fname, "/sdcard/ucf/%s", header->name);
                        }
                    }
                }
                else
                {
                    if (endsWith(header->name, ".log"))
                    {
                        sprintf(fname, "/sdcard/%s", indexOf(header->name, "/") + 1);
                    } else if (endsWith(header->name, ".csv"))
                    {
                        if (devid != NULL) {
                            sprintf(fname, "/sdcard/csv/%d/%s", devid->valueint, lastIndexOf(header->name, "/") + 1);
                        } else {
                            sprintf(fname, "/sdcard/csv/%s", lastIndexOf(header->name, "/") + 1);
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
                    } else if (endsWith(header->name, "empty.txt")) {
                        ESP_LOGD(__FUNCTION__,"%s is empty.", header->name);
                    } else {
                        if (devid != NULL) {
                            sprintf(fname, "/sdcard/ocf/%d/%s", devid->valueint, lastIndexOf(header->name, "/") + 1);
                        } else {
                            sprintf(fname, "/sdcard/ocf/%s", lastIndexOf(header->name, "/") + 1);
                        }
                    }
                }
                if (strlen(buf)==0){
                    ret = stat(fname, &st);
                    if (((ret == 0) && (st.st_size != header->size)) || (ret != 0)){
                        fw = fOpenCd(fname, "w", true);
                        if (fw != NULL)
                        {
                            len=0;
                            while (len < header->size)
                            {
                                chunkLen = fmin(header->size - len, 8192);
                                mtar_read_data(&tar, buf, chunkLen);
                                fWrite(buf, 1, chunkLen, fw);
                                len += chunkLen;
                            }
                            fClose(fw);
                            ESP_LOGD(__FUNCTION__, "Saved as %s (tar header %d bytes, file %d bytes, wrote %d bytes)\n", fname, header->size, (int)st.st_size, len);
                        }
                        else
                        {
                            ESP_LOGE(__FUNCTION__, "Cannot write %s", fname);
                            retVal = false;
                        }
                    } else {
                        ESP_LOGD(__FUNCTION__, "Skippng %s (%d bytes)..%s", fname, header->size,mtar_strerror(ret));
                    }
                }
            }
            if ((ret = mtar_next(&tar)) != MTAR_ESUCCESS)
            {
                ESP_LOGE(__FUNCTION__, "Error reading %s %s", header->name, mtar_strerror(ret));
                retVal = false;
                break;
            }
            ESP_LOGV(__FUNCTION__,"Tar next:%s",mtar_strerror(ret));
        }
        if (ret != MTAR_ENULLRECORD)
        {
            ESP_LOGE(__FUNCTION__, "Error parsing %s %s", header->name, mtar_strerror(ret));
            retVal = false;
        }
        mtar_close(&tar);
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot unter the tar %s:%s", tarFName, mtar_strerror(ret));
        retVal = false;
    }
    ldfree(buf);
    ldfree(fname);
    ldfree(header);
    return retVal;
}

cJSON* GetDeviceConfig(esp_ip4_addr_t *ipInfo) {
    return GetDeviceConfig(ipInfo,0);
}

cJSON* GetDeviceConfig(esp_ip4_addr_t *ipInfo,uint32_t deviceId)
{
    esp_http_client_handle_t client = NULL;
    esp_http_client_config_t *config = NULL;
    cJSON* ret = NULL;

    config = (esp_http_client_config_t *)dmalloc(sizeof(esp_http_client_config_t));
    memset(config, 0, sizeof(esp_http_client_config_t));
    config->url = (char *)dmalloc(100);
    if (deviceId)
        sprintf((char *)config->url, "http://" IPSTR "/config/%d", IP2STR(ipInfo),deviceId);
    else
        sprintf((char *)config->url, "http://" IPSTR "/config", IP2STR(ipInfo));

    config->method = HTTP_METHOD_POST;
    config->timeout_ms = 30000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count = 0;
    config->port = 80;
    ESP_LOGV(__FUNCTION__, "Getting %s", config->url);
    esp_err_t err=ESP_ERR_HW_CRYPTO_BASE;
    int len=0,hlen=0;
    int retCode=-1;
    char* sjson = NULL;
    if ((client = esp_http_client_init(config)) &&
        ((err = esp_http_client_open(client,0)) == ESP_OK) && 
        ((hlen=esp_http_client_fetch_headers(client))>=0) &&
        ((retCode=esp_http_client_get_status_code(client)) == 200) &&
        ((sjson = (char*)dmalloc(hlen?hlen:JSON_BUFFER_SIZE))!=NULL) &&
        ((len=esp_http_client_read(client,sjson,hlen?hlen:JSON_BUFFER_SIZE))>0) &&
        ((ret = cJSON_ParseWithLength(sjson,hlen?hlen:JSON_BUFFER_SIZE)) != NULL))
    {
        ESP_LOGD(__FUNCTION__,"Parsed %d bytes of config",len);
    } else {
        ESP_LOGE(__FUNCTION__,"Failed sending config request(%s). client is %snull, err:%s, hlen:%d, retCode:%d, len:%d sjson:%s",config->url, client?"not ":"",esp_err_to_name(err),hlen,retCode,len, sjson == NULL ? "null":sjson);
    }

    if (sjson)
        ldfree(sjson);

    if (client){
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
    }
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
        return;
    }
    isPulling = true;
    esp_ip4_addr_t *ipInfo = (esp_ip4_addr_t *)pvParameter;
    int retryCtn = 10;
    cJSON* jcfg = NULL;
    char* tarFName= (char*)dmalloc(255);
    bool isAllGood = false;
    while (((jcfg = GetDeviceConfig(ipInfo)) == NULL) && (retryCtn-- >= 0))
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    char* strftime_buf = (char*)dmalloc(64);
    if (jcfg)
    {
        AppConfig* cfg = new AppConfig(jcfg,NULL);
        ESP_LOGD(__FUNCTION__,"Pulling from " IPSTR "/%d", IP2STR(ipInfo),cfg->GetIntProperty("deviceid"));
        free(cfg);
        cJSON_Delete(jcfg);
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
            int respCode=0;

            if ((respCode = esp_http_client_get_status_code(client)) == 200)
            {
                isAllGood=true;
                esp_http_client_close(client);
                esp_http_client_cleanup(client);
                ldfree((void *)config->url);

                CheckOTA(ipInfo);

                memset(config, 0, sizeof(esp_http_client_config_t));
                config->url = (char *)dmalloc(255);
                sprintf((char *)config->url, "http://" IPSTR "/status/wifi", IP2STR(ipInfo));
                config->method = HTTP_METHOD_PUT;
                config->timeout_ms = 9000;
                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                config->max_redirection_count = 0;
                config->port = 80;
                esp_http_client_handle_t client = esp_http_client_init(config);
                const char *postData = "{\"enabled\":\"no\"}";
                ESP_LOGD(__FUNCTION__, "Sending wifi off %s to %s", postData, config->url);
                esp_err_t ret = ESP_FAIL;
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
                    esp_http_client_close(client);
                    esp_http_client_cleanup(client);
                }
                else
                {
                    ESP_LOGW(__FUNCTION__, "Send wifi off request failed: %s", esp_err_to_name(ret));
                }
            }
            else
            {
                ESP_LOGW(__FUNCTION__, "Cannot pull:%s retCode:%d", esp_err_to_name(err),respCode);
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
        //CreateWokeBackgroundTask(commitTripToDisk, "commitTripToDisk", 4096, NULL, tskIDLE_PRIORITY, NULL);
    }
    ldfree(tarFName);
    ldfree(strftime_buf);
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
            fWrite(evt->data, 1, evt->data_len, (FILE *)evt->user_data);
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