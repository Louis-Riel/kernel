#include "mfile.h"
#include "math.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

MFile* MFile::openFiles[];
uint8_t MFile::numOpenFiles=0;
QueueHandle_t MFile::eventQueue;

MFile::~MFile(){
    
    ESP_LOGD(__FUNCTION__,"Destructor");
    if (file && IsOpen()) {
        Close();
    }
    if (numOpenFiles == 0) {
        EventManager::UnRegisterEventHandler(handlerDescriptors);
    }
}

MFile::MFile()
    :ManagedDevice("MFile")
    ,fileStatus(mfile_state_t::MFILE_INIT)
    ,hasContent(false)
    ,file(NULL)
    ,name(NULL)
{
    if (numOpenFiles == 0) {
        memset(openFiles,0,sizeof(void*)*MAX_OPEN_FILES);
    }

    cJSON_AddNumberToObject(status,"status",fileStatus);

    if (handlerDescriptors == NULL)
        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
}

MFile::MFile(char* fileName):MFile()
{
    uint32_t sz = strlen(fileName)+1;
    if (sz > 1){
        name = (char*)dmalloc(sizeof(fileName)+1);
        name[0]=0;
        strcpy(fileName,name);
        cJSON_AddStringToObject(status,"name",fileName);
        ESP_LOGV(__FUNCTION__,"MFile %s",name);
    } 
}

EventHandlerDescriptor* MFile::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"MFile(%s): BuildHandlerDescriptors",name);
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(fileEventIds::WRITE,"WRITE");
  handler->AddEventDescriptor(fileEventIds::OPEN_CREATE,"OPEN_CREATE");
  handler->AddEventDescriptor(fileEventIds::OPEN_APPEND,"OPEN_APPEND");
  handler->AddEventDescriptor(fileEventIds::FLUSH,"FLUSH");
  handler->AddEventDescriptor(fileEventIds::CLOSE,"CLOSE");
  return handler;
}

MFile* MFile::GetFile(char* fileName){
    if ((fileName == NULL) || !strlen(fileName)) {
        return NULL;
    }

    if (numOpenFiles > MAX_OPEN_FILES) {
        ESP_LOGW(__FUNCTION__,"Ran out of files at %s",fileName);
        return NULL;
    }

    for (uint idx=0; idx < numOpenFiles; idx++) {
        MFile* file = openFiles[idx];
        if (strcmp(fileName,file->name) == 0) {
            return file;
        }
    }
    return openFiles[numOpenFiles++]=new MFile(fileName);
}

void MFile::Open(const char* mode){
    file = fopen(name, mode,true);
    if (file == NULL)
    {
        ESP_LOGE(__FUNCTION__, "Failed to open %s for %s", name, mode);
        fileStatus = mfile_state_t::MFILE_FAILED;
        return;
    }
    ESP_LOGV(__FUNCTION__, "Open %s for %s",name, mode);
        struct stat st;

    int ret = 0;

    ret = stat(name, &st);

    if (ret == 0)
    {
        hasContent=st.st_size>0;
    }

    fileStatus = (mfile_state_t)(fileStatus|mfile_state_t::MFILE_OPENED);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_CLOSED);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_CLOSED_PENDING_WRITE);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_INIT);
    fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_FAILED);
    cJSON_SetNumberValue(cJSON_GetObjectItem(status,"status"),fileStatus);
}

void MFile::Close(){
    if (file != NULL)
    {
        ESP_LOGV(__FUNCTION__, "Closed %s",name);
        fClose(file);
        file=NULL;
        fileStatus = (mfile_state_t)(fileStatus|mfile_state_t::MFILE_CLOSED);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_OPENED);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_CLOSED_PENDING_WRITE);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_INIT);
        fileStatus = (mfile_state_t)(fileStatus & ~mfile_state_t::MFILE_FAILED);
        cJSON_SetNumberValue(cJSON_GetObjectItem(status,"status"),fileStatus);
    }
}

bool MFile::IsOpen(){
    return file != NULL && (fileStatus & mfile_state_t::MFILE_OPENED);
}

void MFile::Write(uint8_t* data, uint32_t len){
    bool wasOpened = IsOpen();
    if (!wasOpened) {
        Open("a");
    }
    if (file != NULL)
    {
        fwrite(data,sizeof(uint8_t),len,file);
    }
    if (!wasOpened){
        Close();
    }
}

void MFile::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    ESP_LOGV(__FUNCTION__,"Event %s-%d",base,id);
    if (strcmp(base,"MFile") == 0) {
        MFile* efile;
        cJSON* params=*(cJSON**)event_data;;
        switch (id)
        {
        case fileEventIds::OPEN_CREATE:
            if ((params != NULL) && cJSON_HasObjectItem(params,"name")){
                efile = GetFile(cJSON_GetStringValue(cJSON_GetObjectItem(params,"name")));
                if (efile)
                    efile->Open("c");
            } else {
                ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
            }
            break;
        case fileEventIds::OPEN_APPEND:
            if ((params != NULL) && cJSON_HasObjectItem(params,"name")){
                efile = GetFile(cJSON_GetStringValue(cJSON_GetObjectItem(params,"name")));
                if (efile)
                    efile->Open("a");
            } else {
                ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
            }
            break;
        case fileEventIds::CLOSE:
            if ((params != NULL) && cJSON_HasObjectItem(params,"name")){
                efile = GetFile(cJSON_GetStringValue(cJSON_GetObjectItem(params,"name")));
                if (efile)
                    efile->Close();
            } else {
                ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
            }
            break;
        case fileEventIds::FLUSH:
            if ((params != NULL) && cJSON_HasObjectItem(params,"name")){
                BufferedFile::GetFile(cJSON_GetStringValue(cJSON_GetObjectItem(params,"name")))->Flush();
            } else {
                
            }
            break;
        case fileEventIds::WRITE:
            if ((params != NULL) && cJSON_HasObjectItem(params,"name")){
                efile = GetFile(cJSON_GetStringValue(cJSON_GetObjectItem(params,"name")));
                uint8_t* buf = (uint8_t*)cJSON_GetStringValue(cJSON_GetObjectItem(params,"name"));
                if (efile && buf) {
                    efile->Write(buf,strlen((char*)buf));
                }
            } else {
                ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
            }
            break;
        default:
            break;
        }
    }
}

BufferedFile::BufferedFile(char* fileName)
    :MFile(fileName){
}


void BufferedFile::Flush() {
    if (buf && pos) {
        MFile::Write(buf,pos);
    }
}

void BufferedFile::FlushAll() {
    for (int idx=0; idx < MAX_OPEN_FILES; idx++) {
        if (openFiles[idx]){
            ((BufferedFile*)openFiles[idx])->Flush();
        }
    }
}

BufferedFile* BufferedFile::GetFile(char* fileName){
    if ((fileName == NULL) || !strlen(fileName)) {
        return NULL;
    }

    if (numOpenFiles > MAX_OPEN_FILES) {
        ESP_LOGW(__FUNCTION__,"Ran out of files at %s",fileName);
        return NULL;
    }

    for (uint idx=0; idx < numOpenFiles; idx++) {
        BufferedFile* file = (BufferedFile*)openFiles[idx];
        if (strcmp(fileName,file->name) == 0) {
            return file;
        }
    }
    return (BufferedFile*)(openFiles[numOpenFiles++]=new BufferedFile(fileName));
}

void BufferedFile::WriteLine(uint8_t* data, uint32_t len) {
    Write(data,len);
    Write((uint8_t*)&eol,1);
}

void BufferedFile::Write(uint8_t* data, uint32_t len) {
    if (buf == NULL) {
        buf = (uint8_t*)dmalloc(maxBufSize);
    }
    if (buf != NULL) {
        hasContent=true;
        if ((pos+len)<maxBufSize) {
            memcpy(data,buf+pos,len);
            pos+=len;
        } else {
            uint32_t chunckSize = maxBufSize-pos;
            uint32_t dataPos=0;
            uint32_t remaining=len;
            while (remaining>0){
                memcpy(data+dataPos,buf+pos,chunckSize);
                pos+=chunckSize;
                dataPos+=chunckSize;
                remaining-=chunckSize;
                if (pos == (maxBufSize-1)){
                    Flush();
                }
                chunckSize=remaining>maxBufSize?maxBufSize:remaining;
            }
        }
    }
}

void BufferedFile::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    ESP_LOGV(__FUNCTION__,"Event %s-%d",base,id);
    if (strcmp(base,"MFile") == 0) {
        cJSON* params = *(cJSON**)event_data;
        BufferedFile* efile = (BufferedFile*)GetFile(EventHandlerDescriptor::GetParsedValue(cJSON_GetStringValue(cJSON_GetObjectItem(params,"name"))));
        switch (id)
        {
        case fileEventIds::WRITE:
        case fileEventIds::WRITE_LINE:
            if (efile && params && cJSON_HasObjectItem(params,"value")){
                uint8_t* buf = (uint8_t*)EventHandlerDescriptor::GetParsedValue(cJSON_GetStringValue(cJSON_GetObjectItem(params,"value")));
                if (buf) {
                    if (cJSON_HasObjectItem(params,"header")) {
                        uint8_t* headerLine = (uint8_t*)EventHandlerDescriptor::GetParsedValue(cJSON_GetStringValue(cJSON_GetObjectItem(params,"header")));
                        id==fileEventIds::WRITE_LINE?efile->WriteLine(headerLine,strlen((char*)headerLine)):efile->Write(headerLine,strlen((char*)headerLine));
                        ldfree(headerLine);
                    }
                    id==fileEventIds::WRITE_LINE?efile->WriteLine(buf,strlen((char*)buf)):efile->Write(buf,strlen((char*)buf));
                    ldfree(buf);
                }
            } else {
                ESP_LOGW(__FUNCTION__,"Missing params:%d or file:%d or missing param when both are true",params==NULL,efile==NULL);
            }
            break;
        default:
            MFile::ProcessEvent(handler_args,base,id,event_data);
            break;
        }
    }
}
