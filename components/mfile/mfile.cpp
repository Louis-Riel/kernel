#include "mfile.h"
#include "math.h"

//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))

#define FILEBUFFER_UNFLUSHED_TIMEOUT_SECS 30

MFile* MFile::openFiles[];
uint8_t MFile::numOpenFiles=0;
QueueHandle_t MFile::eventQueue;
esp_timer_handle_t BufferedFile::refreshHandle=nullptr;
esp_event_handler_instance_t* MFile::handlerInstance=nullptr;
const char* MFile::MFILE_BASE="MFile";

MFile::~MFile(){
    ESP_LOGV(__PRETTY_FUNCTION__,"Destructor %s",name);
    if (file && IsOpen()) {
        Close();
    }
}

MFile::MFile()
    :ManagedDevice(MFILE_BASE)
    ,file(nullptr)
{
    ESP_LOGV(__PRETTY_FUNCTION__,"Building MFile");
    status = BuildStatus(this);
    if (numOpenFiles == 0) {
        memset(openFiles,0,sizeof(void*)*MAX_OPEN_FILES);
    }

    if (handlerDescriptors == nullptr){
        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
    }
}

MFile::MFile(const char* fileName)
    :ManagedDevice(MFILE_BASE, fileName)
    ,file(nullptr)
{
    if (strlen(fileName) > 1){
        status = BuildStatus(this);
        if (numOpenFiles == 0) {
            memset(openFiles,0,sizeof(void*)*MAX_OPEN_FILES);
        }

        if (handlerDescriptors == nullptr){
            EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
        }

        ESP_LOGV(__PRETTY_FUNCTION__,"Opening file %s",name);
        cJSON* jcfg;
        auto* apin = new AppConfig((jcfg=ManagedDevice::status),AppConfig::GetAppStatus());
        apin->SetIntProperty("status",mfile_state_t::MFILE_INIT);
        apin->SetBoolProperty("hasContent",0);
        apin->SetIntProperty("bytesWritten",0);
        cJSON* methods = cJSON_AddArrayToObject(jcfg,"commands");
        cJSON* flush = cJSON_CreateObject();
        cJSON_AddItemToArray(methods,flush);
        cJSON_AddStringToObject(flush,"command","flush");
        cJSON_AddStringToObject(flush,"HTTP_METHOD","PUT");
        cJSON_AddStringToObject(flush,"caption","flush");

        status = apin->GetPropertyHolder("status");
        hasContent = apin->GetPropertyHolder("hasContent");
        bytesWritten = apin->GetPropertyHolder("bytesWritten");
        delete apin;
    } 
}

EventHandlerDescriptor* MFile::BuildHandlerDescriptors(){
  ESP_LOGV(__PRETTY_FUNCTION__,"MFile: BuildHandlerDescriptors");
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(fileEventIds::WRITE_LINE,"WRITE_LINE");
  handler->AddEventDescriptor(fileEventIds::WRITE,"WRITE");
  handler->AddEventDescriptor(fileEventIds::OPEN_CREATE,"OPEN_CREATE");
  handler->AddEventDescriptor(fileEventIds::OPEN_APPEND,"OPEN_APPEND");
  handler->AddEventDescriptor(fileEventIds::FLUSH,"FLUSH");
  handler->AddEventDescriptor(fileEventIds::CLOSE,"CLOSE");
  return handler;
}

MFile* MFile::GetFile(const char* fileName){
    if ((fileName == nullptr) || !strlen(fileName)) {
        return nullptr;
    }

    if (numOpenFiles > MAX_OPEN_FILES) {
        ESP_LOGW(__PRETTY_FUNCTION__,"Ran Out of files at %s",fileName);
        for (uint8_t idx = 0; idx < numOpenFiles; idx++) {
            if (openFiles[idx])
                ESP_LOGI(__PRETTY_FUNCTION__,"%s-%d",openFiles[idx]->GetFilename(),openFiles[idx]->fileStatus);
        }
        return nullptr;
    }

    for (uint idx=0; idx < numOpenFiles; idx++) {
        MFile* file = openFiles[idx];
        if (strcmp(fileName,file->GetFilename()) == 0) {
            ESP_LOGV(__PRETTY_FUNCTION__,"Pulling %s from open files as %d, %d open files",file->GetFilename(),idx, numOpenFiles);
            return file;
        }
    }
    ESP_LOGV(__PRETTY_FUNCTION__,"Opening %s into open files as %d",fileName,numOpenFiles);
    return openFiles[numOpenFiles++]=new MFile(fileName);
}

void MFile::Open(const char* mode){
    if ((name == nullptr) || (strlen(name) == 0)) {
        ESP_LOGE(__PRETTY_FUNCTION__,"Empty name error");
        fileStatus = mfile_state_t::MFILE_FAILED;
        return;
    }

    if (startsWith(name, "/sdcard")) {
        initStorage(SDCARD_FLAG);
    } else {
        initStorage(SPIFF_FLAG);
    }

    file = fopenCd(name, mode,true);
    if (file == nullptr)
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Failed to open %s for %s", name, mode);
        fileStatus = mfile_state_t::MFILE_FAILED;
        return;
    }
    ESP_LOGV(__PRETTY_FUNCTION__, "Open %s for %s",name, mode);
    struct stat st;

    int ret = 0;

    ret = stat(name, &st);

    if (ret == 0)
    {
        cJSON_SetIntValue(hasContent,st.st_size>0);
    }

    fileStatus = (mfile_state_t)(fileStatus|mfile_state_t::MFILE_OPENED);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_CLOSED);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_CLOSED_PENDING_WRITE);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_INIT);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_FAILED);
}

void MFile::Close(){
    if (file != nullptr)
    {
        ESP_LOGV(__PRETTY_FUNCTION__, "Closed %s",name);
        fclose(file);
        file=nullptr;
        fileStatus = (mfile_state_t)(fileStatus|mfile_state_t::MFILE_CLOSED);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_OPENED);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_CLOSED_PENDING_WRITE);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_INIT);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_FAILED);
        if (startsWith(name, "/sdcard")) {
            deinitStorage(SDCARD_FLAG);
        } else {
            deinitStorage(SPIFF_FLAG);
        }
    }
}

bool MFile::IsOpen() const{
    return file != nullptr && (fileStatus & mfile_state_t::MFILE_OPENED);
}

void MFile::Write(uint8_t* data, uint32_t len) {
    bool wasOpened = IsOpen();
    if (!wasOpened) {
        Open("a");
    }
    if (file != nullptr)
    {
        ESP_LOGV(__PRETTY_FUNCTION__,"Writing %d",len);
        fwrite(data,sizeof(uint8_t),len,file);
        bytesWritten->valuedouble = bytesWritten->valueint = bytesWritten->valueint + len;
    }
    if (!wasOpened){
        Close();
    }
}

void MFile::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    ESP_LOGV(__PRETTY_FUNCTION__,"Event %s-%d",base,id);
    MFile* efile;
    auto* params = new AppConfig(*(cJSON**)event_data);
    //char* name = EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("name"));
    if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
        char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(nullptr));
        ESP_LOGW(__PRETTY_FUNCTION__,"Params:%s...",tmp);
        ldfree(tmp);
    }

    switch (id)
    {
    case fileEventIds::OPEN_CREATE:
        if ((params != nullptr) && params->HasProperty("name")){
            efile = GetFile(params->GetStringProperty("name"));
            if (efile)
                efile->Open("c");
        } else {
            ESP_LOGW(__PRETTY_FUNCTION__,"Missing params:%d",params!=nullptr);
        }
        break;
    case fileEventIds::OPEN_APPEND:
        if ((params != nullptr) && params->HasProperty("name")){
            efile = GetFile(params->GetStringProperty("name"));
            if (efile)
                efile->Open("a");
        } else {
            ESP_LOGW(__PRETTY_FUNCTION__,"Missing params:%d",params!=nullptr);
        }
        break;
    case fileEventIds::CLOSE:
        if ((params != nullptr) && params->HasProperty("name")){
            ESP_LOGV(__PRETTY_FUNCTION__,"Closing");
            efile = GetFile(params->GetStringProperty("name"));
            if (efile)
                efile->Close();
        } else {
            ESP_LOGW(__PRETTY_FUNCTION__,"Missing params:%d",params!=nullptr);
        }
        break;
    case fileEventIds::FLUSH:
        if ((params != nullptr) && params->HasProperty("name")){
            BufferedFile::GetFile(params->GetStringProperty("name"))->Flush();
        }
        break;
    case fileEventIds::WRITE:
        if ((params != nullptr) && params->HasProperty("name")){
            efile = GetFile(params->GetStringProperty("name"));
            auto* buf = (uint8_t*)params->GetStringProperty("name");
            if (efile && buf) {
                efile->Write(buf,strlen((char*)buf));
            }
        } else {
            ESP_LOGW(__PRETTY_FUNCTION__,"Missing params:%d",params!=nullptr);
        }
        break;
    default:
        break;
    }
    delete params;
}

esp_event_base_t MFile::GetEventBase(){
    return MFILE_BASE;    
}

const char* MFile::GetFilename(){
    return name;
}

const char* BufferedFile::GetFilename(){
    return MFile::GetFilename();
}

BufferedFile::BufferedFile()
    :MFile()
{
    if (handlerInstance == nullptr)
        ESP_ERROR_CHECK(esp_event_handler_instance_register(MFILE_BASE, ESP_EVENT_ANY_ID, ProcessEvent, this, handlerInstance));
}

BufferedFile::BufferedFile(const char* fileName)
    :MFile(fileName)
    ,isNewOrEmpty(false) {
    struct stat st;
    int ret = 0;
    ret = stat(fileName, &st);
    isNewOrEmpty = (ret != 0) || (st.st_size==0);
    if (ret == ESP_OK) {
        ESP_LOGV(__PRETTY_FUNCTION__,"Opening file %s is new or empty:%d, size:%li",fileName, isNewOrEmpty, st.st_size);
    } else {
        ESP_LOGV(__PRETTY_FUNCTION__,"Creating file %s is new or empty:%d",fileName, isNewOrEmpty);
    }
    auto* apin = new AppConfig(((MFile*)this)->status,AppConfig::GetAppStatus());
    apin->SetIntProperty("bytesCached",0);
    bytesCached = apin->GetPropertyHolder("bytesCached");
    delete apin;
    if (numOpenFiles < MAX_OPEN_FILES)
        openFiles[numOpenFiles++]=this;
}


void BufferedFile::Flush() {
    if (buf && pos) {
        ESP_LOGV(__PRETTY_FUNCTION__,"Flushing %s",GetFilename());
        MFile::Write(buf,pos);
        pos=0;
        cJSON_SetIntValue(bytesCached,pos);
        AppConfig::SignalStateChange(state_change_t::MAIN);
    } else {
        ESP_LOGV(__PRETTY_FUNCTION__,"Not Flushing %s, pos:%d",GetFilename(),pos);
    }
}

void BufferedFile::FlushAll() {
    ESP_LOGV(__PRETTY_FUNCTION__,"Flushing all");
    for (int idx=0; idx < MAX_OPEN_FILES; idx++) {
        if (openFiles[idx]){
            ((BufferedFile*)openFiles[idx])->Flush();
        }
    }
}

void BufferedFile::CloseAll() {
    for (int idx=0; idx < MAX_OPEN_FILES; idx++) {
        if (openFiles[idx]){
            ((BufferedFile*)openFiles[idx])->Flush();
            ((BufferedFile*)openFiles[idx])->Close();
        }
    }
}

BufferedFile* BufferedFile::GetOpenedFile(const char* fileName){
    if ((fileName == nullptr) || !strlen(fileName)) {
        return nullptr;
    }

    if (numOpenFiles > MAX_OPEN_FILES) {
        ESP_LOGW(__PRETTY_FUNCTION__,"Ran out of files at %s",fileName);
        for (uint8_t idx = 0; idx < numOpenFiles; idx++) {
            if (openFiles[idx])
                ESP_LOGV(__PRETTY_FUNCTION__,"%s-%d",openFiles[idx]->GetFilename(),openFiles[idx]->fileStatus);
        }
        return nullptr;
    }

    for (uint idx=0; idx < numOpenFiles; idx++) {
        auto* file = (BufferedFile*)openFiles[idx];
        if (strcmp(file->GetFilename(),fileName) == 0) {
            return file;
        }
    }
    ESP_LOGV(__PRETTY_FUNCTION__,"No open file %s",fileName);
    return nullptr;
}

BufferedFile* BufferedFile::GetFile(const char* fileName){
    if ((fileName == nullptr) || !strlen(fileName)) {
        return nullptr;
    }

    for (uint idx=0; idx < numOpenFiles; idx++) {
        auto* file = (BufferedFile*)openFiles[idx];
        if (strcmp(file->GetFilename(),fileName) == 0) {
            return file;
        }
    }
    if (numOpenFiles > MAX_OPEN_FILES) {
        ESP_LOGW(__PRETTY_FUNCTION__,"Ran out of files at %s",fileName);
        for (uint8_t idx = 0; idx < numOpenFiles; idx++) {
            if (openFiles[idx])
                ESP_LOGV(__PRETTY_FUNCTION__,"%s-%d",openFiles[idx]->GetFilename(),openFiles[idx]->fileStatus);
        }

        return nullptr;
    }

    ESP_LOGV(__PRETTY_FUNCTION__,"Creating file %s",fileName);
    return new BufferedFile(fileName);
}

void BufferedFile::WriteLine(uint8_t* data, uint32_t len) {
    Write(data,len);
    Write((uint8_t*)&eol,1);
}

void BufferedFile::waitingWrites(void* params){
    BufferedFile::FlushAll();
}

void BufferedFile::Write(uint8_t* data, uint32_t len) {
    if (BufferedFile::refreshHandle != nullptr) {
        esp_timer_stop(BufferedFile::refreshHandle);
        esp_timer_delete(BufferedFile::refreshHandle);
        BufferedFile::refreshHandle=nullptr;
    }
    esp_timer_create_args_t logTimerArgs=(esp_timer_create_args_t){
        waitingWrites,
        (void*)nullptr,
        esp_timer_dispatch_t::ESP_TIMER_TASK,
        "BF Waiter"};
    ESP_ERROR_CHECK(esp_timer_create(&logTimerArgs,&BufferedFile::refreshHandle));
    ESP_ERROR_CHECK(esp_timer_start_once(BufferedFile::refreshHandle, FILEBUFFER_UNFLUSHED_TIMEOUT_SECS * 1000000));

    if (buf == nullptr) {
        buf = (uint8_t*)dmalloc(maxBufSize);
    }
    if (buf != nullptr) {
        cJSON_SetIntValue(hasContent,true);
        uint32_t remaining=len;
        uint32_t dataPos=0;
        while (remaining>0){
            if ((pos+remaining) >= maxBufSize) {
                ESP_LOGV(__PRETTY_FUNCTION__,"Flushing for first chuck");
                Flush();
            }

            if ((pos+len) > maxBufSize) {
                if (pos > 0) {
                    ESP_LOGV(__PRETTY_FUNCTION__,"Flushing for next chuck");
                    Flush();
                }
                MFile::Write(data+dataPos,maxBufSize);
                dataPos+=maxBufSize;
            } else {
                memcpy(buf+pos,data,len);
                pos+=len;
                ESP_LOGV(__PRETTY_FUNCTION__,"Buffered %d",pos);
                cJSON_SetIntValue(bytesCached,bytesCached->valueint+len);
            }
            remaining-=len;
        }
    }
}

void BufferedFile::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    if (strcmp(base,MFILE_BASE) == 0) {
        ESP_LOGV(__PRETTY_FUNCTION__,"Event %s-%d",base,id);

        if (event_data == nullptr) {
            ESP_LOGE(__PRETTY_FUNCTION__,"Missing params, no go");
            return;
        }

        if (cJSON_IsInvalid(*(cJSON**)event_data)) {
            ESP_LOGW(__PRETTY_FUNCTION__,"Invalid input json 0x%" PRIXPTR ", no go",(uintptr_t)*(cJSON**)event_data);
            return;
        }

        if (LOG_LOCAL_LEVEL >= ESP_LOG_VERBOSE){
            char *tmp = cJSON_Print(*(cJSON**)event_data);
            ESP_LOGW(__PRETTY_FUNCTION__, "Missing event id or base:%s", tmp == nullptr ? "null" : tmp);
            if (tmp)
                ldfree(tmp);
        }

        auto* params = new AppConfig(*(cJSON**)event_data);
        uint8_t* strbuf = nullptr;
        uint8_t* headerLine = nullptr;
        char* name = EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("name"));

        ESP_LOGV(__PRETTY_FUNCTION__,"NAME:%s...",name);
        auto* efile = GetFile(name);
        if (!efile) {
            delete params;
            ESP_LOGE(__PRETTY_FUNCTION__,"No files available, no go");
            return;
        }
        switch (id)
        {
        case fileEventIds::CLOSE:
            if (efile) {
                efile->Close();
            }
        break;
        case fileEventIds::WRITE:
        case fileEventIds::WRITE_LINE:
            if (efile && params && params->HasProperty("value")){
                if (params->HasProperty("header") && !efile->hasContent->valueint) {
                    headerLine = (uint8_t*)EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("header"));
                    if (headerLine) {
                        ESP_LOGV(__PRETTY_FUNCTION__,"header:%s",headerLine);
                        id==fileEventIds::WRITE_LINE?efile->WriteLine(headerLine,strlen((char*)headerLine)):efile->Write(headerLine,strlen((char*)headerLine));
                        ldfree(headerLine);
                    } else {
                        ESP_LOGW(__PRETTY_FUNCTION__,"Invalid of empty header line");
                    }
                }
                strbuf = (uint8_t*)EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("value"));
                ESP_LOGV(__PRETTY_FUNCTION__,"value:%s",strbuf);
                id==fileEventIds::WRITE_LINE?efile->WriteLine(strbuf,strlen((char*)strbuf)):efile->Write(strbuf,strlen((char*)strbuf));
                ldfree(strbuf);
            } else {
                ESP_LOGW(__PRETTY_FUNCTION__,"Missing params:%d or file:%d name:%d or missing param when both are true.",params==nullptr,efile==nullptr, name==nullptr);
                if (name != nullptr) {
                    ESP_LOGV(__PRETTY_FUNCTION__,"Name:%s",name);
                }
            }
            break;
        default:
            MFile::ProcessEvent(handler_args,base,id,event_data);
            break;
        }
        ldfree(name);
        delete params;
    }
}

void BufferedFile::Close(){
    Flush();
    MFile::Close();
    if (buf)
        ldfree(buf);
}