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
            /* Copy null terminated value string into buffer */
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
    esp_err_t ret=ESP_OK;
    uint32_t sLen=strlen(res);
    ESP_LOGV(__FUNCTION__,"Parsing %s",path);
    char* theFolders = (char*)malloc(1024);
    memset(theFolders,0,1024);
    char* theFName = (char*)malloc(1024);
    memset(theFName,0,1024);
    uint32_t fpos=0;
    uint32_t fcnt=0;
    uint32_t dcnt=0;
    struct stat st;

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
                //if ((fcnt > 30) || stat(theFName,&st) == 0){
                    sLen+=sprintf(res+sLen,"{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d},",
                        path,
                        fi->d_name,
                        fi->d_type == DT_DIR ? "folder":"file",
                        (uint32_t)st.st_size
                    );
                //}
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
    ESP_LOGV(__FUNCTION__,"Getting %s url:%s",req->uri+11,req->uri);
    if (findFiles(req,(char*)(req->uri+11),NULL,false,jsonbuf,JSON_BUFFER_SIZE-1) != ESP_OK){
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

esp_err_t config_handler(httpd_req_t *req)
{
    esp_err_t ret = ESP_OK;
    app_config_t* appcfg=getAppConfig();
    app_state_t* appstate=getAppState();
    poiConfig_t* pc=appcfg->pois;
    char* pois = NULL;
    char* jsonbuf= (char*)malloc(JSON_BUFFER_SIZE);
    httpd_resp_set_type(req, "application/json");
    appcfg = getAppConfig();
    the_wifi_config* wcfg = getWifiConfig();
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
    return ret;
}
    
esp_err_t status_handler(httpd_req_t *req)
{
    esp_err_t ret = ESP_FAIL;
    app_config_t* appcfg=getAppConfig();
    app_state_t* appstate=getAppState();
    the_wifi_config* wcfg = getWifiConfig();


    cJSON* jresponse=NULL;
    cJSON* jitem=NULL;
    cJSON* status=NULL;
    cJSON* wifi=NULL;
    cJSON* jclients=NULL;

    Aper* client = NULL;
    char* path = (char*)req->uri+7;
    char* sjson = NULL;
    char* buf = (char*)malloc(255);
    char* postData = NULL;
    uint32_t idx=0,jsonlen=0;
    uint64_t upTime=0,sleepTime=0;
    poiConfig_t* pc;

    ESP_LOGV(__FUNCTION__,"uri:%s absolute: %s, method: %s",req->uri,path, req->method == HTTP_POST ? "POST" : "PUT" );

    if (req->method==HTTP_POST){
        httpd_resp_set_type(req, "application/json");
        status=cJSON_CreateObject();
        wifi=cJSON_CreateObject();
        jclients = cJSON_CreateArray();
        upTime=getUpTime();
        sleepTime=getSleepTime();

        sprintf(buf,"%02d:%02d:%02d",(uint32_t)floor(upTime/3600),(uint32_t)floor((upTime%3600)/60),(uint32_t)upTime%60);
        cJSON_AddStringToObject(status, "uptime", buf);
        sprintf(buf,"%02d:%02d:%02d",(uint32_t)floor(sleepTime/3600),(uint32_t)floor((sleepTime%3600)/60),(uint32_t)sleepTime%60);
        cJSON_AddStringToObject(status, "sleeptime", buf);
        cJSON_AddItemToObject(status, "freeram",  cJSON_CreateNumber(esp_get_free_heap_size()));
        cJSON_AddItemToObject(status, "totalram", cJSON_CreateNumber(heap_caps_get_total_size(MALLOC_CAP_DEFAULT)));
        cJSON_AddItemToObject(status, "battery",  cJSON_CreateNumber(getBatteryVoltage()));

        cJSON_AddStringToObject(wifi, "type", appcfg->purpose == app_config_t::purpose_t::PULLER?"AP":"Station");
        cJSON_AddStringToObject(wifi, "enabled", xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_UP_BIT?"yes":"no");
        cJSON_AddStringToObject(wifi, "connected", xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_CONNECTED_BIT?"yes":"no");
        cJSON_AddStringToObject(wifi, "scanning", xEventGroupGetBits(wcfg->s_wifi_eg)&WIFI_SCANING_BIT?"yes":"no");
        tcpip_adapter_ip_info_t* ipInfo=GetIpInfo();

        sprintf(buf,IPSTR, IP2STR(&ipInfo->ip));
        cJSON_AddStringToObject(wifi, "ip", buf);

        Aper** clients = GetClients();
        for (Aper* client = clients[idx++];idx < MAX_NUM_CLIENTS; client = clients[idx++]){
            ESP_LOGV(__FUNCTION__,"client(%d),%s",idx,client==NULL?"null":"not null");
            if (client) {
                cJSON_AddItemToArray(jclients,client->toJson());
            }
        }
        cJSON_AddItemToObject(wifi,"clients",jclients);

        cJSON_AddItemToObject(status,"wifi",wifi);
        if (strcmp(path,"/")==0) {
            sjson = cJSON_PrintUnformatted(status);
            ret= httpd_resp_send(req, sjson, strlen(sjson));
        }
        if (strcmp(path,"/wifi")==0) {
            sjson = cJSON_PrintUnformatted(wifi);
            ret= httpd_resp_send(req, sjson, strlen(sjson));
        }
    }
    if ((req->method==HTTP_PUT) || (startsWith(path,"/form") && (req->method==HTTP_POST))){
        if (strcmp(path,"/form")==0) {
            postData = (char*) malloc(JSON_BUFFER_SIZE);
            httpd_req_recv(req,postData,JSON_BUFFER_SIZE);
            char pval[100];
            char pname[100];
            appcfg = getAppConfig();
            appcfg->sdcard_config.MisoPin=(gpio_num_t)atoi(getPostField("MisoPin",postData,pval));
            appcfg->sdcard_config.MosiPin=(gpio_num_t)atoi(getPostField("MosiPin",postData,pval));
            appcfg->sdcard_config.ClkPin=(gpio_num_t)atoi(getPostField("ClkPin",postData,pval));
            appcfg->sdcard_config.Cspin=(gpio_num_t)atoi(getPostField("CsPin",postData,pval));

            if (appcfg->purpose == app_config_t::purpose_t::TRACKER) {
                appcfg->gps_config.enPin=(gpio_num_t)atoi(getPostField("enPin",postData,pval));
                appcfg->gps_config.txPin=(gpio_num_t)atoi(getPostField("txPin",postData,pval));
                appcfg->gps_config.rxPin=(gpio_num_t)atoi(getPostField("rxPin",postData,pval));
                idx=0;
                sprintf(pname,"syncpoints[%d].lat",idx);
                pc = appcfg->pois;
                while (getPostField(pname,postData,pval)) {
                    pc->lat=atof(pval);
                    sprintf(pname,"syncpoints[%d].lng",idx);
                    getPostField(pname,postData,pval);
                    pc->lng=atof(pval);
                    pc+=sizeof(poiConfig_t);
                    idx++;
                }
            }

            httpd_req_get_hdr_value_str(req,"Referer",pval,100);
            ESP_LOGV(__FUNCTION__,"Referer - %s",pval);
            saveConfig();
            ret=httpd_resp_send(req,"OK",2);
        }
        if (strcmp(path,"/wifi")==0) {
            postData = (char*) malloc(JSON_BUFFER_SIZE);
            int rlen=httpd_req_recv(req,postData,JSON_BUFFER_SIZE);
            if (rlen == 0) {
                httpd_resp_send_500(req);
                ESP_LOGE(__FUNCTION__,"no body");
            } else {
                ESP_LOGV(__FUNCTION__,"Got %s",postData);
                jresponse = cJSON_Parse(postData);
                if (jresponse != NULL) {
                    jitem = cJSON_GetObjectItemCaseSensitive(jresponse,"enabled");
                    if (jitem && (strcmp(jitem->valuestring,"no")==0)) {
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
                }
            }
        }
    }

    if (ret != ESP_OK){
        httpd_resp_send_500(req);
    }

    if (status) {
        cJSON_Delete(status);
    }

    if (jresponse) {
        cJSON_Delete(jresponse);
    }

    if (postData) {
        free(postData);
    }
    free(buf);

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
            ESP_LOGV(__FUNCTION__,"Sent final");
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

esp_err_t sendFile(httpd_req_t *req){
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
    sprintf(path,"%s",req->uri);
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