#include "rest.h"
#include "route.h"
#include <cstdio>
#include <cstring>

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

char* tarfname=(char*)malloc(255);

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
        if (xEventGroupGetBits(eventGroup) & GETTING_TRIP_LIST) {
            if (evt->data_len<KML_BUFFER_SIZE ){
                assert(evt->user_data);
                char* kmlFiles=(char*)evt->user_data;
                ESP_LOGV(__FUNCTION__, "kmllist data len:%d", evt->data_len);
                memcpy(kmlFiles+strlen(kmlFiles),evt->data,evt->data_len);
                ESP_LOGV(__FUNCTION__,"%s",kmlFiles);
            } else {
                ESP_LOGE(__FUNCTION__,"Cannot fit %d in the ram",evt->data_len);
            }
        } else {
            ESP_LOGV(__FUNCTION__,"Got data with no dest: strlen(tarfname):%d",strlen(tarfname));
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        if (xEventGroupGetBits(eventGroup)&GETTING_TRIP_LIST){
            xEventGroupClearBits(eventGroup,GETTING_TRIP_LIST);
            xEventGroupSetBits(eventGroup,GETTING_TRIPS);
        }
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}

esp_err_t filedownload_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(__FUNCTION__, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_CONNECTED");
        xEventGroupClearBits(eventGroup,DOWNLOAD_STARTED);
        xEventGroupClearBits(eventGroup,DOWNLOAD_FINISHED);
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        if (strcmp(evt->header_key,"filename") == 0) {
            FILE* dfile = fopen(*((char*)evt->user_data) != '/' ? evt->header_value : (char*)evt->user_data,"w",true);
            if (dfile == NULL) {
                ESP_LOGE(__FUNCTION__,"Error whiilst opening %s",evt->header_value);
                return ESP_FAIL;
            }
            *(FILE**)evt->user_data = dfile;
            xEventGroupSetBits(eventGroup,DOWNLOAD_STARTED);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if (*(FILE**)evt->user_data != NULL) {
            fwrite(evt->data,1,evt->data_len,*(FILE**)evt->user_data);
        } else {
            ESP_LOGW(__FUNCTION__,"Data with no dest file %d bytes",evt->data_len);
            if (evt->data_len < 200) {
                ESP_LOGW(__FUNCTION__,"%s",(char*)evt->data);
            }
            return ESP_FAIL;
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_ON_FINISH ");
        if (*(FILE**)evt->user_data != NULL) {
            fclose(*(FILE**)evt->user_data);
        } else {
            ESP_LOGW(__FUNCTION__,"Close file with no dest file %d bytes",evt->data_len);
        }
        xEventGroupSetBits(eventGroup,DOWNLOAD_FINISHED);
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGV(__FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}

bool moveFolder(char* folderName, char* toFolderName) {
    if ((folderName == NULL) || (toFolderName == NULL)) {
        ESP_LOGE(__FUNCTION__,"Empty params passed: foldername:%s, toFolderName:%s",folderName==NULL?"null":"not nukll",toFolderName==NULL?"null":"not nukll");
        return false;
    }
    if ((strlen(folderName) == 0) || (strlen(toFolderName) == 0)) {
        ESP_LOGE(__FUNCTION__,"Empty params passed: foldername len:%d, toFolderName len:%d",strlen(folderName),strlen(toFolderName));
        return false;
    }
    FF_DIR theFolder;
    FILE* theFile;
    FILE* theDestFile;
    uint32_t len=0;
    char* fName=(char*)malloc(270);
    char* destfName=(char*)malloc(270);
    void* buf = malloc(F_BUF_SIZE);
    FILINFO* fi = (FILINFO*)malloc(sizeof(FILINFO));
    bool retval=true;

    if (f_opendir(&theFolder, folderName) == FR_OK)
    {
        ESP_LOGD(__FUNCTION__, "reading files in %s", folderName);
        while (f_readdir(&theFolder, fi) == FR_OK)
        {
            if (strlen(fi->fname) == 0)
            {
                break;
            }
            if (!(fi->fattrib & AM_DIR)){
                sprintf(fName,"/sdcard%s/%s",folderName,fi->fname);
                sprintf(destfName,"/sdcard%s%s/%s",toFolderName,folderName,fi->fname);
                ESP_LOGD(__FUNCTION__, "Moving %s - %d to %s", fName, fi->fsize,destfName);
                if (moveFile(fName,destfName)){
                    ESP_LOGD(__FUNCTION__,"%s deleted",fName);
                } else {
                    ESP_LOGE(__FUNCTION__,"Cannot move %s",fName);
                    retval=false;
                    break;
                }
            } else {
                sprintf(fName,"%s/%s",folderName,fi->fname);
                ESP_LOGD(__FUNCTION__,"Moving sub folder %s of %s as %s",fi->fname, folderName,fName);
                moveFolder(fName,"/sent");
            }
        }
        f_closedir(&theFolder);
        if (f_unlink(folderName)!= 0) {
            ESP_LOGE(__FUNCTION__,"Cannot delete folder %s",folderName);
            retval=false;
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Cannot read dir %s",folderName);
        retval=false;
    }
    free(fi);
    free(fName);
    free(destfName);
    free(buf);
    return retval;
}

bool GetKmls(ip4_addr_t* ipInfo){
    bool retVal=true;
    char* kmlFiles = (char*)malloc(KML_BUFFER_SIZE);
    memset(kmlFiles,0,KML_BUFFER_SIZE);
    esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
    memset(config,0,sizeof(esp_http_client_config_t));
    config->url=(char*)malloc(255);
    sprintf((char*)config->url,"http://" IPSTR "/listentities/csv",IP2STR(ipInfo));
    config->method=HTTP_METHOD_POST;
    config->timeout_ms = 9000;
    config->event_handler = json_event_handler;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count=0;
    config->port=80;
    config->user_data=kmlFiles;
    config->timeout_ms=3000;
    ESP_LOGD(__FUNCTION__,"Getting %s",config->url);
    esp_http_client_handle_t client = esp_http_client_init(config);
    esp_err_t err;
    uint8_t retryCnt=0;
    uint32_t kmlFilesPos=0;
    xEventGroupSetBits(eventGroup,GETTING_TRIP_LIST);
    while (((err = esp_http_client_perform(client)) == ESP_ERR_HTTP_CONNECT) && (retryCnt++<10))
    {
        ESP_LOGD(__FUNCTION__, "HTTP GET listtrip failed: %s", esp_err_to_name(err));
        vTaskDelay(500/portTICK_RATE_MS);
    }

    if (err != ESP_OK) {
        ESP_LOGD(__FUNCTION__,"Cannot get triplist. Probably not a tracker but a lurker. %s. len:%d", esp_err_to_name(err),kmlFilesPos);
        esp_http_client_cleanup(client);
        free((void*)config->url);
        free((void*)config);
        free(kmlFiles);
        deinitSPISDCard();
        vTaskDelete(NULL);
    }
    the_wifi_config* wc = getWifiConfig();

    xEventGroupWaitBits(eventGroup,GETTING_TRIPS,pdFALSE,pdTRUE,portMAX_DELAY);
    esp_http_client_cleanup(client);
    cJSON* json = NULL;
    if (kmlFiles){
        ESP_LOGV(__FUNCTION__,"got back %s",kmlFiles);
        json = cJSON_Parse(kmlFiles);
    }
    cJSON* jfile = NULL;
    if (json != NULL) {
        uint32_t alen = cJSON_GetArraySize(json);
        ESP_LOGD(__FUNCTION__,"Parsing %d trips",alen);
        for (uint32_t idx = 0; idx < alen; idx++){
            jfile = cJSON_GetArrayItem(json,idx);
            if (jfile != NULL) {
                cJSON* fname = cJSON_GetObjectItemCaseSensitive(jfile,"name");
                cJSON* folder = cJSON_GetObjectItemCaseSensitive(jfile,"folder");
                if ((fname == NULL) || (folder == NULL)){
                    ESP_LOGW(__FUNCTION__, "Weirdness is afoot with this one:%s[%d]",kmlFiles,idx);
                    continue;
                }
                ESP_LOGI(__FUNCTION__,"Getting file %s", fname->valuestring);
                free((void*)config->url);
                memset(config,0,sizeof(esp_http_client_config_t));
                config->url=(char*)malloc(255);
                memset((void*)config->url,0,255);
                sprintf((char*)config->url,"http://" IPSTR "%s/%s",IP2STR(ipInfo),folder->valuestring,fname->valuestring);
                config->method=HTTP_METHOD_GET;
                config->timeout_ms = 9000;
                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                config->max_redirection_count=0;
                config->port=80;
                config->event_handler = filedownload_event_handler;
                void* tmp = (void*)new FILE();
                config->user_data = tmp;
                client = esp_http_client_init(config);
                if (client != NULL){
                    esp_http_client_set_header(client,"movetosent","yes");
                    if ((err = esp_http_client_perform(client)) == ESP_OK){
                        ESP_LOGD(__FUNCTION__,"Waiting on download");
                        if (!xEventGroupWaitBits(eventGroup,DOWNLOAD_FINISHED,pdFALSE,pdTRUE,2000/portTICK_RATE_MS)){
                            ESP_LOGE(__FUNCTION__,"Timeout in waiting for return");
                        }
                    }
                } else {
                    ESP_LOGE(__FUNCTION__, "KML GET request failed: %s", esp_err_to_name(err));
                }

                esp_http_client_cleanup(client);
                memset(config,0,sizeof(esp_http_client_config_t));
            }
        }
    } else {
        retVal=false;
        ESP_LOGE(__FUNCTION__,"Error whilst parsing csv json");
    }
    cJSON_Delete(json);
    free((void*)config);
    free((void*)kmlFiles);
    return retVal;
}

bool CheckOTA(ip4_addr_t* ipInfo) {
    esp_err_t err=ESP_OK;
    bool retCode=true;
    esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
    memset(config,0,sizeof(esp_http_client_config_t));
    config->url=(char*)malloc(255);
    sprintf((char*)config->url,"http://" IPSTR "/ota/getmd5",IP2STR(ipInfo));
    config->method=HTTP_METHOD_POST;
    config->timeout_ms = 9000;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count=0;
    config->timeout_ms=3000;
    config->port=80;
    esp_http_client_handle_t client = esp_http_client_init(config);
    char dmd5[37];
    esp_err_t ret=ESP_OK;
    ESP_LOGD(__FUNCTION__,"Checking Version");
    if ((ret = esp_http_client_perform(client)) == ESP_OK){
        uint32_t len=0;
        if ((len=esp_http_client_read(client,dmd5,36)) >= 0) {
            ESP_LOGV(__FUNCTION__,"Got back (%s)%d char of md5",dmd5,len);
            char ch;
            bool isValid = strcmp(dmd5,"BADMD5")==0;
            if (!isValid){
                for (int idx=0; idx < 32; idx++){
                    ch = *(dmd5+idx);
                    isValid = (((ch >= 'a') && ( ch <= 'z' )) ||
                        ((ch >= '0') && ( ch <= '9' )));
                    if (!isValid) {
                        ESP_LOGD(__FUNCTION__,"Invalid char for %s at pos %d(%c)",dmd5,idx,ch);
                        break;
                    }
                }
            }
            FILE* fw = NULL;
            if (isValid && (fw = fopen("/lfs/firmware/current.bin.md5","r")) != NULL) {
                char ccmd5[37];
                if ((len=fread((void*)ccmd5,1,36,fw)) >= 0) {
                    ccmd5[36]=0;
                    dmd5[36]=0;
                    if (strcmp(ccmd5,dmd5)==0) {
                        ESP_LOGD(__FUNCTION__,"No firmware update needed, is up to date");
                    } else {
                        ESP_LOGI(__FUNCTION__,"firmware update needed, %s!=%s",dmd5,ccmd5);
                        esp_http_client_cleanup(client);
                        memset(config,0,sizeof(esp_http_client_config_t));
                        fclose(fw);
                        uint32_t ilen;
                        uint8_t* img = loadImage(false,&ilen);
                        if (ilen > 0){
                            free((void*)config->url);
                            memset(config,0,sizeof(esp_http_client_config_t));
                            config->url=(char*)malloc(255);
                            memset((void*)config->url,0,255);
                            sprintf((char*)config->url,"http://" IPSTR "/ota/flash?md5=%s&len=%d",IP2STR(ipInfo),ccmd5,ilen);
                            config->method=HTTP_METHOD_POST;
                            config->timeout_ms = 9000;
                            config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                            config->max_redirection_count=0;
                            config->port=80;
                            client = esp_http_client_init(config);
                            char dmd5[37];
                            esp_err_t ret=ESP_OK;
                            if ((ret = esp_http_client_open(client,ilen)) == ESP_OK){
                                ESP_LOGD(__FUNCTION__,"Sending firmware %d bytes",ilen);
                                if (esp_http_client_write(client,(char*)img,ilen) == ilen){
                                    ESP_LOGD(__FUNCTION__,"Sent firmware %d bytes",ilen);
                                    esp_http_client_fetch_headers(client);
                                    if ((len = esp_http_client_get_status_code(client)) != 200) {
                                        ESP_LOGE(__FUNCTION__,"Status code:%d",len);
                                    } else {
                                        if ((len=esp_http_client_read(client,dmd5,36)) > 0) {
                                            if (strcmp(dmd5,"Flashing")==0) {
                                                ESP_LOGI(__FUNCTION__,"Station will be updated:%s",dmd5);
                                                retCode=false;
                                            } else {
                                                ESP_LOGE(__FUNCTION__,"Station will not be updated:%s",dmd5);
                                            }
                                            esp_http_client_close(client);
                                        }else {
                                            ESP_LOGE(__FUNCTION__,"Got empty response on flash.");
                                        }
                                    }
                                } else {
                                    ESP_LOGE(__FUNCTION__,"Failed in writing image");
                                }
                            } else {
                                ESP_LOGE(__FUNCTION__,"error whilst invoking %s",config->url);
                            }
                        } else {
                            ESP_LOGE(__FUNCTION__,"Get a empty image");
                        }
                    }
                } else {
                    ESP_LOGE(__FUNCTION__,"Error with weird md5 len %d", len);
                }
                fclose(fw);
            } else {
                ESP_LOGE(__FUNCTION__,"Failed in opeing md5");
            }
        } else {
            ESP_LOGE(__FUNCTION__,"Unexpected output from get md5 len:%d",len);
        }
    } else {
        ESP_LOGE(__FUNCTION__, "Version chack request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    free((void*)config->url);
    free((void*)config);
    return retCode;
}

bool GetLogs(ip4_addr_t* ipInfo,uint32_t devId){
    char* kmlFiles = (char*)malloc(KML_BUFFER_SIZE);
    esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
    memset(kmlFiles,0,KML_BUFFER_SIZE);
    memset(config,0,sizeof(esp_http_client_config_t));
    config->url=(char*)malloc(255);
    memset((void*)config->url,0,255);
    sprintf((char*)config->url,"http://" IPSTR "/listentities/log",IP2STR(ipInfo));
    config->method=HTTP_METHOD_POST;
    config->timeout_ms = 9000;
    config->event_handler = json_event_handler;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count=0;
    config->timeout_ms=3000;
    config->port=80;
    config->user_data=kmlFiles;
    ESP_LOGD(__FUNCTION__,"Getting %s",config->url);
    esp_http_client_handle_t client = esp_http_client_init(config);
    uint32_t kmlFilesPos=0;
    xEventGroupSetBits(eventGroup,GETTING_TRIP_LIST);
    esp_err_t err;
    uint8_t retryCnt=0;
    while (((err = esp_http_client_perform(client)) == ESP_ERR_HTTP_CONNECT) && (retryCnt++<10) && (kmlFilesPos == 0))
    {
        ESP_LOGD(__FUNCTION__, "HTTP GET log failed: %s", esp_err_to_name(err));
        vTaskDelay(500/portTICK_RATE_MS);
    }
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGD(__FUNCTION__,"Cannot get loglist. Probably not a tracker but a lurker");
        free((void*)config->url);
        free((void*)config);
        free(kmlFiles);
        deinitSPISDCard();
        return false;
    }

    xEventGroupWaitBits(eventGroup,GETTING_TRIPS,pdFALSE,pdTRUE,portMAX_DELAY);
    ESP_LOGV(__FUNCTION__,"%s",kmlFiles);
    cJSON* json = cJSON_Parse(kmlFiles);
    cJSON* jfile = NULL;
    if (json != NULL) {
        uint32_t alen = cJSON_GetArraySize(json);
        ESP_LOGD(__FUNCTION__,"Parsing %d logs",alen);
        for (uint32_t idx = 0; idx < alen; idx++){
            jfile = cJSON_GetArrayItem(json,idx);
            if (jfile != NULL) {
                cJSON* fname = cJSON_GetObjectItemCaseSensitive(jfile,"name");
                cJSON* folder = cJSON_GetObjectItemCaseSensitive(jfile,"folder");
                if ((fname == NULL) || (folder == NULL)){
                    ESP_LOGW(__FUNCTION__, "Weirdness is afoot with this one:%s[%d]",kmlFiles,idx);
                    continue;
                }
                ESP_LOGI(__FUNCTION__,"Getting file %s", fname->valuestring);
                memset(config,0,sizeof(esp_http_client_config_t));
                config->url=(char*)malloc(255);
                memset((void*)config->url,0,255);
                sprintf((char*)config->url,"http://" IPSTR "%s/%s",IP2STR(ipInfo),folder->valuestring,fname->valuestring);
                config->method=HTTP_METHOD_GET;
                config->timeout_ms = 9000;
                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                config->max_redirection_count=0;
                config->port=80;
                char* destfname = (char*)malloc(100);
                char* destpath = (char*)malloc(512);
                strcpy(destfname,strchr(fname->valuestring,'-')+1);
                *(destfname+4)=0;
                *(destfname+7)=0;
                *(destfname+10)=0;
                sprintf(destpath,"/sdcard/logs/%d/%s/%s/%s/%s",devId,destfname,destfname+5,destfname+8,fname->valuestring);
                ESP_LOGD(__FUNCTION__, "Saving as:%s",destpath);
                config->user_data = destpath;
                config->event_handler = filedownload_event_handler;
                client = esp_http_client_init(config);
                if (client != NULL){
                    esp_http_client_set_header(client,"movetosent","yes");
                    if ((err = esp_http_client_perform(client)) == ESP_OK){
                        ESP_LOGD(__FUNCTION__,"Waiting on download");
                        if (!xEventGroupWaitBits(eventGroup,DOWNLOAD_FINISHED,pdFALSE,pdTRUE,2000/portTICK_RATE_MS)){
                            ESP_LOGE(__FUNCTION__,"Timeout in waiting for return");
                        }
                    } else {
                        ESP_LOGE(__FUNCTION__, "log GET request failed: %s", esp_err_to_name(err));
                        free(destfname);
                        free(destpath);
                        free((void*)config->url);
                        esp_http_client_cleanup(client);
                        break;
                    }
                } else {
                    ESP_LOGE(__FUNCTION__, "log GET request alloc failed: %s", esp_err_to_name(err));
                }
                free(destfname);
                free(destpath);
                free((void*)config->url);
                esp_http_client_cleanup(client);
            }
        }
        free((void*)config);
        free((void*)kmlFiles);
        cJSON_Delete(json);
    } else {
        ESP_LOGE(__FUNCTION__,"Error whilst parsing log json");
    }
    return true;
}

cJSON* GetConfig(ip4_addr_t* ipInfo){
    bool retVal=true;
    char* kmlFiles = (char*)malloc(KML_BUFFER_SIZE);
    esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
    memset(kmlFiles,0,KML_BUFFER_SIZE);
    memset(config,0,sizeof(esp_http_client_config_t));
    config->url=(char*)malloc(255);
    memset((void*)config->url,0,255);
    sprintf((char*)config->url,"http://" IPSTR "/config",IP2STR(ipInfo));
    config->method=HTTP_METHOD_POST;
    config->timeout_ms = 9000;
    config->event_handler = json_event_handler;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count=0;
    config->timeout_ms=3000;
    config->port=80;
    config->user_data=kmlFiles;
    ESP_LOGD(__FUNCTION__,"Getting %s",config->url);
    esp_http_client_handle_t client = esp_http_client_init(config);
    uint32_t kmlFilesPos=0;
    xEventGroupSetBits(eventGroup,GETTING_TRIP_LIST);
    esp_err_t err;
    uint8_t retryCnt=0;
    while (((err = esp_http_client_perform(client)) == ESP_ERR_HTTP_CONNECT) && (retryCnt++<10) && (kmlFilesPos == 0))
    {
        ESP_LOGD(__FUNCTION__, "HTTP GET config failed: %s", esp_err_to_name(err));
        vTaskDelay(500/portTICK_RATE_MS);
    }
    esp_http_client_cleanup(client);
    free((void*)config->url);
    free((void*)config);

    if (err != ESP_OK) {
        ESP_LOGD(__FUNCTION__,"Probably not a tracker but a lurker %s", esp_err_to_name(err));
        //free(kmlFiles);
        //deinitSPISDCard();
        //vTaskDelete(NULL);
    }

    xEventGroupWaitBits(eventGroup,GETTING_TRIPS,pdFALSE,pdTRUE,portMAX_DELAY);
    ESP_LOGV(__FUNCTION__,"Got %s",kmlFiles);
    cJSON* json = cJSON_Parse(kmlFiles);
    if (json != NULL) {
        char* fname = (char*)malloc(255);
        sprintf(fname,"/lfs/config/%d.json",cJSON_GetObjectItem(json,"devId")->valueint);
        ESP_LOGV(__FUNCTION__,"Writing %s",fname);
        int res;
        FILE* destF = fopen(fname,"w",true);
        if (destF != NULL) {
            fputs(kmlFiles,destF);
            fclose(destF);
            ESP_LOGV(__FUNCTION__,"Wrote %s",fname);
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot open dest %s",fname);
            retVal = false;
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Error whilst parsing config");
    }
    return json;
}

cJSON* GetStatus(ip4_addr_t* ipInfo,uint32_t devId){
    bool retVal=true;
    char* kmlFiles = (char*)malloc(KML_BUFFER_SIZE);
    esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
    memset(kmlFiles,0,KML_BUFFER_SIZE);
    memset(config,0,sizeof(esp_http_client_config_t));
    config->url=(char*)malloc(255);
    memset((void*)config->url,0,255);
    sprintf((char*)config->url,"http://" IPSTR "/status/",IP2STR(ipInfo));
    config->method=HTTP_METHOD_POST;
    config->timeout_ms = 9000;
    config->event_handler = json_event_handler;
    config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
    config->max_redirection_count=0;
    config->timeout_ms=3000;
    config->port=80;
    config->user_data=kmlFiles;
    ESP_LOGD(__FUNCTION__,"Getting %s",config->url);
    esp_http_client_handle_t client = esp_http_client_init(config);
    uint32_t kmlFilesPos=0;
    xEventGroupSetBits(eventGroup,GETTING_TRIP_LIST);
    esp_err_t err;
    uint8_t retryCnt=0;
    while (((err = esp_http_client_perform(client)) == ESP_ERR_HTTP_CONNECT) && (retryCnt++<10) && (kmlFilesPos == 0))
    {
        ESP_LOGD(__FUNCTION__, "HTTP GET status failed: %s", esp_err_to_name(err));
        vTaskDelay(500/portTICK_RATE_MS);
    }
    esp_http_client_cleanup(client);
    free((void*)config->url);
    free((void*)config);

    if (err != ESP_OK) {
        ESP_LOGD(__FUNCTION__,"Probably not a tracker but a lurker %s", esp_err_to_name(err));
        //free(kmlFiles);
        //deinitSPISDCard();
        //vTaskDelete(NULL);
    }

    xEventGroupWaitBits(eventGroup,GETTING_TRIPS,pdFALSE,pdTRUE,portMAX_DELAY);
    ESP_LOGV(__FUNCTION__,"Got %s",kmlFiles);
    cJSON* json = cJSON_Parse(kmlFiles);
    if (json != NULL) {
        char* fname = (char*)malloc(255);
        sprintf(fname,"/lfs/status/%d.json",devId);
        ESP_LOGV(__FUNCTION__,"Writing %s",fname);
        int res;
        FILE* destF = fopen(fname,"w",true);
        if (destF != NULL) {
            fputs(kmlFiles,destF);
            fclose(destF);
            ESP_LOGV(__FUNCTION__,"Wrote %s",fname);
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot open dest %s",fname);
            retVal = false;
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Error whilst parsing config");
    }
    return json;
}

void pullStation(void *pvParameter) {
    if (initSPISDCard()){
        ip4_addr_t* ipInfo= (ip4_addr_t*) pvParameter;
        if (GetKmls(ipInfo) && CheckOTA(ipInfo)){
            cJSON* cfg = GetConfig(ipInfo);
            if (cfg){
                uint32_t devId = cJSON_GetObjectItem(cfg,"devId")->valueint;
                GetStatus(ipInfo,devId);
                GetLogs(ipInfo,devId);
                esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
                memset(config,0,sizeof(esp_http_client_config_t));
                config->url=(char*)malloc(255);
                memset((void*)config->url,0,255);
                sprintf((char*)config->url,"http://" IPSTR "/status/wifi",IP2STR(ipInfo));
                config->method=HTTP_METHOD_PUT;
                config->timeout_ms = 9000;
                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                config->max_redirection_count=0;
                config->port=80;
                esp_http_client_handle_t client = esp_http_client_init(config);
                char* postData="{\"enabled\":\"no\"}";
                ESP_LOGV(__FUNCTION__,"Sending wifi off %s",postData);
                esp_err_t ret;
                if ((ret = esp_http_client_open(client,strlen(postData))) == ESP_OK){
                    if (esp_http_client_write(client,postData,strlen(postData)) != strlen(postData))
                    {
                        ESP_LOGD(__FUNCTION__, "Turn off wifi faile, but that is to be expected: %s", esp_err_to_name(ret));
                    } else {
                        ESP_LOGD(__FUNCTION__,"Sent wifi off");
                    }
                } else {
                    ESP_LOGE(__FUNCTION__, "KML GET request failed: %s", esp_err_to_name(ret));
                }
                esp_http_client_cleanup(client);
                free((void*)config->url);
                free((void*)config);
            }
        }
        deinitSPISDCard();
        xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void*)((BIT1)|(BIT2)|(BIT3)), tskIDLE_PRIORITY, NULL);
        ESP_LOGD(__FUNCTION__,"Done with request");
    }
    vTaskDelete(NULL);
}

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
        if (strcmp(evt->header_key,"filename")==0) {
            sprintf(tarfname,"/sdcard/kml/%s",evt->header_value);
            if ((*(FILE**)evt->user_data=fopen(tarfname,"w"))!=NULL){
                ESP_LOGD(__FUNCTION__, "%s opened", tarfname);
            } else {
                ESP_LOGE(__FUNCTION__,"Cannot read %s",tarfname);
            }
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if ((*(FILE**)evt->user_data != NULL) && (strlen(tarfname)>0)) {
            fwrite(evt->data,sizeof(uint8_t),evt->data_len,*(FILE**)evt->user_data);
            ESP_LOGV(__FUNCTION__, "tar data len:%d\n", evt->data_len);
        } else {
            ESP_LOGW(__FUNCTION__,"Got data with no dest tarnull:%s strlen(tarfname):%d",*(FILE**)evt->user_data == NULL?"NULL":"NOT NULL",strlen(tarfname));
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        fclose(*(FILE**)evt->user_data);
        ESP_LOGD(__FUNCTION__, "HTTP_EVENT_ON_FINISH");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGD(__FUNCTION__, "HTTP_EVENT_DISCONNECTED\n");
        break;
    }
    return ESP_OK;
}


void oldpullStation(void *pvParameter) {
    if (initSPISDCard()){
        esp_ip4_addr_t* ipInfo = (esp_ip4_addr_t*) pvParameter;
        esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
        memset(config,0,sizeof(esp_http_client_config_t));
        config->url=(char*)malloc(30);
        sprintf((char*)config->url,"http://" IPSTR "/trips",IP2STR(ipInfo));
        config->method=HTTP_METHOD_POST;
        config->timeout_ms = 9000;
        config->event_handler = http_tar_download_event_handler;
        config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
        config->max_redirection_count=0;
        config->port=80;
        ESP_LOGD(__FUNCTION__,"Getting %s",config->url);
        esp_http_client_handle_t client = esp_http_client_init(config);
        esp_err_t err;
        uint8_t retryCnt=0;
        while (((err = esp_http_client_perform(client)) == ESP_ERR_HTTP_CONNECT) && (retryCnt++<10))
        {
            ESP_LOGE(__FUNCTION__, "\nHTTP GET request failed: %s", esp_err_to_name(err));
            vTaskDelay(500/portTICK_RATE_MS);
        }
        ESP_LOGI(__FUNCTION__, "\nURL: %s\n\nHTTP GET Status = %d, content_length = %d\n",
                config->url,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        esp_http_client_cleanup(client);
        free((void*)config->url);
        free((void*)config);
        deinitSPISDCard();
    }
    vTaskDelete(NULL);
}

