#include "rest.h"
#include "route.h"
#include "math.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "../esp_littlefs/include/esp_littlefs.h"

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

//char* kmlFileName=(char*)malloc(255);

cJSON* status_json()
{
    ESP_LOGV(__FUNCTION__,"Status Handler");

    app_config_t* appcfg=getAppConfig();
    app_state_t* appstate=getAppState();
    the_wifi_config* wcfg = getWifiConfig();
    char strftime_buf[64];
    struct tm timeinfo;
    time_t now;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

    cJSON* status=NULL;
    cJSON* wifi=NULL;
    cJSON* jclients=NULL;

    char* buf = (char*)malloc(255);
    uint32_t idx=0;
    uint64_t upTime=0,sleepTime=0;

    status=cJSON_CreateObject();
    wifi=cJSON_CreateObject();
    jclients = cJSON_CreateArray();
    upTime=getUpTime();
    sleepTime=getSleepTime();

    cJSON_AddItemToObject(status, "deviceid",  cJSON_CreateNumber(appcfg->devId));
    sprintf(buf,"%d:%02d:%02d",(uint32_t)floor(upTime/3600),(uint32_t)floor((upTime%3600)/60),(uint32_t)upTime%60);
    cJSON_AddStringToObject(status, "uptime", buf);
    sprintf(buf,"%d:%02d:%02d",(uint32_t)floor(sleepTime/3600),(uint32_t)floor((sleepTime%3600)/60),(uint32_t)sleepTime%60);
    cJSON_AddStringToObject(status, "sleeptime", buf);
    cJSON_AddItemToObject(status, "freeram",  cJSON_CreateNumber(esp_get_free_heap_size()));
    cJSON_AddItemToObject(status, "totalram", cJSON_CreateNumber(heap_caps_get_total_size(MALLOC_CAP_DEFAULT)));
    cJSON_AddItemToObject(status, "battery",  cJSON_CreateNumber(getBatteryVoltage()));
    cJSON_AddItemToObject(status, "systemtime",  cJSON_CreateString(strftime_buf));
    cJSON_AddStringToObject(wifi, "type", appcfg->purpose == app_config_t::purpose_t::PULLER?"AP":"Station");
    cJSON_AddStringToObject(wifi, "enabled", xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_UP_BIT?"yes":"no");
    cJSON_AddStringToObject(wifi, "connected", xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_CONNECTED_BIT?"yes":"no");
    cJSON_AddStringToObject(wifi, "scanning", xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_SCANING_BIT?"yes":"no");
    tcpip_adapter_ip_info_t* ipInfo=GetIpInfo();

    sprintf(buf,IPSTR, IP2STR(&ipInfo->ip));
    cJSON_AddStringToObject(wifi, "ip", buf);

    if (!(appstate->gps&item_state_t::INACTIVE)) {
        cJSON* gps = cJSON_CreateObject();
        cJSON_AddNumberToObject(gps, "Lattitude", appstate->lattitude);
        cJSON_AddNumberToObject(gps, "Longitude", appstate->longitude);
        cJSON_AddItemToObject(status,"gps",gps);
    }

    Aper** clients = GetClients();
    for (Aper* client = clients[idx++];idx < MAX_NUM_CLIENTS; client = clients[idx++]){
        ESP_LOGV(__FUNCTION__,"client(%d),%s",idx,client==NULL?"null":"not null");
        if (client) {
            cJSON_AddItemToArray(jclients,client->toJson());
        }
    }

    char* taskList = (char*)malloc(JSON_BUFFER_SIZE);
    memset(taskList,0,JSON_BUFFER_SIZE);
    vTaskGetRunTimeStats(taskList);
    cJSON_AddStringToObject(status,"Runstats",taskList);
    
    memset(taskList,0,JSON_BUFFER_SIZE);
    vTaskList(taskList);
    cJSON_AddStringToObject(status,"Tasklist",taskList);
    free(taskList);
    cJSON_AddItemToObject(wifi,"clients",jclients);

    cJSON_AddItemToObject(status,"wifi",wifi);
    free(buf);
    return status;
}


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

    while ((update_partition = esp_ota_get_next_update_partition(update_partition))->address == configured->address){
        assert(update_partition != NULL);
        ESP_LOGV(__FUNCTION__, "Skipping partition subtype %d at offset 0x%x",
                update_partition->subtype, update_partition->address);
    }
    assert(update_partition != NULL);
    ESP_LOGI(__FUNCTION__, "Writing to partition subtype %d at offset 0x%x",
            update_partition->subtype, update_partition->address);

    if (initSPISDCard()) {
        FILE* fw = NULL;
        if ((getAppConfig()->purpose == app_config_t::purpose_t::PULLER ) && (fw = fopen("/lfs/firmware/current.bin","w",true)) != NULL) {
            if (fwrite((void*)img,1,totLen,fw) == totLen) {
                fclose(fw);
            } else {
                ESP_LOGE(__FUNCTION__,"Firmware not backedup");
            }
        } else {
            if (getAppConfig()->purpose == app_config_t::purpose_t::PULLER ){
                ESP_LOGE(__FUNCTION__,"Failed to open /tfs/firmware/current.bin");
            }
        }
        if ((fw = fopen("/lfs/firmware/current.bin.md5","w",true)) != NULL) {
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
            ESP_LOGI(__FUNCTION__,"Firmware md5 written");
        } else {
            ESP_LOGE(__FUNCTION__,"Failed in opeing /firmware/current.bin.md5");
        }
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
                    deinitSPISDCard();
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

esp_err_t stat_handler(httpd_req_t *req){
    char* fname = (char*)(req->uri + 5);
    ESP_LOGV(__FUNCTION__,"Getting stats on %s",fname);
    struct stat st;
    uint8_t retryCnt=0;
    int ret=0;
    
    while ((retryCnt++ < 5) && ((ret=stat(fname,&st)) != 0)) {
        vTaskDelay(50/portTICK_PERIOD_MS);
    }

    if (ret == 0) {
        char* res = (char*)malloc(JSON_BUFFER_SIZE);
        char* path = (char*)malloc(255);
        strcpy(path,fname);
        char* fpos = strrchr(path,'/');
        *fpos=0;
        esp_err_t ret = httpd_resp_send(req, res, sprintf(res,"{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d}",
            path,
            fpos+1,
            "file",
            (uint32_t)st.st_size
        ));
        free(path);
        free(res);
        return ret;

    }
    ESP_LOGE(__FUNCTION__,"Cannot stat %s err:%d",fname,ret);
    return httpd_resp_send_500(req);
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
            if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
                ESP_LOGV(__FUNCTION__, "Found header => Host: %s", buf);
            }
            free(buf);
        }

        buf_len = httpd_req_get_url_query_len(req) + 1;
        char md5[36];
        md5[0]=0;

        if (buf_len > 1) {
            buf = (char*)malloc(buf_len);
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
                ESP_LOGV(__FUNCTION__, "Found URL query => %s", buf);
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
            ESP_LOGV(__FUNCTION__,"Total: %d/%d",totLen,curLen);
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

            if (strcmp((char*)ccmd5,(char*)md5) == 0) {
                ESP_LOGD(__FUNCTION__,"Flashing md5:(%s)%dvs%d",ccmd5,totLen,curLen);
                ESP_LOGI(__FUNCTION__, "RAM:%d", esp_get_free_heap_size());
                httpd_resp_send(req, "Flashing", 9);
                vTaskDelay(pdMS_TO_TICKS(500));
                xTaskCreate(&flashTheThing,"flashTheThing",4096, NULL, 5, NULL);
                stopGps();
                esp_wifi_stop();
                httpd_stop(&server);
            } else {
                ESP_LOGD(__FUNCTION__,"md5:(%s)(%s)",ccmd5,md5);
                httpd_resp_send(req, "Bad Checksum", 13);
            }
        } else {
            httpd_resp_send(req, "Not OK", 6);
        }
        return ESP_OK;
    } else if (indexOf(req->uri,"/ota/getmd5")==req->uri){
        if (initSPISDCard()){
            FILE* fw = NULL;
            if ((fw = fopen("/lfs/firmware/current.bin.md5","r",true)) != NULL) {
                char ccmd5[36];
                uint32_t len=0;
                if ((len=fread((void*)ccmd5,1,36,fw)) == 36) {
                    httpd_resp_send(req,ccmd5,36);
                    fclose(fw);
                    deinitSPISDCard();
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

esp_err_t findFiles(httpd_req_t *req,char* path,const char* ext,bool recursive, char* res, uint32_t resLen){
    if ((path == NULL) || (strlen(path)==0)) {
        return ESP_FAIL;
    }
    if ( strcmp(path,"/") == 0){
        sprintf(res,"[{\"name\":\"sdcard\",\"ftype\":\"folder\",\"size\":0},{\"name\":\"lfs\",\"ftype\":\"folder\",\"size\":0}]");
        return ESP_OK;
    }
    if (!initSPISDCard()) {
        return ESP_FAIL;
    }
    esp_err_t ret=ESP_OK;
    uint32_t sLen=strlen(res);
    ESP_LOGD(__FUNCTION__,"Parsing %s",path);
    char* theFolders = (char*)malloc(1024);
    memset(theFolders,0,1024);
    char* theFName = (char*)malloc(1024);
    memset(theFName,0,1024);
    uint32_t fpos=0;
    uint32_t fcnt=0;
    uint32_t dcnt=0;
    struct stat st;
    char* kmlFileName=(char*)malloc(1024);

    DIR* theFolder;
    struct dirent* fi;
    if ((theFolder = opendir(path)) != NULL){
        while ((fi = readdir(theFolder)) != NULL)
        {
            if (strlen(fi->d_name) == 0)
            {
                break;
            }
            if (fi->d_type == DT_DIR){
                if (recursive){
                    dcnt++;
                    sprintf(kmlFileName,"%s/%s",path,fi->d_name);
                    fpos+=sprintf(theFolders+fpos,"%s",kmlFileName)+1;
                    ESP_LOGV(__FUNCTION__,"%s currently has %d files and %d folders subfolder len:%d. Adding dir %s", path,fcnt,dcnt,fpos,kmlFileName);
                }
                if ((ext == NULL) || (strlen(ext)==0)){
                    if (sLen > (resLen-100)){
                        ESP_LOGV(__FUNCTION__,"Buffer Overflow, flushing");
                        if ((ret=httpd_resp_send_chunk(req,res,sLen)) != ESP_OK){
                            ESP_LOGE(__FUNCTION__,"Error sending chunk %s sLenL%d, actuallen:%d",esp_err_to_name(ret),sLen,strlen(res));
                            break;
                        }
                        memset(res,0,resLen);
                        sLen=0;
                    }
                    
                    sLen+=sprintf(res+sLen,"{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d},",
                        path,
                        fi->d_name,
                        fi->d_type == DT_DIR ? "folder":"file",
                        0
                    );
                }
            } else if ((ext == NULL) || (strlen(ext)==0) || endsWith(fi->d_name,ext)){
                fcnt++;
                ESP_LOGV(__FUNCTION__,"%s currently has %d files and %d folders subfolder len:%d. Adding file %s", path,fcnt,dcnt,fpos,fi->d_name);
                if (sLen > (resLen-100)){
                    ESP_LOGV(__FUNCTION__,"Buffer Overflow, flushing");
                    if ((ret=httpd_resp_send_chunk(req,res,sLen)) != ESP_OK){
                        ESP_LOGE(__FUNCTION__,"Error sending chunk %s sLenL%d, actuallen:%d",esp_err_to_name(ret),sLen,strlen(res));
                        break;
                    }
                    memset(res,0,resLen);
                    sLen=0;
                }
                sprintf(theFName,"%s/%s",path,fi->d_name);
                st.st_size=0;
                sLen+=sprintf(res+sLen,"{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d},",
                    path,
                    fi->d_name,
                    fi->d_type == DT_DIR ? "folder":"file",
                    (uint32_t)st.st_size
                );
            }
        }
        closedir(theFolder);
        ESP_LOGV(__FUNCTION__,"%s has %d files and %d folders subfolder len:%d", path,fcnt,dcnt,fpos);
        uint32_t ctpos=0;
        while (dcnt-->0) {
            ESP_LOGV(__FUNCTION__,"%d-%s: Getting sub-folder(%d) %s",dcnt, path,ctpos,theFolders+ctpos);
            if (findFiles(req, theFolders+ctpos,ext,recursive,res,resLen) != ESP_OK) {
                ESP_LOGW(__FUNCTION__,"Error invoking getSdFiles for %s", kmlFileName);
            }
            ctpos+=strlen(theFolders)+1;
        }
    } else {
        ESP_LOGW(__FUNCTION__,"Error opening %s", path);
        ret=ESP_FAIL;
    }

    free(theFName);
    free(theFolders);
    free(kmlFileName);
    deinitSPISDCard();
    return ret;
}

esp_err_t list_entity_handler(httpd_req_t *req)
{
    char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
    memset(jsonbuf,0,JSON_BUFFER_SIZE);
    *jsonbuf='[';
    xEventGroupClearBits(getWifiConfig()->s_wifi_eg,WIFI_CLIENT_DONE);
    ESP_LOGD(__FUNCTION__,"Getting %s url:%s",req->uri+11,req->uri);
    if (endsWith(req->uri,"kml")){
        if (findFiles(req,"/kml","kml",true,jsonbuf,JSON_BUFFER_SIZE-1) != ESP_OK){
            ESP_LOGE(__FUNCTION__,"Error wilst sending kml list");
            free(jsonbuf);
            return httpd_resp_send_500(req);
        }
    } else if (endsWith(req->uri,"csv")){
        ESP_LOGD(__FUNCTION__,"Getting csv url:%s",req->uri);
        if (findFiles(req,"/lfs/csv","csv",false,jsonbuf,JSON_BUFFER_SIZE) != ESP_OK){
            ESP_LOGE(__FUNCTION__,"Error wilst sending csv list");
            free(jsonbuf);
            return httpd_resp_send_500(req);
        }
    } else if (endsWith(req->uri,"log")){
        if (findFiles(req,"/sdcard/logs","log",false,jsonbuf,JSON_BUFFER_SIZE) != ESP_OK){
            ESP_LOGE(__FUNCTION__,"Error wilst sending log list");
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
    ESP_LOGV(__FUNCTION__,"Sent final chunck of %d",strlen(jsonbuf));
    free(jsonbuf);
    return httpd_resp_send_chunk(req,NULL,0);
}

esp_err_t list_files_handler(httpd_req_t *req)
{
    char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
    memset(jsonbuf,0,JSON_BUFFER_SIZE);
    *jsonbuf='[';
    ESP_LOGV(__FUNCTION__,"Getting %s url:%s",req->uri+6,req->uri);
    if (findFiles(req,(char*)(req->uri+6),NULL,false,jsonbuf,JSON_BUFFER_SIZE-1) != ESP_OK){
        ESP_LOGE(__FUNCTION__,"Error wilst sending file list");
        free(jsonbuf);
        return httpd_resp_send_500(req);
    }
    if (strlen(jsonbuf) > 1 )
        *(jsonbuf+strlen(jsonbuf)-1)=']';
    else
    {
        sprintf(jsonbuf,"%s","[]");
    }
    
    httpd_resp_send_chunk(req,jsonbuf,strlen(jsonbuf));
    ESP_LOGV(__FUNCTION__,"Sent final chunck of %d",strlen(jsonbuf));
    free(jsonbuf);
    return httpd_resp_send_chunk(req,NULL,0);
}

void UpdateGpioProp(cfg_gpio_t* cfg,gpio_num_t val) {
    if ((val == NULL) || (cfg->value != val)) {
        ESP_LOGV(__FUNCTION__,"Updating from %d to %d",cfg->value, val);
        cfg->value = val;
        cfg->version++;
    }
}

void UpdateStringProp(cfg_label_t* cfg,char* val) {
    if ((val == NULL) || (strcmp(cfg->value,val)!=0)) {
        strcpy(cfg->value,val == NULL ? "" : val );
        cfg->version++;
    }
}

void cJSON_AddVersionedStringToObject(cfg_label_t* itemToAdd, char* name, cJSON* dest) {
    cJSON* item=cJSON_CreateObject(); 
    cJSON_AddItemToObject(item, "value",  cJSON_CreateString(itemToAdd->value));
    cJSON_AddItemToObject(item, "version",  cJSON_CreateNumber(itemToAdd->version));
    cJSON_AddItemToObject(dest, name,  item);
}

void cJSON_AddVersionedGpioToObject(cfg_gpio_t* itemToAdd, char* name,  cJSON* dest) {
    cJSON* item=cJSON_CreateObject(); 
    cJSON_AddItemToObject(item, "value",  cJSON_CreateNumber(itemToAdd->value));
    cJSON_AddItemToObject(item, "version",  cJSON_CreateNumber(itemToAdd->version));
    cJSON_AddItemToObject(dest, name,  item);
}

cJSON* config_json()
{
    app_config_t* appcfg=getAppConfig();
    app_state_t* appstate=getAppState();
    appcfg = getAppConfig();
    appstate = getAppState();

    ESP_LOGV(__FUNCTION__,"Building Json");
    cJSON* config=cJSON_CreateObject();
    cJSON* sdcard=cJSON_CreateObject();
    cJSON* gps=cJSON_CreateObject();
    cJSON_AddItemToObject(config, "sdcard",  sdcard);
    cJSON_AddItemToObject(config, "gps",  gps);

    cJSON_AddItemToObject(config, "type",  cJSON_CreateString(appcfg->purpose == app_config_t::purpose_t::PULLER?"AP":"Station"));
    cJSON_AddItemToObject(config, "deviceid",  cJSON_CreateNumber(appcfg->devId));
    cJSON_AddVersionedStringToObject(&appcfg->devName,"devName",config);

    cJSON_AddItemToObject(sdcard, "state",  cJSON_CreateString(appstate->sdCard&item_state_t::ERROR?"invalid":appstate->sdCard&item_state_t::INACTIVE?"inactive":"valid"));
    cJSON_AddVersionedGpioToObject(&appcfg->sdcard_config.MisoPin,"MisoPin",sdcard);
    cJSON_AddVersionedGpioToObject(&appcfg->sdcard_config.MosiPin,"MosiPin",sdcard);
    cJSON_AddVersionedGpioToObject(&appcfg->sdcard_config.ClkPin,"ClkPin",sdcard);
    cJSON_AddVersionedGpioToObject(&appcfg->sdcard_config.Cspin,"Cspin",sdcard);

    cJSON_AddItemToObject(gps, "state",  cJSON_CreateString(appstate->gps&item_state_t::ERROR?"invalid":appstate->gps&item_state_t::INACTIVE?"inactive":"valid"));
    cJSON_AddVersionedGpioToObject(&appcfg->gps_config.txPin,"txPin",gps);
    cJSON_AddVersionedGpioToObject(&appcfg->gps_config.rxPin,"rxPin",gps);
    cJSON_AddVersionedGpioToObject(&appcfg->gps_config.enPin,"enPin",gps);

    cJSON* pois = cJSON_CreateArray();
    cJSON_AddItemToObject(gps,"pois",pois);

    ESP_LOGV(__FUNCTION__,"Getting POIs");
    for (poiConfig_t pc :appcfg->pois){
        ESP_LOGV(__FUNCTION__,"poi min distance: %d",pc.minDistance);
        if ((pc.minDistance < 2000) && (pc.minDistance > 0)) {
            cJSON* poi = cJSON_CreateObject();
            cJSON_AddItemToObject(poi, "lat",  cJSON_CreateNumber(pc.lat));
            cJSON_AddItemToObject(poi, "lng",  cJSON_CreateNumber(pc.lng));
            cJSON_AddItemToObject(poi, "distance",  cJSON_CreateNumber(pc.minDistance));
            cJSON_AddItemToArray(pois, poi);
        }
    }

    return config;
}

esp_err_t config_handler(httpd_req_t *req)
{
    ESP_LOGV(__FUNCTION__,"Config Handler");
    esp_err_t ret = ESP_OK;
    app_config_t* appcfg=getAppConfig();
    app_state_t* appstate=getAppState();
    appcfg = getAppConfig();
    appstate = getAppState();

    httpd_resp_set_type(req, "application/json");

    char* postData = (char*) malloc(JSON_BUFFER_SIZE);
    int len = httpd_req_recv(req,postData,JSON_BUFFER_SIZE);

    if (len) {
        *(postData+len)=0;
        ESP_LOGV(__FUNCTION__,"postData(%d):%s",len,postData);
        cJSON* config=cJSON_Parse(postData);
        cJSON* sdcard=cJSON_GetObjectItemCaseSensitive(config,"sdcard");
        cJSON* gps=cJSON_GetObjectItemCaseSensitive(config,"gps");
        UpdateStringProp(&appcfg->devName,cJSON_GetObjectItem(config,"devName")->valuestring);
        appcfg->purpose = strcmp(cJSON_GetObjectItem(config,"type")->valuestring,"AP")==0 ? app_config_t::purpose_t::PULLER : app_config_t::purpose_t::TRACKER;
        if (sdcard){
            ESP_LOGV(__FUNCTION__,"Configuring sdcard");
            UpdateGpioProp(&appcfg->sdcard_config.MisoPin,(gpio_num_t) cJSON_GetObjectItem(sdcard,"MisoPin")->valueint);
            UpdateGpioProp(&appcfg->sdcard_config.MosiPin, (gpio_num_t) cJSON_GetObjectItem(sdcard,"MosiPin")->valueint);
            UpdateGpioProp(&appcfg->sdcard_config.ClkPin, (gpio_num_t) cJSON_GetObjectItem(sdcard,"ClkPin")->valueint);
            UpdateGpioProp(&appcfg->sdcard_config.Cspin, (gpio_num_t) cJSON_GetObjectItem(sdcard,"CsPin")->valueint);
        } else {
            ESP_LOGW(__FUNCTION__,"No sdcard config");
        }
        if (gps){
            ESP_LOGV(__FUNCTION__,"Configuring gps");
            UpdateGpioProp(&appcfg->gps_config.txPin, (gpio_num_t) cJSON_GetObjectItem(gps,"txPin")->valueint);
            UpdateGpioProp(&appcfg->gps_config.rxPin, (gpio_num_t) cJSON_GetObjectItem(gps,"rxPin")->valueint);
            UpdateGpioProp(&appcfg->gps_config.enPin, (gpio_num_t) cJSON_GetObjectItem(gps,"enPin")->valueint);
        } else {
            ESP_LOGW(__FUNCTION__,"No gps config");
        }
        saveConfig();
        initConfig();
        if (initSPISDCard()) {
            deinitSPISDCard();
        }
        cJSON_Delete(config);
    }

    cJSON* config=config_json();
    char* sjson = cJSON_PrintUnformatted(config);
    ret= httpd_resp_send(req,sjson,strlen(sjson));
    cJSON_Delete(config);
    free(sjson);
    free(postData);
    return ret;
}

esp_err_t HandleWifiCommand(httpd_req_t *req)
{
    esp_err_t ret=0;
    char* postData = (char*) malloc(JSON_BUFFER_SIZE);
    int rlen=httpd_req_recv(req,postData,JSON_BUFFER_SIZE);
    if (rlen == 0) {
        httpd_resp_send_500(req);
        ESP_LOGE(__FUNCTION__,"no body");
    } else {
        *(postData+rlen)=0;
        ESP_LOGV(__FUNCTION__,"Got %s",postData);
        cJSON* jresponse = cJSON_Parse(postData);
        if (jresponse != NULL) {
            cJSON* jitem = cJSON_GetObjectItemCaseSensitive(jresponse,"enabled");
            if (jitem && (strcmp(jitem->valuestring,"no")==0)) {
                xEventGroupSetBits(getWifiConfig()->s_wifi_eg,WIFI_CLIENT_DONE);
                xEventGroupSetBits(*getAppEG(),app_bits_t::TRIPS_SYNCED);
                ESP_LOGD(__FUNCTION__,"All done wif wifi");
                ret = httpd_resp_send(req,"OK",2);
                wifiStop(NULL);
            }
            cJSON_Delete(jresponse);
        } else {
            ESP_LOGE(__FUNCTION__,"Error whilst parsing json");
        }
    }
    return ret;
}

void parseFiles(void* param) {
    DIR* tarFolder;
    DIR* childFolder;
    dirent* di;
    dirent* fi;
    char* folderName = (char*)malloc(300);
    char* fileName = (char*)malloc(300);
    if ((tarFolder = opendir("/sdcard/tars")) != NULL){
        while ((di = readdir(tarFolder)) != NULL) {
            ESP_LOGV(__FUNCTION__,"tarlist:%s",di->d_name);
            if (di->d_type == DT_DIR){
                sprintf(folderName,"/sdcard/tars/%s",di->d_name);
                if ((childFolder = opendir(folderName)) != NULL){
                    while ((fi = readdir(childFolder)) != NULL) {
                        if (fi->d_type != DT_DIR){
                            sprintf(fileName,"%s/%s",folderName,fi->d_name);
                            ESP_LOGV(__FUNCTION__,"filelist:%s",fileName);
                            extractClientTar(fileName);
                            unlink(fileName);
                        }
                    }
                } else {
                    ESP_LOGE(__FUNCTION__,"Cannot open %s", folderName);
                }
                closedir(childFolder);
            }
        }
        closedir(tarFolder);
    } else {
        ESP_LOGE(__FUNCTION__,"Cannot open /sdcard/tars");
    }
    free(fileName);
    free(folderName);
    xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void*)(BIT2|BIT3), tskIDLE_PRIORITY, NULL);
    vTaskDelete(NULL);
}

esp_err_t HandleSystemCommand(httpd_req_t *req)
{
    esp_err_t ret=0;
    char* postData = (char*) malloc(JSON_BUFFER_SIZE);
    int rlen=httpd_req_recv(req,postData,JSON_BUFFER_SIZE);
    if (rlen == 0) {
        httpd_resp_send_500(req);
        ESP_LOGE(__FUNCTION__,"no body");
    } else {
        *(postData+rlen)=0;
        ESP_LOGV(__FUNCTION__,"Got %s",postData);
        cJSON* jresponse = cJSON_Parse(postData);
        if (jresponse != NULL) {
            cJSON* jitem = cJSON_GetObjectItemCaseSensitive(jresponse,"command");
            if (jitem && (strcmp(jitem->valuestring,"reboot")==0)) {
                esp_restart();
            }
            if (jitem && (strcmp(jitem->valuestring,"parseFiles")==0)) {
                xTaskCreate(parseFiles, "parseFiles", 4096, NULL, tskIDLE_PRIORITY, NULL);
            }
            cJSON_Delete(jresponse);
        } else {
            ESP_LOGE(__FUNCTION__,"Error whilst parsing json");
        }
    }
    return ret;
}

esp_err_t status_handler(httpd_req_t *req)
{
    ESP_LOGV(__FUNCTION__,"Status Handler");
    esp_err_t ret = ESP_FAIL;

    ESP_LOGV(__FUNCTION__,"uri:%s method: %s",req->uri, req->method == HTTP_POST ? "POST" : "PUT" );

    if (req->method==HTTP_POST){
        httpd_resp_set_type(req, "application/json");
        cJSON* status=status_json();
        char* sjson = cJSON_PrintUnformatted(status);
        ret= httpd_resp_send(req, sjson, strlen(sjson));
        cJSON_Delete(status);
        free(sjson);
    }
    if (req->method==HTTP_PUT){
        if (endsWith(req->uri,"/wifi")) {
            ret = HandleWifiCommand(req);
        }
        if (endsWith(req->uri,"/cmd")) {
            ret = HandleSystemCommand(req);
        }
    }

    if (ret != ESP_OK){
        httpd_resp_send_500(req);
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
    size_t len = 0;
    esp_err_t ret;
    while(xEventGroupWaitBits(eventGroup,TAR_BUFFER_FILLED,pdFALSE,pdTRUE,portMAX_DELAY)){
        if (sp.sendLen > 0){
            //printf(".");
            ESP_LOGV(__FUNCTION__,"Sending chunk of %d",sp.sendLen);
            len+=sp.sendLen;
            ret=httpd_resp_send_chunk(sp.req,(const char*)sp.sendBuf,sp.sendLen);

            if (ret == ESP_OK){
                sp.sendLen=0;
                xEventGroupClearBits(eventGroup,TAR_BUFFER_FILLED);
                xEventGroupSetBits(eventGroup,TAR_BUFFER_SENT);
            }
            else {
                ESP_LOGE(__FUNCTION__,"Chunk won't go: %s",esp_err_to_name(ret));
                xEventGroupSetBits(eventGroup, TAR_SEND_DONE);
                break;
            }
        }
        if (xEventGroupGetBits(eventGroup)&TAR_BUILD_DONE) {
            break;
        }
    }
    httpd_resp_send_chunk(sp.req,NULL,0);
    ESP_LOGD(__FUNCTION__,"Tar sent %d bytes",len);
    xEventGroupSetBits(eventGroup, TAR_SEND_DONE);
    vTaskDelete(NULL);
}

int tarRead(mtar_t *tar, void *data, unsigned size){
    ESP_LOGE(__FUNCTION__,"Cannot read");
    return ESP_FAIL;
}

int tarWrite(mtar_t *tar, const void *data, unsigned size){
    if (size == 0) {
        ESP_LOGW(__FUNCTION__,"empty set");
        return ESP_OK;
    }

    if (sp.bufLen+size >= HTTP_CHUNK_SIZE) {
        if (xEventGroupGetBits(eventGroup) & TAR_SEND_DONE) {
            ESP_LOGE(__FUNCTION__, "SendeR Died");
            return ESP_FAIL;
        }

        xEventGroupWaitBits(eventGroup,TAR_BUFFER_SENT,pdTRUE,pdTRUE,portMAX_DELAY);
        ESP_LOGV(__FUNCTION__, "Preparing chunck of %d", sp.bufLen);
        sp.sendLen = sp.bufLen;
        if (sp.sendBuf == NULL){
            sp.sendBuf = (uint8_t*)malloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf,(const void*)sp.tarBuf,sp.bufLen);
        sp.bufLen=0;
        xEventGroupSetBits(eventGroup,TAR_BUFFER_FILLED);
    }
    if (size > 1) {
        memcpy(sp.tarBuf+sp.bufLen,data,size);
        sp.bufLen+=size;
        sp.len+=size;
        ESP_LOGV(__FUNCTION__,"chunck: %d buflen:%d tot:%d",size,sp.bufLen,sp.len);
    } else {
        *(sp.tarBuf+sp.bufLen++)=*((uint8_t*)data);
        sp.len++;
    }
    return ESP_OK;
}

int tarSeek(mtar_t *tar, unsigned pos){
    ESP_LOGE(__FUNCTION__,"Cannot seek");
    return ESP_FAIL;
}

int tarClose(mtar_t *tar){
    xEventGroupWaitBits(eventGroup,TAR_BUFFER_SENT,pdTRUE,pdTRUE,portMAX_DELAY);
    xEventGroupSetBits(eventGroup,TAR_BUILD_DONE);
    sp.sendLen = sp.bufLen;
    if (sp.sendLen > 0) {
        if (sp.sendBuf == NULL){
            sp.sendBuf = (uint8_t*)malloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf,(const void*)sp.tarBuf,sp.bufLen);
        ESP_LOGV(__FUNCTION__,"Sending final chunk of %d",sp.bufLen);
    }
    xEventGroupSetBits(eventGroup,TAR_BUFFER_FILLED);
    xEventGroupWaitBits(eventGroup,TAR_SEND_DONE,pdFALSE,pdTRUE,portMAX_DELAY);
    ESP_LOGD(__FUNCTION__,"Wrote %d bytes",sp.len);
    if (sp.sendBuf != NULL) free(sp.sendBuf);
    if (sp.tarBuf != NULL) free(sp.tarBuf);
    return ESP_OK;
}

esp_err_t sendFile(httpd_req_t *req){
    bool moveTheSucker = false;
    if (httpd_req_get_hdr_value_len(req,"movetosent")>1){
        moveTheSucker=true;
        trip* curTrip = getActiveTrip();
        if (moveTheSucker) {
            if (endsWith(req->uri,"kml")){
                if ((strlen(curTrip->fname) > 0) && endsWith(curTrip->fname,req->uri)) {
                    ESP_LOGD(__FUNCTION__,"Not moving %s as is it active(%s)",req->uri,curTrip->fname);
                    moveTheSucker=false;
                } else {
                    ESP_LOGD(__FUNCTION__,"Will move %s as it is not active trip %s",req->uri,curTrip->fname);
                }
            }
            if (endsWith(req->uri,"log")){
                char* clfn = getLogFName();
                if (!clfn && (strlen(clfn) > 0) && endsWith(clfn,req->uri)) {
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
    sprintf(path,"%s",req->uri);
    ESP_LOGI(__FUNCTION__,"Sending %s willmove:%d",path,moveTheSucker);
    httpd_resp_set_hdr(req,"filename",path);
    FILE* theFile;
    if (initSPISDCard()) {
        if ((theFile=fopen(path,"r"))!=NULL) {
            ESP_LOGV(__FUNCTION__, "%s opened", path);
            uint8_t* buf = (uint8_t*)malloc(F_BUF_SIZE);
            uint32_t len=0;
            while (!feof(theFile)){
                if ((len=fread(buf,1,F_BUF_SIZE,theFile))>0) {
                    ESP_LOGV(__FUNCTION__, "%d read", len);
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
                }
            }
        } else {
            httpd_resp_send(req, "Not Found", 9);
            httpd_resp_set_status(req,HTTPD_404);
        }
    }
    ESP_LOGI(__FUNCTION__,"Sent %s",path);
    deinitSPISDCard();
    free(path);
    return ESP_OK;
}

esp_err_t app_handler(httpd_req_t *req)
{
   if (endsWith(req->uri,"favicon.ico")) {
        httpd_resp_set_type(req, "image/x-icon");
        return httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);
    }
    if (endsWith(req->uri,"app.js")) {
        httpd_resp_set_type(req, "text/javascript");
        return httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start-1);
    }
    if (endsWith(req->uri,"app.css")) {
        httpd_resp_set_type(req, "text/css");
        return httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start-1);
    }

    if (!endsWith(req->uri,"/")) {
        return sendFile(req);
    }
    return httpd_resp_send(req, (const char *)index_html_start, (index_html_end-index_html_start));
}

esp_err_t tarString(mtar_t* tar,const char* path,const char* data){
    mtar_write_file_header(tar, path, strlen(data));
    ESP_LOGV(__FUNCTION__,"tarString(%d) %s",strlen(data),data);
    return mtar_write_data(tar, data, strlen(data));
}

esp_err_t tarFiles(mtar_t* tar,char* path,const char* ext,bool recursive,const char* excludeList, uint32_t maxSize){
    if ((path == NULL) || (strlen(path)==0)) {
        return ESP_FAIL;
    }
    if (!initSPISDCard()) {
        return ESP_FAIL;
    }

    DIR* theFolder;
    FILE* theFile;
    struct dirent* fi;
    struct stat fileStat;

    esp_err_t ret=ESP_OK;
    uint32_t fpos=0;
    uint32_t fcnt=0;
    uint32_t dcnt=0;
    uint32_t len=0;
    uint32_t tarStat=MTAR_ESUCCESS;

    char* theFolders = (char*)malloc(1024);
    char* theFName = (char*)malloc(300);
    char* buf = (char*)malloc(sizeof(char)*F_BUF_SIZE);
    char* kmlFileName=(char*)malloc(300);

    memset(kmlFileName,0,300);
    memset(theFolders,0,1024);
    memset(theFName,0,300);

    ESP_LOGD(__FUNCTION__,"Parsing %s",path);
    struct timeval tv_start,tv_end,tv_open,tv_stat,tv_rstart,tv_rend,tv_wstart,tv_wend;

    if ((theFolder = opendir(path)) != NULL){
        sprintf(kmlFileName,"%s/",path+1);
        tarStat = mtar_write_dir_header(tar, kmlFileName);
        while ((tarStat == MTAR_ESUCCESS) && ((fi = readdir(theFolder)) != NULL))
        {
            if (strlen(fi->d_name) == 0)
            {
                break;
            }
            if (tar->pos > maxSize) {
                break;
            }
            if (fi->d_type == DT_DIR){
                if (recursive){
                    dcnt++;
                    sprintf(kmlFileName,"%s/%s",path,fi->d_name);
                    sprintf(theFolders+fpos,"%s",kmlFileName);
                    fpos+=strlen(kmlFileName)+1;
                    ESP_LOGD(__FUNCTION__,"%s currently has %d files and %d folders subfolder len:%d. Adding dir (%s)%d", path,fcnt,dcnt,fpos,kmlFileName,strlen(kmlFileName));
                }
            } else if ((ext == NULL) || (strlen(ext)==0) || endsWith(fi->d_name,ext)){
                sprintf(theFName,"%s/%s", path, fi->d_name);
                if ((excludeList == NULL) || (strcmp(fi->d_name,excludeList)!=0)){
                    gettimeofday(&tv_start, NULL);
                    if ((theFile=fopen(theFName,"r")) && 
                        (gettimeofday(&tv_open, NULL) == 0)){
                        
                        uint32_t startPos = tar->pos;
                        fcnt++;
                        bool headerWritten = false;
                        while ((tarStat == MTAR_ESUCCESS) && !feof(theFile)){
                            gettimeofday(&tv_rstart, NULL);
                            if ((len=fread(buf,1,F_BUF_SIZE,theFile))>0) {
                                gettimeofday(&tv_rend, NULL);
                                ESP_LOGV(__FUNCTION__, "%d read", len);

                                if (!headerWritten){
                                    headerWritten = true;
                                    if (len == F_BUF_SIZE){
                                        fstat(fileno(theFile),&fileStat);
                                        gettimeofday(&tv_stat, NULL);
                                        tarStat = mtar_write_file_header(tar, theFName+1, fileStat.st_size);
                                        ESP_LOGD(__FUNCTION__,"stat %s: %d files. file %s, ram %d len: %li", path,fcnt,fi->d_name,heap_caps_get_free_size(MALLOC_CAP_DEFAULT),fileStat.st_size);
                                    } else {
                                        tv_stat = tv_end;
                                        tarStat = mtar_write_file_header(tar, theFName+1, len);
                                        ESP_LOGD(__FUNCTION__,"full %s: %d files. file %s, ram %d len: %d", path,fcnt,fi->d_name,heap_caps_get_free_size(MALLOC_CAP_DEFAULT),len);
                                    }
                                }

                                gettimeofday(&tv_wstart, NULL);
                                tarStat = mtar_write_data(tar, buf, len);
                                gettimeofday(&tv_wend, NULL);
                            }
                            gettimeofday(&tv_end, NULL);
                            int64_t start_time_ms = (int64_t)tv_start.tv_sec * 1000L + ((int64_t)tv_start.tv_usec/1000);
                            int64_t end_time_ms = (int64_t)tv_end.tv_sec * 1000L + ((int64_t)tv_end.tv_usec/1000);
                            int64_t oend_time_ms = (int64_t)tv_open.tv_sec * 1000L + ((int64_t)tv_open.tv_usec/1000);
                            int64_t ostat_time_ms = (int64_t)tv_stat.tv_sec * 1000L + ((int64_t)tv_stat.tv_usec/1000);
                            int64_t rstart_time_ms = (int64_t)tv_rstart.tv_sec * 1000L + ((int64_t)tv_rstart.tv_usec/1000);
                            int64_t rend_time_ms = (int64_t)tv_rend.tv_sec * 1000L + ((int64_t)tv_rend.tv_usec/1000);
                            int64_t wstart_time_ms = (int64_t)tv_wstart.tv_sec * 1000L + ((int64_t)tv_wstart.tv_usec/1000);
                            int64_t wend_time_ms = (int64_t)tv_wend.tv_sec * 1000L + ((int64_t)tv_wend.tv_usec/1000);
                            ESP_LOGV(__FUNCTION__,"%s: Total Time: %f,Open Time: %f,Stat Time: %f,Read Time: %f,Write Time: %f, Len: %d, Rate %f/s ram %d, ", 
                                        path,
                                        (end_time_ms-start_time_ms)/1000.0,
                                        (oend_time_ms-start_time_ms)/1000.0,
                                        (ostat_time_ms-oend_time_ms)/1000.0,
                                        (rend_time_ms-rstart_time_ms)/1000.0,
                                        (wend_time_ms-wstart_time_ms)/1000.0,
                                        tar->pos-startPos, 
                                        (tar->pos-startPos)/((end_time_ms-start_time_ms)/1000.0) ,
                                        heap_caps_get_free_size(MALLOC_CAP_DEFAULT));
                        }
                        fclose(theFile);
                    } else {
                        ESP_LOGE(__FUNCTION__,"Cannot read %s",theFName);
                        ret=ESP_FAIL;
                        break;
                    }
                }
            }
        }
        closedir(theFolder);
        ESP_LOGD(__FUNCTION__,"%s has %d files and %d folders subfolder len:%d", path,fcnt,dcnt,fpos);
        uint32_t ctpos=0;
        while (dcnt-->0) {
            ESP_LOGV(__FUNCTION__,"%d-%s: Getting sub-folder(%d) %s",dcnt, path,ctpos,theFolders+ctpos);
            if ((ret = tarFiles(tar, theFolders+ctpos,ext,recursive,excludeList,maxSize)) != ESP_OK) {
                ESP_LOGW(__FUNCTION__,"Error invoking getSdFiles for %s", kmlFileName);
            }
            ctpos+=strlen(theFolders+ctpos)+1;
        }
    } else {
        ESP_LOGW(__FUNCTION__,"Error opening %s", path);
        ret=ESP_FAIL;
    }

    free(theFName);
    free(theFolders);
    free(buf);
    free(kmlFileName);
    deinitSPISDCard();
    return ret;
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
    memset(&tar,0,sizeof(tar));
    tar.read=tarRead;
    tar.close=tarClose;
    tar.seek=tarSeek;
    tar.write=tarWrite;

    httpd_resp_set_type(req, "application/x-tar");
    httpd_resp_set_hdr(req,"filename","trips.tar");

    if (initSPISDCard())
    {
        xEventGroupClearBits(eventGroup,TAR_BUFFER_FILLED);
        xEventGroupSetBits(eventGroup,TAR_BUFFER_SENT);
        xEventGroupClearBits(eventGroup,TAR_BUILD_DONE);
        xTaskCreate(sendTar,"sendTar",4096,NULL,5,NULL);

        sp.tarBuf=(uint8_t*)malloc(HTTP_BUF_SIZE);
        sp.sendBuf=NULL;
        sp.bufLen=0;
        sp.req=req;
        sp.len=0;

        cJSON* status = status_json();
        cJSON* config = config_json();
        if ((tarString(&tar,"status.json",cJSON_PrintUnformatted(status)) == ESP_OK) &&
            (tarString(&tar,"config.json",cJSON_PrintUnformatted(config)) == ESP_OK) &&
            (tarFiles(&tar,"/lfs","",true,"current.bin",1048576)==ESP_OK) && 
            (tarFiles(&tar,"/sdcard/logs","",true,NULL,1048576)==ESP_OK)) {
            ESP_LOGV(__FUNCTION__, "Finalizing tar");
            mtar_finalize(&tar);
            ESP_LOGV(__FUNCTION__, "Closing tar");
            mtar_close(&tar);
            deinitSPISDCard();
        } else {
            ESP_LOGE(__FUNCTION__,"Error sending trips");
        }
        cJSON_Delete(config);
        cJSON_Delete(status);
    } else  {
        ESP_LOGE(__FUNCTION__,"Cannot mount the fucking sd card");
        httpd_resp_send(req, "No Bueno", 8);
    }
    deinitSPISDCard();
    return ESP_FAIL;
}