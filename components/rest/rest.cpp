#include <sys/types.h>
#include <sys/socket.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_eth.h"
#include <esp_http_server.h>
#include "freertos/FreeRTOS.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp32/rom/md5_hash.h"
#include "rest.h"
#include "../wifi/station.h"
#include "../microtar/src/microtar.h"
#include "../../main/utils.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#define F_BUF_SIZE 2048

static httpd_handle_t server = NULL;
static uint8_t* img=NULL;
static uint32_t totLen=0;

static char* jsonbuf= (char*)malloc(8192);
static wifi_config* cfg;

static char mounts[3][6] = {"/kml","/sent","/"};

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

    update_partition = esp_ota_get_next_update_partition(NULL);
    ESP_LOGI(__FUNCTION__, "Writing to partition subtype %d at offset 0x%x",
            update_partition->subtype, update_partition->address);
    assert(update_partition != NULL);

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

static esp_err_t flashmode_handler(httpd_req_t *req)
{
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    if (configured->address == 0x00030000) {
        httpd_resp_send(req,"flashmode",9);
        return ESP_OK;
    }

    esp_partition_iterator_t  pi ;                                  // Iterator for find
    const esp_partition_t*    factory ;                             // Factory partition
    esp_err_t                 err ;

    pi = esp_partition_find ( ESP_PARTITION_TYPE_APP,               // Get partition iterator for
                              ESP_PARTITION_SUBTYPE_APP_FACTORY,    // factory partition
                              "factory" ) ;
    if ( pi == NULL )                                               // Check result
    {
        ESP_LOGE ( __FUNCTION__, "Failed to find factory partition" ) ;
    }
    else
    {
        factory = esp_partition_get ( pi ) ;                        // Get partition struct
        esp_partition_iterator_release ( pi ) ;                     // Release the iterator
        err = esp_ota_set_boot_partition ( factory ) ;              // Set partition for boot
        if ( err != ESP_OK )                                        // Check error
        {
                httpd_resp_send(req,"Nope",4);
                ESP_LOGE ( __FUNCTION__, "Failed to set boot partition" ) ;
        }
        else
        {
                httpd_resp_send(req,"Rebooting",9);
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_restart() ;                                         // Restart ESP
        }
    }
    return ESP_OK;
}

static esp_err_t ota_handler(httpd_req_t *req)
{
    const esp_partition_t *configured = esp_ota_get_boot_partition();
    char ret[30]="                             ";
    if (configured->address != 0x00030000) {
        uint32_t len = sprintf(ret,"bad offset 0x%08x",configured->address);
        httpd_resp_send(req,ret,len);
        return ESP_OK;
    }
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
    uint16_t port = 0;
    char md5[36];
    md5[0]=0;

    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGD(__FUNCTION__, "Found URL query => %s", buf);
            char param[33];
            if (httpd_query_key_value(buf, "port", param, sizeof(param)) == ESP_OK) {
                port=atoi(param);
                ESP_LOGI(__FUNCTION__, "Found URL query parameter => port=%s", param);
            }
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


    if (port && totLen && md5[0]) {
        ESP_LOGI(__FUNCTION__, "RAM:%d...", esp_get_free_heap_size());
        img = (uint8_t*)malloc(totLen);
        ESP_LOGI(__FUNCTION__, "RAM:%d...", esp_get_free_heap_size());

        char addr_str[128];
        int addr_family;
        int ip_protocol;

        struct sockaddr_in dest_addr;
        dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(dest_addr.sin_addr, addr_str, sizeof(addr_str) - 1);

        int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
        if (listen_sock < 0) {
            ESP_LOGE(__FUNCTION__, "Unable to create socket: errno %d", errno);
            vTaskDelete(NULL);
            return ESP_FAIL;
        }
        ESP_LOGD(__FUNCTION__, "Socket created");

        int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(__FUNCTION__, "Socket unable to bind: errno %d", errno);
        }
        ESP_LOGD(__FUNCTION__, "Socket bound, port %d", port);

        err = listen(listen_sock, 1);
        if (err != 0) {
            ESP_LOGE(__FUNCTION__, "Error occurred during listen: errno %d", errno);
        }

        ESP_LOGD(__FUNCTION__, "Socket listening");

        struct sockaddr_in6 source_addr; // Large enough for both IPv4 or IPv6
        uint addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(__FUNCTION__, "Unable to accept connection: errno %d", errno);
            return ESP_FAIL;
        }

        // Convert ip address to string
        if (source_addr.sin6_family == PF_INET) {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr.s_addr, addr_str, sizeof(addr_str) - 1);
        } else if (source_addr.sin6_family == PF_INET6) {
            inet6_ntoa_r(source_addr.sin6_addr, addr_str, sizeof(addr_str) - 1);
        }
        ESP_LOGD(__FUNCTION__, "Socket accepted ip address: %s", addr_str);

        int len=0;
        ESP_LOGI(__FUNCTION__, "RAM:%d...", esp_get_free_heap_size());
        uint8_t cmd5[33];
        uint8_t ccmd5[33];
        MD5Context md5_context;
        MD5Init(&md5_context);
        uint32_t curLen=0;

        do {
            len = recv(sock, img+curLen, MESSAGE_BUFFER_SIZE, 0);
            if (len < 0) {
                ESP_LOGE(__FUNCTION__, "Error occurred during receiving: errno %d", errno);
            } else if (len == 0) {
                ESP_LOGW(__FUNCTION__, "Connection closed...");
            } else {
                MD5Update(&md5_context, img+curLen, len);
                curLen+=len;
            }
        } while (len > 0);
        ESP_LOGD(__FUNCTION__,"Total: %d/%d",totLen,curLen);
        MD5Final(cmd5, &md5_context);

        for (uint8_t i = 0; i < 16; ++i) {
            sprintf((char*)&ccmd5[i * 2], "%02x", (unsigned int)cmd5[i]);
        }

        ESP_LOGD(__FUNCTION__,"md5:(%s)",ccmd5);
        ESP_LOGD(__FUNCTION__,"md5:(%s)",md5);

        MD5Init(&md5_context);
        MD5Update(&md5_context, img, totLen);
        MD5Final(cmd5, &md5_context);
        for (uint8_t i = 0; i < 16; ++i) {
            sprintf((char*)&ccmd5[i * 2], "%02x", (unsigned int)cmd5[i]);
        }

        ESP_LOGD(__FUNCTION__,"md5:(%s)%d",ccmd5,totLen);

        shutdown(sock, 0);
        close(sock);
        close(listen_sock);
        ESP_LOGI(__FUNCTION__, "RAM:%d...", esp_get_free_heap_size());

        if (strcmp((char*)ccmd5,(char*)md5) == 0) {
            httpd_resp_send(req, "Flashing", 8);
            vTaskDelay(pdMS_TO_TICKS(500));
            xTaskCreate(&flashTheThing,"flashTheThing",4096, NULL, 5, NULL);
            esp_wifi_stop();
            httpd_stop(&server);
        } else {
            httpd_resp_send(req, "Bad Checksum", 12);
        }
    } else {
        httpd_resp_send(req, "Not OK", 6);
    }
    return ESP_OK;
}

static void getPostField(const char* pname, const char* postData,char* dest) {
    char* param=strstr(postData,pname);
    uint16_t plen=strlen(pname)+1;
    if (param) {
        char* endPos=strstr(param,"&");
        if (endPos) {
            memcpy(dest,param+plen,endPos-param-plen);
            dest[endPos-param-plen]=0;
        } else {
            strcpy(dest,param+plen);
        }
    }
}

static void saveNvs(const char* partName, const char* paramName, void* blob,uint32_t sz) {
    ESP_LOGD(__FUNCTION__,"%s Opening nvs",__func__);
    nvs_handle my_handle;
    esp_err_t err;

    err = nvs_open(partName, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(__FUNCTION__, "%s Failed to open wnvs", __func__);
    } else {
        // Read the size of memory space required for blob
        ESP_LOGD(__FUNCTION__,"%s Opening blob %s",__func__,paramName);
        size_t required_size = 0;  // value will default to 0, if not set yet in NVS
        err = nvs_get_blob(my_handle, paramName, NULL, &required_size);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(__FUNCTION__,"%s Failed to get blob", __func__);
        } else {
            ESP_LOGD(__FUNCTION__,"%s config save %d %d",__func__,required_size,sz);
            err = nvs_set_blob(my_handle, paramName, blob, sz);
            if (err != ESP_OK) {
                ESP_LOGE(__FUNCTION__,"%s Failed to set blob %d",__func__,err);
            } else {
                err = nvs_commit(my_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(__FUNCTION__,"%s Failed to commit blob %d",__func__,err);
                } else {
                    nvs_close(my_handle);
                    ESP_LOGD(__FUNCTION__,"%s %s config saved",__func__,paramName);
                }
            }
        }
    }
}

static esp_err_t config_update(httpd_req_t *req)
{
//    int total_len = req->content_len;
//    char*  buf;
//    size_t buf_len;
//    int received = 0;
//    ESP_LOGD(__FUNCTION__, "Config Update!!!!!");
//
//    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
//    if (buf_len > 1) {
//        buf = (char*)malloc(buf_len);
//        /* Copy null terminated value string into buffer */
//        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
//            ESP_LOGD(__FUNCTION__, "Found header => Host: %s", buf);
//        }
//        free(buf);
//    }
//    buf = (char*)malloc(8192);
//    int cur_len=0;
//
//    while (cur_len < total_len) {
//        received = httpd_req_recv(req, buf + cur_len, total_len);
//        if (received <= 0) {
//            /* Respond with 500 Internal Server Error */
//            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to post control value");
//            return ESP_FAIL;
//        }
//        cur_len += received;
//    }
//    buf[total_len] = '\0';
//    getPostField("wifiname",buf,cfg->wname);
//    ESP_LOGD(__FUNCTION__,"%s %s---%s",__func__,"wifiname",cfg->wname);
//    getPostField("wifipwd",buf,cfg->wpdw);
//    ESP_LOGD(__FUNCTION__,"%s %s---%s",__func__,"wifipwd",cfg->wpdw);
//    char tmp[15];
//    tmp[0]=0;
//    getPostField("wifitype",buf,tmp);
//    if (strlen(tmp)){
//        cfg->wifi_mode=(wifi_mode_t)atoi(tmp);
//        ESP_LOGD(__FUNCTION__,"%s %s---%d",__func__,"wifitype",cfg->wifi_mode);
//    }
//    tmp[0]=0;
//    getPostField("wifi_workperiod",buf,tmp);
//    if (strlen(tmp)){
//        cfg->workPeriod=(uint32_t)atoi(tmp);
//        ESP_LOGD(__FUNCTION__,"%s %s---%d",__func__,"wifi_workperiod",cfg->workPeriod);
//    }
//    tmp[0]=0;
//    getPostField("wifi_scanperiod",buf,tmp);
//    if (strlen(tmp)){
//        cfg->scanPeriod=(uint32_t)atoi(tmp);
//        ESP_LOGD(__FUNCTION__,"%s %s---%d",__func__,"wifi_scanperiod",cfg->scanPeriod);
//    }
//    tmp[0]=0;
//    getPostField("bt_scanperiod",buf,tmp);
//    if (strlen(tmp)){
//        cfg->btc->scanPeriod=(uint32_t)atoi(tmp);
//        ESP_LOGD(__FUNCTION__,"%s %s---%d",__func__,"bt_scanperiod",cfg->btc->scanPeriod);
//    }
//    tmp[0]=0;
//    getPostField("bt_workperiod",buf,tmp);
//    if (strlen(tmp)){
//        cfg->btc->workPeriod=(uint32_t)atoi(tmp);
//        ESP_LOGD(__FUNCTION__,"%s %s---%d",__func__,"bt_workperiod",cfg->btc->workPeriod);
//    }
//
//    saveNvs("wnvs","wifi_config",cfg->,sizeof(wifi_config));
//    saveNvs("bnvs","bt_config",cfg->btc,sizeof(bt_config));
//    ESP_LOGD(__FUNCTION__,"%s %s---%d",__func__,"wifi_workperiod",cfg->workPeriod);
//
//    int idx=0;
//    for (cyberTiroir_config* ct = cfg->tiroirs[idx++]; idx<=2; ct = cfg->tiroirs[idx++]) {
//        tmp[0]=0;
//        jsonbuf[0]=0;
//        sprintf(jsonbuf,"ct%d_stepPin",idx);
//        getPostField(jsonbuf,buf,tmp);
//        ESP_LOGD(__FUNCTION__,"%s %s %s",__func__,jsonbuf,tmp);
//        if (strlen(tmp)){
//            ct->stepPin=(uint32_t)atoi(tmp);
//        }
//        ESP_LOGD(__FUNCTION__,"%s %s %d",__func__,jsonbuf,ct->stepPin);
//        jsonbuf[0]=0;
//        sprintf(jsonbuf,"ct%d_dirPin",idx);
//        getPostField(jsonbuf,buf,tmp);
//        if (strlen(tmp)){
//            ct->dirPin=(uint32_t)atoi(tmp);
//        }
//        ESP_LOGD(__FUNCTION__,"%s %s %d",__func__,jsonbuf,ct->stepPin);
//        jsonbuf[0]=0;
//        sprintf(jsonbuf,"ct%d_openPin",idx);
//        getPostField(jsonbuf,buf,tmp);
//        if (strlen(tmp)){
//            ct->openPin=(uint32_t)atoi(tmp);
//        }
//        ESP_LOGD(__FUNCTION__,"%s %s %d",__func__,jsonbuf,ct->stepPin);
//        jsonbuf[0]=0;
//        sprintf(jsonbuf,"ct%d_closePin",idx);
//        getPostField(jsonbuf,buf,tmp);
//        if (strlen(tmp)){
//            ct->closePin=(uint32_t)atoi(tmp);
//        }
//        ESP_LOGD(__FUNCTION__,"%s %s %d",__func__,jsonbuf,ct->stepPin);
//        jsonbuf[0]=0;
//        sprintf(jsonbuf,"ct%d_buttonPin",idx);
//        getPostField(jsonbuf,buf,tmp);
//        if (strlen(tmp)){
//            ct->buttonPin=(uint32_t)atoi(tmp);
//        }
//        ESP_LOGD(__FUNCTION__,"%s %s %d",__func__,jsonbuf,ct->stepPin);
//        jsonbuf[0]=0;
//        sprintf(jsonbuf,"ct%d_config",idx);
//        saveNvs("bnvs",jsonbuf,ct,sizeof(cyberTiroir_config));
//        ESP_LOGD(__FUNCTION__,"%s %s %d",__func__,jsonbuf,ct->stepPin);
//    }
//
//    tmp[0]=0;
//    getPostField("ct_resetPin",buf,tmp);
//    if (strlen(tmp)){
//        cfg->resetPin=(uint32_t)atoi(tmp);
//        ESP_LOGD(__FUNCTION__,"%s %s---%d",__func__,"ct_resetPin",cfg->resetPin);
//    }
//
//    httpd_resp_send(req, "OK", 2);
//    free(buf);
//    vTaskDelay(pdMS_TO_TICKS(1000));
//    esp_restart();
    return ESP_OK;
}

static esp_err_t config_get_handler(httpd_req_t *req)
{
    FF_DIR theFolder;
    uint32_t len=sprintf(jsonbuf,"{\"wifi\":\
                                    {\"type\":%d,\
                                    \"name\":\"%s\",\
                                    \"password\":\"%s\",\
                                    \"workperiod\":%d,\
                                    \"scanperiod\":%d},\
                                   ",
                                cfg->wifi_mode,
                                cfg->wname,
                                cfg->wpdw,
                                cfg->workPeriod,
                                cfg->scanPeriod
                                );

    httpd_resp_send_chunk(req, jsonbuf, len);
    if (initSPISDCard() && (f_opendir(&theFolder, "/") == FR_OK))
    {
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
                    ESP_LOGV(__FUNCTION__, "%s - %d", fi.fname, fi.fsize);
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
        }

        len=sprintf(jsonbuf,"]\n");
        httpd_resp_send_chunk(req, jsonbuf, len);

        if (esp_vfs_fat_sdmmc_unmount() == ESP_OK)
        {
            ESP_LOGD(__FUNCTION__, "Unmounted SD Card");
        } else {
            ESP_LOGE(__FUNCTION__, "Failed to unmount SD Card");
        }

        spi_bus_free((spi_host_device_t)getSDHost()->slot);
    } else {
        ESP_LOGD(__FUNCTION__,"Cannot mount the fucking sd card");
    }
    httpd_resp_send_chunk(req, "}", 2);
    httpd_resp_send_chunk(req, NULL, 0);
    httpd_resp_set_status(req,HTTPD_200);

    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    //extern const unsigned char status_html_start[] asm("_binary_status_html_start");
    //extern const unsigned char status_html_end[]   asm("_binary_status_html_end");
    //const size_t status_html_size = (status_html_end - status_html_start);
    //httpd_resp_set_type(req, "text/html");
    //httpd_resp_send(req, (const char *) status_html_start, status_html_size);
    return ESP_OK;
}

static esp_err_t uploadTripTar(httpd_req_t *req)
{
    FILE *fd = NULL;
    struct stat file_stat;

    const char *filename = "/sdcard/temp.tar";

    fd = fopen(filename, "r");

    if (stat(filename, &file_stat) == -1) {
        ESP_LOGE(__FUNCTION__, "Failed to stat file : %s", filename);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
        return ESP_FAIL;
    }

    if (!fd) {
        ESP_LOGE(__FUNCTION__, "Failed to read existing file : %s", filename);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
        return ESP_FAIL;
    }

    ESP_LOGD(__FUNCTION__, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
    httpd_resp_set_type(req, "application/x-tar");

    char *chunk = (char*)malloc(4096);
    size_t chunksize;
    size_t sentBytes=0;
    do {
        chunksize = fread(chunk, 1, 4096, fd);
        sentBytes+=chunksize;
        if (chunksize > 0) {
            if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
                fclose(fd);
                ESP_LOGE(__FUNCTION__, "File sending failed!");
                httpd_resp_sendstr_chunk(req, NULL);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                free(chunk);
                return ESP_FAIL;
           }
        }
    } while (chunksize != 0);
    free(chunk);
    fclose(fd);
    ESP_LOGD(__FUNCTION__, "File sending complete %ud",sentBytes);

    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t trips_handler(httpd_req_t *req)
{
    mtar_t tar;

    if (initSPISDCard())
    {
        if (mtar_open(&tar, "/sdcard/temp.tar", "w") == MTAR_ESUCCESS) {
            FF_DIR theFolder;
            FILE* theFile;
            uint32_t len=0;
            char fName[270];
            void* buf = malloc(F_BUF_SIZE);
            ESP_LOGD(__FUNCTION__, "Open %s", "/kml");
            FILINFO fi;
            uint32_t fileCount=0;

            if (f_opendir(&theFolder, "/kml") == FR_OK)
            {
                ESP_LOGD(__FUNCTION__, "reading sdcard files in %s", "/kml");
                while (f_readdir(&theFolder, &fi) == FR_OK)
                {
                    fileCount++;
                    if (strlen(fi.fname) == 0)
                    {
                        break;
                    }
                    if (!(fi.fattrib & AM_DIR)){
                        ESP_LOGD(__FUNCTION__, "%s - %d", fi.fname, fi.fsize);
                        mtar_write_file_header(&tar, fi.fname, fi.fsize);
                        sprintf(fName,"/sdcard/kml/%s",fi.fname);
                        if ((theFile=fopen(fName,"r"))!=NULL){
                            ESP_LOGV(__FUNCTION__, "%s opened", fName);
                            while (!feof(theFile)){
                                if ((len=fread(buf,1,F_BUF_SIZE,theFile))>0) {
                                    ESP_LOGV(__FUNCTION__, "%d written", len);
                                    mtar_write_data(&tar, buf, len);
                                }
                            }
                            fclose(theFile);
                        } else {
                            ESP_LOGE(__FUNCTION__,"Cannot read %s",fName);
                        }
                    }
                }
                f_closedir(&theFolder);
                mtar_finalize(&tar);
                mtar_close(&tar);
                uploadTripTar(req);
            } else {
                ESP_LOGE(__FUNCTION__,"Cannot open %s","/kml");
            }
            free(buf);
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot create tar file");
        }

        if (esp_vfs_fat_sdmmc_unmount() == ESP_OK)
        {
            ESP_LOGD(__FUNCTION__, "Unmounted SD Card");
        } else {
            ESP_LOGE(__FUNCTION__, "Failed to unmount SD Card");
        }

        spi_bus_free((spi_host_device_t)getSDHost()->slot);
        return ESP_OK;
    } else {
        ESP_LOGD(__FUNCTION__,"Cannot mount the fucking sd card");
    }
    httpd_resp_send(req, "No Buono", 9);

    return ESP_FAIL;
}

static const httpd_uri_t configUri = {
    .uri       = "/trips",
    .method    = HTTP_GET,
    .handler   = trips_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t configGetUri = {
    .uri       = "/rest/config",
    .method    = HTTP_GET,
    .handler   = config_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t configSetUri = {
    .uri       = "/config",
    .method    = HTTP_POST,
    .handler   = config_update,
    .user_ctx  = NULL
};

static const httpd_uri_t otaUri = {
    .uri       = "/ota",
    .method    = HTTP_POST,
    .handler   = ota_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t flashUri = {
    .uri       = "/flashmode",
    .method    = HTTP_POST,
    .handler   = flashmode_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t statusUri = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = status_handler,
    .user_ctx  = NULL
};

void restSallyForth(void *pvParameter) {
    cfg = (wifi_config*)pvParameter;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    vTaskDelay(pdMS_TO_TICKS(500));
    xEventGroupWaitBits(cfg->s_wifi_eg,WIFI_CONNECTED_BIT,pdFALSE,pdFALSE,portMAX_DELAY);

    ESP_LOGI(__FUNCTION__, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(__FUNCTION__, "Registering URI handlers");
        httpd_register_uri_handler(server, &statusUri);
        httpd_register_uri_handler(server, &otaUri);
        httpd_register_uri_handler(server, &configUri);
        httpd_register_uri_handler(server, &configGetUri);
        httpd_register_uri_handler(server, &configSetUri);
        httpd_register_uri_handler(server, &flashUri);
    } else {
        ESP_LOGI(__FUNCTION__, "Error starting server!");
    }

    vTaskDelete( NULL );
}

