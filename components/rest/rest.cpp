#include "rest.h"
#include "route.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static httpd_handle_t server = NULL;
static uint8_t* img=NULL;
static uint32_t totLen=0;
FILE* downloadedFile=NULL;
static char* jsonbuf=NULL;
static char* kmlFiles=NULL;
static uint32_t kmlFilesPos=0;
char* tarfname=(char*)malloc(255);

char* getPostField(const char* pname, const char* postData,char* dest) {
    char* param=strstr(postData,pname);
    if (param) {
        uint16_t plen=strlen(pname)+1;
        char* endPos=strstr(param,"&");
        if (endPos) {
            memcpy(dest,param+plen,endPos-param-plen);
            dest[endPos-param-plen]=0;
        } else {
            strcpy(dest,param+plen);
        }
    } else {
        return NULL;
    }
    return dest;
}

esp_err_t kmllist_event_handler(esp_http_client_event_t *evt)
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
            if (kmlFilesPos+evt->data_len<KML_BUFFER_SIZE ){
                ESP_LOGV(__FUNCTION__, "kmllist data len:%d", evt->data_len);
                memcpy(kmlFiles+kmlFilesPos,evt->data,evt->data_len);
                kmlFiles[kmlFilesPos+evt->data_len]=0;
                kmlFilesPos+=evt->data_len;
                ESP_LOGV(__FUNCTION__,"%s",kmlFiles);
            }
        } else {
            ESP_LOGW(__FUNCTION__,"Got data with no dest tarnull:%s strlen(tarfname):%d",downloadedFile == NULL?"NULL":"NOT NULL",strlen(tarfname));
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

esp_err_t kmldownload_event_handler(esp_http_client_event_t *evt)
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
        ESP_LOGD(__FUNCTION__, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        if (strcmp(evt->header_key,"filename") == 0) {
            downloadedFile = fopen(evt->header_value,"w",true);
            if (downloadedFile == NULL) {
                ESP_LOGE(__FUNCTION__,"Error whiilst opening %s",evt->header_value);
                return ESP_FAIL;
            }
            xEventGroupSetBits(eventGroup,DOWNLOAD_STARTED);
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if (downloadedFile != NULL) {
            fwrite(evt->data,1,evt->data_len,downloadedFile);
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
        if (downloadedFile != NULL) {
            fclose(downloadedFile);
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

bool routeHttpTraffic(const char *reference_uri, const char *uri_to_match, size_t match_upto){
    ESP_LOGV(__FUNCTION__,"routing ref:%s uri:%s",reference_uri,uri_to_match);
    if ((strlen(reference_uri)==1) && (reference_uri[0]=='*'))
        return true;

    size_t tLen=strlen(reference_uri);
    size_t sLen=strlen(uri_to_match);
    sLen=sLen>match_upto?match_upto:sLen;
    bool matches=true;
    uint8_t tc;
    uint8_t sc;
    int32_t sidx=0;
    int32_t tidx=0;
    bool eot=false;
    bool eos=false;

    while (matches && (sidx<sLen)) {
        tc=reference_uri[tidx];
        sc=uri_to_match[sidx];
        if (tidx >= 0){
            if (tc=='*') {
                ESP_LOGV(__FUNCTION__,"Match on wildcard");
                break;
            }
            if (!eot && !eos && (tc != sc)) {
                ESP_LOGV(__FUNCTION__,"Missmatch on tpos:%d spos:%d %c!=%c",tidx,sidx,tc,sc);
                matches=false;
                break;
            }
            if (tidx < tLen){
                tidx++;
            } else {
                eot=true;
                if (tc=='/') {
                    break;
                }
                ESP_LOGV(__FUNCTION__,"Missmatch slen being longer at tpos:%d tlen:%d spos:%d slen:%d",tidx,tLen,sidx,sLen);
                matches=false;
                break;
            }
            if (sidx < (sLen-1)){
                sidx++;
            } else {
                eos=true;
                if ((tLen == sLen) ||
                    ((sLen == (tLen-1)) && (reference_uri[tLen-1] == '/')) ||
                    ((sLen == (tLen-1)) && (reference_uri[tLen-1] == '*')) ) {
                    break;
                }
                ESP_LOGV(__FUNCTION__,"Missmatch slen being sorter at tpos:%d spos:%d",tidx,sidx);
                matches=false;
                break;
            }
        }
    }
    return matches;
}

void pullStation(void *pvParameter) {
    if (initSPISDCard()){
        esp_ip4_addr_t* ipInfo= (esp_ip4_addr_t*) pvParameter;
        kmlFiles = (char*)malloc(KML_BUFFER_SIZE);
        memset(kmlFiles,0,KML_BUFFER_SIZE);
        esp_http_client_config_t* config = (esp_http_client_config_t*)malloc(sizeof(esp_http_client_config_t));
        memset(config,0,sizeof(esp_http_client_config_t));
        config->url=(char*)malloc(255);
        sprintf((char*)config->url,"http://" IPSTR "/listtrips/csv",IP2STR(ipInfo));
        config->method=HTTP_METHOD_GET;
        config->timeout_ms = 9000;
        config->event_handler = kmllist_event_handler;
        config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
        config->max_redirection_count=0;
        config->port=80;
        ESP_LOGD(__FUNCTION__,"Getting %s",config->url);
        esp_http_client_handle_t client = esp_http_client_init(config);
        esp_err_t err;
        uint8_t retryCnt=0;
        kmlFilesPos=0;
        xEventGroupSetBits(eventGroup,GETTING_TRIP_LIST);
        while (((err = esp_http_client_perform(client)) == ESP_ERR_HTTP_CONNECT) && (retryCnt++<10))
        {
            ESP_LOGD(__FUNCTION__, "HTTP GET listtrip failed: %s", esp_err_to_name(err));
            vTaskDelay(500/portTICK_RATE_MS);
        }
        esp_http_client_cleanup(client);

        if (err != ESP_OK) {
            ESP_LOGD(__FUNCTION__,"Cannot get triplist. Probably not a tracker but a lurker");
            free((void*)config->url);
            free((void*)config);
            free(kmlFiles);
            deinitSPISDCard();
            vTaskDelete(NULL);
        }
        the_wifi_config* wc = getWifiConfig();

        if ((err == ESP_OK) && (strlen(kmlFiles) > 0)) {
            xEventGroupWaitBits(eventGroup,GETTING_TRIPS,pdFALSE,pdTRUE,portMAX_DELAY);
            ESP_LOGD(__FUNCTION__,"Parsing %d bytes of trips",kmlFilesPos);
            ESP_LOGV(__FUNCTION__,"%s",kmlFiles);
            char* bpos=kmlFiles;
            char* epos=strchr(bpos,10);
            char fname[255];
            if (epos == NULL) {
                if (strlen(bpos) > 0) {
                    epos=bpos+strlen(bpos);
                } else {
                    ESP_LOGW(__FUNCTION__,"Got weird or empty output from kml list:%s",kmlFiles);
                }
            }
            while (epos > bpos) {
                *epos=0;
                ESP_LOGI(__FUNCTION__,"Getting file %s",bpos);
                free((void*)config->url);
                memset(config,0,sizeof(esp_http_client_config_t));
                config->url=(char*)malloc(255);
                memset((void*)config->url,0,255);
                sprintf((char*)config->url,"http://" IPSTR "%s",IP2STR(ipInfo),bpos);
                config->method=HTTP_METHOD_GET;
                config->timeout_ms = 9000;
                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                config->max_redirection_count=0;
                config->port=80;
                config->event_handler = kmldownload_event_handler;
                client = esp_http_client_init(config);
                esp_http_client_set_header(client,"movetosent","yes");
                if ((err = esp_http_client_perform(client)) == ESP_OK){
                    ESP_LOGD(__FUNCTION__,"Waiting on download");
                    if (xEventGroupWaitBits(eventGroup,DOWNLOAD_FINISHED,pdFALSE,pdTRUE,2000/portTICK_RATE_MS)){
                        if (downloadedFile!=NULL){
                            ESP_LOGD(__FUNCTION__,"%s downloaded",bpos);
                        } else {
                            ESP_LOGE(__FUNCTION__,"Download failed %s",bpos);
                            err=ESP_FAIL;
                        }
                        downloadedFile=NULL;
                    } else {
                        ESP_LOGE(__FUNCTION__,"Timeout in waiting for return");
                    }
                } else {
                    ESP_LOGE(__FUNCTION__, "KML GET request failed: %s", esp_err_to_name(err));
                }

                esp_http_client_cleanup(client);
                memset(config,0,sizeof(esp_http_client_config_t));

                bpos=epos+1;
                if (*bpos != 0){
                    epos=strchr(bpos,10);
                    if ((epos == NULL) && (*bpos != 0)) { // last entry if not line fead
                        epos = bpos+strlen(bpos);
                    }
                } else {
                    bpos=NULL;
                    epos=NULL;
                }
            }

        }

        free((void*)config->url);
        memset(config,0,sizeof(esp_http_client_config_t));
        config->url=(char*)malloc(255);
        memset((void*)config->url,0,255);
        sprintf((char*)config->url,"http://" IPSTR "/ota/getmd5",IP2STR(ipInfo));
        config->method=HTTP_METHOD_POST;
        config->timeout_ms = 9000;
        config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
        config->max_redirection_count=0;
        config->port=80;
        client = esp_http_client_init(config);
        char dmd5[37];
        esp_err_t ret=ESP_OK;
        ESP_LOGD(__FUNCTION__,"Checking Version");
        if ((ret = esp_http_client_perform(client)) == ESP_OK){
            uint32_t len=0;
            if ((len=esp_http_client_read(client,dmd5,36)) >= 0) {
                ESP_LOGD(__FUNCTION__,"ot back %d char of md5",len);
                FILE* fw = NULL;
                if ((fw = fopen("/sdcard/firmware/current.bin.md5","r",true)) != NULL) {
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
                            if ((fw = fopen("/sdcard/firmware/current.bin","r",true)) != NULL) {
                                img = (uint8_t*)heap_caps_malloc(1500000,MALLOC_CAP_SPIRAM);
                                uint32_t ilen = fread((void*)img,1,1500000,fw);
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
                                        if (esp_http_client_write(client,(char*)img,ilen) == ilen){
                                            esp_http_client_fetch_headers(client);
                                            if ((len = esp_http_client_get_status_code(client)) != 200) {
                                                ESP_LOGE(__FUNCTION__,"Status code:%d",len);
                                            } else {
                                                if ((len=esp_http_client_read(client,dmd5,37)) > 0) {
                                                    if (strcmp(dmd5,"Flashing")==0) {
                                                        ESP_LOGI(__FUNCTION__,"Station will be updated:%s",dmd5);
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
                                free(img);
                            } else {
                                ESP_LOGE(__FUNCTION__,"Failed to read firmware");
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
            ESP_LOGE(__FUNCTION__, "\nKML GET request failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
        memset(config,0,sizeof(esp_http_client_config_t));

        memset(kmlFiles,0,KML_BUFFER_SIZE);
        free((void*)config->url);
        memset(config,0,sizeof(esp_http_client_config_t));
        config->url=(char*)malloc(255);
        memset((void*)config->url,0,255);
        sprintf((char*)config->url,"http://" IPSTR "/listtrips/log",IP2STR(ipInfo));
        config->method=HTTP_METHOD_GET;
        config->timeout_ms = 9000;
        config->event_handler = kmllist_event_handler;
        config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
        config->max_redirection_count=0;
        config->port=80;
        ESP_LOGD(__FUNCTION__,"Getting %s",config->url);
        client = esp_http_client_init(config);
        kmlFilesPos=0;
        xEventGroupSetBits(eventGroup,GETTING_TRIP_LIST);
        while (((err = esp_http_client_perform(client)) == ESP_ERR_HTTP_CONNECT) && (retryCnt++<10))
        {
            ESP_LOGD(__FUNCTION__, "HTTP GET listtrip failed: %s", esp_err_to_name(err));
            vTaskDelay(500/portTICK_RATE_MS);
        }
        esp_http_client_cleanup(client);

        if (err != ESP_OK) {
            ESP_LOGD(__FUNCTION__,"Cannot get loglist. Probably not a tracker but a lurker");
            free((void*)config->url);
            free((void*)config);
            free(kmlFiles);
            deinitSPISDCard();
            vTaskDelete(NULL);
        }

        if ((err == ESP_OK) && (strlen(kmlFiles) > 0)) {
            xEventGroupWaitBits(eventGroup,GETTING_TRIPS,pdFALSE,pdTRUE,portMAX_DELAY);
            ESP_LOGD(__FUNCTION__,"Parsing %d bytes of trips",kmlFilesPos);
            ESP_LOGV(__FUNCTION__,"%s",kmlFiles);
            char* bpos=kmlFiles;
            char* epos=strchr(bpos,10);
            char fname[255];
            if (epos == NULL) {
                if (strlen(bpos) > 0) {
                    epos=bpos+strlen(bpos);
                } else {
                    ESP_LOGW(__FUNCTION__,"Got weird or empty output from kml list:%s",kmlFiles);
                }
            }
            while (epos > bpos) {
                *epos=0;
                ESP_LOGI(__FUNCTION__,"Getting file %s",bpos);
                free((void*)config->url);
                memset(config,0,sizeof(esp_http_client_config_t));
                config->url=(char*)malloc(255);
                memset((void*)config->url,0,255);
                sprintf((char*)config->url,"http://" IPSTR "%s",IP2STR(ipInfo),bpos);
                config->method=HTTP_METHOD_GET;
                config->timeout_ms = 9000;
                config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
                config->max_redirection_count=0;
                config->port=80;
                config->event_handler = kmldownload_event_handler;
                client = esp_http_client_init(config);
                esp_http_client_set_header(client,"movetosent","yes");
                if ((err = esp_http_client_perform(client)) == ESP_OK){
                    ESP_LOGD(__FUNCTION__,"Waiting on download");
                    if (xEventGroupWaitBits(eventGroup,DOWNLOAD_FINISHED,pdFALSE,pdTRUE,2000/portTICK_RATE_MS)){
                        if (downloadedFile!=NULL){
                            ESP_LOGD(__FUNCTION__,"%s downloaded",bpos);
                        } else {
                            ESP_LOGE(__FUNCTION__,"Download failed %s",bpos);
                            err=ESP_FAIL;
                        }
                        downloadedFile=NULL;
                    } else {
                        ESP_LOGE(__FUNCTION__,"Timeout in waiting for return");
                    }
                } else {
                    ESP_LOGE(__FUNCTION__, "LOG GET request failed: %s", esp_err_to_name(err));
                }

                esp_http_client_cleanup(client);
                memset(config,0,sizeof(esp_http_client_config_t));

                bpos=epos+1;
                if (*bpos != 0){
                    epos=strchr(bpos,10);
                    if ((epos == NULL) && (*bpos != 0)) { // last entry if not line fead
                        epos = bpos+strlen(bpos);
                    }
                } else {
                    bpos=NULL;
                    epos=NULL;
                }
            }

        }
        if (kmlFiles){
            free(kmlFiles);
            kmlFiles=NULL;
        }

        free((void*)config->url);
        memset(config,0,sizeof(esp_http_client_config_t));
        config->url=(char*)malloc(255);
        memset((void*)config->url,0,255);
        sprintf((char*)config->url,"http://" IPSTR "/rest/status/wifi",IP2STR(ipInfo));
        config->method=HTTP_METHOD_POST;
        config->timeout_ms = 9000;
        config->buffer_size = HTTP_RECEIVE_BUFFER_SIZE;
        config->max_redirection_count=0;
        config->port=80;
        client = esp_http_client_init(config);
        char* postData="{\"enabled\":\"no\"}";
        if ((ret = esp_http_client_open(client,strlen(postData))) == ESP_OK){
            if ((err = esp_http_client_write(client,postData,strlen(postData))) != ESP_ERR_HTTP_CONNECT)
            {
                ESP_LOGD(__FUNCTION__, "\nTurn off wifi faile, but that is to be expected: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE(__FUNCTION__, "\nKML GET request failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
        free((void*)config->url);
        free((void*)config);
        free((void*)kmlFiles);
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
            if ((downloadedFile=fopen(tarfname,"w"))!=NULL){
                ESP_LOGD(__FUNCTION__, "%s opened", tarfname);
            } else {
                ESP_LOGE(__FUNCTION__,"Cannot read %s",tarfname);
            }
        }
        break;
    case HTTP_EVENT_ON_DATA:
        if ((downloadedFile != NULL) && (strlen(tarfname)>0)) {
            fwrite(evt->data,sizeof(uint8_t),evt->data_len,downloadedFile);
            ESP_LOGV(__FUNCTION__, "tar data len:%d\n", evt->data_len);
        } else {
            ESP_LOGW(__FUNCTION__,"Got data with no dest tarnull:%s strlen(tarfname):%d",downloadedFile == NULL?"NULL":"NOT NULL",strlen(tarfname));
        }
        break;
    case HTTP_EVENT_ON_FINISH:
        fclose(downloadedFile);
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
        config->method=HTTP_METHOD_GET;
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

void restSallyForth(void *pvParameter) {
    assert(pvParameter);
    cfg = (the_wifi_config*)pvParameter;
    xEventGroupWaitBits(cfg->s_wifi_eg,WIFI_CONNECTED_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = routeHttpTraffic;
    eventGroup = xEventGroupCreate();
    ESP_LOGI(__FUNCTION__, "Starting server on port %d", config.server_port);
    xEventGroupClearBits(eventGroup,HTTP_SERVING);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(__FUNCTION__, "Registering URI handlers");
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &otaUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &tripUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &listTripsUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &getRestUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &postReqUri));
        ESP_ERROR_CHECK(httpd_register_uri_handler(server, &rootUri));
        xEventGroupSetBits(eventGroup,HTTP_SERVING);
    } else {
        ESP_LOGI(__FUNCTION__, "Error starting server!");
    }

    vTaskDelete( NULL );
}
