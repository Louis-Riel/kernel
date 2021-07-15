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
#include "../eventmgr/eventmgr.h"
#include "../mfile/mfile.h"
#include "../TinyGPS/TinyGPS++.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define F_BUF_SIZE 8192
#define HTTP_BUF_SIZE 8192
#define HTTP_CHUNK_SIZE 8192
#define HTTP_RECEIVE_BUFFER_SIZE 1024
#define JSON_BUFFER_SIZE 8192
#define KML_BUFFER_SIZE 204600

esp_err_t sendFile(httpd_req_t *req, const char* path);

void flashTheThing(uint8_t *img,uint32_t totLen)
{
    esp_ota_handle_t update_handle = 0;
    const esp_partition_t *update_partition = NULL;

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    const esp_partition_t *running = esp_ota_get_running_partition();

    if (configured != running)
    {
        ESP_LOGW(__FUNCTION__, "Configured OTA boot partition at offset 0x%08x, but running from offset 0x%08x",
                 configured->address, running->address);
        ESP_LOGW(__FUNCTION__, "(This can happen if either the OTA boot data or preferred boot image become corrupted somehow.)");
    }
    ESP_LOGD(__FUNCTION__, "Running partition type %d subtype %d (offset 0x%08x)",
             running->type, running->subtype, running->address);

    update_partition = esp_ota_get_next_update_partition(update_partition);

    bool isOnOta = false;
    if (update_partition->address == configured->address)
    {
        isOnOta = true;
        assert(update_partition != NULL);
        ESP_LOGD(__FUNCTION__, "Skipping partition subtype %d at offset 0x%x",
                 update_partition->subtype, update_partition->address);
    }
    assert(update_partition != NULL);
    ESP_LOGD(__FUNCTION__, "Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);

    if (initSPISDCard())
    {
        if (isOnOta)
        {
            ESP_LOGW(__FUNCTION__, "Wring partition to update from");
        }
        else
        {
            esp_err_t err = esp_ota_begin(update_partition, totLen, &update_handle);
            if (err == ESP_OK)
            {
                ESP_LOGD(__FUNCTION__, "esp_ota_begin succeeded %d ", totLen);
                err = esp_ota_write(update_handle, (const void *)img, totLen);
                if (err == ESP_OK)
                {
                    ESP_LOGI(__FUNCTION__, "esp_ota_write succeeded");
                    err = esp_ota_end(update_handle);
                    if (err == ESP_OK)
                    {
                        err = esp_ota_set_boot_partition(update_partition);
                        if (err == ESP_OK)
                        {
                            ESP_LOGI(__FUNCTION__, "esp_ota_set_boot_partition succeeded");
                            moveFile("/lfs/firmware/tobe.bin.md5", "/lfs/firmware/current.bin.md5");
                            deinitSPISDCard();
                            esp_restart();
                        }
                        else
                        {
                            ESP_LOGE(__FUNCTION__, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
                        }
                    }
                    else
                    {
                        ESP_LOGE(__FUNCTION__, "esp_ota_write failed");
                    }
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "esp_ota_write failed");
                }
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "esp_ota_begin failed (%s)", esp_err_to_name(err));
            }
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Failed in opeing /firmware/current.bin.md5");
    }
}

void UpgradeFirmware()
{
    const char *fwf = "/lfs/firmware/current.bin";
    const char *fmd = "/lfs/firmware/tobe.bin.md5";
    FILE *fw;
    struct stat st;

    if (stat(fmd, &st) == 0)
    {
        ESP_LOGD(__FUNCTION__, "Applying firmware upgrade");
        if (stat(fwf, &st) == 0)
        {
            ESP_LOGD(__FUNCTION__, "Opened %s", fwf);
            uint8_t *img = (uint8_t *)dmalloc(st.st_size);
            uint8_t *buf = (uint8_t *)dmalloc(F_BUF_SIZE);
            ESP_LOGV(__FUNCTION__, "Allocated %d buffer", F_BUF_SIZE);
            uint32_t len = 0;
            uint32_t totLen = 0;
            if ((fw = fopen(fwf, "r", true)) != NULL)
            {
                while (!feof(fw))
                {
                    if ((len = fread(buf, 1, F_BUF_SIZE, fw)) > 0)
                    {
                        memcpy(img + totLen, buf, len);
                        totLen += len;
                        ESP_LOGV(__FUNCTION__, "firmware readin %d bytes", totLen);
                    }
                }
                fclose(fw);
                flashTheThing(img, totLen);
                ldfree(img);
            }
            ESP_LOGD(__FUNCTION__, "firmware read %d bytes", totLen);
            deinitSPISDCard();
        }
    }
}

//char* kmlFileName=(char*)dmalloc(255);
float temperatureReadFixed()
{
    SET_PERI_REG_BITS(SENS_SAR_MEAS_WAIT2_REG, SENS_FORCE_XPD_SAR, 3, SENS_FORCE_XPD_SAR_S);
    SET_PERI_REG_BITS(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_CLK_DIV, 10, SENS_TSENS_CLK_DIV_S);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    CLEAR_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP_FORCE);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_POWER_UP);
    ets_delay_us(100);
    SET_PERI_REG_MASK(SENS_SAR_TSENS_CTRL_REG, SENS_TSENS_DUMP_OUT);
    ets_delay_us(5);
    float temp_f = (float)GET_PERI_REG_BITS2(SENS_SAR_SLAVE_ADDR3_REG, SENS_TSENS_OUT, SENS_TSENS_OUT_S);
    float temp_c = (temp_f - 32) / 1.8;
    return temp_c;
}

cJSON *tasks_json()
{
    volatile UBaseType_t numTasks = uxTaskGetNumberOfTasks();
    uint32_t totalRunTime;
    TaskStatus_t *statuses = (TaskStatus_t *)dmalloc(numTasks * sizeof(TaskStatus_t));
    cJSON *tasks = NULL;
    if (statuses != NULL)
    {
        numTasks = uxTaskGetSystemState(statuses, numTasks, &totalRunTime);
        if (totalRunTime > 0)
        {
            tasks = cJSON_CreateArray();
            for (uint32_t taskNo = 0; taskNo < numTasks; taskNo++)
            {
                cJSON *task = cJSON_CreateObject();
                cJSON_AddNumberToObject(task, "TaskNumber", statuses[taskNo].xTaskNumber);
                cJSON_AddStringToObject(task, "Name", statuses[taskNo].pcTaskName);
                cJSON_AddNumberToObject(task, "Priority", statuses[taskNo].uxCurrentPriority);
                cJSON_AddNumberToObject(task, "Runtime", statuses[taskNo].ulRunTimeCounter);
                cJSON_AddNumberToObject(task, "Core", statuses[taskNo].xCoreID > 100 ? -1 : statuses[taskNo].xCoreID);
                cJSON_AddNumberToObject(task, "State", statuses[taskNo].eCurrentState);
                cJSON_AddNumberToObject(task, "Stackfree", statuses[taskNo].usStackHighWaterMark * 4);
                cJSON_AddNumberToObject(task, "Pct", ((double)statuses[taskNo].ulRunTimeCounter / totalRunTime) * 100.0);
                cJSON_AddItemToArray(tasks, task);
            }
        }
        ldfree(statuses);
    }
    return tasks;
}

cJSON *status_json()
{
    ESP_LOGV(__FUNCTION__, "Status Handler");

    AppConfig *appcfg = AppConfig::GetAppConfig();
    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

    cJSON *status = cJSON_CreateObject();

    cJSON_AddItemToObject(status, "deviceid", cJSON_CreateNumber(appcfg->GetIntProperty("deviceid")));
    cJSON_AddNumberToObject(status, "uptime_sec", getUpTime());
    cJSON_AddNumberToObject(status, "sleeptime_us", getSleepTime());
    cJSON_AddItemToObject(status, "freeram", cJSON_CreateNumber(esp_get_free_heap_size()));
    cJSON_AddItemToObject(status, "totalram", cJSON_CreateNumber(heap_caps_get_total_size(MALLOC_CAP_DEFAULT)));
    cJSON_AddItemToObject(status, "battery", cJSON_CreateNumber(getBatteryVoltage()));
    cJSON_AddItemToObject(status, "temperature", cJSON_CreateNumber(temperatureReadFixed()));
    cJSON_AddItemToObject(status, "hallsensor", cJSON_CreateNumber(hall_sensor_read()));
    cJSON_AddItemToObject(status, "openfiles", cJSON_CreateNumber(GetNumOpenFiles()));
    cJSON_AddItemToObject(status, "runtime_ms", cJSON_CreateNumber(xTaskGetTickCount() * portTICK_PERIOD_MS));
    cJSON_AddItemToObject(status, "systemtime_us", cJSON_CreateNumber(time_us));
    return status;
}

esp_err_t stat_handler(httpd_req_t *req)
{
    char *fname = (char *)(req->uri + 5);
    if (req->method == HTTP_POST)
    {
        ESP_LOGV(__FUNCTION__, "Getting stats on %s", fname);
        struct stat st;
        esp_err_t ret = ESP_FAIL;
        size_t tlen;
        char *opeartion = NULL;
        char *fileType = NULL;

        ret = stat(fname, &st);

        if (ret == 0)
        {
            if ((tlen = httpd_req_get_hdr_value_len(req, "operation")) &&
                (opeartion = (char *)dmalloc(tlen + 1)) &&
                (httpd_req_get_hdr_value_str(req, "operation", opeartion, tlen + 1) == ESP_OK) &&
                (tlen = httpd_req_get_hdr_value_len(req, "ftype")) &&
                (fileType = (char *)dmalloc(tlen + 1)) &&
                (httpd_req_get_hdr_value_str(req, "ftype", fileType, tlen + 1) == ESP_OK))
            {
                if (opeartion[0] == 'd')
                {
                    ESP_LOGD(__FUNCTION__, "Deleting %s", fname);
                    if (fileType[0] == 'f')
                    {
                        ESP_LOGD(__FUNCTION__, "%s is a file", fname);
                        if (deleteFile(fname))
                        {
                            ret = httpd_resp_send(req, "OK", 3);
                        }
                        else
                        {
                            ret = httpd_resp_send_500(req);
                            ESP_LOGE(__FUNCTION__, "Failed to rm -fr %s", fname);
                        }
                    }
                    else
                    {
                        ESP_LOGD(__FUNCTION__, "%s is a folder", fname);
                        if (rmDashFR(fname))
                        {
                            ret = httpd_resp_send(req, "OK", 3);
                        }
                        else
                        {
                            ret = httpd_resp_send_500(req);
                            ESP_LOGE(__FUNCTION__, "Failed in rm -fr on %s", fname);
                        }
                    }
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "bad operation");
                    ret = httpd_resp_send_408(req);
                }
            }
            else
            {
                char *res = (char *)dmalloc(JSON_BUFFER_SIZE);
                char *path = (char *)dmalloc(255);
                strcpy(path, fname);
                char *fpos = strrchr(path, '/');
                *fpos = 0;
                httpd_resp_set_type(req, "application/json");
                ret = httpd_resp_send(req, res, sprintf(res, "{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d}", path, fpos + 1, "file", (uint32_t)st.st_size));
                ldfree(path);
                ldfree(res);
            }
            if (opeartion)
                ldfree(opeartion);
            if (fileType)
                ldfree(fileType);
            return ret;
        }
        return httpd_resp_send_500(req);
    }
    else
    {
        return httpd_resp_send_408(req);
    }
}

esp_err_t ota_handler(httpd_req_t *req)
{
    if (indexOf(req->uri, "/ota/flash") == req->uri)
    {
        char *buf;
        size_t buf_len;
        ESP_LOGD(__FUNCTION__, "OTA REQUEST!!!!! RAM:%d...", esp_get_free_heap_size());
        TinyGPSPlus* gps = TinyGPSPlus::runningInstance();
        if (gps) {
            gps->gpsStop();
        }

        buf_len = httpd_req_get_url_query_len(req) + 1;
        char md5[33];
        md5[0] = 0;
        uint32_t totLen=0;

        if (buf_len > 1)
        {
            buf = (char *)dmalloc(buf_len);
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
            {
                ESP_LOGV(__FUNCTION__, "Found URL query => %s", buf);
                char param[33];
                if (httpd_query_key_value(buf, "md5", param, sizeof(param)) == ESP_OK)
                {
                    strcpy(md5, param);
                    ESP_LOGV(__FUNCTION__, "Found URL query parameter => md5=%s", md5);
                }
                if (httpd_query_key_value(buf, "len", param, sizeof(param)) == ESP_OK)
                {
                    totLen = atoi(param);
                    ESP_LOGV(__FUNCTION__, "Found URL query parameter => len=%s", param);
                }
            } else {
                ESP_LOGE(__FUNCTION__,"Cannot get query");
                if (gps) {
                    gps->gpsStart();
                }
                return ESP_FAIL;
            }
            ldfree(buf);
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot get query len");
            if (gps) {
                gps->gpsStart();
            }
            return ESP_FAIL;
        }

        if (md5[0] && initSPISDCard())
        {
            FILE *fw = NULL;
            esp_err_t ret;
            ESP_LOGD(__FUNCTION__, "Reading MD5 RAM:%d...", esp_get_free_heap_size());
            if ((fw = fopen("/lfs/firmware/current.bin.md5", "r", true)) != NULL)
            {
                char ccmd5[33];
                uint32_t len = 0;
                if ((len = fread((void *)ccmd5, 1, 33, fw)) > 0)
                {
                    ccmd5[len]=0;
                    ESP_LOGD(__FUNCTION__,"Local MD5:%s",ccmd5);
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Error with weird md5 len %d", len);
                    httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Error with weird md5 len");
                }
                fClose(fw);
                if ((ret == ESP_OK) && (strcmp(ccmd5,md5) == 0)) {
                    if (gps) {
                        gps->gpsStart();
                    }

                    ESP_LOGD(__FUNCTION__, "Firmware is not updated RAM:%d", esp_get_free_heap_size());
                    return httpd_resp_send(req,"Not new",7);
                } else {
                    ESP_LOGD(__FUNCTION__, "Firmware will update RAM:%d...", esp_get_free_heap_size());
                }
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "Failed in opeing md5");
            }
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot init the fucking sd card or the md5:%d", md5[0]);
        }

        if (totLen && md5[0])
        {
            uint8_t *img = (uint8_t *)dmalloc(totLen); //heap_caps_malloc(totLen, MALLOC_CAP_SPIRAM);
            memset(img, 0, totLen);
            ESP_LOGV(__FUNCTION__, "RAM:%d...%d md5:%s", esp_get_free_heap_size(), totLen, md5);

            uint8_t cmd5[16];
            uint8_t ccmd5[33];
            MD5Context md5_context;
            MD5Init(&md5_context);
            uint32_t curLen = 0;
            int len = 0;

            do
            {
                len = httpd_req_recv(req, (char *)img + curLen, MESSAGE_BUFFER_SIZE);
                if (len < 0)
                {
                    ESP_LOGE(__FUNCTION__, "Error occurred during receiving: errno %d", errno);
                    break;
                }
                else if (len == 0)
                {
                    ESP_LOGW(__FUNCTION__, "Connection closed...");
                    break;
                }
                else if ((curLen+len) > totLen)
                {
                    ESP_LOGW(__FUNCTION__, "Bad len at (curLen(%d+%d) > totLen(%d)", curLen, len, totLen);
                    break;
                }
                else
                {
                    MD5Update(&md5_context, img + curLen, len);
                    curLen+=len;
                }
            } while (curLen < totLen);
            MD5Final(cmd5, &md5_context);
            ESP_LOGV(__FUNCTION__, "Total: %d/%d %s", totLen, curLen, md5);

            for (uint8_t i = 0; i < 16; ++i)
            {
                sprintf((char *)&ccmd5[i * 2], "%02x", (unsigned int)cmd5[i]);
            }

            FILE* fw;
            if (strcmp((char *)ccmd5, md5) == 0)
            {
                ESP_LOGD(__FUNCTION__, "Flashing md5:(%s)%dvs%d", ccmd5, totLen, curLen);
                ESP_LOGV(__FUNCTION__, "RAM:%d", esp_get_free_heap_size());

                struct stat st;

                if (stat("/lfs/firmware/current.bin", &st) == 0)
                {
                    ESP_LOGV(__FUNCTION__,"current bin exists %d bytes",(int)st.st_size);
                    if (!deleteFile("/lfs/firmware/current.bin")){
                        ESP_LOGE(__FUNCTION__,"Cannot delete current bin");
                    }
                }

                if ((fw = fopen("/lfs/firmware/current.bin", "w", true)) != NULL)
                {
                    ESP_LOGD(__FUNCTION__, "Writing /lfs/firmware/current.bin");
                    httpd_resp_send(req, "Flashing", 8);
                    if (fwrite((void *)img, 1, totLen, fw) == totLen)
                    {
                        fClose(fw);

                        if ((fw = fopen("/lfs/firmware/tobe.bin.md5", "w", true)) != NULL)
                        {
                            fwrite((void *)ccmd5, 1, sizeof(ccmd5), fw);
                            fClose(fw);
                            ESP_LOGD(__FUNCTION__, "Firmware md5 written");
                        } else {
                            ESP_LOGE(__FUNCTION__, "Cannot open /lfs/firmware/tobe.bin.md5");
                            httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot open /lfs/firmware/tobe.bin.md5");
                        }

                        esp_partition_iterator_t pi;    // Iterator for find
                        const esp_partition_t *factory; // Factory partition
                        esp_err_t err;

                        pi = esp_partition_find(ESP_PARTITION_TYPE_APP,            // Get partition iterator for
                                                ESP_PARTITION_SUBTYPE_APP_FACTORY, // factory partition
                                                "factory");
                        if (pi == NULL) // Check result
                        {
                            ESP_LOGE(__FUNCTION__, "Failed to find factory partition");
                            httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Failed to find factory partition");
                        }
                        else
                        {
                            factory = esp_partition_get(pi);           // Get partition struct
                            esp_partition_iterator_release(pi);        // Release the iterator
                            err = esp_ota_set_boot_partition(factory); // Set partition for boot
                            if (err != ESP_OK)                         // Check error
                            {
                                ESP_LOGE(__FUNCTION__, "Failed to set boot partition");
                            }
                            else
                            {
                                esp_restart(); // Restart ESP
                            }
                        }

                    }
                    else
                    {
                        ESP_LOGE(__FUNCTION__, "Firmware not backedup");
                        httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Firmware not backedup");
                    }
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Failed to open /lfs/firmware/current.bin");
                    httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"Failed to open /lfs/firmware/current.bin");
                }
                deinitSPISDCard();
                ldfree(img);
            }
            else
            {
                ESP_LOGV(__FUNCTION__, "md5:(%s)(%s)", ccmd5, md5);
                httpd_resp_send(req, "Bad Checksum", 12);
            }
            if (gps) {
                gps->gpsStart();
            }
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "Missing len %d or md5 %d",totLen,md5[0]);
            httpd_resp_send(req, "Missing len or md5", 18);
        }
        return ESP_OK;
    }
    else if (indexOf(req->uri, "/ota/getmd5") == req->uri)
    {
        if (initSPISDCard())
        {
            FILE *fw = NULL;
            if ((fw = fopen("/lfs/firmware/current.bin.md5", "r", true)) != NULL)
            {
                char ccmd5[33];
                uint32_t len = 0;
                if ((len = fread((void *)ccmd5, 1, 33, fw)) > 0)
                {
                    ccmd5[len]=0;
                    esp_err_t ret;
                    if ((ret = httpd_resp_send(req, ccmd5, 33)) != ESP_OK) {
                        ESP_LOGE(__FUNCTION__,"Error sending MD5:%s",esp_err_to_name(ret));
                    }
                    ESP_LOGD(__FUNCTION__,"Sent MD5:%s",ccmd5);
                    fClose(fw);
                    deinitSPISDCard();
                    return ret;
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Error with weird md5 len %d", len);
                }
                fClose(fw);
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "Failed in opeing md5");
            }
            deinitSPISDCard();
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot init the fucking sd card");
        }
    }
    httpd_resp_send(req, "BADMD5", 6);
    return ESP_FAIL;
}

esp_err_t findFiles(httpd_req_t *req, char *path, const char *ext, bool recursive, char *res, uint32_t resLen)
{
    if ((path == NULL) || (strlen(path) == 0))
    {
        return ESP_FAIL;
    }
    if (strcmp(path, "/") == 0)
    {
        sprintf(res, "[{\"name\":\"sdcard\",\"ftype\":\"folder\",\"size\":0},{\"name\":\"lfs\",\"ftype\":\"folder\",\"size\":0}]");
        return ESP_OK;
    }
    if (!initSPISDCard())
    {
        return ESP_FAIL;
    }
    esp_err_t ret = ESP_OK;
    uint32_t sLen = strlen(res);
    ESP_LOGD(__FUNCTION__, "Parsing %s", path);
    char *theFolders = (char *)dmalloc(1024);
    memset(theFolders, 0, 1024);
    char *theFName = (char *)dmalloc(1024);
    memset(theFName, 0, 1024);
    uint32_t fpos = 0;
    uint32_t fcnt = 0;
    uint32_t dcnt = 0;
    struct stat st;
    char *kmlFileName = (char *)dmalloc(1024);

    DIR *theFolder;
    struct dirent *fi;
    if ((theFolder = opendir(path)) != NULL)
    {
        while ((fi = readdir(theFolder)) != NULL)
        {
            if (strlen(fi->d_name) == 0)
            {
                break;
            }
            if (fi->d_type == DT_DIR)
            {
                if (recursive)
                {
                    dcnt++;
                    sprintf(kmlFileName, "%s/%s", path, fi->d_name);
                    fpos += sprintf(theFolders + fpos, "%s", kmlFileName) + 1;
                    ESP_LOGV(__FUNCTION__, "%s currently has %d files and %d folders subfolder len:%d. Adding dir %s", path, fcnt, dcnt, fpos, kmlFileName);
                }
                if ((ext == NULL) || (strlen(ext) == 0))
                {
                    if (sLen > (resLen - 100))
                    {
                        ESP_LOGV(__FUNCTION__, "Buffer Overflow, flushing");
                        if ((ret = httpd_resp_send_chunk(req, res, sLen)) != ESP_OK)
                        {
                            ESP_LOGE(__FUNCTION__, "Error sending chunk %s sLenL%d, actuallen:%d", esp_err_to_name(ret), sLen, strlen(res));
                            break;
                        }
                        memset(res, 0, resLen);
                        sLen = 0;
                    }

                    sLen += sprintf(res + sLen, "{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d},",
                                    path,
                                    fi->d_name,
                                    fi->d_type == DT_DIR ? "folder" : "file",
                                    0);
                }
            }
            else if ((ext == NULL) || (strlen(ext) == 0) || endsWith(fi->d_name, ext))
            {
                fcnt++;
                ESP_LOGV(__FUNCTION__, "%s currently has %d files and %d folders subfolder len:%d. Adding file %s", path, fcnt, dcnt, fpos, fi->d_name);
                if (sLen > (resLen - 100))
                {
                    ESP_LOGV(__FUNCTION__, "Buffer Overflow, flushing");
                    if ((ret = httpd_resp_send_chunk(req, res, sLen)) != ESP_OK)
                    {
                        ESP_LOGE(__FUNCTION__, "Error sending chunk %s sLenL%d, actuallen:%d", esp_err_to_name(ret), sLen, strlen(res));
                        break;
                    }
                    memset(res, 0, resLen);
                    sLen = 0;
                }
                sprintf(theFName, "%s/%s", path, fi->d_name);
                st.st_size = 0;
                sLen += sprintf(res + sLen, "{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d},",
                                path,
                                fi->d_name,
                                fi->d_type == DT_DIR ? "folder" : "file",
                                (uint32_t)st.st_size);
            }
        }
        closedir(theFolder);
        ESP_LOGV(__FUNCTION__, "%s has %d files and %d folders subfolder len:%d", path, fcnt, dcnt, fpos);
        uint32_t ctpos = 0;
        while (dcnt-- > 0)
        {
            ESP_LOGV(__FUNCTION__, "%d-%s: Getting sub-folder(%d) %s", dcnt, path, ctpos, theFolders + ctpos);
            if (findFiles(req, theFolders + ctpos, ext, recursive, res, resLen) != ESP_OK)
            {
                ESP_LOGW(__FUNCTION__, "Error invoking getSdFiles for %s", kmlFileName);
            }
            ctpos += strlen(theFolders) + 1;
        }
    }
    else
    {
        ESP_LOGW(__FUNCTION__, "Error opening %s:%s", path, esp_err_to_name(errno));
        ret = ESP_FAIL;
    }

    ldfree(theFName);
    ldfree(theFolders);
    ldfree(kmlFileName);
    deinitSPISDCard();
    return ret;
}

esp_err_t list_entity_handler(httpd_req_t *req)
{
    char *jsonbuf = (char *)dmalloc(JSON_BUFFER_SIZE);
    memset(jsonbuf, 0, JSON_BUFFER_SIZE);
    *jsonbuf = '[';
    ESP_LOGD(__FUNCTION__, "Getting %s url:%s", req->uri + 11, req->uri);
    if (endsWith(req->uri, "kml"))
    {
        if (findFiles(req, "/kml", "kml", true, jsonbuf, JSON_BUFFER_SIZE - 1) != ESP_OK)
        {
            ESP_LOGE(__FUNCTION__, "Error wilst sending kml list");
            ldfree(jsonbuf);
            return httpd_resp_send_500(req);
        }
    }
    else if (endsWith(req->uri, "csv"))
    {
        ESP_LOGD(__FUNCTION__, "Getting csv url:%s", req->uri);
        if (findFiles(req, "/lfs/csv", "csv", false, jsonbuf, JSON_BUFFER_SIZE) != ESP_OK)
        {
            ESP_LOGE(__FUNCTION__, "Error wilst sending csv list");
            ldfree(jsonbuf);
            return httpd_resp_send_500(req);
        }
    }
    else if (endsWith(req->uri, "log"))
    {
        if (findFiles(req, "/sdcard/logs", "log", false, jsonbuf, JSON_BUFFER_SIZE) != ESP_OK)
        {
            ESP_LOGE(__FUNCTION__, "Error wilst sending log list");
            ldfree(jsonbuf);
            return httpd_resp_send_500(req);
        }
    }
    if (strlen(jsonbuf) > 1)
        *(jsonbuf + strlen(jsonbuf) - 1) = ']';
    else
    {
        sprintf(jsonbuf, "%s", "[]");
    }

    httpd_resp_send_chunk(req, jsonbuf, strlen(jsonbuf));
    ESP_LOGV(__FUNCTION__, "Sent final chunck of %d", strlen(jsonbuf));
    ldfree(jsonbuf);
    return httpd_resp_send_chunk(req, NULL, 0);
}

esp_err_t list_files_handler(httpd_req_t *req)
{
    char *jsonbuf = (char *)dmalloc(JSON_BUFFER_SIZE);
    memset(jsonbuf, 0, JSON_BUFFER_SIZE);
    *jsonbuf = '[';
    ESP_LOGV(__FUNCTION__, "Getting %s url:%s", req->uri + 6, req->uri);
    if (findFiles(req, (char *)(req->uri + 6), NULL, false, jsonbuf, JSON_BUFFER_SIZE - 1) != ESP_OK)
    {
        ESP_LOGE(__FUNCTION__, "Error wilst sending file list");
        ldfree(jsonbuf);
        return httpd_resp_send_500(req);
    }
    if (strlen(jsonbuf) > 1)
        *(jsonbuf + strlen(jsonbuf) - 1) = ']';
    else
    {
        sprintf(jsonbuf, "%s", "[]");
    }
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, jsonbuf, strlen(jsonbuf));
    ESP_LOGV(__FUNCTION__, "Sent final chunck of %d", strlen(jsonbuf));
    ldfree(jsonbuf);
    return ret;
}

void UpdateGpioProp(cfg_gpio_t *cfg, gpio_num_t val)
{
    if (cfg->value != val)
    {
        ESP_LOGV(__FUNCTION__, "Updating from %d to %d", cfg->value, val);
        cfg->value = val;
        cfg->version++;
    }
}

void UpdateStringProp(cfg_label_t *cfg, char *val)
{
    if ((val == NULL) || (strcmp(cfg->value, val) != 0))
    {
        strcpy(cfg->value, val == NULL ? "" : val);
        cfg->version++;
    }
}

esp_err_t config_handler(httpd_req_t *req)
{
    ESP_LOGV(__FUNCTION__, "Config Handler");
    esp_err_t ret = ESP_OK;
    AppConfig *appcfg = AppConfig::GetAppConfig();

    httpd_resp_set_type(req, "application/json");

    char *postData = (char *)dmalloc(JSON_BUFFER_SIZE);
    int len = 0, curLen=-1;
    
    while ((curLen=httpd_req_recv(req, postData+len, JSON_BUFFER_SIZE))>0){
        len+=curLen;
    }
    int devId = appcfg->GetIntProperty("deviceid");

    if (!endsWith(req->uri,"config")){
        devId = atoi(indexOf(req->uri+1,"/")+1);
    }

    if (len)
    {
        *(postData + len) = 0;
        ESP_LOGD(__FUNCTION__, "postData(%d):%s", len, postData);
        cJSON* newCfg = cJSON_Parse(postData);
        if (newCfg){
            char* sjson = cJSON_Print(newCfg);
            if (devId == appcfg->GetIntProperty("deviceid")){
                ESP_LOGD(__FUNCTION__, "Updating local config");
                appcfg->SetAppConfig(newCfg);
            } else {
                char* fname = (char*)dmalloc(255);
                sprintf(fname,"/lfs/config/%d.json",devId);
                ESP_LOGD(__FUNCTION__, "Updating %s config", fname);
                FILE* cfg = fopen(fname,"w");
                if (cfg) {
                    fwrite(sjson,strlen(sjson),sizeof(char),cfg);
                    ret = httpd_resp_send(req, sjson, strlen(sjson));
                    fclose(cfg);
                } else {
                    httpd_resp_send_err(req,httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot save config");
                }
                ldfree(fname);
            }
            cJSON_Delete(newCfg);
            ldfree(sjson);
        } else {
            ESP_LOGE(__FUNCTION__,"Cannot parse JSON");
            ret = httpd_resp_send_err(req, httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot parse JSON");
        }
    }
    else
    {
        if (devId == appcfg->GetIntProperty("deviceid")){
            char *sjson = cJSON_PrintUnformatted(appcfg->GetJSONConfig(NULL));
            ret = httpd_resp_send(req, sjson, strlen(sjson));
            ldfree(sjson);
        } else {
            char* fname = (char*)dmalloc(255);
            sprintf(fname,"/lfs/config/%d.json", devId);
            ret = sendFile(req,fname);
        }
    }
    ldfree(postData);

    return ret;
}

esp_err_t HandleWifiCommand(httpd_req_t *req)
{
    esp_err_t ret = 0;
    char *postData = (char *)dmalloc(JSON_BUFFER_SIZE);
    int rlen = httpd_req_recv(req, postData, JSON_BUFFER_SIZE);
    if (rlen == 0)
    {
        httpd_resp_send_500(req);
        ESP_LOGE(__FUNCTION__, "no body");
    }
    else
    {
        *(postData + rlen) = 0;
        ESP_LOGD(__FUNCTION__, "Got %s", postData);
        cJSON *jresponse = cJSON_Parse(postData);
        if (jresponse != NULL)
        {
            cJSON *jitem = cJSON_GetObjectItemCaseSensitive(jresponse, "enabled");
            if (jitem && (strcmp(jitem->valuestring, "no") == 0))
            {
                ESP_LOGD(__FUNCTION__, "All done wif wifi");
                ret = httpd_resp_send(req, "OK", 2);
                TheWifi::GetInstance()->wifiStop(NULL);
            }
            cJSON_Delete(jresponse);
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "Error whilst parsing json");
        }
    }
    ldfree(postData);
    xEventGroupSetBits(*getAppEG(), app_bits_t::TRIPS_COMMITTED);

    return ret;
}

void parseFolderForTars(char *folder)
{
    ESP_LOGD(__FUNCTION__, "Looking for tars in %s", folder);
    DIR *tarFolder;
    dirent *di;
    char *fileName = (char *)dmalloc(300);
    if ((tarFolder = opendir(folder)))
    {
        while ((di = readdir(tarFolder)) != NULL)
        {
            ESP_LOGV(__FUNCTION__, "tarlist:%s", di->d_name);
            if (di->d_type == DT_DIR)
            {
                sprintf(fileName, "%s/%s", folder, di->d_name);
                ESP_LOGV(__FUNCTION__, "folder:%s", fileName);
                parseFolderForTars(fileName);
            }
            else
            {
                sprintf(fileName, "%s/%s", folder, di->d_name);
                ESP_LOGV(__FUNCTION__, "filelist:%s", fileName);
                extractClientTar(fileName);
                unlink(fileName);
            }
        }
        closedir(tarFolder);
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot open %s", folder);
    }
    ldfree(fileName);
}

void parseFiles(void *param)
{
    parseFolderForTars("/sdcard/tars");
    xTaskCreate(commitTripToDisk, "commitTripToDisk", 8192, (void *)(BIT2 | BIT3), tskIDLE_PRIORITY, NULL);
    vTaskDelete(NULL);
}

esp_err_t HandleSystemCommand(httpd_req_t *req)
{
    esp_err_t ret = 0;
    char *postData = (char *)dmalloc(JSON_BUFFER_SIZE);
    int rlen = httpd_req_recv(req, postData, JSON_BUFFER_SIZE);
    if (rlen == 0)
    {
        httpd_resp_send_500(req);
        ESP_LOGE(__FUNCTION__, "no body");
    }
    else
    {
        *(postData + rlen) = 0;
        ESP_LOGD(__FUNCTION__, "Got %s", postData);
        cJSON *jresponse = cJSON_Parse(postData);
        if (jresponse != NULL)
        {
            cJSON *jitem = cJSON_GetObjectItemCaseSensitive(jresponse, "command");
            if (jitem && (strcmp(jitem->valuestring, "reboot") == 0))
            {
                esp_restart();
            }
            if (jitem && (strcmp(jitem->valuestring, "parseFiles") == 0))
            {
                xTaskCreate(parseFiles, "parseFiles", 8192, NULL, tskIDLE_PRIORITY, NULL);
                ret = httpd_resp_send(req, "OK", 2);
            }
            else if (jitem && (strcmp(jitem->valuestring, "factoryReset") == 0))
            {
                AppConfig::ResetAppConfig(true);
                ret = httpd_resp_send(req, "OK", 2);
            }
            else
            {
                ret = httpd_resp_send_408(req);
            }
            cJSON_Delete(jresponse);
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "Error whilst parsing json");
            ret = httpd_resp_send_err(req, httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR,"Error whilst parsing json");
        }
    }
    ldfree(postData);
    return ret;
}

esp_err_t status_handler(httpd_req_t *req)
{
    ESP_LOGV(__FUNCTION__, "Status Handler");
    esp_err_t ret = ESP_FAIL;

    char *path = (char *)req->uri + 8;
    ESP_LOGV(__FUNCTION__, "uri:%s method: %s path:%s", req->uri, req->method == HTTP_POST ? "POST" : "PUT", path);

    if (req->method == HTTP_POST)
    {
        httpd_resp_set_type(req, "application/json");
        cJSON *status = NULL;
        char *sjson = NULL;
        if (strlen(path) == 0)
        {
            status = status_json();
            ESP_LOGV(__FUNCTION__, "Getting root");
            sjson = cJSON_PrintUnformatted(status);
            cJSON_Delete(status);
        }
#ifdef DEBUG_MALLOC
        else if (strcmp(path, "mallocs") == 0)
        {
            status = getMemoryStats();
            ESP_LOGV(__FUNCTION__, "Getting mallocs");
            sjson = cJSON_PrintUnformatted(status);
            cJSON_Delete(status);
        }
#endif
        else if (strcmp(path, "tasks") == 0)
        {
            status = tasks_json();
            ESP_LOGV(__FUNCTION__, "Getting tasks");
            sjson = cJSON_PrintUnformatted(status);
            cJSON_Delete(status);
        }
        else if (strcmp(path, "app") == 0)
        {
            sjson = cJSON_PrintUnformatted(AppConfig::GetAppStatus()->GetJSONConfig(NULL));
        }
        else
        {
            sjson = cJSON_PrintUnformatted(AppConfig::GetAppStatus()->GetJSONConfig(path));
        }
        if ((sjson != NULL) && (strlen(sjson) > 0))
        {
            ret = httpd_resp_send(req, sjson, strlen(sjson));
        }
        else
        {
            ret = httpd_resp_send_404(req);
        }
        ldfree(sjson);
    }
    if (req->method == HTTP_PUT)
    {
        if (endsWith(req->uri, "/wifi"))
        {
            ret = HandleWifiCommand(req);
        }
        else if (endsWith(req->uri, "/cmd"))
        {
            ret = HandleSystemCommand(req);
        }
        else
        {
            ESP_LOGW(__FUNCTION__, "Unimplemented methed:%s", req->uri);
        }
    }

    if (ret != ESP_OK)
    {
        httpd_resp_send_500(req);
    }
    ESP_LOGV(__FUNCTION__, "Done Getting");

    return ret;
}

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

void sendTar(void *param)
{
    size_t len = 0;
    esp_err_t ret;
    while (xEventGroupWaitBits(eventGroup, TAR_BUFFER_FILLED, pdFALSE, pdTRUE, portMAX_DELAY))
    {
        if (sp.sendLen > 0)
        {
            //printf(".");
            ESP_LOGV(__FUNCTION__, "Sending chunk of %d", sp.sendLen);
            len += sp.sendLen;
            ret = httpd_resp_send_chunk(sp.req, (const char *)sp.sendBuf, sp.sendLen);

            if (ret == ESP_OK)
            {
                sp.sendLen = 0;
                xEventGroupClearBits(eventGroup, TAR_BUFFER_FILLED);
                xEventGroupSetBits(eventGroup, TAR_BUFFER_SENT);
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "Chunk len %d won't go: %s", sp.sendLen, esp_err_to_name(ret));
                xEventGroupSetBits(eventGroup, TAR_SEND_DONE);
                break;
            }
        }
        if (xEventGroupGetBits(eventGroup) & TAR_BUILD_DONE)
        {
            break;
        }
    }
    httpd_resp_send_chunk(sp.req, NULL, 0);
    ESP_LOGD(__FUNCTION__, "Tar sent %d bytes", len);
    xEventGroupSetBits(eventGroup, TAR_SEND_DONE);
    vTaskDelete(NULL);
}

int tarRead(mtar_t *tar, void *data, unsigned size)
{
    ESP_LOGE(__FUNCTION__, "Cannot read");
    return ESP_FAIL;
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
        if (xEventGroupGetBits(eventGroup) & TAR_SEND_DONE)
        {
            ESP_LOGE(__FUNCTION__, "SendeR Died");
            return ESP_FAIL;
        }

        xEventGroupWaitBits(eventGroup, TAR_BUFFER_SENT, pdTRUE, pdTRUE, portMAX_DELAY);
        ESP_LOGV(__FUNCTION__, "Preparing chunck of %d", sp.bufLen);
        sp.sendLen = sp.bufLen;
        if (sp.sendBuf == NULL)
        {
            sp.sendBuf = (uint8_t *)dmalloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf, (const void *)sp.tarBuf, sp.bufLen);
        sp.bufLen = 0;
        xEventGroupSetBits(eventGroup, TAR_BUFFER_FILLED);
    }
    if (size > 1)
    {
        memcpy(sp.tarBuf + sp.bufLen, data, size);
        sp.bufLen += size;
        sp.len += size;
        ESP_LOGV(__FUNCTION__, "chunck: %d buflen:%d tot:%d", size, sp.bufLen, sp.len);
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

int tarClose(mtar_t *tar)
{
    xEventGroupWaitBits(eventGroup, TAR_BUFFER_SENT, pdTRUE, pdTRUE, portMAX_DELAY);
    xEventGroupSetBits(eventGroup, TAR_BUILD_DONE);
    sp.sendLen = sp.bufLen;
    if (sp.sendLen > 0)
    {
        if (sp.sendBuf == NULL)
        {
            sp.sendBuf = (uint8_t *)dmalloc(HTTP_BUF_SIZE);
        }
        memcpy(sp.sendBuf, (const void *)sp.tarBuf, sp.bufLen);
        ESP_LOGV(__FUNCTION__, "Sending final chunk of %d", sp.bufLen);
    }
    xEventGroupSetBits(eventGroup, TAR_BUFFER_FILLED);
    xEventGroupWaitBits(eventGroup, TAR_SEND_DONE, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGD(__FUNCTION__, "Wrote %d bytes", sp.len);
    if (sp.sendBuf != NULL)
        ldfree(sp.sendBuf);
    if (sp.tarBuf != NULL)
        ldfree(sp.tarBuf);
    return ESP_OK;
}

esp_err_t sendFile(httpd_req_t *req, const char* path)
{
    bool moveTheSucker = false;
    if (httpd_req_get_hdr_value_len(req, "movetosent") > 1)
    {
        moveTheSucker = true;

        if (moveTheSucker)
        {
            if (endsWith(path, "log"))
            {
                char *clfn = getLogFName();
                if (!clfn && (strlen(clfn) > 0) && endsWith(clfn, path))
                {
                    ESP_LOGD(__FUNCTION__, "Not moving %s as is it active(%s)", path, clfn);
                    moveTheSucker = false;
                }
                else
                {
                    ESP_LOGD(__FUNCTION__, "Will move %s as it is not active trip %s", path, clfn);
                }
            }
        }
    }
    else
    {
        ESP_LOGD(__FUNCTION__, "No move in request");
    }
    if (endsWith(path, "tar"))
        httpd_resp_set_type(req, "application/x-tar");
    else if (endsWith(path, "kml"))
        httpd_resp_set_type(req, "application/vnd.google-earth.kml+xml");
    else if (endsWith(path, "json"))
        httpd_resp_set_type(req, "application/json");
    else
        httpd_resp_set_type(req, "application/octet-stream");

    ESP_LOGD(__FUNCTION__, "Sending %s willmove:%d", path, moveTheSucker);
    httpd_resp_set_hdr(req, "filename", path);
    FILE *theFile;
    if (initSPISDCard())
    {
        if ((theFile = fOpen(path, "r")) != NULL)
        {
            ESP_LOGV(__FUNCTION__, "%s opened", path);
            uint8_t *buf = (uint8_t *)dmalloc(F_BUF_SIZE);
            uint32_t len = 0;
            while (!feof(theFile))
            {
                if ((len = fread(buf, 1, F_BUF_SIZE, theFile)) > 0)
                {
                    ESP_LOGV(__FUNCTION__, "%d read", len);
                    httpd_resp_send_chunk(req, (char *)buf, len);
                }
            }
            httpd_resp_send_chunk(req, NULL, 0);
            ldfree(buf);
            fClose(theFile);
            if (moveTheSucker)
            {
                char *topath = (char *)dmalloc(530);
                memset(topath, 0, 530);
                sprintf(topath, "/sdcard/sent%s", path);
                if (!moveFile(path, topath))
                {
                    ESP_LOGE(__FUNCTION__, "Cannot move %s to %s", path, topath);
                }
                ldfree(topath);
            }
        }
        else
        {
            httpd_resp_set_status(req, HTTPD_404);
            httpd_resp_send(req, "Not Found", 9);
        }
    }
    ESP_LOGD(__FUNCTION__, "Sent %s", path);
    deinitSPISDCard();
    return ESP_OK;
}

esp_err_t app_handler(httpd_req_t *req)
{
    if (endsWith(req->uri, "favicon.ico"))
    {
        httpd_resp_set_type(req, "image/x-icon");
        return httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);
    }
    if (endsWith(req->uri, "app.js"))
    {
        httpd_resp_set_type(req, "text/javascript");
        return httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start - 1);
    }
    if (endsWith(req->uri, "app.css"))
    {
        httpd_resp_set_type(req, "text/css");
        return httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start - 1);
    }

    if (endsWith(req->uri, "configschema.json"))
    {
        httpd_resp_set_type(req, "text/json");
        return httpd_resp_send(req, (const char *)jsonschema_start, jsonschema_end - jsonschema_start - 1);
    }

    if (!endsWith(req->uri, "/") && !indexOf(req->uri,"/?"))
    {
        return sendFile(req,req->uri);
    }
    return httpd_resp_send(req, (const char *)index_html_start, (index_html_end - index_html_start));
}

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

    if ((theFolder = opendir(path)) != NULL)
    {
        sprintf(kmlFileName, "%s/", path + 1);
        tarStat = mtar_write_dir_header(tar, kmlFileName);
        while ((tarStat == MTAR_ESUCCESS) && ((fi = readdir(theFolder)) != NULL))
        {
            if (strlen(fi->d_name) == 0)
            {
                break;
            }
            if (fi->d_type == DT_DIR)
            {
                if (recursive)
                {
                    dcnt++;
                    sprintf(kmlFileName, "%s/%s", path, fi->d_name);
                    sprintf(theFolders + fpos, "%s", kmlFileName);
                    fpos += strlen(kmlFileName) + 1;
                    ESP_LOGV(__FUNCTION__, "%s currently has %d files and %d folders subfolder len:%d. Adding dir (%s)%d", path, fcnt, dcnt, fpos, kmlFileName, strlen(kmlFileName));
                }
            }
            else if ((ext == NULL) || (strlen(ext) == 0) || endsWith(fi->d_name, ext))
            {
                sprintf(theFName, "%s/%s", path, fi->d_name);
                if ((excludeList == NULL) || (indexOf(fi->d_name, excludeList) != 0))
                {
                    gettimeofday(&tv_start, NULL);
                    if ((theFile = fOpen(theFName, "r")) &&
                        (gettimeofday(&tv_open, NULL) == 0))
                    {
                        uint32_t startPos = tar->pos;
                        fcnt++;
                        bool headerWritten = false;
                        bool allDone = false;
                        while ((tarStat == MTAR_ESUCCESS) && !allDone)
                        {
                            gettimeofday(&tv_rstart, NULL);
                            if ((len = fread(buf, 1, F_BUF_SIZE, theFile)) > 0)
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
                                    ESP_LOGD(__FUNCTION__, "Closing %s", theFName);
                                    if (removeSrc && !endsWith(theFName,".json"))
                                        deleteFile(theFName);
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
                    }
                    else
                    {
                        ESP_LOGE(__FUNCTION__, "Cannot read %s", theFName);
                        ret = ESP_FAIL;
                        break;
                    }
                }
            }
            if (tar->pos > maxSize)
            {
                //break;
            }
        }
        closedir(theFolder);
        ESP_LOGV(__FUNCTION__, "%s had %d files and %d folders subfolder len:%d", path, fcnt, dcnt, fpos);
        uint32_t ctpos = 0;
        while (dcnt-- > 0)
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

bool dumpFolder(char *folderName, mtar_t *tar)
{
    FF_DIR theFolder;
    FILE *theFile;
    uint32_t len = 0;
    char *fName = (char *)dmalloc(270);
    void *buf = dmalloc(F_BUF_SIZE);
    FILINFO *fi = (FILINFO *)dmalloc(sizeof(FILINFO));
    bool retval = true;

    if (f_opendir(&theFolder, folderName) == FR_OK)
    {
        ESP_LOGD(__FUNCTION__, "reading trip files in %s", folderName);
        while (f_readdir(&theFolder, fi) == FR_OK)
        {
            if (strlen(fi->fname) == 0)
            {
                break;
            }
            if (!(fi->fattrib & AM_DIR))
            {
                ESP_LOGD(__FUNCTION__, "%s - %d", fi->fname, fi->fsize);
                sprintf(fName, "/sdcard%s/%s", folderName, fi->fname);
                mtar_write_file_header(tar, fName, fi->fsize);
                if ((theFile = fOpen(fName, "r")) != NULL)
                {
                    ESP_LOGV(__FUNCTION__, "%s opened", fName);
                    while (!feof(theFile))
                    {
                        if ((len = fread(buf, 1, F_BUF_SIZE, theFile)) > 0)
                        {
                            ESP_LOGV(__FUNCTION__, "%d written", len);
                            mtar_write_data(tar, buf, len);
                        }
                    }
                    fClose(theFile);
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Cannot read %s", fName);
                    retval = false;
                    break;
                }
            }
            else
            {
                sprintf(fName, "%s/%s", folderName, fi->fname);
                ESP_LOGD(__FUNCTION__, "Parsing sub folder %s of %s as %s", fi->fname, folderName, fName);
                dumpFolder(fName, tar);
            }
        }
        f_closedir(&theFolder);
    }
    else
    {
        retval = false;
    }
    ldfree(fi);
    ldfree(fName);
    ldfree(buf);
    return retval;
}

esp_err_t trips_handler(httpd_req_t *req)
{
    BufferedFile::CloseAll();
    AppConfig *config = AppConfig::GetAppConfig();
    mtar_t tar;
    memset(&tar, 0, sizeof(tar));
    tar.read = tarRead;
    tar.close = tarClose;
    tar.seek = tarSeek;
    tar.write = tarWrite;
    char tarFName[50];
    TinyGPSPlus* gps = TinyGPSPlus::runningInstance();
    if (gps) {
        gps->gpsStop();
    }

    sprintf(tarFName, "%d.tar", config->GetIntProperty("deviceid"));

    httpd_resp_set_type(req, "application/x-tar");
    httpd_resp_set_hdr(req, "filename", tarFName);
    xEventGroupSetBits(*getAppEG(), app_bits_t::TRIPS_SYNCING);

    if (initSPISDCard())
    {
        dumpLogs();
        ESP_LOGD(__FUNCTION__, "Sending Trips.");
        xEventGroupClearBits(eventGroup, TAR_BUFFER_FILLED);
        xEventGroupSetBits(eventGroup, TAR_BUFFER_SENT);
        xEventGroupClearBits(eventGroup, TAR_BUILD_DONE);
        xTaskCreate(sendTar, "sendTar", 8196, NULL, 5, NULL);

        sp.tarBuf = (uint8_t *)dmalloc(HTTP_BUF_SIZE);
        sp.sendBuf = NULL;
        sp.bufLen = 0;
        sp.req = req;
        sp.len = 0;

        cJSON *status = status_json();
        cJSON *astatus = AppConfig::GetAppStatus()->GetJSONConfig(NULL);
        cJSON *stat;
        cJSON_ArrayForEach(stat, astatus)
        {
            cJSON_AddItemReferenceToObject(status, stat->string, stat);
        }
        char *cs = NULL, *ss = NULL;
        if ((tarString(&tar, "status.json", (cs = cJSON_PrintUnformatted(status))) == ESP_OK) &&
            (tarString(&tar, "config.json", (ss = cJSON_PrintUnformatted(config->GetJSONConfig(NULL)))) == ESP_OK) &&
            (tarFiles(&tar, "/lfs", "", true, "current.bin current.bin.md5", 1048576, true) == ESP_OK))
        {
            ESP_LOGV(__FUNCTION__, "Finalizing tar");
            mtar_finalize(&tar);
            ESP_LOGV(__FUNCTION__, "Closing tar");
            mtar_close(&tar);
            deinitSPISDCard();
        }
        else
        {
            ESP_LOGE(__FUNCTION__, "Error sending trips");
        }
        cJSON_Delete(status);
        ldfree(cs);
        ldfree(ss);
    }
    else
    {
        ESP_LOGE(__FUNCTION__, "Cannot mount the fucking sd card");
        httpd_resp_send(req, "No Bueno", 8);
    }
    xEventGroupClearBits(*getAppEG(), app_bits_t::TRIPS_SYNCING);
    deinitSPISDCard();
    if (gps) {
        gps->gpsStart();
    }
    return ESP_FAIL;
}