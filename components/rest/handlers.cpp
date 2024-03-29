#include "route.h"
#include "math.h"
#include "time.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "esp_littlefs.h"
#include "esp32/rom/ets_sys.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"
#include "driver/adc.h"
#include "eventmgr.h"
#include "mfile.h"
#include "../TinyGPS/TinyGPS++.h"
#include "pins.h"
#include "mbedtls/md.h"
#include <cstdio>
#include <cstring>
#include <sys/dirent.h>
#include "../../main/utils.h"
#include "camera.h"
#include "esp_ota_ops.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

static cJSON *tasks = nullptr;
static const char* RESP_OK = "OK";
static TheRest *restInstance = nullptr;

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

void UpsertTaskJson(cJSON* task, TaskStatus_t const * taskStatus,uint32_t totalRunTime) {
    if (cJSON_HasObjectItem(task,"Name")) {
        cJSON_SetNumberValue(cJSON_GetObjectItem(task, "TaskNumber"), taskStatus->xTaskNumber);
        cJSON_SetNumberValue(cJSON_GetObjectItem(task, "Priority"), taskStatus->uxCurrentPriority);
        cJSON_SetNumberValue(cJSON_GetObjectItem(task, "Runtime"), taskStatus->ulRunTimeCounter);
        cJSON_SetNumberValue(cJSON_GetObjectItem(task, "Core"), taskStatus->xCoreID > 100 ? -1 : taskStatus->xCoreID);
        cJSON_SetNumberValue(cJSON_GetObjectItem(task, "State"), taskStatus->eCurrentState);
        cJSON_SetNumberValue(cJSON_GetObjectItem(task, "Stackfree"), taskStatus->usStackHighWaterMark * 4);
        cJSON_SetNumberValue(cJSON_GetObjectItem(task, "Pct"), ((double)taskStatus->ulRunTimeCounter / totalRunTime) * 100.0);
    } else {
        cJSON_AddStringToObject(task, "Name", taskStatus->pcTaskName);
        cJSON_AddNumberToObject(task, "TaskNumber", taskStatus->xTaskNumber);
        cJSON_AddNumberToObject(task, "Priority", taskStatus->uxCurrentPriority);
        cJSON_AddNumberToObject(task, "Runtime", taskStatus->ulRunTimeCounter);
        cJSON_AddNumberToObject(task, "Core", taskStatus->xCoreID > 100 ? -1 : taskStatus->xCoreID);
        cJSON_AddNumberToObject(task, "State", taskStatus->eCurrentState);
        cJSON_AddNumberToObject(task, "Stackfree", taskStatus->usStackHighWaterMark * 4);
        cJSON_AddNumberToObject(task, "Pct", ((double)taskStatus->ulRunTimeCounter / totalRunTime) * 100.0);
    }
}

cJSON *TheRest::tasks_json()
{
    UBaseType_t numTasks = uxTaskGetNumberOfTasks();
    TheRest* rest = GetServer();
    if (rest->statusesLen < numTasks) {
        if (rest->statusesLen) {
            ldfree(rest->statuses);
        }
        rest->statuses = (TaskStatus_t *)dmalloc(numTasks * sizeof(TaskStatus_t));
        rest->statusesLen = numTasks;
    }
    if (rest->statuses != nullptr)
    {
        memset(rest->statuses,0,rest->statusesLen * sizeof(TaskStatus_t));
        uint32_t totalRunTime;
        cJSON *task = nullptr;
        cJSON* toGo[32];
        int numToGo=0;
        numTasks = uxTaskGetSystemState(rest->statuses, numTasks, &totalRunTime);
        if (totalRunTime > 0)
        {
            if (tasks == nullptr){
                tasks = cJSON_CreateArray();
            }
            for (uint32_t taskNo = 0; taskNo < numTasks; taskNo++)
            {
                bool found = false;
                cJSON_ArrayForEach(task, tasks) {
                    cJSON const* taskName = task ? cJSON_GetObjectItem(task,"Name") : nullptr;
                    if (taskName && (strcmp(taskName->valuestring,rest->statuses[taskNo].pcTaskName) == 0)) {
                        found=true;
                        UpsertTaskJson(task, &rest->statuses[taskNo],totalRunTime);
                    } else if (taskName != nullptr) {
                        bool stillExists = false;
                        for (uint32_t ttaskNo = 0; ttaskNo < numTasks; ttaskNo++) {
                            stillExists = strcmp(rest->statuses[ttaskNo].pcTaskName,taskName->valuestring)==0;
                            if (stillExists) {
                                break;
                            }
                        }
                        if (!stillExists && (numToGo < 32)) {
                            toGo[numToGo++]=task;
                        }
                    }
                }

                if (!found) {
                    task=cJSON_CreateObject();
                    if (task && cJSON_AddItemToArray(tasks,task)) {
                        UpsertTaskJson(task, &rest->statuses[taskNo],totalRunTime);
                    }
                }
            }
        }
        while (--numToGo>=0)
        {
            int taskIdx = -1;
            cJSON_ArrayForEach(task,tasks) {
                taskIdx++;
                if (task == toGo[numToGo]) {
                    cJSON_DeleteItemFromArray(tasks,taskIdx);
                    break;
                }
            }
        }
    }

    return tasks;
}

cJSON* TheRest::status_json() {
    TheRest* rest = GetServer();
    ESP_LOGV(__PRETTY_FUNCTION__, "Status Handler");

    struct timeval tv_now;
    gettimeofday(&tv_now, nullptr);
    int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

    if (!rest->system_status) {
        rest->system_status = rest->bake_status_json();
    }

    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "uptime_sec"), getUpTime());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "sleeptime_us"), getSleepTime());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "freeram"), esp_get_free_heap_size());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "totalram"), heap_caps_get_total_size(MALLOC_CAP_DEFAULT));

    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_EXEC"), heap_caps_get_free_size(MALLOC_CAP_EXEC));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_32BIT"), heap_caps_get_free_size(MALLOC_CAP_32BIT));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_8BIT"), heap_caps_get_free_size(MALLOC_CAP_8BIT));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_DMA"), heap_caps_get_free_size(MALLOC_CAP_32BIT));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_PID2"), heap_caps_get_free_size(MALLOC_CAP_PID2));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_PID3"), heap_caps_get_free_size(MALLOC_CAP_PID3));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_PID4"), heap_caps_get_free_size(MALLOC_CAP_PID4));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_PID5"), heap_caps_get_free_size(MALLOC_CAP_PID5));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_PID6"), heap_caps_get_free_size(MALLOC_CAP_PID6));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_PID7"), heap_caps_get_free_size(MALLOC_CAP_PID7));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_SPIRAM"), heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_INTERNAL"), heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "MALLOC_CAP_IRAM_8BIT"), heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT));

    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "battery"), getBatteryVoltage());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "temperature"), temperatureReadFixed());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "hallsensor"), hall_sensor_read());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "openfiles"), GetNumOpenFiles());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "runtime_us"), esp_timer_get_time());
    cJSON_SetNumberValue(cJSON_GetObjectItem(rest->system_status, "systemtime_us"), time_us);

    return rest->system_status;
}

cJSON* TheRest::bake_status_json()
{
    ESP_LOGV(__PRETTY_FUNCTION__, "Bake Status Handler");

    struct timeval tv_now;
    gettimeofday(&tv_now, nullptr);
    int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;

    cJSON* status = cJSON_CreateObject();

    cJSON_AddNumberToObject(status, "deviceid", AppConfig::GetAppConfig()->GetIntProperty("deviceid"));
    cJSON_AddNumberToObject(status, "uptime_sec", getUpTime());
    cJSON_AddNumberToObject(status, "sleeptime_us", getSleepTime());
    cJSON_AddItemToObject(status, "freeram", cJSON_CreateNumber(esp_get_free_heap_size()));
    cJSON_AddItemToObject(status, "totalram", cJSON_CreateNumber(heap_caps_get_total_size(MALLOC_CAP_DEFAULT)));

    cJSON_AddItemToObject(status, "MALLOC_CAP_EXEC", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_EXEC)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_32BIT", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_32BIT)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_8BIT", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_8BIT)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_DMA", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_32BIT)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_PID2", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_PID2)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_PID3", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_PID3)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_PID4", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_PID4)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_PID5", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_PID5)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_PID6", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_PID6)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_PID7", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_PID7)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_SPIRAM", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_INTERNAL", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
    cJSON_AddItemToObject(status, "MALLOC_CAP_IRAM_8BIT", cJSON_CreateNumber(heap_caps_get_free_size(MALLOC_CAP_IRAM_8BIT)));

    cJSON_AddItemToObject(status, "battery", cJSON_CreateNumber(getBatteryVoltage()));
    cJSON_AddItemToObject(status, "temperature", cJSON_CreateNumber(temperatureReadFixed()));
    cJSON_AddItemToObject(status, "hallsensor", cJSON_CreateNumber(hall_sensor_read()));
    cJSON_AddItemToObject(status, "openfiles", cJSON_CreateNumber(GetNumOpenFiles()));
    cJSON_AddItemToObject(status, "runtime_us", cJSON_CreateNumber(esp_timer_get_time()));
    cJSON_AddItemToObject(status, "systemtime_us", cJSON_CreateNumber(time_us));
    return status;
}

esp_err_t TheRest::findFiles(httpd_req_t *req, const char *path, const char *ext, bool recursive, char *res, uint32_t resLen)
{
    if ((path == nullptr) || (strlen(path) == 0))
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Missing path");
        return ESP_FAIL;
    }
    if (strcmp(path, "/") == 0)
    {
        sprintf(res, "[{\"name\":\"sdcard\",\"ftype\":\"folder\",\"size\":0},{\"name\":\"lfs\",\"ftype\":\"folder\",\"size\":0}]");
        return ESP_OK;
    }
    if (!(startsWith(path, "/sdcard") ? initStorage(SDCARD_FLAG) : initStorage(SPIFF_FLAG))) {
        ESP_LOGE(__PRETTY_FUNCTION__, "Cannot init storage");
        return ESP_FAIL;
    }
    esp_err_t ret = ESP_OK;
    uint32_t sLen = strlen(res);
    ESP_LOGV(__PRETTY_FUNCTION__, "Parsing %s", path);
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
    if ((theFolder = opendir(path)) != nullptr)
    {
        while ((fi = readdir(theFolder)) != nullptr)
        {
            if (strlen(fi->d_name) > 0)
            {
                if (fi->d_type == DT_DIR)
                {
                    if (recursive)
                    {
                        dcnt++;
                        sprintf(kmlFileName, "%s/%s", path, fi->d_name);
                        fpos += sprintf(theFolders + fpos, "%s", kmlFileName) + 1;
                        ESP_LOGV(__PRETTY_FUNCTION__, "%s currently has %d files and %d folders subfolder len:%d. Adding dir %s", path, fcnt, dcnt, fpos, kmlFileName);
                    }
                    if ((ext == nullptr) || (strlen(ext) == 0))
                    {
                        if (sLen > (resLen - 100))
                        {
                            ESP_LOGV(__PRETTY_FUNCTION__, "Buffer Overflow, flushing");
                            if ((ret = httpd_resp_send_chunk(req, res, sLen)) != ESP_OK)
                            {
                                ESP_LOGE(__PRETTY_FUNCTION__, "Error sending chunk %s sLenL%d, actuallen:%d", esp_err_to_name(ret), sLen, strlen(res));
                                break;
                            }
                            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += strlen(res);
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
                else if ((ext == nullptr) || (strlen(ext) == 0) || endsWith(fi->d_name, ext))
                {
                    fcnt++;
                    ESP_LOGV(__PRETTY_FUNCTION__, "%s currently has %d files and %d folders subfolder len:%d. Adding file %s", path, fcnt, dcnt, fpos, fi->d_name);
                    if (sLen > (resLen - 100))
                    {
                        ESP_LOGV(__PRETTY_FUNCTION__, "Buffer Overflow, flushing");
                        if ((ret = httpd_resp_send_chunk(req, res, sLen)) != ESP_OK)
                        {
                            ESP_LOGE(__PRETTY_FUNCTION__, "Error sending chunk %s sLenL%d, actuallen:%d", esp_err_to_name(ret), sLen, strlen(res));
                            break;
                        }
                        else
                        {
                            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += strlen(res);
                        }
                        memset(res, 0, resLen);
                        sLen = 0;
                    }
                    sprintf(theFName, "%s/%s", path, fi->d_name);
                    if (fcnt < 20)
                        stat(theFName, &st);
                    else
                        st.st_size = 0;
                    sLen += sprintf(res + sLen, "{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d},",
                                    path,
                                    fi->d_name,
                                    fi->d_type == DT_DIR ? "folder" : "file",
                                    (uint32_t)st.st_size);
                }
            }
        }
        closedir(theFolder);
        ESP_LOGV(__PRETTY_FUNCTION__, "%s has %d files and %d folders subfolder len:%d", path, fcnt, dcnt, fpos);
        uint32_t ctpos = 0;
        while (dcnt-- > 0)
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "%d-%s: Getting sub-folder(%d) %s", dcnt, path, ctpos, theFolders + ctpos);
            if (findFiles(req, theFolders + ctpos, ext, recursive, res, resLen) != ESP_OK)
            {
                ESP_LOGW(__PRETTY_FUNCTION__, "Error invoking getSdFiles for %s", kmlFileName);
            }
            ctpos += strlen(theFolders) + 1;
        }
    }
    else
    {
        ESP_LOGW(__PRETTY_FUNCTION__, "Error opening %s:%s", path, esp_err_to_name(errno));
        ret = ESP_FAIL;
    }

    ldfree(theFName);
    ldfree(theFolders);
    ldfree(kmlFileName);
    startsWith(path, "/sdcard") ? deinitStorage(SDCARD_FLAG) : deinitStorage(SPIFF_FLAG);
    return ret;
}

esp_err_t TheRest::eventDescriptor_handler(httpd_req_t *req){
    if (strlen(req->uri) <= 20) {
        return httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"Bad request");
    }
    char uri[513];
    strcpy(uri,req->uri);
    char* eventBase = uri+17;
    char* eventId = indexOf(eventBase,"/");
    if (eventId) {
        *eventId++=0;
        ESP_LOGV(__PRETTY_FUNCTION__,"base:%s id:%s",eventBase,eventId);
        EventDescriptor_t const* ed = nullptr;
        if (eventId[0] <= 9) {
            ed=EventHandlerDescriptor::GetEventDescriptor(eventBase,atoi(eventId));
        } else {
            ed=EventHandlerDescriptor::GetEventDescriptor(eventBase,eventId);
        }
        if (ed) {
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp,"eventBase",ed->baseName);
            cJSON_AddStringToObject(resp,"eventName",ed->eventName);
            cJSON_AddNumberToObject(resp,"eventId",ed->id);
            switch (ed->dataType)
            {
            case event_data_type_tp::JSON:
                cJSON_AddStringToObject(resp,"dataType","JSON");
                break;
            case event_data_type_tp::Number:
                cJSON_AddStringToObject(resp,"dataType","Number");
                break;
            case event_data_type_tp::String:
                cJSON_AddStringToObject(resp,"dataType","String");
                break;
            default:
                cJSON_AddStringToObject(resp,"dataType","Unknown");
                break;
            }
            char* stmp = cJSON_PrintUnformatted(resp);
            esp_err_t ret = httpd_resp_send(req,stmp,strlen(stmp));
            cJSON_Delete(resp);
            ldfree(stmp);
            return ret;
        } else {
            return httpd_resp_send_404(req);
        }
    } else {
        return httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"Bad request.");
    }
}

void UpdateGpioProp(cfg_gpio_t *cfg, gpio_num_t val)
{
    if (cfg->value != val)
    {
        ESP_LOGV(__PRETTY_FUNCTION__, "Updating from %d to %d", cfg->value, val);
        cfg->value = val;
        cfg->version++;
    }
}

void UpdateStringProp(cfg_label_t *cfg, char *val)
{
    if ((val == nullptr) || (strcmp(cfg->value, val) != 0))
    {
        strcpy(cfg->value, val == nullptr ? "" : val);
        cfg->version++;
    }
}

esp_err_t TheRest::HandleStatusChange(httpd_req_t *req){
    esp_err_t ret = ESP_FAIL;
    if (restInstance == nullptr) {
        restInstance = TheRest::GetServer();
    }
    int rlen = httpd_req_recv(req, restInstance->postData, JSON_BUFFER_SIZE);
    if (rlen == 0)
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "no body");
        ret = httpd_resp_send_500(req);
    }
    else
    {
        *(restInstance->postData+rlen)=0;
        cJSON* stat = cJSON_ParseWithLength(restInstance->postData,JSON_BUFFER_SIZE);
        if (stat) {
            auto* appStat = new AppConfig(stat);
            if (appStat->HasProperty("name")) {
                const char* name = appStat->GetStringProperty("name");
                ManagedDevice* dev = ManagedDevice::GetByName(name);
                if (dev) {
                    ESP_LOGV(__PRETTY_FUNCTION__,"Posting state to %s",dev->eventBase);
                    EventDescriptor_t const* ed = EventHandlerDescriptor::GetEventDescriptor(dev->eventBase,"STATUS");
                    if (ed){
                        EventInterpretor::RunMethod(nullptr,"Post",(void*)&stat,false);
                        ESP_LOGV(__PRETTY_FUNCTION__,"Posted %s to %s",restInstance->postData, name);
                        char* nstat = cJSON_Print(dev->status);
                        ret=httpd_resp_send(req,nstat,strlen(nstat));
                        ldfree(nstat);
                    } else {
                        ESP_LOGE(__PRETTY_FUNCTION__,"%s is not implemented",name);
                        ret=httpd_resp_send_err(req,httpd_err_code_t::HTTPD_501_METHOD_NOT_IMPLEMENTED,dev->GetName());
                    }
                } else {
                    ret=httpd_resp_send_err(req,httpd_err_code_t::HTTPD_404_NOT_FOUND,dev->GetName());
                    ESP_LOGE(__PRETTY_FUNCTION__,"Cannot find device named %s",name);
                }
            } else {
                ret=httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"Missing name");
                ESP_LOGE(__PRETTY_FUNCTION__,"Missing name in %s",restInstance->postData);
            }
            delete appStat;
            cJSON_Delete(stat);
        } else {
            ret=httpd_resp_send_err(req,httpd_err_code_t::HTTPD_400_BAD_REQUEST,"Cannot parse JSON");
            ESP_LOGE(__PRETTY_FUNCTION__,"Cannot parse(%s)",restInstance->postData);
        }
    }
    return ret;
}

esp_err_t TheRest::HandleSystemCommand(httpd_req_t *req)
{
    if (restInstance == nullptr) {
        restInstance = TheRest::GetServer();
    }
    esp_err_t ret = 0;
    int rlen = httpd_req_recv(req, restInstance->postData, JSON_BUFFER_SIZE);
    if (rlen == 0)
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "no body");
        ret = httpd_resp_send_500(req);
    }
    else
    {
        TheRest::GetServer()->jBytesIn->valuedouble = TheRest::GetServer()->jBytesIn->valueint += rlen;
        *(restInstance->postData + rlen) = 0;
        ESP_LOGV(__PRETTY_FUNCTION__, "Got %s", restInstance->postData);
        cJSON *jresponse = cJSON_ParseWithLength(restInstance->postData, rlen);
        if (jresponse != nullptr)
        {
            cJSON *jClass = cJSON_GetObjectItem(jresponse, "className");
            if ((jClass != nullptr) && (ManagedDevice::GetNumRunningInstances() > 0)) {
                ManagedDevice **dev = ManagedDevice::GetRunningInstances();
                bool processed = false;
                for (uint32_t idx = 0; idx < ManagedDevice::GetNumRunningInstances(); idx++) {
                    if (dev[idx] && strcmp(jClass->valuestring, dev[idx]->eventBase) == 0) {
                        processed |= dev[idx]->ProcessCommand(jresponse);
                    }
                }
                if (!processed) {
                    ESP_LOGE(__PRETTY_FUNCTION__, "Cannot find class %s", jClass->valuestring);
                    ret = httpd_resp_send_err(req, httpd_err_code_t::HTTPD_404_NOT_FOUND, jClass->valuestring);
                } else {
                    ret = httpd_resp_send(req, RESP_OK, 2);
                }
            } else {
                cJSON const *jcommand = cJSON_GetObjectItemCaseSensitive(jresponse, "command");
                if (jcommand && (strcmp(jcommand->valuestring, "reboot") == 0))
                {
                    ret = httpd_resp_send(req, "Rebooting", 9);
                    dumpTheLogs((void *)true);
                    esp_system_abort("Rebooting");
                    esp_restart();
                }
                else if (jcommand && (strcmp(jcommand->valuestring, "scanNetwork") == 0))
                {
                    CreateBackgroundTask(TheRest::ScanNetwork,"ScanNetwork",4096, nullptr, tskIDLE_PRIORITY,nullptr);
                    ret = httpd_resp_send(req, RESP_OK, 2);
                }
                else if (jcommand && (strcmp(jcommand->valuestring, "factoryReset") == 0))
                {
                    AppConfig::ResetAppConfig(true);
                    ret = httpd_resp_send(req, RESP_OK, 2);
                    TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                }
                else if (jcommand && (strcmp(jcommand->valuestring, "flush") == 0))
                {
                    cJSON *fileName = cJSON_GetObjectItemCaseSensitive(jresponse, "name");
                    uint32_t slen = 0;
                    if (fileName && fileName->valuestring && (slen=strlen(fileName->valuestring))) {
                        if ((slen == 1) && (fileName->valuestring[0] == '*')) {
                            BufferedFile::FlushAll();
                            ret = httpd_resp_send(req, RESP_OK, 2);
                            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                        } else {
                            BufferedFile* bfile = (BufferedFile*)ManagedDevice::GetByName(fileName->valuestring);
                            if (bfile) {
                                bfile->Flush();
                                ret = httpd_resp_send(req, RESP_OK, 2);
                                TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                                AppConfig::SignalStateChange(state_change_t::MAIN);
                            } else {
                                ret = httpd_resp_send_err(req,httpd_err_code_t::HTTPD_404_NOT_FOUND,"File not found");
                                TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 14;
                            }
                        }
                        ret = httpd_resp_send(req, RESP_OK, 2);
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                    } else {
                        ret = httpd_resp_send_err(req,httpd_err_code_t::HTTPD_501_METHOD_NOT_IMPLEMENTED,"Not Implemented");
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 15;
                    }
                }
                else if (jcommand && (strcmp(jcommand->valuestring, "scanaps") == 0))
                {
                    if (TheWifi::GetInstance()->wifiScan()) {
                        ret = httpd_resp_send(req, RESP_OK, 2);
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                    } else {
                        ret = httpd_resp_send_err(req,httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR,"Cannot Scan Wifi");
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 16;
                    }
                } 
                else if (jcommand && (strcmp(jcommand->valuestring, "gpsstate") == 0))
                {
                    cJSON *event = cJSON_GetObjectItemCaseSensitive(jresponse, "param1");
                    TinyGPSPlus* gps = TinyGPSPlus::runningInstance();
                    if (gps) {
                        if (strcmp(event->valuestring, "pause") == 0) {
                            gps->gpsPause();
                        }
                        if (strcmp(event->valuestring, "resume") == 0) {
                            gps->gpsResume();
                        }
                        if (strcmp(event->valuestring, "on") == 0) {
                            gps->gpsStart();
                        }
                        if (strcmp(event->valuestring, "off") == 0) {
                            gps->gpsStop();
                        }
                        ret = httpd_resp_send(req, RESP_OK, 2);
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                    } else {
                        ret = httpd_resp_send_err(req,httpd_err_code_t::HTTPD_404_NOT_FOUND,"GPS not running");
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 15;
                    }
                }
                else if (jcommand && (strcmp(jcommand->valuestring, "refreshRate") == 0))
                {
                    cJSON *event = cJSON_GetObjectItemCaseSensitive(jresponse, "param1");
                    TinyGPSPlus* gps = TinyGPSPlus::runningInstance();
                    if (gps && (event->valuedouble > 0.0)) {
                        gps->setRefreshRate(event->valuedouble);
                        ret = httpd_resp_send(req, RESP_OK, 2);
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                    } else {
                        ret = httpd_resp_send_err(req,httpd_err_code_t::HTTPD_404_NOT_FOUND,"GPS not running");
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 15;
                    }
                }
                else
                {
                    ret = httpd_resp_send_err(req, httpd_err_code_t::HTTPD_400_BAD_REQUEST, "Invalid command");
                    TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 15;
                }
            }
            cJSON_Delete(jresponse);
        }
        else
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "Error whilst parsing json");
            ret = httpd_resp_send_err(req, httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR, "Error whilst parsing json");
            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 25;
        }
    }
    return ret;
}

esp_err_t TheRest::sendFile(httpd_req_t *req, const char *path)
{
    bool moveTheSucker = false;
    if (httpd_req_get_hdr_value_len(req, "movetosent") > 1)
    {
        moveTheSucker = true;

        if (moveTheSucker)
        {
            if (endsWith(path, "log"))
            {
                const char *clfn = getLogFName();
                if (clfn && (strlen(clfn) > 0) && endsWith(clfn, path))
                {
                    ESP_LOGI(__PRETTY_FUNCTION__, "Not moving %s as is it active(%s)", path, clfn);
                    moveTheSucker = false;
                }
                else
                {
                    ESP_LOGI(__PRETTY_FUNCTION__, "Will move %s as it is not active log %s", path, clfn);
                }
            }
        }
    }
    else
    {
        ESP_LOGV(__PRETTY_FUNCTION__, "No move in request");
    }

    ESP_LOGV(__PRETTY_FUNCTION__, "Sending %s willmove:%d", path, moveTheSucker);
    //httpd_resp_set_hdr(req, "filename", path);
    FILE *theFile;
    uint32_t len = 0;
    if (startsWith(path, "/sdcard") ? initStorage(SDCARD_FLAG) : initStorage(SPIFF_FLAG))
    {
        if ((theFile = fopen(path, "r")) != nullptr)
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "%s opened", path);
            uint8_t *buf = (uint8_t *)dmalloc(F_BUF_SIZE);
            uint32_t chunckLen = 0;
            while (!feof(theFile))
            {
                if ((chunckLen = fread(buf, 1, F_BUF_SIZE, theFile)) > 0)
                {
                    len += chunckLen;
                    ESP_LOGV(__PRETTY_FUNCTION__, "%d read", chunckLen);
                    httpd_resp_send_chunk(req, (char *)buf, chunckLen);
                }
                else
                {
                    ESP_LOGW(__PRETTY_FUNCTION__, "Failed in reading %s", path);
                    break;
                }
            }
            httpd_resp_send_chunk(req, nullptr, 0);
            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += len;
            ldfree(buf);
            fclose(theFile);
            if (moveTheSucker)
            {
                auto *topath = (char *)dmalloc(530);
                memset(topath, 0, 530);
                sprintf(topath, "/sdcard/sent%s", path);
                if (!moveFile(path, topath))
                {
                    ESP_LOGE(__PRETTY_FUNCTION__, "Cannot move %s to %s", path, topath);
                }
                ldfree(topath);
            }
        }
        else
        {
            httpd_resp_set_status(req, HTTPD_404);
            httpd_resp_send(req, "Not Found", 9);
            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 9;
        }
    }
    ESP_LOGV(__PRETTY_FUNCTION__, "Sent %s(%d)", path, len);
    startsWith(path, "/sdcard") ? deinitStorage(SDCARD_FLAG) : deinitStorage(SPIFF_FLAG);
    return ESP_OK;
}

esp_err_t add_type(httpd_req_t *req, const char* fileName) {
    if (endsWith(fileName,"html") || (strcmp(fileName,"/")==0)) {
        ESP_LOGV(__PRETTY_FUNCTION__,"html type for :%s",fileName);
        return httpd_resp_set_type(req,"text/html");
    }
    if (endsWith(fileName,"js")) {
        ESP_LOGV(__PRETTY_FUNCTION__,"js type for :%s",fileName);
        return httpd_resp_set_type(req,"text/javascript");
    }
    if (endsWith(fileName,"css")) {
        ESP_LOGV(__PRETTY_FUNCTION__,"css type for :%s",fileName);
        return httpd_resp_set_type(req,"text/css");
    }
    if (endsWith(fileName,"txt")) {
        ESP_LOGV(__PRETTY_FUNCTION__,"txt type for :%s",fileName);
        return httpd_resp_set_type(req,"text/txt");
    }
    if (endsWith(fileName,"json")) {
        ESP_LOGV(__PRETTY_FUNCTION__,"json type for :%s",fileName);
        return httpd_resp_set_type(req,"text/json");
    }
    if (endsWith(fileName,"ico")) {
        ESP_LOGV(__PRETTY_FUNCTION__,"ico type for :%s",fileName);
        return httpd_resp_set_type(req,"image/x-icon");
    }
    if (endsWith(fileName,"png")) {
        ESP_LOGV(__PRETTY_FUNCTION__,"png type for :%s",fileName);
        return httpd_resp_set_type(req,"image/png");
    }
    if (endsWith(fileName,"svg")) {
        ESP_LOGV(__PRETTY_FUNCTION__,"png type for :%s",fileName);
        return httpd_resp_set_type(req,"image/svg+xml");
    }
    ESP_LOGV(__PRETTY_FUNCTION__,"No type for :%s",fileName);
    httpd_resp_set_type(req, "application/octet-stream");
    return ESP_OK;
}

esp_err_t TheRest::app_handler(httpd_req_t *req)
{
    if (restInstance && restInstance->appUri.user_ctx) {
        req->user_ctx = restInstance->appUri.user_ctx;
        if (checkTheSum(req) != ESP_OK) {
            return ESP_FAIL;
        }
    }

    ESP_ERROR_CHECK(add_type(req,req->uri));
    ESP_LOGV(__PRETTY_FUNCTION__,"File:%s",req->uri);
    if (strcmp(req->uri, "/favicon.ico")==0)
    {
        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += (favicon_ico_end - favicon_ico_start);
        return httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_end - favicon_ico_start);
    }
    if (strcmp(req->uri, "/dist/app-min.js")==0)
    {
        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += (app_js_end - app_js_start - 1);
        return httpd_resp_send(req, (const char *)app_js_start, app_js_end - app_js_start - 1);
    }
    if (strcmp(req->uri, "/dist/app-min.css")==0)
    {
        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += (app_css_end - app_css_start - 1);
        return httpd_resp_send(req, (const char *)app_css_start, app_css_end - app_css_start - 1);
    }

    if (!endsWith(req->uri, "/") && !indexOf(req->uri, "/?") && !indexOf(req->uri,"index_sd.html"))
    {
        return sendFile(req, req->uri);
    }
    bool isOffline = httpd_req_get_hdr_value_len(req, "offline") || indexOf(req->uri,"index_sd.html");
    if (AppConfig::HasSDCard() && isOffline) {
        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += (index_sd_html_end - index_sd_html_start);
        return httpd_resp_send(req, (const char *)index_sd_html_start, (index_sd_html_end - index_sd_html_start - 1));
    }

    TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += (index_html_end - index_html_start);
    return httpd_resp_send(req, (const char *)index_html_start, (index_html_end - index_html_start - 1));
}

esp_err_t TheRest::stat_handler(httpd_req_t *req)
{
    auto *fname = req->uri + 5;
    if (req->method == HTTP_POST)
    {
        ESP_LOGV(__PRETTY_FUNCTION__, "Getting stats on %s", fname);
        struct stat st;
        esp_err_t ret = ESP_FAIL;
        size_t tlen;
        char *opeartion = TheRest::GetServer()->postData;
        char *fileType = TheRest::GetServer()->postData+500;

        ret = stat(fname, &st);

        if (ret == 0)
        {
            if ((tlen = httpd_req_get_hdr_value_len(req, "operation")) &&
                (httpd_req_get_hdr_value_str(req, "operation", opeartion, tlen + 1) == ESP_OK) &&
                (tlen = httpd_req_get_hdr_value_len(req, "ftype")) &&
                (httpd_req_get_hdr_value_str(req, "ftype", fileType, tlen + 1) == ESP_OK))
            {
                if (opeartion[0] == 'd')
                {
                    ESP_LOGI(__PRETTY_FUNCTION__, "Deleting %s", fname);
                    if (fileType[0] == 'f')
                    {
                        ESP_LOGV(__PRETTY_FUNCTION__, "%s is a file", fname);
                        if (deleteFile(fname))
                        {
                            ret = httpd_resp_send(req, RESP_OK, 2);
                            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                        }
                        else
                        {
                            ret = httpd_resp_send_500(req);
                            ESP_LOGE(__PRETTY_FUNCTION__, "Failed to rm -fr %s", fname);
                        }
                    }
                    else
                    {
                        ESP_LOGV(__PRETTY_FUNCTION__, "%s is a folder", fname);
                        if (rmDashFR(fname))
                        {
                            ret = httpd_resp_send(req, RESP_OK, 3);
                            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 2;
                        }
                        else
                        {
                            ret = httpd_resp_send_500(req);
                            ESP_LOGE(__PRETTY_FUNCTION__, "Failed in rm -fr on %s", fname);
                        }
                    }
                }
                else
                {
                    ESP_LOGE(__PRETTY_FUNCTION__, "bad operation.");
                    ret = httpd_resp_send_408(req);
                }
            }
            else if (restInstance)
            {
                char *path = restInstance->postData+1024;
                strcpy(path, fname);
                char *fpos = strrchr(path, '/');
                *fpos = 0;
                httpd_resp_set_type(req, "application/json");
                sprintf(restInstance->postData+2048, "{\"folder\":\"%s\",\"name\":\"%s\",\"ftype\":\"%s\",\"size\":%d}", path, fpos + 1, "file", (uint32_t)st.st_size);
                ret = httpd_resp_send(req, restInstance->postData+2048, strlen(restInstance->postData+2048));
                TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += strlen(restInstance->postData+2048);
            }
            return ret;
        }
        return httpd_resp_send_500(req);
    }
    else
    {
        return httpd_resp_send_408(req);
    }
}

esp_err_t TheRest::config_template_handler(httpd_req_t *req)
{
    if (restInstance == nullptr) {
        restInstance = TheRest::GetServer();
    }
    ESP_LOGV(__PRETTY_FUNCTION__, "Config template Handler:%s", req->uri);
    if (req->method == HTTP_POST)
    {
        if (indexOf(req->uri,"templates/config")) {
            cJSON* jret = ManagedDevice::GetConfigTemplates();
            char* json = cJSON_Print(jret);
            esp_err_t ret = httpd_resp_send(req, json, strlen(json));
            ldfree(json);
            return ret;
        } else {
            return httpd_resp_send_404(req);
        }
    }
    else
    {
        return httpd_resp_send_408(req);
    }
}

esp_err_t TheRest::config_handler(httpd_req_t *req)
{
    if (restInstance == nullptr) {
        restInstance = TheRest::GetServer();
    }
    ESP_LOGV(__PRETTY_FUNCTION__, "Config Handler.");
    esp_err_t ret = ESP_OK;
    AppConfig *appcfg = AppConfig::GetAppConfig();
    httpd_resp_set_type(req, "application/json");

    int len = 0, curLen = -1;
//    char *postData = (char *)dmalloc(JSON_BUFFER_SIZE * 2);
    while ((curLen = httpd_req_recv(req, restInstance->postData + len, len - (JSON_BUFFER_SIZE * 2))) > 0)
    {
        len += curLen;
    }
    ESP_LOGV(__PRETTY_FUNCTION__, "Post content len:%d method:%d", len, req->method);
    TheRest::GetServer()->jBytesIn->valuedouble = TheRest::GetServer()->jBytesIn->valueint += len;

    if (!endsWith(req->uri, "config") && !endsWith(req->uri, "config/"))
    {
        //devId = atoi(indexOf(req->uri + 1, "/") + 1);
        //ESP_LOGV(__PRETTY_FUNCTION__, "Remote devid:%d Post len:%d", devId, len);
    }

    if (req->method == http_method::HTTP_PUT)
    {
        if (len)
        {
            restInstance->postData[len] = 0;
            ESP_LOGV(__PRETTY_FUNCTION__, "postData(%d):%s", len, restInstance->postData);
            cJSON *newCfg = cJSON_ParseWithLength(restInstance->postData, len);
            if (newCfg)
            {
                if (true)
                {
                    ESP_LOGI(__PRETTY_FUNCTION__, "Updating local config");
                    appcfg->SetAppConfig(newCfg);
                    ret = httpd_resp_send(req, restInstance->postData, strlen(restInstance->postData));
                }
                else
                {
                    // char *fname = (char *)dmalloc(255);
                    // sprintf(fname, "/lfs/config/%d.json", devId);
                    // ESP_LOGI(__PRETTY_FUNCTION__, "Updating %s config", fname);
                    // FILE *cfg = fopen(fname, "w");
                    // if (cfg)
                    // {
                    //     fwrite(restInstance->postData, strlen(restInstance->postData), sizeof(char), cfg);
                    //     ret = httpd_resp_send(req, restInstance->postData, strlen(restInstance->postData));
                    //     TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += strlen(restInstance->postData);
                    //     fclose(cfg);
                    // }
                    // else
                    // {
                    //     httpd_resp_send_err(req, httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot save config");
                    // }
                    // ldfree(fname);
                }
                cJSON_Delete(newCfg);
            }
            else
            {
                ESP_LOGE(__PRETTY_FUNCTION__, "Cannot parse JSON");
                ret = httpd_resp_send_err(req, httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot parse JSON");
            }
        }
        else
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "Nothing posted");
            ret = httpd_resp_send_err(req, httpd_err_code_t::HTTPD_500_INTERNAL_SERVER_ERROR, "Empty request");
        }
    }
    else
    {
        if (true)
        {
            cJSON const *jcfg = appcfg->GetJSONConfig(nullptr);
            char *sjson = cJSON_PrintUnformatted(jcfg);
            if (sjson)
            {
                ret = httpd_resp_send(req, sjson, strlen(sjson));
                TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += strlen(sjson);
                ldfree(sjson);
            }
            else
            {
                if (jcfg) {
                    if (cJSON_IsInvalid(jcfg)){
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot printf the json as it's invalid.");
                    } else {
                        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot printf the json and I don't know why.");
                    }
                } else {
                    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot printf the json as it is somehow null");
                }
            }
        }
        else
        {
            // char *fname = (char *)dmalloc(255);
            // sprintf(fname, "/lfs/config/%d.json", devId);
            // ret = sendFile(req, fname);
            // ldfree(fname);
        }
    }

    return ret;
}

esp_err_t TheRest::list_files_handler(httpd_req_t *req)
{
    auto *jsonbuf = (char *)dmalloc(JSON_BUFFER_SIZE);
    memset(jsonbuf, 0, JSON_BUFFER_SIZE);
    *jsonbuf = '[';
    if (findFiles(req, req->uri + 6, nullptr, false, jsonbuf, JSON_BUFFER_SIZE - 1) != ESP_OK)
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Error wilst sending file list");
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
    ESP_LOGV(__PRETTY_FUNCTION__, "Getting %s url:%s(%d)", req->uri + 6, req->uri, strlen(jsonbuf));
    esp_err_t ret = httpd_resp_send_chunk(req, jsonbuf, strlen(jsonbuf));
    ESP_LOGV(__PRETTY_FUNCTION__, "Sent final chunck of %d", strlen(jsonbuf));
    ret = httpd_resp_send_chunk(req, nullptr, 0);
    ldfree(jsonbuf);
    return ret;
}

esp_err_t TheRest::status_handler(httpd_req_t *req)
{
    ESP_LOGV(__PRETTY_FUNCTION__, "Status Handler");
    esp_err_t ret = ESP_FAIL;

    const char *path = req->uri + 8;
    ESP_LOGV(__PRETTY_FUNCTION__, "uri:%s method: %s path:%s", req->uri, req->method == HTTP_POST ? "POST" : "PUT", path);

    if (req->method == http_method::HTTP_POST)
    {
        httpd_resp_set_type(req, "application/json");
        char *sjson = nullptr;
        if (strlen(path) == 0 || (strlen(path) == 1 && path[0] == '/'))
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "Getting root");
            sjson = cJSON_PrintUnformatted(status_json());
        }
#ifdef DEBUG_MALLOC
        else if (strcmp(path, "mallocs") == 0)
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "Getting mallocs");
            sjson = cJSON_PrintUnformatted(getMemoryStats());
        }
#endif
        else if (strcmp(path, "tasks") == 0)
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "Getting tasks");
            sjson = cJSON_PrintUnformatted(tasks_json());
        }
        else if (strcmp(path, "repeating_tasks") == 0)
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "Getting tasks");
            cJSON const *status = ManagedThreads::GetRepeatingTaskStatus();
            if (status){
                sjson = cJSON_PrintUnformatted(status);
            } else {
                sjson = (char*)dmalloc(3);
                strcpy(sjson, "[]");
            }
        }
        else if (strcmp(path, "app") == 0)
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "Getting app");
            sjson = cJSON_PrintUnformatted(AppConfig::GetAppStatus()->GetJSONConfig(nullptr));
        }
        else if (AppConfig::GetAppStatus()->HasProperty(path))
        {
            ESP_LOGV(__PRETTY_FUNCTION__, "Getting %s status", path);
            sjson = cJSON_PrintUnformatted(AppConfig::GetAppStatus()->GetJSONConfig(path));
        }
        if (sjson != nullptr)
        {
            ret = httpd_resp_send(req, sjson, strlen(sjson));
            TheRest::GetServer()->jBytesOut->valuedouble += strlen(sjson);
            ldfree(sjson);
        }
        else
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "Invalid uri:(%s) method: %s path:(%s) json:%s", req->uri, req->method == HTTP_POST ? "POST" : "PUT", path, sjson ? sjson : "null");
            ret = httpd_resp_send_404(req);
        }
    }
    if (req->method == HTTP_PUT)
    {
        if (endsWith(req->uri, "/cmd"))
        {
            ret = HandleSystemCommand(req);
        }
        else if (endsWith(req->uri, "status/"))
        {
            ret = HandleStatusChange(req);
        }
    }

    if (ret != ESP_OK)
    {
        httpd_resp_send_500(req);
    }
    ESP_LOGV(__PRETTY_FUNCTION__, "Done Getting");

    return ret;
}

esp_err_t TheRest::download_handler(httpd_req_t *req)
{
    ESP_LOGI(__PRETTY_FUNCTION__, "Downloading %s", req->uri);
    if (restInstance == nullptr) {
        restInstance = TheRest::GetServer();
    }

    int len = 0, curLen = -1, fnameLen=httpd_req_get_hdr_value_len(req, "filename");
    MFile *dest = nullptr;
    if (fnameLen++) {
        auto* fname = (char*)dmalloc(fnameLen);
        httpd_req_get_hdr_value_str(req,"filename",fname,fnameLen);
        dest = new MFile(fname);
        ldfree(fname);
    } else {
        dest = new MFile(req->uri);
    }
    
    while ((curLen = httpd_req_recv(req, restInstance->postData, JSON_BUFFER_SIZE)) > 0)
    {
        ESP_LOGV(__PRETTY_FUNCTION__, "Chunck len:%d, cur:%d method:%d", curLen, len, req->method);
        dest->Write((uint8_t *)restInstance->postData, curLen);
        len += curLen;
    }
    dest->Close();
    delete dest;
    
    ESP_LOGI(__PRETTY_FUNCTION__, "Post content len:%d method:%d", len, req->method);
    TheRest::GetServer()->jBytesIn->valuedouble = TheRest::GetServer()->jBytesIn->valueint += 2;

    return httpd_resp_send(req, RESP_OK, 2);
}

esp_err_t TheRest::ota_handler(httpd_req_t *req)
{
    if (indexOf(req->uri, "/ota/flash") == req->uri)
    {
        char *buf;
        size_t buf_len;
        ESP_LOGI(__PRETTY_FUNCTION__, "OTA REQUEST!!!!! RAM:%d.%s", esp_get_free_heap_size(), req->uri);
        esp_err_t ret = ESP_OK;

        buf_len = httpd_req_get_url_query_len(req) + 1;
        char md5[70];
        memset(md5, 0, 70);
        uint32_t totLen = 0;

        if ((buf_len > 1) && (buf_len < 4096000))
        {
            buf = (char *)dmalloc(buf_len);
            if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK)
            {
                ESP_LOGI(__PRETTY_FUNCTION__, "Found URL query => %s", buf);
                char param[30];
                memset(param, 0, 30);
                if ((httpd_query_key_value(buf, "md5", md5, 70) == ESP_OK) && (httpd_query_key_value(buf, "len", param, 30) == ESP_OK))
                {
                    ESP_LOGI(__PRETTY_FUNCTION__, "Found URL query parameter => md5=%s len=%s", md5, param);
                    totLen = atoi(param);
                    if (totLen <= 0) {
                        ESP_LOGE(__PRETTY_FUNCTION__, "Cannot get query len param:%d", totLen);
                        ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad len param");
                    } else if (md5[0] == 0) {
                        ESP_LOGE(__PRETTY_FUNCTION__, "Cannot get query md5 param:%s", md5);
                        ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad len param");
                    } else {
                        ESP_LOGI(__PRETTY_FUNCTION__,"remote md5:%s len:%d", md5, totLen);
                        if (initStorage(SPIFF_FLAG))
                        {
                            struct stat st;
                            bool hasmd5 = stat("/lfs/firmware/current.bin.md5", &st) == 0;

                            FILE *fw = nullptr;
                            if (!hasmd5 || ((fw = fopenCd("/lfs/firmware/current.bin.md5", "r", true)) != nullptr))
                            {
                                char ccmd5[70];
                                memset(ccmd5, 0, 70);
                                int len = 0;
                                if (!hasmd5 || ((len = fread((void *)ccmd5, 1, 70, fw)) > 0))
                                {
                                    if (hasmd5) {
                                        fclose(fw);
                                        ESP_LOGI(__PRETTY_FUNCTION__, "Local MD5:%s remote md5:%s", ccmd5, md5);
                                    }
                                    if (hasmd5 && (strcmp(ccmd5, md5) == 0))
                                    {
                                        ESP_LOGI(__PRETTY_FUNCTION__, "Firmware is not updated RAM:%d", esp_get_free_heap_size());
                                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 7;
                                        deinitStorage(SPIFF_FLAG);
                                        ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not new");
                                    }
                                    else
                                    {
                                        auto *img = (uint8_t *)dmalloc(totLen); //heap_caps_malloc(totLen, MALLOC_CAP_SPIRAM);
                                        memset(img, 0, totLen);
                                        ESP_LOGI(__PRETTY_FUNCTION__, "RAM:%d...len:%d md5:%s", esp_get_free_heap_size(), totLen, md5);

                                        memset(ccmd5, 0, 70);
                                        mbedtls_md_context_t ctx;
                                        mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

                                        mbedtls_md_init(&ctx);
                                        mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
                                        mbedtls_md_starts(&ctx);

                                        uint32_t curLen = 0;

                                        do
                                        {
                                            len = httpd_req_recv(req, (char *)img + curLen, MESSAGE_BUFFER_SIZE);
                                            TheRest::GetServer()->jBytesIn->valuedouble = TheRest::GetServer()->jBytesIn->valueint += len;
                                            if (len < 0)
                                            {
                                                ESP_LOGE(__PRETTY_FUNCTION__, "Error occurred during receiving: errno %d", errno);
                                                break;
                                            }
                                            else if (len == 0)
                                            {
                                                ESP_LOGW(__PRETTY_FUNCTION__, "Connection closed...");
                                                break;
                                            }
                                            else if ((curLen + len) > totLen)
                                            {
                                                ESP_LOGW(__PRETTY_FUNCTION__, "Bad len at (curLen(%d+%d) > totLen(%d)", curLen, len, totLen);
                                                break;
                                            }
                                            else
                                            {
                                                mbedtls_md_update(&ctx, img + curLen, len);
                                                curLen += len;
                                            }
                                        } while (curLen < totLen);

                                        uint8_t shaResult[70];
                                        mbedtls_md_finish(&ctx, shaResult);
                                        mbedtls_md_free(&ctx);
                                        ESP_LOGI(__PRETTY_FUNCTION__, "Total: %d/%d %s", totLen, curLen, md5);

                                        for (uint8_t i = 0; i < 32; ++i)
                                        {
                                            sprintf((char *)&ccmd5[i * 2], "%02x", (unsigned int)shaResult[i]);
                                        }

                                        FILE *fw;
                                        if (strcmp((char *)ccmd5, md5) == 0)
                                        {
                                            ESP_LOGI(__PRETTY_FUNCTION__, "Flashing md5:(%s)%dvs%d", ccmd5, totLen, curLen);
                                            ESP_LOGV(__PRETTY_FUNCTION__, "RAM:%d", esp_get_free_heap_size());

                                            struct stat st;

                                            if (stat("/lfs/firmware/current.bin", &st) == 0)
                                            {
                                                ESP_LOGV(__PRETTY_FUNCTION__, "current bin exists %d bytes", (int)st.st_size);
                                                if (!deleteFile("/lfs/firmware/current.bin"))
                                                {
                                                    ESP_LOGE(__PRETTY_FUNCTION__, "Cannot delete current bin");
                                                }
                                            }

                                            if ((fw = fopenCd("/lfs/firmware/current.bin", "w", true)) != nullptr)
                                            {
                                                ESP_LOGI(__PRETTY_FUNCTION__, "Writing /lfs/firmware/current.bin");
                                                if (fwrite((void *)img, 1, totLen, fw) == totLen)
                                                {
                                                    fclose(fw);

                                                    if ((fw = fopenCd("/lfs/firmware/tobe.bin.md5", "w", true)) != nullptr)
                                                    {
                                                        fwrite((void *)ccmd5, 1, sizeof(ccmd5), fw);
                                                        fclose(fw);
                                                        ESP_LOGI(__PRETTY_FUNCTION__, "Firmware md5 written");
                                                        esp_partition_iterator_t pi;
                                                        const esp_partition_t *factory;
                                                        esp_err_t err;

                                                        pi = esp_partition_find(ESP_PARTITION_TYPE_APP,            // Get partition iterator for
                                                                                ESP_PARTITION_SUBTYPE_APP_FACTORY, // Ashy Flashy partition
                                                                                "factory");
                                                        if (pi == nullptr) // Check result
                                                        {
                                                            ESP_LOGE(__PRETTY_FUNCTION__, "Failed to find factory partition");
                                                            ret=httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to find factory partition");
                                                        }
                                                        else
                                                        {
                                                            factory = esp_partition_get(pi);           // Get partition struct
                                                            esp_partition_iterator_release(pi);        // Release the iterator
                                                            err = esp_ota_set_boot_partition(factory); // Set partition for boot
                                                            if (err != ESP_OK)                         // Check error
                                                            {
                                                                ESP_LOGE(__PRETTY_FUNCTION__, "Failed to set boot partition");
                                                                ret=httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set boot partition");
                                                            }
                                                            else
                                                            {
                                                                httpd_resp_send(req, "Flashing...", 8);
                                                                TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 8;
                                                                dumpLogs();
                                                                WaitToSleep();
                                                                vTaskDelay(1000 / portTICK_PERIOD_MS);
                                                                dumpTheLogs((void *)true);
                                                                esp_system_abort("Flashing");
                                                                esp_restart(); // Restart ESP
                                                            }
                                                        }
                                                    }
                                                    else
                                                    {
                                                        ESP_LOGE(__PRETTY_FUNCTION__, "Cannot open /lfs/firmware/tobe.bin.md5.");
                                                        ret=httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Cannot open /lfs/firmware/tobe.bin.md5");
                                                    }
                                                }
                                                else
                                                {
                                                    ESP_LOGE(__PRETTY_FUNCTION__, "Firmware not backedup");
                                                    fclose(fw);
                                                    ret=httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Firmware not backedup");
                                                }
                                            }
                                            else
                                            {
                                                ESP_LOGE(__PRETTY_FUNCTION__, "Failed to open /lfs/firmware/current.bin");
                                                ret=httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to open /lfs/firmware/current.bin");
                                            }
                                            ldfree(img);
                                        }
                                        else
                                        {
                                            char* tmp = (char*)dmalloc(200);
                                            sprintf(tmp, "Bad Checksum:(%s)(%s) len:%d", ccmd5, md5, totLen);
                                            ESP_LOGV(__PRETTY_FUNCTION__, "%s", tmp);
                                            ret=httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, tmp);
                                            TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += strlen(tmp);
                                            ldfree(tmp);
                                            ldfree(img);
                                        }
                                    }
                                }
                                else
                                {
                                    fclose(fw);
                                    ESP_LOGE(__PRETTY_FUNCTION__, "Error with weird md5 len %d.", len);
                                    deinitStorage(SPIFF_FLAG);
                                    ret = httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Error with weird md5 len");
                                }
                            }
                            else
                            {
                                ESP_LOGE(__PRETTY_FUNCTION__, "Failed in opeing md5..");
                            }
                            deinitStorage(SPIFF_FLAG);
                        }
                        else
                        {
                            ESP_LOGE(__PRETTY_FUNCTION__, "Cannot init the fucking spiff or the md5:%d", md5[0]);
                        }
                    }
                }
            }
            else
            {
                ESP_LOGE(__PRETTY_FUNCTION__, "Cannot get query");
                ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cannot get query");
            }
            ldfree(buf);
        }
        else
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "Cannot get query len:%d", buf_len);
            ret = httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Cannot get query len");
        }

        return ret;
    }
    else if (indexOf(req->uri, "/ota/getmd5") == req->uri)
    {
        if (initStorage(SPIFF_FLAG))
        {
            FILE *fw = nullptr;
            if ((fw = fopenCd("/lfs/firmware/current.bin.md5", "r", true)) != nullptr)
            {
                char ccmd5[70];
                uint32_t len = 0;
                if ((len = fread((void *)ccmd5, 1, 70, fw)) > 0)
                {
                    ccmd5[len] = 0;
                    esp_err_t ret;
                    if ((ret = httpd_resp_send(req, ccmd5, len)) != ESP_OK)
                    {
                        ESP_LOGE(__PRETTY_FUNCTION__, "Error sending MD5:%s", esp_err_to_name(ret));
                    }
                    else
                    {
                        TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += len;
                        ESP_LOGI(__PRETTY_FUNCTION__, "Sent MD5:%s", ccmd5);
                    }
                    fclose(fw);
                    deinitStorage(SPIFF_FLAG);
                    return ret;
                }
                else
                {
                    ESP_LOGE(__PRETTY_FUNCTION__, "Error with weird md5 len %d", len);
                }
                fclose(fw);
            }
            else
            {
                ESP_LOGE(__PRETTY_FUNCTION__, "Failed in opeing md5...");
            }
        }
        else
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "Cannot init the fucking sd card");
        }
    }
    
    TheRest::GetServer()->jBytesOut->valuedouble = TheRest::GetServer()->jBytesOut->valueint += 6;
    deinitStorage(SPIFF_FLAG);
    return httpd_resp_send(req, "BADMD5", 6);
}

esp_err_t TheRest::stream_handler(httpd_req_t *req) {
    ESP_LOGI(__PRETTY_FUNCTION__,"Stream request");
    if (startsWith(req->uri, "/stream") && (req->method == HTTP_POST)) {
        ManagedDevice **dev = ManagedDevice::GetRunningInstances();
        for (uint32_t idx = 0; idx < ManagedDevice::GetNumRunningInstances(); idx++) {
            if (dev[idx] && (strcmp("Camera", dev[idx]->eventBase) == 0) && endsWith(req->uri,dev[idx]->GetName())) {
                return ((Camera*) dev[idx])->streamVideo(req);
            }
        }
        ESP_LOGI(__PRETTY_FUNCTION__,"Camera not found:%s",req->uri);
        return httpd_resp_send_404(req);
    } else {
        ESP_LOGI(__PRETTY_FUNCTION__,"Invalid Camrea request:%s",req->uri);
        return httpd_resp_send_500(req);
    }
}
