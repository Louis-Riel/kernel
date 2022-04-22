#include "eventmgr.h"
// #include <stdio.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include <limits.h>
// #include "driver/gpio.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "esp_event.h"
// #include "freertos/event_groups.h"
// #include "../../main/logs.h"
#include "../../main/utils.h"
// #include "cJSON.h"
// #include <regex>


ManagedThreads::ManagedThreads()
        :managedThreadBits(xEventGroupCreate()) 
        ,threadSema(xSemaphoreCreateMutex())
        ,threads((ManagedThreads::mThread_t **)dmalloc(32 * sizeof(void *)))
        ,numThreadSlot(0)
    {
        memset(threads, 0, 32 * sizeof(void *));
        xEventGroupSetBits(managedThreadBits,0xffff);
    }

uint8_t ManagedThreads::NumAllocatedThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 0 : 1;
        }
        return ret;
    }

    uint8_t ManagedThreads::NumUnallocatedThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 1 : 0;
        }
        return ret;
    }

    uint8_t ManagedThreads::NumRunningThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 0 : thread->isRunning ? 1
                                                          : 0;
        }
        return ret;
    }

    uint8_t ManagedThreads::NumDoneThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == NULL ? 0 : thread->started && !thread->isRunning ? 1
                                                                              : 0;
        }
        return ret;
    }

    cJSON* ManagedThreads::GetStatus()
    {
        cJSON *stat = cJSON_CreateObject();
        cJSON *jthreads = cJSON_AddArrayToObject(stat, "threads");
        cJSON_AddNumberToObject(stat, "allocated", NumAllocatedThreads());
        cJSON_AddNumberToObject(stat, "availableslots", NumUnallocatedThreads());
        cJSON_AddNumberToObject(stat, "running", NumRunningThreads());
        cJSON_AddNumberToObject(stat, "done", NumDoneThreads());
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            if (thread)
            {
                cJSON *jthread = cJSON_CreateObject();
                cJSON_AddItemToArray(jthreads, jthread);
                if (thread->pcName)
                    cJSON_AddStringToObject(jthread, "name", thread->pcName);
                cJSON_AddBoolToObject(jthread, "started", thread->started);
                cJSON_AddBoolToObject(jthread, "running", thread->isRunning);
            }
        }
        return stat;
    }

    void ManagedThreads::PrintState() {
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
        {
            for (uint8_t idx = 0; idx < 32; idx++)
            {
                mThread_t *thread = threads[idx];
                if (thread)
                {
                    ESP_LOGV(__FUNCTION__,"%d %s:isRunning:%d started:%d waitToSleep:%d",idx,thread->pcName,thread->isRunning, thread->started, thread->waitToSleep);
                }
            }
        }
    }

    void ManagedThreads::WaitToSleep()
    {
        uint32_t bitsToWaitFor = 0;
        PrintState();
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            if (thread && thread->waitToSleep && thread->isRunning)
            {
                ESP_LOGD(__FUNCTION__,"%s is sleep blocked running", thread->pcName);
                bitsToWaitFor += (1 << idx);
            }
        }
        WaitForThreads(bitsToWaitFor);
    }

    void ManagedThreads::WaitToSleepExceptFor(const char* name)
    {
        uint32_t bitsToWaitFor = 0;
        PrintState();
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            if (thread && thread->waitToSleep && thread->isRunning && !startsWith(thread->pcName,name))
            {
                ESP_LOGD(__FUNCTION__,"%s is sleep blocked running", thread->pcName);
                bitsToWaitFor += (1 << idx);
            }
        }
        if (bitsToWaitFor)
            WaitForThreads(bitsToWaitFor);
    }

    void ManagedThreads::WaitForThreads(uint32_t bitsToWaitFor)
    {
        if (bitsToWaitFor)
        {
            ESP_LOGD(__FUNCTION__, "Waiting for threads %d", bitsToWaitFor);
            xEventGroupWaitBits(managedThreadBits, bitsToWaitFor, false, true, portMAX_DELAY);
            ESP_LOGD(__FUNCTION__, "Threads %d done", bitsToWaitFor);
        }
        else
        {
            ESP_LOGV(__FUNCTION__, "No threads to wait for");
        }
    }

    uint8_t ManagedThreads::CreateBackgroundManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        UBaseType_t uxPriority,
        TaskHandle_t *const pvCreatedTask,
        const bool allowRelaunch,
        const bool waitToSleep)
    {

        if (!allowRelaunch && IsThreadRunning(pcName))
        {
            ESP_LOGW(__FUNCTION__, "Cannot run %s as it is already running", pcName);
            return UINT8_MAX;
        }
        ESP_LOGD(__FUNCTION__, "Running %s", pcName);
        xSemaphoreTake(threadSema,portMAX_DELAY);
        uint8_t bitNo = GetFreeBit(pcName);
        if (bitNo != UINT8_MAX)
        {
            ESP_LOGV(__FUNCTION__, "Running %s(%d)", pcName,bitNo);
            mThread_t *thread = threads[bitNo];
            if (thread->pcName) {
                ldfree(thread->pcName);
            }
            thread->pcName = (char *)dmalloc(strlen(pcName) + 1);
            strcpy(thread->pcName, pcName);
            thread->pvTaskCode = pvTaskCode;
            thread->usStackDepth = usStackDepth;
            thread->pvParameters = pvParameters;
            thread->uxPriority = uxPriority;
            thread->bitNo = bitNo;
            thread->waitToSleep = waitToSleep;
            thread->started = true;
            xSemaphoreGive(threadSema);
            xEventGroupClearBits(managedThreadBits, 1 << bitNo);

            BaseType_t ret = pdPASS;
            uint8_t retryCtn = 10;
            uint32_t runningBits = 0;
            while (retryCtn-- && (ret = xTaskCreate(ManagedThreads::runThread,
                                                    thread->pcName,
                                                    thread->usStackDepth,
                                                    (void *)thread,
                                                    thread->uxPriority,
                                                    &thread->pvCreatedTask)) != pdPASS)
            {
                if ((runningBits = GetRunningBits()))
                {
                    ESP_LOGW(__FUNCTION__, "Error in creating thread for %s(0x%" PRIXPTR "), retry %d, waiting on %d. %s", pcName, (uintptr_t)thread->pvTaskCode, retryCtn, runningBits, esp_err_to_name(ret));
                    xEventGroupWaitBits(managedThreadBits, runningBits, pdFALSE, pdFALSE, portMAX_DELAY);
                }
                else
                {
                    ESP_LOGW(__FUNCTION__, "Error in creating thread for %s(0x%" PRIXPTR "), retry %d. %s", pcName, (uintptr_t)thread->pvTaskCode, retryCtn, esp_err_to_name(ret));
                }
            }
            if (ret != pdPASS)
            {
                ESP_LOGE(__FUNCTION__, "Failed in creating thread for %s(0x%" PRIXPTR "). %s", pcName, (uintptr_t)thread->pvTaskCode, esp_err_to_name(ret));
                dumpTheLogs((void*)true);
                esp_restart();
            }
            if (pvCreatedTask != NULL)
            {
                *pvCreatedTask = thread->pvCreatedTask;
                AppConfig::SignalStateChange(state_change_t::THREADS);
            }
            return bitNo;
        }
        else
        {
            xSemaphoreGive(threadSema);
            ESP_LOGE(__FUNCTION__, "No more bits for %s", pcName);
            for (uint8_t idx = 0; idx < 32; idx++)
            {
                if ((threads[idx] != NULL) && threads[idx]->isRunning) {
                    ESP_LOGD(__FUNCTION__,"%d-%s",idx,threads[idx]->pcName);
                }
            }
        }

        return UINT8_MAX;
    };

    BaseType_t ManagedThreads::CreateInlineManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        const bool canRelanch,
        const bool waitToSleep)
    {
        return CreateInlineManagedTask(
            pvTaskCode, pcName, usStackDepth, pvParameters, canRelanch, waitToSleep, false);
    }

    BaseType_t ManagedThreads::CreateInlineManagedTask(
        TaskFunction_t pvTaskCode,
        const char *const pcName,
        const uint32_t usStackDepth,
        void *const pvParameters,
        const bool canRelanch,
        const bool waitToSleep,
        const bool onMainThread)
    {
        if (!canRelanch && IsThreadRunning(pcName))
        {
            return ESP_ERR_INVALID_STATE;
        }

        xSemaphoreTake(threadSema,portMAX_DELAY);
        uint8_t bitNo = GetFreeBit(pcName);
        if (bitNo != UINT8_MAX)
        {
            ESP_LOGV(__FUNCTION__, "Running %s(%d)", pcName,bitNo);
            mThread_t *thread = threads[bitNo];
            if (thread->pcName) {
                ldfree(thread->pcName);
            }
            thread->pcName = (char *)dmalloc(strlen(pcName) + 1);
            strcpy(thread->pcName, pcName);
            thread->pvTaskCode = pvTaskCode;
            thread->pvParameters = pvParameters;
            thread->usStackDepth = usStackDepth;
            thread->waitToSleep = waitToSleep;
            thread->started = true;
            xSemaphoreGive(threadSema);
            xEventGroupClearBits(managedThreadBits, 1 << bitNo);
            BaseType_t ret = ESP_OK;
            if (onMainThread)
            {
                ESP_LOGD(__FUNCTION__, "Starting the %s service", thread->pcName);
                thread->isRunning = true;
                thread->pvTaskCode(thread->pvParameters);
                thread->isRunning = false;
                thread->started = false;
                ESP_LOGV(__FUNCTION__, "Done initializing the %s service", thread->pcName);
                if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
                {
                    char *tmp = cJSON_Print(thread->parent->GetStatus());
                    ESP_LOGV(__FUNCTION__, "%s", tmp);
                    ldfree(tmp);
                }
            }
            else
            {
                if ((ret = xTaskCreate(ManagedThreads::runThread,
                                       thread->pcName,
                                       thread->usStackDepth,
                                       (void *)thread,
                                       tskIDLE_PRIORITY,
                                       &thread->pvCreatedTask)) == pdPASS)
                {
                    ESP_LOGV(__FUNCTION__, "Waiting for %s(0x%" PRIXPTR ") to finish", thread->pcName, (uintptr_t)thread->pvTaskCode);
                    xEventGroupWaitBits(managedThreadBits, 1 << bitNo, pdFALSE, pdTRUE, portMAX_DELAY);
                    ESP_LOGV(__FUNCTION__, "Done running %s(0x%" PRIXPTR ")", thread->pcName, (uintptr_t)thread->pvTaskCode);
                    thread->isRunning = false;
                    AppConfig::SignalStateChange(state_change_t::THREADS);
                    return ESP_OK;
                }
                else
                {
                    ESP_LOGE(__FUNCTION__, "Error running %s(0x%" PRIXPTR "): %s stack depth:%d", thread->pcName, (uintptr_t)thread->pvTaskCode, esp_err_to_name(ret), usStackDepth);
                    dumpTheLogs((void*)true);
                    esp_restart();
                }
            }
        } else {
            xSemaphoreGive(threadSema);
        }
        return ESP_ERR_NO_MEM;
    };

    void ManagedThreads::runThread(void *param)
    {
        mThread_t *thread = (mThread_t *)param;
        ESP_LOGV(__FUNCTION__, "Running the %s thread", thread->pcName);
        xEventGroupClearBits(thread->parent->managedThreadBits, 1 << thread->bitNo);
        thread->isRunning = true;

        size_t stacksz = heap_caps_get_free_size(MALLOC_CAP_32BIT);
        thread->pvTaskCode(thread->pvParameters);
        size_t diff = heap_caps_get_free_size(MALLOC_CAP_32BIT) - stacksz;
        if (diff != 0) {
            ESP_LOGV(__FUNCTION__,"%s %d bytes memleak",thread->pcName,diff);
        }

        thread->isRunning = false;
        thread->started = false;
        ESP_LOGV(__FUNCTION__, "Done running the %s thread", thread->pcName);
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
        {
            char *tmp = cJSON_Print(thread->parent->GetStatus());
            ESP_LOGV(__FUNCTION__, "%s", tmp);
        }

        xEventGroupSetBits(thread->parent->managedThreadBits, 1 << thread->bitNo);
        vTaskDelete(NULL);
    }

    ManagedThreads::mThread_t* ManagedThreads::GetThreadByName(const char *const pcName)
    {
        for (uint8_t idx = 0; idx < numThreadSlot; idx++)
        {
            if (strcmp(threads[idx]->pcName, pcName) == 0)
            {
                return threads[idx];
            }
        }
        return NULL;
    }

    bool ManagedThreads::IsThreadRunning(const char *const pcName)
    {
        mThread_t *t = GetThreadByName(pcName);
        return t ? t->isRunning : false;
    }

    uint32_t ManagedThreads::GetRunningBits()
    {
        uint32_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            if ((threads[idx] == NULL) || (threads[idx]->started && threads[idx]->isRunning))
            {
                ret |= (1 >> idx);
            }
        }
        return ret;
    }

    uint8_t ManagedThreads::GetFreeBit(const char* name)
    {
        uint8_t freeThread = UINT8_MAX;
        uint8_t unallocatedThread = UINT8_MAX;
        uint8_t lastRunning = UINT8_MAX;
        uint8_t ret = UINT8_MAX;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            if ((threads[idx] == NULL) || !threads[idx]->allocated) {
                unallocatedThread = idx;
                break;
            } else {
                if (!threads[idx]->started) {
                    freeThread=idx;
                }
                if (name && strcmp(name,threads[idx]->pcName) == 0) {
                    lastRunning = idx;
                }
            }
        }
        ESP_LOGV(__FUNCTION__,"freeThread(%d) unallocatedThread(%d) lastRunning(%d)",freeThread, unallocatedThread, lastRunning);

        if (name && (lastRunning != UINT8_MAX) && threads[lastRunning]->isRunning) {
            ESP_LOGW(__FUNCTION__,"%s already running", name);
        } else if (freeThread == UINT8_MAX) {
            if (unallocatedThread != UINT8_MAX){
                ESP_LOGV(__FUNCTION__,"Allocating new slot at %d for %s(%d)",unallocatedThread, name, threads[unallocatedThread] == NULL);
                threads[unallocatedThread] = (mThread_t*)dmalloc(sizeof(mThread_t));
                memset(threads[unallocatedThread],0,sizeof(mThread_t));
                threads[unallocatedThread]->parent = this;
                threads[unallocatedThread]->allocated = true;
                ESP_LOGV(__FUNCTION__,"Allocated new slot at %d for %s(%d)",unallocatedThread, name, threads[unallocatedThread] == NULL);
                ret = unallocatedThread;
            } else {
                ESP_LOGE(__FUNCTION__,"No more free slots");
            }
        } else {
            ret = freeThread;
        }
        return ret;
    }

BaseType_t CreateForegroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    void *const pvParameters)
{
    return ManagedThreads::GetInstance()->CreateInlineManagedTask(pvTaskCode, pcName, 8192, pvParameters, false, false, false);
};

void WaitToSleepExceptFor(const char* name)
{
    ManagedThreads::GetInstance()->WaitToSleepExceptFor(name);
}

void WaitToSleep()
{
    ManagedThreads::GetInstance()->WaitToSleep();
}

uint8_t CreateBackgroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *const pvCreatedTask)
{
    return ManagedThreads::GetInstance()->CreateBackgroundManagedTask(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pvCreatedTask, false, false);
};

uint8_t CreateWokeBackgroundTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    const uint32_t usStackDepth,
    void *const pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *const pvCreatedTask)
{
    return ManagedThreads::GetInstance()->CreateBackgroundManagedTask(pvTaskCode, pcName, usStackDepth, pvParameters, uxPriority, pvCreatedTask, false, true);
};
