#ifndef __mfile_h
#define __mfile_h

#include <stdio.h>
#include <string.h>
#include "esp_system.h"

#include "freertos/FreeRTOS.h"
#include <limits.h>
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "../../main/logs.h"
#include "../../main/utils.h"
#include "cJSON.h"
#include "eventmgr.h"

#define MAX_OPEN_FILES 5
#define MFILE_BUFFER_SIZE 30720

enum fileEventIds {
    WRITE,
    WRITE_LINE,
    OPEN_CREATE,
    OPEN_APPEND,
    FLUSH,
    CLOSE
};

class MFile:ManagedDevice {
public:
    MFile();
    MFile(const char* fileName);
    ~MFile();

    enum mfile_state_t {
        MFILE_INIT=BIT0,
        MFILE_OPENED=BIT1,
        MFILE_CLOSED_PENDING_WRITE=BIT2,
        MFILE_EXISTS=BIT3,
        MFILE_CLOSED=BIT4,
        MFILE_FAILED=BIT5
    } fileStatus;

    static MFile* GetFile(const char* fileName);
    void Open(const char* mode);
    void Close();
    void Write(uint8_t* data, uint32_t len);
    bool IsOpen();
    esp_event_base_t GetEventBase();
    const char *GetName();

    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
protected:

    EventHandlerDescriptor* BuildHandlerDescriptors();
    static cJSON *BuildStatus(void *instance);

    cJSON* status;
    cJSON* hasContent;
    cJSON* bytesWritten;
    static QueueHandle_t eventQueue;
    static MFile* openFiles[MAX_OPEN_FILES];
    static uint8_t numOpenFiles;
private:
    FILE *file;
};

class BufferedFile:MFile {
public:
    BufferedFile();
    BufferedFile(const char* fileName);
    ~BufferedFile();
    static BufferedFile* GetFile(const char* fileName);
    static BufferedFile* GetOpenedFile(const char* fileName);
    void Write(uint8_t* data, uint32_t len);
    void WriteLine(uint8_t* data, uint32_t len);
    void Flush();
    void Close();
    static void FlushAll();
    static void CloseAll();
    const char *GetName();
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
protected:
    cJSON* bytesCached;

private:
    bool isNewOrEmpty;
    uint8_t* buf = NULL;
    uint32_t maxBufSize = MFILE_BUFFER_SIZE;
    uint32_t pos = 0;
    char eol = 10;
};

#endif