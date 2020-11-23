#include "rest.h"
#include "route.h"
#include "math.h"
#include "../../main/logs.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define F_BUF_SIZE 8192
#define HTTP_BUF_SIZE 8192
#define HTTP_CHUNK_SIZE 8192
#define HTTP_RECEIVE_BUFFER_SIZE 1024
#define JSON_BUFFER_SIZE 8192
#define KML_BUFFER_SIZE 204600

static httpd_handle_t server = NULL;
static uint8_t* img=NULL;
static uint32_t totLen=0;

//static char* jsonbuf=NULL;
char* kmlFileName=(char*)malloc(255);

void flashTheThing(void* param){
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running) {
        ESP_LOGW(__FUNCTION__, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                configured->address, running->address);
        ESP_LOGW(__FUNCTION__, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGI(__FUNCTION__, "Running partition type %d subtype %d (offset 0x%08x)",
            running->type, running->subtype, running->address);

    while (((update_partition = esp_ota_get_next_update_partition(update_partition))->address==0x00010000) || (update_partition->address == configured->address)){
        assert(update_partition != NULL);
        ESP_LOGD(__FUNCTION__, "Skipping partition subtype %d at offset 0x%x",
                update_partition->subtype, update_partition->address);
    }
    assert(update_partition != NULL);
    ESP_LOGI(__FUNCTION__, "Writing to partition subtype %d at offset 0x%x",
            update_partition->subtype, update_partition->address);

    if (initSPISDCard()) {
        FILE* fw = NULL;
        if ((fw = fopen("/sdcard/firmware/current.bin","w",true)) != NULL) {
            if (fwrite((void*)img,1,totLen,fw) == totLen) {
                ESP_LOGI(__FUNCTION__,"Firmware Updated");
                fclose(fw);
                if ((fw = fopen("/sdcard/firmware/current.bin.md5","w",true)) != NULL) {
                    ESP_LOGI(__FUNCTION__,"Firmware written");
                    uint8_t md5[16];
                    MD5Context md5_context;
                    MD5Init(&md5_context);
                    MD5Update(&md5_context, img, totLen);
                    MD5Final(md5, &md5_context);
                    char ccmd5[36];
                    for (uint8_t i = 0; i < 16; ++i) {
                        sprintf((char*)&ccmd5[i * 2], "%02x", (unsigned int)md5[i]);
                    }
                    fwrite((void*)ccmd5,1,sizeof(ccmd5),fw);
                    fclose(fw);
                } else {
                    ESP_LOGE(__FUNCTION__,"Failed in opeing /firmware/current.bin.md5");
                }
            } else {
                ESP_LOGE(__FUNCTION__,"Firmware not backed-up");
            }
        } else {
            ESP_LOGE(__FUNCTION__,"Failed to open /firmware/current.bin");
        }
        deinitSPISDCard();
    } else {
        ESP_LOGE(__FUNCTION__,"Failed to mount the fucking sd card");
    }

    esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err == ESP_OK) {
        ESP_LOGI(__FUNCTION__, "esp_ota_begin succeeded %d ",totLen);
        err = esp_ota_write( update_handle, (const void *)img, totLen);
        if (err == ESP_OK) {
            ESP_LOGI(__FUNCTION__, "esp_ota_write succeeded");
            err = esp_ota_end(update_handle);
            if (err == ESP_OK) {
                err = esp_ota_set_boot_partition(update_partition);
                if (err == ESP_OK) {
                    ESP_LOGI(__FUNCTION__, "esp_ota_set_boot_partition succeeded");
                } else {
                    ESP_LOGE(__FUNCTION__, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
                }
            } else {
                ESP_LOGE(__FUNCTION__, "esp_ota_write failed");
            }
        } else {
            ESP_LOGE(__FUNCTION__, "esp_ota_write failed");
        }
    } else {
        ESP_LOGE(__FUNCTION__, "esp_ota_begin failed (%s)", esp_err_to_name(err));
    }
    free(img);
    totLen=0;
    esp_restart();
}

esp_err_t ota_handler(httpd_req_t *req)
{
    if (indexOf(req->uri,"/ota/flash")==req->uri){
        char*  buf;
        size_t buf_len;
        ESP_LOGD(__FUNCTION__, "OTA REQUEST!!!!!");

        buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
        if (buf_len > 1) {
            buf = (char*)malloc(buf_len);
            /* Copy null terminated value string into buffer */
            if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
                ESP_LOGD(__FUNCTION__, "Found header => Host: %s", buf);
            }
            free(buf);
        }

        buf_len = httpd_req_get_url_query_len(req) + 1;
        char md5[36];
        md5[0]=0;

        if (buf_len > 1) {
            buf = (char*)malloc(buf_len);
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                ESP_LOGD(__FUNCTION__, "Found URL query => %s", buf);
                char param[33];
                if (httpd_query_key_value(buf, "md5", param, sizeof(param)) == ESP_OK) {
                    strcpy(md5,param);
                    ESP_LOGI(__FUNCTION__, "Found URL query parameter => md5=%s", param);
                }
                if (httpd_query_key_value(buf, "len", param, sizeof(param)) == ESP_OK) {
                    totLen=atoi(param);
                    ESP_LOGI(__FUNCTION__, "Found URL query parameter => len=%s", param);
                }
            }
            free(buf);
        }

        if (totLen && md5[0]) {
            ESP_LOGI(__FUNCTION__, "RAM:%d...", esp_get_free_heap_size());
            img = (uint8_t*)heap_caps_malloc(totLen,MALLOC_CAP_SPIRAM);
            memset(img,0,totLen);
            ESP_LOGI(__FUNCTION__, "RAM:%d...", esp_get_free_heap_size());

            int len=0;
            uint8_t cmd5[16];
            uint8_t ccmd5[36];
            MD5Context md5_context;
            MD5Init(&md5_context);
            uint32_t curLen=0;

            do {
                len=httpd_req_recv(req,(char*)img+curLen,MESSAGE_BUFFER_SIZE);
                if (len < 0) {
                    ESP_LOGE(__FUNCTION__, "Error occurred during receiving: errno %d", errno);
                } else if (len == 0) {
                    ESP_LOGW(__FUNCTION__, "Connection closed...");
                } else {
                    MD5Update(&md5_context, img+curLen, len);
                    curLen+=len;
                }
            } while (len>0);
            ESP_LOGD(__FUNCTION__,"Total: %d/%d",totLen,curLen);
            MD5Final(cmd5, &md5_context);

            for (uint8_t i = 0; i < 16; ++i) {
                sprintf((char*)&ccmd5[i * 2], "%02x", (unsigned int)cmd5[i]);
            }

            MD5Init(&md5_context);
            MD5Update(&md5_context, img, totLen);
            MD5Final(cmd5, &md5_context);
            for (uint8_t i = 0; i < 16; ++i) {
                sprintf((char*)&ccmd5[i * 2], "%02x", (unsigned int)cmd5[i]);
            }

            ESP_LOGD(__FUNCTION__,"md5:(%s)%dvs%d",ccmd5,totLen,curLen);
            ESP_LOGI(__FUNCTION__, "RAM:%d", esp_get_free_heap_size());

            if (strcmp((char*)ccmd5,(char*)md5) == 0) {
                httpd_resp_send(req, "Flashing", 9);
                vTaskDelay(pdMS_TO_TICKS(500));
                xTaskCreate(&flashTheThing,"flashTheThing",4096, NULL, 5, NULL);
                stopGps();
                esp_wifi_stop();
                httpd_stop(&server);
            } else {
                httpd_resp_send(req, "Bad Checksum", 12);
            }
        } else {
            httpd_resp_send(req, "Not OK", 6);
        }
        return ESP_OK;
    } else if (indexOf(req->uri,"/ota/getmd5")==req->uri){
        if (initSPISDCard()){
            FILE* fw = NULL;
            if ((fw = fopen("/sdcard/firmware/current.bin.md5","r",true)) != NULL) {
                char ccmd5[36];
                uint32_t len=0;
                if ((len=fread((void*)ccmd5,1,36,fw)) == 36) {
                    //ccmd5[0]='z';
                    httpd_resp_send(req,ccmd5,36);
                    fclose(fw);
                    return ESP_OK;
                } else {
                    ESP_LOGE(__FUNCTION__,"Error with weird md5 len %d", len);
                }
                fclose(fw);
            } else {
                ESP_LOGE(__FUNCTION__,"Failed in opeing md5");
            }
            deinitSPISDCard();
        }
    }
    httpd_resp_send(req,"BADMD5",6);
    return ESP_FAIL;
}

esp_err_t renderFolder(httpd_req_t *req,char* path){
    FF_DIR theFolder;
    FILINFO fi;
    uint32_t dirCount=0;
    uint32_t fileCount=0;
    uint32_t totSize=0;
    uint32_t len=0;
    char* tmpstr = (char*)malloc(1024);
    memset(tmpstr,0,1024);
    char* startPos=indexOf((const char*)file_table_html_start,"</tbody>");
    httpd_resp_send_chunk(req, (const char *)file_table_html_start, ((const uint8_t *)startPos-file_table_html_start));
    if (initSPISDCard())
    {
        char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
        if (f_opendir(&theFolder, path) == FR_OK) {
            if (strcmp(path,"/") != 0){
                strcpy(tmpstr,path);
                char* tmpstr2=strrchr(tmpstr,'/');
                if (tmpstr2 != NULL){
                    *(tmpstr+(tmpstr2-tmpstr))=0;
                    len=sprintf(jsonbuf,"<tr><td class='folder'></td><td><a href='%s/'>..</a></td><td></td></tr>\n",
                                    tmpstr
                                    );
                    httpd_resp_send_chunk(req, jsonbuf, len);
                }
            }
            ESP_LOGV(__FUNCTION__, "Reading Folders");
            while (f_readdir(&theFolder, &fi) == FR_OK)
            {
                if (strlen(fi.fname) == 0)
                {
                    break;
                }
                if (fi.fattrib & AM_DIR)
                    dirCount++;
                else
                    continue;
                ESP_LOGV(__FUNCTION__, "%s - %d", fi.fname, fi.fsize);

                len=sprintf(jsonbuf,"<tr><td class='%s'></td><td><a href='%s%s/'>%s</a></td><td></td></tr>\n",
                                fi.fattrib & AM_DIR ? "folder":"file",
                                req->uri,
                                fi.fname,
                                fi.fname
                                );
                httpd_resp_send_chunk(req, jsonbuf, len);
            }
            f_closedir(&theFolder);
            f_opendir(&theFolder, path);
            while (f_readdir(&theFolder, &fi) == FR_OK)
            {
                if (strlen(fi.fname) == 0)
                {
                    break;;
                }
                ESP_LOGV(__FUNCTION__, "%s - %d", fi.fname, fi.fsize);
                if (fi.fattrib & AM_DIR) {
                    continue;
                } else {
                    fileCount++;
                    totSize+=fi.fsize;
                }
                ESP_LOGV(__FUNCTION__, "%s - %d", fi.fname, fi.fsize);

                len=sprintf(jsonbuf,"<tr><td class='%s'></td><td><a href='%s%s'>%s</a></td><td>%d</td></tr>\n",
                                fi.fattrib & AM_DIR ? "folder":"file",
                                req->uri,
                                fi.fname,
                                fi.fname,
                                fi.fsize
                                );
                httpd_resp_send_chunk(req, jsonbuf, len);
            }
            f_closedir(&theFolder);
            free(jsonbuf);
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot read folder %s",path);
            deinitSPISDCard();
            return false;
        }
    } else {
        ESP_LOGD(__FUNCTION__,"Cannot mount the fucking sd card");
    }
    sprintf(tmpstr,startPos,totSize,path);
    httpd_resp_send_chunk(req, tmpstr, strlen(tmpstr) );
    free(tmpstr);
    deinitSPISDCard();

    return ESP_OK;
}


esp_err_t findSdCardFiles(httpd_req_t *req,char* path,const char* ext,bool recursive, char* res, uint32_t resLen){
    if ((path == NULL) || (strlen(path)==0)) {
        return ESP_FAIL;
    }
    esp_err_t ret=ESP_OK;
    uint32_t sLen=strlen(res);
    ESP_LOGV(__FUNCTION__,"Parsing %s",path);
    char* theFolders = (char*)malloc(1024);
    memset(theFolders,0,1024);
    uint32_t fpos=0;
    uint32_t fcnt=0;
    uint32_t dcnt=0;

    if (initSPISDCard()){
        FF_DIR theFolder;
        FILINFO* fi = (FILINFO*)malloc(sizeof(FILINFO));
        if (f_opendir(&theFolder, path) == FR_OK){
            while (f_readdir(&theFolder, fi) == FR_OK)
            {
                if (strlen(fi->fname) == 0)
                {
                    break;
                }
                if (recursive && (fi->fattrib & AM_DIR)){
                    dcnt++;
                    if (strcmp(path,"/")==0) {
                        sprintf(kmlFileName,"/%s",fi->fname);
                    } else {
                        sprintf(kmlFileName,"%s/%s",path,fi->fname);
                    }
                    fpos+=sprintf(theFolders+fpos,"%s",kmlFileName)+1;

                    ESP_LOGV(__FUNCTION__,"%s currently has %d files and %d folders subfolder len:%d. Adding dir %s", path,fcnt,dcnt,fpos,kmlFileName);
                } else if (endsWith(fi->fname,ext)){
                    fcnt++;
                    ESP_LOGV(__FUNCTION__,"%s currently has %d files and %d folders subfolder len:%d. Adding file %s", path,fcnt,dcnt,fpos,fi->fname);
                    if (sLen > (resLen-100)){
                        ESP_LOGD(__FUNCTION__,"Buffer Overflow, flushing");
                        if ((ret=httpd_resp_send_chunk(req,res,sLen)) != ESP_OK){
                            ESP_LOGE(__FUNCTION__,"Error sending chunk %s sLenL%d, actuallen:%d",esp_err_to_name(ret),sLen,strlen(res));
                            break;
                        }
                        memset(res,0,resLen);
                        sLen=0;
                    }

                    if (strcmp(path,"/") == 0)
                        sLen+=sprintf(res+sLen,"\"/%s\",",fi->fname);
                    else
                        sLen+=sprintf(res+sLen,"\"%s/%s\",",path,fi->fname);
                }
            }
            f_closedir(&theFolder);
            ESP_LOGV(__FUNCTION__,"%s has %d files and %d folders subfolder len:%d", path,fcnt,dcnt,fpos);
            uint32_t ctpos=0;
            while (dcnt-->0) {
                ESP_LOGV(__FUNCTION__,"%d-%s: Getting sub-folder(%d) %s",dcnt, path,ctpos,theFolders+ctpos);
                if ((ret=findSdCardFiles(req, theFolders+ctpos,ext,recursive,res,resLen)) != ESP_OK) {
                    ESP_LOGW(__FUNCTION__,"Error invoking getSdFiles for %s", kmlFileName);
                }
                ctpos+=strlen(theFolders)+1;
            }
        } else {
            ESP_LOGW(__FUNCTION__,"Error opening %s", path);
            ret=ESP_FAIL;
        }
        free(fi);
    } else {
        ESP_LOGE(__FUNCTION__,"Cannot mount the fucking sd card");
        ret=ESP_FAIL;
    }
    free(theFolders);
    deinitSPISDCard();
    return ret;
}

esp_err_t list_trips_handler(httpd_req_t *req)
{
    char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
    memset(jsonbuf,0,JSON_BUFFER_SIZE);
    *jsonbuf='[';
    xEventGroupClearBits(getWifiConfig()->s_wifi_eg,WIFI_CLIENT_DONE);
    ESP_LOGD(__FUNCTION__,"Getting %s url:%s",req->uri+11,req->uri);
    if (endsWith(req->uri,"kml")){
        if (findSdCardFiles(req,"/kml","kml",true,jsonbuf,JSON_BUFFER_SIZE-1) != ESP_OK){
            ESP_LOGE(__FUNCTION__,"Error wilst sending kml list");
            free(jsonbuf);
            return httpd_resp_send_500(req);
        }
    } else if (endsWith(req->uri,"csv")){
        if (findSdCardFiles(req,"/","csv",false,jsonbuf,JSON_BUFFER_SIZE) != ESP_OK){
            ESP_LOGE(__FUNCTION__,"Error wilst sending kml list");
            free(jsonbuf);
            return httpd_resp_send_500(req);
        }
    } else if (endsWith(req->uri,"log")){
        if (findSdCardFiles(req,"/logs","log",false,jsonbuf,JSON_BUFFER_SIZE) != ESP_OK){
            ESP_LOGE(__FUNCTION__,"Error wilst sending kml list");
            free(jsonbuf);
            return httpd_resp_send_500(req);
        }
    }
    if (strlen(jsonbuf) > 1 )
        *(jsonbuf+strlen(jsonbuf)-1)=']';
    else
    {
        sprintf(jsonbuf,"%s","[]");
    }
    
    httpd_resp_send_chunk(req,jsonbuf,strlen(jsonbuf));
    ESP_LOGD(__FUNCTION__,"Sent final chunck of %d",strlen(jsonbuf));
    free(jsonbuf);
    return httpd_resp_send_chunk(req,NULL,0);
}

esp_err_t config_get_handler(httpd_req_t *req)
{
    FF_DIR theFolder;
    char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
    uint32_t len=sprintf(jsonbuf,"{\"wifi\":{\"type\":%d,\"name\":\"%s\",\"password\":\"%s\"},",
                                cfg->wifi_mode,
                                cfg->wname,
                                cfg->wpdw
                                );

    httpd_resp_send_chunk(req, jsonbuf, len);
    if (initSPISDCard())
    {
        if (f_opendir(&theFolder, "/") == FR_OK) {
            uint32_t len=sprintf(jsonbuf,"\"files\":[\n");
            httpd_resp_send_chunk(req, jsonbuf, len);
            FF_DIR theFolder;
            for (int idx=0; idx < 3; idx++) {
                ESP_LOGD(__FUNCTION__, "reading sdcard files in %s", mounts[idx]);
                FILINFO fi;
                uint32_t fileCount=0;
                uint32_t dirCount=0;

                if (f_opendir(&theFolder, mounts[idx]) == FR_OK)
                {
                    while (f_readdir(&theFolder, &fi) == FR_OK)
                    {
                        if (strlen(fi.fname) == 0)
                        {
                            break;
                        }
                        ESP_LOGD(__FUNCTION__, "%s - %d", fi.fname, fi.fsize);
                        if (fi.fattrib & AM_DIR)
                            dirCount++;
                        else
                            fileCount++;
                    }
                    f_closedir(&theFolder);
                    uint32_t len=sprintf(jsonbuf,"{\"path\":\"%s\",\
                                        \"fileCount\":%d,\
                                        \"folderCount\":\"%d\"}%s",
                                    mounts[idx],
                                    fileCount,
                                    dirCount,
                                    idx == 2 ? "":","
                                    );
                    httpd_resp_send_chunk(req, jsonbuf, len);
            } else {
                ESP_LOGE(__FUNCTION__,"Cannot open %s",mounts[idx]);
            }
            len=sprintf(jsonbuf,"]\n");
            httpd_resp_send_chunk(req, jsonbuf, len);
            }
        }
        deinitSPISDCard();
    } else {
        ESP_LOGD(__FUNCTION__,"Cannot mount the fucking sd card");
    }
    httpd_resp_send_chunk(req, "}", 2);
    httpd_resp_send_chunk(req, NULL, 0);
    httpd_resp_set_status(req,HTTPD_200);
    free(jsonbuf);

    return ESP_OK;
}

esp_err_t rest_handler(httpd_req_t *req)
{
    esp_err_t ret = ESP_OK;
    cJSON* json=NULL;
    cJSON* jsonItem = NULL;
    app_config_t* appcfg=NULL;
    app_state_t* appstate=NULL;
    poiConfig_t* pc=NULL;
    the_wifi_config* wcfg = NULL;
    char* pois = NULL;
    int64_t upTime=0;
    int64_t sleepTime=0;
    char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
    uint32_t idx = 0;
    uint32_t jp,jp2;
    if (strncmp(req->uri,"/rest/config",17)==0) {
        switch (req->method) {
            case HTTP_GET:
                jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
                httpd_resp_set_type(req, "application/json");
                appcfg = getAppConfig();
                wcfg = getWifiConfig();
                appstate = getAppState();
                pois = jsonbuf+sprintf(jsonbuf,"{\"type\":\"%s\",\"sdcard\":{\"state\":\"%s\",\"MisoPin\":%d,\"MosiPin\":%d,\"ClkPin\":%d,\"CsPin\":%d},\"gps\":{\"state\":\"%s\",\"txPin\":%d,\"rxPin\":%d,\"enPin\":%d,\"pois\":[",
                                            appcfg->purpose == app_config_t::purpose_t::PULLER?"AP":"Station",
                                            appstate->sdCard&item_state_t::ERROR?"invalid":appstate->sdCard&item_state_t::INACTIVE?"inactive":"valid",
                                            appcfg->sdcard_config.MisoPin,
                                            appcfg->sdcard_config.MosiPin,
                                            appcfg->sdcard_config.ClkPin,
                                            appcfg->sdcard_config.Cspin,
                                            appstate->gps&item_state_t::ERROR?"invalid":appstate->gps&item_state_t::INACTIVE?"inactive":"valid",
                                            appcfg->gps_config.txPin,
                                            appcfg->gps_config.rxPin,
                                            appcfg->gps_config.enPin
                                    );
                pc = appcfg->pois;
                while ((pc->minDistance < 2000) && (pc->minDistance > 0)) {
                    pois+=sprintf(pois,"{\"lat\":%f,\"lng\":%f,\"distance\":%d},",pc->lat,pc->lng,pc->minDistance);
                    pc+=sizeof(poiConfig_t);
                }
                if (pc != appcfg->pois) {
                    sprintf(pois-1,"%s","]}}");
                } else {
                    sprintf(pois,"%s","]}}");
                }

                ret=httpd_resp_send(req, jsonbuf, strlen(jsonbuf));
                free(jsonbuf);
                break;
            default:
                break;
        }
    } else if (strncmp(req->uri,"/rest/status/wifi",17)==0) {
            appcfg = getAppConfig();
            wcfg = getWifiConfig();
        switch (req->method) {
            case HTTP_GET:
                jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
                jp = sprintf(jsonbuf,"{\"type\":\"%s\",\"enabled\":\"%s\",\"connected\":\"%s\",\"scanning\":\"%s\",\"ip\":\"" IPSTR "\",\"clients\":",
                                            appcfg->purpose == app_config_t::purpose_t::PULLER?"AP":"Station",
                                            xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_UP_BIT?"yes":"no",
                                            xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_CONNECTED_BIT?"yes":"no",
                                            xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_SCANING_BIT?"yes":"no",
                                            IP2STR(&ipInfo.ip)
                                    );
                jp2=getClientsJson(jsonbuf+jp);
                sprintf(jsonbuf+jp+jp2,"}");
                httpd_resp_set_type(req, "application/json");
                ret=httpd_resp_send(req, jsonbuf, strlen(jsonbuf));
                free(jsonbuf);
                break;
            case HTTP_POST:
                jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
                memset(jsonbuf,0,JSON_BUFFER_SIZE);
                ret=httpd_req_recv(req,jsonbuf,JSON_BUFFER_SIZE);
                ESP_LOGD(__FUNCTION__,"Got %s",jsonbuf);
                json = cJSON_Parse(jsonbuf);
                if (json != NULL) {
                    jsonItem = cJSON_GetObjectItemCaseSensitive(json,"enabled");
                    if (jsonItem && (strcmp(jsonItem->valuestring,"no")==0)) {
                        if (appcfg->pois->minDistance > 0){
                            xEventGroupSetBits(wcfg->s_wifi_eg,WIFI_CLIENT_DONE);
                            xEventGroupSetBits(*getAppEG(),app_bits_t::TRIPS_SYNCED);
                            ESP_LOGD(__FUNCTION__,"All done wif wifi %d",xEventGroupGetBits(wcfg->s_wifi_eg));
                            httpd_resp_send(req,"OK",2);
                            vTaskDelay(pdMS_TO_TICKS(500));
                            wifiStop(NULL);
                        }
                    }
                } else {
                    ESP_LOGE(__FUNCTION__,"Error whilst parsing json");
                    ret= httpd_resp_send_500(req);
                }
                cJSON_Delete(json);
                free(jsonbuf);
                break;
            default:
                ret= httpd_resp_send_500(req);
                break;
        }
    } else if (strncmp(req->uri,"/rest/status/system",17)==0) {
        switch (req->method) {
            case HTTP_GET:
                jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
                httpd_resp_set_type(req, "application/json");
                wcfg = getWifiConfig();
                upTime=getUpTime()/1000000;
                sleepTime=getSleepTime();
                ret=httpd_resp_send(req, jsonbuf, sprintf(jsonbuf,"{\"wifi\":{\"type\":\"%s\",\"ip\":\"" IPSTR "\",\"enabled\":%s,\"connected\":%s,\"scanning\":%s},\"uptime\":\"%02d:%02d:%02d\",\"sleeptime\":\"%02d:%02d:%02d\"}",
                                            wcfg->wifi_mode == wifi_mode_t::WIFI_MODE_AP?"AP":"Station",
                                            IP2STR(&ipInfo.ip),
                                            xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_UP_BIT?"true":"false",
                                            xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_CONNECTED_BIT?"true":"false",
                                            xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_SCANING_BIT?"true":"false",
                                            (uint32_t)floor(upTime/3600),
                                            (uint32_t)floor((upTime%3600)/60),
                                            (uint32_t)(upTime%60),
                                            (uint32_t)floor(sleepTime/3600),
                                            (uint32_t)floor((sleepTime%3600)/60),
                                            (uint32_t)(sleepTime%60)
                                    ));
                free(jsonbuf);
                break;
            case HTTP_POST:
                jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
                memset(jsonbuf,0,JSON_BUFFER_SIZE);
                httpd_req_recv(req,jsonbuf,JSON_BUFFER_SIZE);
                char pval[100];
                char pname[100];
                appcfg = getAppConfig();
                appcfg->sdcard_config.MisoPin=(gpio_num_t)atoi(getPostField("MisoPin",jsonbuf,pval));
                appcfg->sdcard_config.MosiPin=(gpio_num_t)atoi(getPostField("MosiPin",jsonbuf,pval));
                appcfg->sdcard_config.ClkPin=(gpio_num_t)atoi(getPostField("ClkPin",jsonbuf,pval));
                appcfg->sdcard_config.Cspin=(gpio_num_t)atoi(getPostField("CsPin",jsonbuf,pval));

                if (appcfg->purpose == app_config_t::purpose_t::TRACKER) {
                    appcfg->gps_config.enPin=(gpio_num_t)atoi(getPostField("enPin",jsonbuf,pval));
                    appcfg->gps_config.txPin=(gpio_num_t)atoi(getPostField("txPin",jsonbuf,pval));
                    appcfg->gps_config.rxPin=(gpio_num_t)atoi(getPostField("rxPin",jsonbuf,pval));
                    idx=0;
                    sprintf(pname,"syncpoints[%d].lat",idx);
                    pc = appcfg->pois;
                    while (getPostField(pname,jsonbuf,pval)) {
                        pc->lat=atof(pval);
                        sprintf(pname,"syncpoints[%d].lng",idx);
                        getPostField(pname,jsonbuf,pval);
                        pc->lng=atof(pval);
                        pc+=sizeof(poiConfig_t);
                        idx++;
                    }
                }

                httpd_req_get_hdr_value_str(req,"Referer",pval,100);
                ESP_LOGD(__FUNCTION__,"%s - %s",jsonbuf,pval);
                saveConfig();
                httpd_resp_send(req,"OK",2);
                free(jsonbuf);
                break;
            default:
                free(jsonbuf);
                ret= httpd_resp_send_500(req);
                break;
        }
    } else {
        ESP_LOGD(__FUNCTION__,"Cannot route %s",req->uri);
        ret= httpd_resp_send_404(req);
    }
    return ret;
}

typedef struct {
    uint8_t* tarBuf;
    uint8_t* sendBuf;
    uint32_t bufLen;
    uint32_t sendLen;
    httpd_req_t* req;
    uint32_t len;
} sendTarParams;

sendTarParams sp;

void sendTar(void* param) {
    while(xEventGroupWaitBits(eventGroup,TAR_BUFFER_FILLED,pdTRUE,pdTRUE,portMAX_DELAY)){
        ESP_LOGV(__FUNCTION__,"Sent chunck of %d",sp.sendLen);
        if (sp.sendLen > 0){
            ESP_ERROR_CHECK(httpd_resp_send_chunk(sp.req,(const char*)sp.sendBuf,sp.sendLen));
            xEventGroupSetBits(eventGroup,TAR_BUFFER_SENT);
        }
        if (xEventGroupGetBits(eventGroup) & TAR_BUILD_DONE) {
            ESP_ERROR_CHECK(httpd_resp_send_chunk(sp.req,NULL,0));
            ESP_LOGD(__FUNCTION__,"Sent final");
            xEventGroupSetBits(eventGroup,TAR_SEND_DONE);
            break;
        }
    }
    vTaskDelete(NULL);
}

int tarRead(mtar_t *tar, void *data, unsigned size){
    ESP_LOGE(__FUNCTION__,"Cannot read");
    return ESP_FAIL;
}

int tarWrite(mtar_t *tar, const void *data, unsigned size){
    if (size > 0) {
        return httpd_resp_send_chunk(sp.req,(char*)data,size);
    } else {
        ESP_LOGW(__FUNCTION__,"empty set");
        return ESP_OK;
    }
    if (sp.bufLen+size >= HTTP_CHUNK_SIZE) {
        xEventGroupWaitBits(eventGroup,TAR_BUFFER_SENT,pdTRUE,pdTRUE,portMAX_DELAY);
        sp.sendLen = sp.bufLen;
        if (sp.sendBuf == NULL){
            sp.sendBuf = (uint8_t*)malloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf,(const void*)sp.tarBuf,sp.bufLen);
        sp.bufLen=0;
        xEventGroupSetBits(eventGroup,TAR_BUFFER_FILLED);
    }
    memcpy(&sp.tarBuf[sp.bufLen],data,size);
    sp.bufLen+=size;
    sp.len+=size;
    return ESP_OK;
}

int tarSeek(mtar_t *tar, unsigned pos){
    ESP_LOGE(__FUNCTION__,"Cannot seek");
    return ESP_FAIL;
}

int tarClose(mtar_t *tar){
    //return httpd_resp_send_chunk(sp.req,NULL,0);
    if (sp.bufLen > 0){
        sp.sendLen = sp.bufLen;
        if (sp.sendBuf == NULL){
            sp.sendBuf = (uint8_t*)malloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf,(const void*)sp.tarBuf,sp.bufLen);
    }
    ESP_LOGD(__FUNCTION__,"Wrote %d bytes",sp.len);
    xEventGroupSetBits(eventGroup,TAR_BUFFER_FILLED);
    xEventGroupWaitBits(eventGroup,TAR_BUFFER_SENT,pdTRUE,pdTRUE,portMAX_DELAY);
    xEventGroupSetBits(eventGroup,TAR_BUILD_DONE);
    xEventGroupSetBits(eventGroup,TAR_BUFFER_FILLED);
    return ESP_OK;
}

esp_err_t favicon_handler(httpd_req_t *req) {
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
}

esp_err_t sendFile(httpd_req_t *req){
    char resp[30];
    bool moveTheSucker = false;
    if (httpd_req_get_hdr_value_len(req,"movetosent")>1){
        moveTheSucker=true;
        trip* curTrip = getActiveTrip();
        if (moveTheSucker) {
            if (endsWith(req->uri,"kml")){
                if ((strlen(curTrip->fname) == 0) || endsWith(curTrip->fname,req->uri)) {
                    ESP_LOGD(__FUNCTION__,"Not moving %s as is it active(%s)",req->uri,curTrip->fname);
                    moveTheSucker=false;
                } else {
                    ESP_LOGD(__FUNCTION__,"Will move %s as it is not active trip %s",req->uri,curTrip->fname);
                }
            }
            if (endsWith(req->uri,"log")){
                char* clfn = getLogFName();
                if (!clfn || (strlen(clfn) == 0) || endsWith(clfn,req->uri)) {
                    ESP_LOGD(__FUNCTION__,"Not moving %s as is it active(%s)",req->uri,clfn);
                    moveTheSucker=false;
                } else {
                    ESP_LOGD(__FUNCTION__,"Will move %s as it is not active trip %s",req->uri,clfn);
                }
            }
        }
    } else {
        ESP_LOGD(__FUNCTION__,"No move in request");
    }
    if (endsWith(req->uri,"tar"))
        httpd_resp_set_type(req, "application/x-tar");
    else if (endsWith(req->uri,"kml"))
        httpd_resp_set_type(req, "application/vnd.google-earth.kml+xml");
    else
        httpd_resp_set_type(req, "application/octet-stream");
    char* path = (char*)malloc(530);
    memset(path,0,530);
    sprintf(path,"/sdcard%s",req->uri);
    ESP_LOGI(__FUNCTION__,"Sending %s",path);
    httpd_resp_set_hdr(req,"filename",path);
    FILE* theFile;
    if (initSPISDCard()) {
        if ((theFile=fopen(path,"r"))!=NULL) {
            ESP_LOGV(__FUNCTION__, "%s opened", path);
            uint8_t* buf = (uint8_t*)malloc(F_BUF_SIZE);
            uint32_t len=0;
            while (!feof(theFile)){
                if ((len=fread(buf,1,F_BUF_SIZE,theFile))>0) {
                    ESP_LOGV(__FUNCTION__, "%d written", len);
                    httpd_resp_send_chunk(req, (char*)buf, len);
                }
            }
            httpd_resp_send_chunk(req, NULL, 0);
            free(buf);
            fclose(theFile);
            if (moveTheSucker){
                char* topath = (char*)malloc(530);
                memset(topath,0,530);
                sprintf(topath,"/sdcard/sent%s",req->uri);
                if (!moveFile(path,topath)) {
                    ESP_LOGE(__FUNCTION__,"Cannot move %s to %s",path,topath);
                } else {
                    ESP_LOGD(__FUNCTION__,"Moved %s to %s folder",path,topath);
                }
            }
        } else {
            httpd_resp_send(req, "Not Found", 9);
            httpd_resp_set_status(req,HTTPD_404);
        }
    }
    deinitSPISDCard();
    free(path);
    return ESP_OK;
}

esp_err_t renderApp(httpd_req_t *req){
    if (endsWith(req->uri,"favicon.ico")) {
        return favicon_handler(req);
    }
    app_config_t* appcfg = getAppConfig();
    char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
    char path[100];

    if (!endsWith(req->uri,"/")) {
        return sendFile(req);
    }

    strcpy(path,req->uri);

    if (strlen(path)>1) {
        path[strlen(path)-1]=0;
    }

    httpd_resp_set_type(req, "text/html");
    char* startPos=indexOf((const char*)index_html_start,"<body>");
    startPos=indexOf(startPos,"file_section");
    if (startPos != NULL){
        startPos+=14;
        httpd_resp_send_chunk(req, (const char *)index_html_start, ((const uint8_t *)startPos-index_html_start));
        renderFolder(req,path);
        char* endPos=indexOf(startPos,"Sync Points");
        if (endPos!=NULL){
            endPos+=21;
            httpd_resp_send_chunk(req, (const char *)startPos, endPos-startPos);
            app_state_t* appstate = getAppState();
            poiConfig_t* pc = appcfg->pois;
            memset(jsonbuf,0,JSON_BUFFER_SIZE);
            uint32_t pidx=0;
            char* spts = jsonbuf;
            spts+=sprintf(spts,"<ul>");
            while ((pc->minDistance < 2000) && (pc->minDistance > 0)) {
                spts+=sprintf(spts,"<li><label>\
                    Point %d\
                    <input type=""number"" name=""syncpoints[%d].lat"">\
                    <input type=""number"" name=""syncpoints[%d].lng"">\
                </label></li>\n",pidx+1,pidx,pidx);
                pidx++;
                pc+=sizeof(poiConfig_t);
            }
            spts+=sprintf(spts,"</ul>");
            pidx=strlen(jsonbuf);
            if (pidx>0){
                httpd_resp_send_chunk(req, jsonbuf, strlen(jsonbuf));
            }

            httpd_resp_send_chunk(req, endPos, strlen(endPos));
        }
    } else {
        ESP_LOGE(__FUNCTION__,"Parser error 1");
    }
    httpd_resp_send_chunk(req, NULL, 0);
    free(jsonbuf);
    return httpd_resp_set_status(req,HTTPD_200);
}

esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGD(__FUNCTION__,"Router:%s",req->uri);
    return renderApp(req);
}

bool dumpFolder(char* folderName,mtar_t* tar) {
    FF_DIR theFolder;
    FILE* theFile;
    uint32_t len=0;
    char* fName=(char*)malloc(270);
    void* buf = malloc(F_BUF_SIZE);
    FILINFO* fi = (FILINFO*)malloc(sizeof(FILINFO));
    bool retval=true;

    if (f_opendir(&theFolder, folderName) == FR_OK)
    {
        ESP_LOGD(__FUNCTION__, "reading trip files in %s", folderName);
        while (f_readdir(&theFolder, fi) == FR_OK)
        {
            if (strlen(fi->fname) == 0)
            {
                break;
            }
            if (!(fi->fattrib & AM_DIR)){
                ESP_LOGD(__FUNCTION__, "%s - %d", fi->fname, fi->fsize);
                sprintf(fName,"/sdcard%s/%s",folderName,fi->fname);
                mtar_write_file_header(tar, fName, fi->fsize);
                if ((theFile=fopen(fName,"r"))!=NULL){
                    ESP_LOGV(__FUNCTION__, "%s opened", fName);
                    while (!feof(theFile)){
                        if ((len=fread(buf,1,F_BUF_SIZE,theFile))>0) {
                            ESP_LOGV(__FUNCTION__, "%d written", len);
                            mtar_write_data(tar, buf, len);
                        }
                    }
                    fclose(theFile);
                } else {
                    ESP_LOGE(__FUNCTION__,"Cannot read %s",fName);
                    retval=false;
                    break;
                }
            } else {
                sprintf(fName,"%s/%s",folderName,fi->fname);
                ESP_LOGD(__FUNCTION__,"Parsing sub folder %s of %s as %s",fi->fname, folderName,fName);
                dumpFolder(fName,tar);
            }
        }
        f_closedir(&theFolder);
    } else {
        retval=false;
    }
    free(fi);
    free(fName);
    free(buf);
    return retval;
}

esp_err_t trips_handler(httpd_req_t *req)
{
    ESP_LOGD(__FUNCTION__, "Sending Trips");
    mtar_t tar;
    tar.read=tarRead;
    tar.close=tarClose;
    tar.seek=tarSeek;
    tar.write=tarWrite;
    char strftime_buf[64];
    struct tm timeinfo;
    char* srcFolder="/kml";
    time_t now = time(NULL);

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d_%H-%M-%S.tar", &timeinfo);

    httpd_resp_set_type(req, "application/x-tar");
    httpd_resp_set_hdr(req,"filename",strftime_buf);

    if (initSPISDCard())
    {
        ESP_LOGD(__FUNCTION__, "Open %s", srcFolder);
        xEventGroupClearBits(eventGroup,TAR_BUFFER_FILLED);
        xEventGroupSetBits(eventGroup,TAR_BUFFER_SENT);
        xEventGroupClearBits(eventGroup,TAR_BUILD_DONE);
        xTaskCreate(sendTar,"sendTar",4096,NULL,5,NULL);

        sp.tarBuf=(uint8_t*)malloc(HTTP_BUF_SIZE);
        sp.sendBuf=NULL;
        sp.bufLen=0;
        sp.sendBuf=NULL;
        sp.req=req;
        sp.len=0;

        if (!dumpFolder(srcFolder,&tar)) {
            ESP_LOGE(__FUNCTION__, "KMLs NOT sent");
            deinitSPISDCard();
            return ESP_FAIL;
        }
        mtar_finalize(&tar);
        mtar_close(&tar);
        ESP_LOGD(__FUNCTION__, "KMLs sent");

        FF_DIR theFolder;
        FILINFO fi;

        if (f_opendir(&theFolder, srcFolder) == FR_OK)
        {
            ESP_LOGD(__FUNCTION__, "reading trip files in %s", srcFolder);
            char* ffn = (char*)malloc(300);
            char* tfn = (char*)malloc(300);
            while (f_readdir(&theFolder, &fi) == FR_OK)
            {
                if (strlen(fi.fname) == 0)
                {
                    break;
                }

                sprintf(ffn, "/sdcard/kml/%s", fi.fname);
                sprintf(tfn, "/sdcard/sent/%s", fi.fname);
                if ((fi.fattrib & AM_DIR)){
                    if ( !moveFolder(ffn+7, tfn+7))
                    {
                        ESP_LOGE(__FUNCTION__, "Failed moving folder %s to %s", ffn, tfn);
                    }
                } else {
                    if ( !moveFile(ffn, tfn))
                    {
                        ESP_LOGE(__FUNCTION__, "Failed moving file %s to %s", ffn, tfn);
                    }
                }
            }
            f_closedir(&theFolder);
            free(ffn);
            free(tfn);

            xEventGroupWaitBits(eventGroup,TAR_SEND_DONE,pdTRUE,pdTRUE,portMAX_DELAY);
            ESP_LOGD(__FUNCTION__, "KMLs archived");
        }

        deinitSPISDCard();
        wifiStop(NULL);
        free(sp.sendBuf);
        free(sp.tarBuf);
        //xTaskCreate(wifiStop, "wifiStop", 4096, NULL , tskIDLE_PRIORITY, NULL);
        return ESP_OK;
    } else {
        ESP_LOGD(__FUNCTION__,"Cannot mount the fucking sd card");
    }
    httpd_resp_send(req, "No Buono", 9);

    deinitSPISDCard();
    return ESP_FAIL;
}