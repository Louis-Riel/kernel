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
    MFile(char* fileName);
    ~MFile();

    enum mfile_state_t {
        MFILE_INIT=BIT0,
        MFILE_OPENED=BIT1,
        MFILE_CLOSED_PENDING_WRITE=BIT2,
        MFILE_EXISTS=BIT3,
        MFILE_CLOSED=BIT4,
        MFILE_FAILED=BIT5
    } fileStatus;

    static MFile* GetFile(char* fileName);
    void Open(const char* mode);
    void Close();
    void Write(uint8_t* data, uint32_t len);
    bool IsOpen();
    bool hasContent;
    esp_event_base_t GetEventBase();

    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
protected:

    EventHandlerDescriptor* BuildHandlerDescriptors();

    static QueueHandle_t eventQueue;

    char* name;

    static MFile* openFiles[MAX_OPEN_FILES];
    static uint8_t numOpenFiles;
private:
    FILE *file;
};

class BufferedFile:MFile {
public:
    BufferedFile();
    BufferedFile(char* fileName);
    ~BufferedFile();
    static BufferedFile* GetFile(char* fileName);
    void Write(uint8_t* data, uint32_t len);
    void WriteLine(uint8_t* data, uint32_t len);
    void Flush();
    void Close();
    static void FlushAll();
    static void CloseAll();
    static void ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data);
protected:

private:
    bool isNewOrEmpty;
    uint8_t* buf = NULL;
    uint32_t maxBufSize = 8092;
    uint32_t pos = 0;
    char eol = 10;
};

#endif