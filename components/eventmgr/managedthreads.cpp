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

#define MAX_REPEATING_TASKS 12
static bool isRepeatingLooping = false;
static struct repeating_task_t* repeating_tasks = nullptr;
static cJSON* repeatingTasks = nullptr;

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
            ret += thread == nullptr ? 0 : 1;
        }
        return ret;
    }

    uint8_t ManagedThreads::NumUnallocatedThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == nullptr ? 1 : 0;
        }
        return ret;
    }

    uint8_t ManagedThreads::NumRunningThreads()
    {
        uint8_t ret = 0;
        for (uint8_t idx = 0; idx < 32; idx++)
        {
            mThread_t *thread = threads[idx];
            ret += thread == nullptr ? 0 : thread->isRunning ? 1
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
            ret += thread == nullptr ? 0 : thread->started && !thread->isRunning ? 1
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
                ESP_LOGI(__FUNCTION__,"%s is sleep blocked running", thread->pcName);
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
                ESP_LOGI(__FUNCTION__,"%s is sleep blocked running", thread->pcName);
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
            ESP_LOGI(__FUNCTION__, "Waiting for threads %d", bitsToWaitFor);
            xEventGroupWaitBits(managedThreadBits, bitsToWaitFor, false, true, portMAX_DELAY);
            ESP_LOGI(__FUNCTION__, "Threads %d done", bitsToWaitFor);
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
        ESP_LOGI(__FUNCTION__, "Running %s", pcName);
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
                    xEventGroupWaitBits(managedThreadBits, runningBits, pdFALSE, pdFALSE, pdMS_TO_TICKS(1000));
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
            if (pvCreatedTask != nullptr)
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
                if ((threads[idx] != nullptr) && threads[idx]->isRunning) {
                    ESP_LOGI(__FUNCTION__,"%d-%s",idx,threads[idx]->pcName);
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
                ESP_LOGI(__FUNCTION__, "Starting the %s service", thread->pcName);
                thread->isRunning = true;
                thread->pvTaskCode(thread->pvParameters);
                thread->isRunning = false;
                thread->started = false;
                ESP_LOGV(__FUNCTION__, "Done initializing the %s service", thread->pcName);
                if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
                {
                    cJSON* jtmp = thread->parent->GetStatus();
                    char *tmp = cJSON_Print(jtmp);
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
        ESP_LOGI(__FUNCTION__, "Running the %s thread", thread->pcName);
        xEventGroupClearBits(thread->parent->managedThreadBits, 1 << thread->bitNo);
        thread->isRunning = true;

        //size_t stacksz = heap_caps_get_free_size(MALLOC_CAP_32BIT);
        thread->pvTaskCode(thread->pvParameters);
        //size_t diff = heap_caps_get_free_size(MALLOC_CAP_32BIT) - stacksz;
        //if (diff != 0) {
        //    ESP_LOGV(__FUNCTION__,"%s %d bytes memleak",thread->pcName,diff);
        //}

        thread->isRunning = false;
        thread->started = false;
        ESP_LOGV(__FUNCTION__, "Done running the %s thread", thread->pcName);
        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE)
        {
            cJSON* jtmp = thread->parent->GetStatus();
            char *tmp = cJSON_Print(jtmp);
            ESP_LOGV(__FUNCTION__, "%s", tmp);
        }

        xEventGroupSetBits(thread->parent->managedThreadBits, 1 << thread->bitNo);

        if (thread->pcName) {
            ldfree(thread->pcName);
            thread->pcName=nullptr;
        }

        vTaskDelete(nullptr);
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
        return nullptr;
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
            if ((threads[idx] == nullptr) || (threads[idx]->started && threads[idx]->isRunning))
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
            if ((threads[idx] == nullptr) || !threads[idx]->allocated) {
                unallocatedThread = idx;
                break;
            } else {
                if (!threads[idx]->started) {
                    freeThread=idx;
                }
                if (name && threads[idx] && threads[idx]->pcName && strcmp(name,threads[idx]->pcName) == 0) {
                    lastRunning = idx;
                }
            }
        }
        ESP_LOGV(__FUNCTION__,"freeThread(%d) unallocatedThread(%d) lastRunning(%d)",freeThread, unallocatedThread, lastRunning);

        if (name && (lastRunning != UINT8_MAX) && threads[lastRunning]->isRunning) {
            ESP_LOGW(__FUNCTION__,"%s already running", name);
        } else if (freeThread == UINT8_MAX) {
            if (unallocatedThread != UINT8_MAX){
                ESP_LOGV(__FUNCTION__,"Allocating new slot at %d for %s(%d)",unallocatedThread, name, threads[unallocatedThread] == nullptr);
                threads[unallocatedThread] = (mThread_t*)dmalloc(sizeof(mThread_t));
                memset(threads[unallocatedThread],0,sizeof(mThread_t));
                threads[unallocatedThread]->parent = this;
                threads[unallocatedThread]->allocated = true;
                ESP_LOGV(__FUNCTION__,"Allocated new slot at %d for %s(%d)",unallocatedThread, name, threads[unallocatedThread] == nullptr);
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

struct repeating_task_t {
    TaskFunction_t pvTaskCode;
    char * pcName;
    void * pvParameters;
    uint32_t period;
    TickType_t last_run;
    TickType_t next_run;
    uint64_t last_run_delta;
    cJSON* jName;
    cJSON* jPeriod;
    cJSON* jLastRun;
    cJSON* jNextRun;
    cJSON* jLastRunDelta;
    cJSON* jNumRuns;
};

bool RTDue(repeating_task_t* task, TickType_t now) {
    return ((task->next_run == 0) || (now >= task->next_run));
}

cJSON* ManagedThreads::GetRepeatingTaskStatus() {
    return repeatingTasks;
}

void TheRepeatingLoop(void* param) {
    TickType_t now = 0;
    TickType_t next_run = 0;
    ESP_LOGI(__FUNCTION__, "Starting repeating task loop");
    repeatingTasks = cJSON_CreateArray();
    while (isRepeatingLooping) {
        now = xTaskGetTickCount();
        next_run = 0;
        for (uint8_t idx=0; idx < MAX_REPEATING_TASKS; idx++) {
            repeating_task_t* task = &repeating_tasks[idx];
            if (task->pvTaskCode && RTDue(task, now)) {
                task->last_run = now;
                task->last_run_delta = esp_timer_get_time();
                task->pvTaskCode(task->pvParameters);
                task->next_run = now + pdMS_TO_TICKS(task->period);
                task->last_run_delta = esp_timer_get_time() - task->last_run_delta;

                if (task->jName == nullptr) {
                    cJSON* js = cJSON_CreateObject();
                    cJSON_AddItemToArray(repeatingTasks, js);
                    task->jName = cJSON_AddStringToObject(js, "name", task->pcName);
                    task->jPeriod = cJSON_AddNumberToObject(js, "period", task->period);
                    task->jLastRun = cJSON_AddNumberToObject(js, "last_run", task->last_run);
                    task->jNextRun = cJSON_AddNumberToObject(js, "next_run", task->next_run);
                    task->jLastRunDelta = cJSON_AddNumberToObject(js, "last_run_delta", task->last_run_delta);
                    task->jNumRuns = cJSON_AddNumberToObject(js, "executions", 1);
                } else {
                    cJSON_SetNumberValue(task->jLastRun, task->last_run);
                    cJSON_SetNumberValue(task->jPeriod, task->period);
                    cJSON_SetNumberValue(task->jNextRun, task->next_run);
                    cJSON_SetNumberValue(task->jLastRunDelta, task->last_run_delta);
                    cJSON_SetNumberValue(task->jNumRuns, task->jNumRuns->valueint + 1);
                }
            }

            if (task->pvTaskCode && ((next_run == 0) || (next_run > task->next_run))) {
                next_run = task->next_run;
            }
        }

        if ((next_run != 0) && (next_run > now)) {
            vTaskDelayUntil(&now, next_run - now);
        } else {
            if (next_run != 0) {
                ESP_LOGW(__FUNCTION__,"Repeating task overrun");
            } else {
                ESP_LOGW(__FUNCTION__,"No repeating tasks found");
                isRepeatingLooping = false;
            }
        }
    }
    for (uint8_t idx=0; idx < MAX_REPEATING_TASKS; idx++) {
        repeating_task_t* task = &repeating_tasks[idx];
        if (task->pcName) {
            ldfree((void*)task->pcName);
        }
    }
    ESP_LOGI(__FUNCTION__, "Stopped repeating task loop");
    cJSON_free(repeatingTasks);
    ldfree(repeating_tasks);
    repeating_tasks=nullptr;
}

void RunRepeatingLoop() {
    if (!isRepeatingLooping) {
        isRepeatingLooping = true;
        CreateBackgroundTask(TheRepeatingLoop, "RepeatingLoop", 8192, nullptr, tskIDLE_PRIORITY, nullptr);
    }
}

uint8_t GetRepeatingIndex() {
    if (repeating_tasks == nullptr) {
        repeating_tasks = (struct repeating_task_t*)dmalloc(sizeof(struct repeating_task_t)*MAX_REPEATING_TASKS);
        memset(repeating_tasks,0,sizeof(struct repeating_task_t)*MAX_REPEATING_TASKS);
    } 
    for (uint8_t idx = 0; idx < MAX_REPEATING_TASKS; idx++) {
        if (repeating_tasks[idx].pvTaskCode == nullptr) {
            return idx;
        }
    }
    return UINT8_MAX;
}

uint8_t CreateRepeatingTask(
    TaskFunction_t pvTaskCode,
    const char *const pcName,
    void *const pvParameters,
    const uint32_t repeatPeriod) {

    uint8_t idx = GetRepeatingIndex();

    if (idx == UINT8_MAX) {
        ESP_LOGE(__FUNCTION__,"No more repeating tasks");
        return UINT8_MAX;
    }

    repeating_task_t* task = &repeating_tasks[idx];
    task->pvTaskCode = pvTaskCode;
    task->pcName = strdup(pcName);
    task->pvParameters = pvParameters;
    task->period = repeatPeriod;

    if (!isRepeatingLooping) {
        RunRepeatingLoop();
    }
    return idx;
}

void UpdateRepeatingTaskPeriod(
    const uint32_t idx,
    const uint32_t repeatPeriod) {
    if (idx < UINT8_MAX) {
        repeating_tasks[idx].period = repeatPeriod;
    }
}
