#include "mfile.h"
#include "math.h"

#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG

MFile* MFile::openFiles[];
uint8_t MFile::numOpenFiles=0;
QueueHandle_t MFile::eventQueue;

MFile::~MFile(){
    ESP_LOGV(__FUNCTION__,"Destructor %s",name);
    if (file && IsOpen()) {
        Close();
    }
}

MFile::MFile()
    :ManagedDevice("MFile","MFile",BuildStatus)
    ,fileStatus(mfile_state_t::MFILE_INIT)
    ,hasContent(false)
    ,name(NULL)
    ,file(NULL)
{
    ESP_LOGV(__FUNCTION__,"Building MFile");
    if (numOpenFiles == 0) {
        memset(openFiles,0,sizeof(void*)*MAX_OPEN_FILES);
    }

    if (handlerDescriptors == NULL){
        EventManager::RegisterEventHandler((handlerDescriptors=BuildHandlerDescriptors()));
    }
}

MFile::MFile(char* fileName):MFile()
{
    uint32_t sz = strlen(fileName)+1;
    ESP_LOGV(__FUNCTION__,"Opening file %s(%d)",fileName, sz);
    if (sz > 1){
        name = (char*)dmalloc(sz+1);
        name[0]=0;
        strcpy(name, fileName);
        ESP_LOGV(__FUNCTION__,"file %s(%d)...",name, sz);
    } 
    status = BuildStatus(this);
}

EventHandlerDescriptor* MFile::BuildHandlerDescriptors(){
  ESP_LOGV(__FUNCTION__,"MFile: BuildHandlerDescriptors");
  EventHandlerDescriptor* handler = ManagedDevice::BuildHandlerDescriptors();
  handler->AddEventDescriptor(fileEventIds::WRITE_LINE,"WRITE_LINE");
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
            ESP_LOGV(__FUNCTION__,"Pulling %s from open files as %d, %d open files",file->name,idx, numOpenFiles);
            return file;
        }
    }
    ESP_LOGV(__FUNCTION__,"Opening %s into open files as %d",fileName,numOpenFiles);
    return openFiles[numOpenFiles++]=new MFile(fileName);
}

cJSON* MFile::BuildStatus(void* instance){
    MFile* theFile = (MFile*)instance;

    cJSON* sjson = NULL;
    AppConfig* apin = new AppConfig(sjson=ManagedDevice::BuildStatus(instance),AppConfig::GetAppStatus());
    if (theFile->name)
        apin->SetStringProperty("name",theFile->name);
    apin->SetIntProperty("status",theFile->fileStatus);
    apin->SetBoolProperty("open",theFile->IsOpen());
    apin->SetBoolProperty("hasContent",theFile->hasContent);
    delete apin;
    return sjson;
}

void MFile::Open(const char* mode){
    if ((name == NULL) || (strlen(name) == 0)) {
        ESP_LOGE(__FUNCTION__,"Empty name error");
        fileStatus = mfile_state_t::MFILE_FAILED;
        return;
    }

    file = fOpenCd(name, mode,true);
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
    }
}

bool MFile::IsOpen(){
    return file != NULL && (fileStatus & mfile_state_t::MFILE_OPENED);
}

void MFile::Write(uint8_t* data, uint32_t len) {
    bool wasOpened = IsOpen();
    if (!wasOpened) {
        Open("a");
    }
    if (file != NULL)
    {
        ESP_LOGV(__FUNCTION__,"Writing %d in %s",len,data);
        fWrite(data,sizeof(uint8_t),len,file);
    }
    if (!wasOpened){
        Close();
    }
}

void MFile::ProcessEvent(void *handler_args, esp_event_base_t base, int32_t id, void *event_data){
    ESP_LOGV(__FUNCTION__,"Event %s-%d",base,id);
    MFile* efile;
    AppConfig* params = new AppConfig(*(cJSON**)event_data,NULL);
    //char* name = EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("name"));
    if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
        char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(NULL));
        ESP_LOGW(__FUNCTION__,"Params:%s...",tmp);
        ldfree(tmp);
    }

    switch (id)
    {
    case fileEventIds::OPEN_CREATE:
        if ((params != NULL) && params->HasProperty("name")){
            efile = GetFile(params->GetStringProperty("name"));
            if (efile)
                efile->Open("c");
        } else {
            ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
        }
        break;
    case fileEventIds::OPEN_APPEND:
        if ((params != NULL) && params->HasProperty("name")){
            efile = GetFile(params->GetStringProperty("name"));
            if (efile)
                efile->Open("a");
        } else {
            ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
        }
        break;
    case fileEventIds::CLOSE:
        if ((params != NULL) && params->HasProperty("name")){
            ESP_LOGV(__FUNCTION__,"Closing");
            efile = GetFile(params->GetStringProperty("name"));
            if (efile)
                efile->Close();
        } else {
            ESP_LOGW(__FUNCTION__,"Missing params:%d",params!=NULL);
        }
        break;
    case fileEventIds::FLUSH:
        if ((params != NULL) && params->HasProperty("name")){
            BufferedFile::GetFile(params->GetStringProperty("name"))->Flush();
        } else {
            
        }
        break;
    case fileEventIds::WRITE:
        if ((params != NULL) && params->HasProperty("name")){
            efile = GetFile(params->GetStringProperty("name"));
            uint8_t* buf = (uint8_t*)params->GetStringProperty("name");
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
    free(params);
}

esp_event_base_t MFile::GetEventBase(){
    return MFile::eventBase;    
}

BufferedFile::BufferedFile()
    :MFile(){
    ESP_ERROR_CHECK(esp_event_handler_instance_register(MFile::GetEventBase(), ESP_EVENT_ANY_ID, ProcessEvent, this, NULL));
}

BufferedFile::BufferedFile(char* fileName)
    :MFile(fileName)
    ,isNewOrEmpty(false) {
    struct stat st;
    int ret = 0;
    ret = stat(fileName, &st);
    isNewOrEmpty = (ret != 0) || (st.st_size==0);
    if (ret == ESP_OK) {
        ESP_LOGD(__FUNCTION__,"Opening file %s is new or empty:%d, size:%li",fileName, isNewOrEmpty, st.st_size);
    } else {
        ESP_LOGD(__FUNCTION__,"Creating file %s is new or empty:%d",fileName, isNewOrEmpty);
    }
}


void BufferedFile::Flush() {
    if (buf && pos) {
        ESP_LOGV(__FUNCTION__,"Flushing");
        MFile::Write(buf,pos);
        pos=0;
    }
}

void BufferedFile::FlushAll() {
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

BufferedFile* BufferedFile::GetOpenedFile(char* fileName){
    if ((fileName == NULL) || !strlen(fileName)) {
        return NULL;
    }

    if (numOpenFiles > MAX_OPEN_FILES) {
        ESP_LOGW(__FUNCTION__,"Ran out of files at %s",fileName);
        return NULL;
    }

    for (uint idx=0; idx < numOpenFiles; idx++) {
        BufferedFile* file = (BufferedFile*)openFiles[idx];
        if (strcmp(file->name,fileName) == 0) {
            return file;
        }
    }
    ESP_LOGV(__FUNCTION__,"No open file %s",fileName);
    return NULL;
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
        if (strcmp(file->name,fileName) == 0) {
            return file;
        }
    }
    ESP_LOGV(__FUNCTION__,"Opening file %s",fileName);
    return (BufferedFile*)(openFiles[numOpenFiles++]=new BufferedFile(fileName));
}

void BufferedFile::WriteLine(uint8_t* data, uint32_t len) {
    Write(data,len);
    Write((uint8_t*)&eol,1);
}

void BufferedFile::Write(uint8_t* data, uint32_t len) {
    ESP_LOGV(__FUNCTION__,"Writing (%s)%d to %s",data,len,name);
    if (buf == NULL) {
        buf = (uint8_t*)dmalloc(maxBufSize);

    }
    if (buf != NULL) {
        hasContent=true;
        if ((pos+len)<maxBufSize) {
            memcpy(buf+pos,data,len);
            pos+=len;
        } else {
            uint32_t chunckSize = maxBufSize-pos;
            uint32_t dataPos=0;
            uint32_t remaining=len;
            while (remaining>0){
                memcpy(buf+pos,data+dataPos,chunckSize);
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
    if (strcmp(base,"MFile") == 0) {
        ESP_LOGV(__FUNCTION__,"Event %s-%d",base,id);
        AppConfig* params = new AppConfig(*(cJSON**)event_data,NULL);
        char* name = EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("name"));
        if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
            char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(NULL));
            ESP_LOGW(__FUNCTION__,"Params:%s...",tmp);
            ldfree(tmp);
        }
        ESP_LOGV(__FUNCTION__,"NAME:%s...",name);
        BufferedFile* efile = (BufferedFile*)GetFile(name);
         if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
            char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(NULL));
            ESP_LOGW(__FUNCTION__,"Params2:%s...",tmp);
            ldfree(tmp);
        }
        if (!efile) {
            ESP_LOGE(__FUNCTION__,"No files available, no go");
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
                uint8_t* strbuf = (uint8_t*)EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("value"));
                if (params->HasProperty("header") && efile->isNewOrEmpty) {
                    uint8_t* headerLine = (uint8_t*)EventHandlerDescriptor::GetParsedValue(params->GetStringProperty("header"));
                    if (headerLine) {
                        ESP_LOGV(__FUNCTION__,"header:%s",headerLine);
                        id==fileEventIds::WRITE_LINE?efile->WriteLine(headerLine,strlen((char*)headerLine)):efile->Write(headerLine,strlen((char*)headerLine));
                        ldfree(headerLine);
                    } else {
                        ESP_LOGW(__FUNCTION__,"Invalid of empty header line");
                        if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
                            char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(NULL));
                            ESP_LOGW(__FUNCTION__,"Params:%s.",tmp);
                            ldfree(tmp);
                        }
                    }
                }
                ESP_LOGV(__FUNCTION__,"value:%s",strbuf);
                if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
                    char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(NULL));
                    ESP_LOGW(__FUNCTION__,"Params3:%s...",tmp);
                    ldfree(tmp);
                }
                id==fileEventIds::WRITE_LINE?efile->WriteLine(strbuf,strlen((char*)strbuf)):efile->Write(strbuf,strlen((char*)strbuf));
                if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
                    char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(NULL));
                    ESP_LOGW(__FUNCTION__,"Params4:%s...",tmp);
                    ldfree(tmp);
                }
                ldfree(strbuf);
            } else {
                ESP_LOGW(__FUNCTION__,"Missing params:%d or file:%d name:%d or missing param when both are true.",params==NULL,efile==NULL, name==NULL);
                if (name != NULL) {
                    ESP_LOGV(__FUNCTION__,"Name:%s",name);
                }
            }
            break;
        default:
            MFile::ProcessEvent(handler_args,base,id,event_data);
            break;
        }
        if (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE){
            char* tmp = cJSON_PrintUnformatted(params->GetJSONConfig(NULL));
            ESP_LOGW(__FUNCTION__,"Params5:%s...",tmp);
            ldfree(tmp);
        }
        free(name);
        free(params);
    }
}

void BufferedFile::Close(){
    Flush();
    MFile::Close();
    if (buf)
        ldfree(buf);
}