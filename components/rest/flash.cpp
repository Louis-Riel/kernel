#include "route.h"

bool TheRest::GetLocalMD5(char* ccmd5) {
    FILE *fw = NULL;
    esp_err_t ret;
    bool retVal=false;
    ESP_LOGD(__FUNCTION__, "Reading MD5 RAM:%d...", esp_get_free_heap_size());
    if ((fw = fOpenCd("/lfs/firmware/current.bin.md5", "r", true)) != NULL)
    {
        uint32_t len = 0;
        if ((len = fRead((void *)ccmd5, 1, 33, fw)) > 0)
        {
            ccmd5[32] = 0;
            ESP_LOGD(__FUNCTION__, "Local MD5:%s", ccmd5);
            retVal=true;
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
    return retVal;
}

bool TheRest::DownloadFirmware(char* srvMd5) {
    char* url = (char*)dmalloc(255);
    sprintf(url, "http://%s/stat/lfs/firmware/current.bin", gwAddr);
    size_t plen=0;

    cJSON *fwFile = PostJSonRequest(url);
    if (fwFile && (cJSON_HasObjectItem(fwFile, "size")))
    {
        size_t fwLen = cJSON_GetNumberValue(cJSON_GetObjectItem(fwFile, "size"));
        ESP_LOGD(__FUNCTION__,"Firmware len:%d", fwLen);
        char *newFw = (char*) dmalloc(fwLen);
        sprintf(url, "http://%s/lfs/firmware/current.bin", gwAddr);
        plen = fwLen;
        SendRequest(url,HTTP_METHOD_GET,&plen,newFw);
        if (newFw && (plen == fwLen))
        {
            MD5Context md5_context;
            uint8_t srvmd[16];
            char ssrvmd[33];
            MD5Init(&md5_context);
            MD5Update(&md5_context, (uint8_t *)newFw, plen);
            MD5Final(srvmd, &md5_context);
            for (uint8_t i = 0; i < 16; ++i)
            {
                sprintf(&ssrvmd[i * 2], "%02x", (unsigned int)srvmd[i]);
            }

            if (strcmp(srvMd5, ssrvmd) == 0)
            {
                ESP_LOGD(__FUNCTION__, "MD5 matched, writing FW");
                sprintf(url, "/lfs/firmware/current.bin");
                FILE *newFWf = fOpen(url, "w");
                if (newFWf)
                {
                    if (fWrite(newFw, sizeof(uint8_t), plen, newFWf) == plen)
                    {
                        fClose(newFWf);
                        sprintf(url, "/lfs/firmware/tobe.bin.md5");
                        FILE *toBeMd5 = fOpen(url, "w");
                        if (toBeMd5)
                        {
                            if (fWrite(srvMd5, sizeof(uint8_t), 32, toBeMd5) == 32)
                            {
                                fClose(toBeMd5);
                                ESP_LOGI(__FUNCTION__, "Updated Firmware File");
                                return true;
                            }
                            else
                            {
                                ESP_LOGE(__FUNCTION__, "Error openeing to be md5");
                            }
                            fClose(toBeMd5);
                        }
                        else
                        {
                            ESP_LOGE(__FUNCTION__, "Error openeing to be md5");
                        }
                    }
                    else
                    {
                        ESP_LOGE(__FUNCTION__, "Failed to write firmware");
                    }
                    fClose(newFWf);
                }
            }
            else
            {
                ESP_LOGE(__FUNCTION__, "Missmatch in MD5 reported md5(%d):%s content MD5(%d):%s",fwLen, srvMd5, plen, ssrvmd);
            }

            ldfree(newFw);
        } else {
            ESP_LOGE(__FUNCTION__,"%s downloaed len:%d reported len:%d",fwFile==NULL?"Malloc error":"Size missmatch", plen, fwLen);
        }
    }
    else
    {
        ESP_LOGE(__FUNCTION__,"weird fw stat");
    }

    if (fwFile)
        cJSON_Delete(fwFile);
    ldfree(url);
    return false;
}

void TheRest::CheckUpgrade(void* param){
    ESP_LOGD(__FUNCTION__,"Checking firmware");
    char localMd5[33];
    char serverMd5[33];
    char* url =(char*)dmalloc(266);
    bool needsUpgrade = false;
    memset(localMd5,0,33);
    memset(serverMd5,0,33);
    size_t len;
    TheRest* theRest = GetServer();
    if (theRest->GetLocalMD5(localMd5)) {
        len=33;
        sprintf(url,"http://%s/ota/getmd5",theRest->gwAddr);
        theRest->SendRequest(url,HTTP_METHOD_POST,&len,serverMd5);
        if (strlen(serverMd5) == strlen(localMd5)) {
            ESP_LOGD(__FUNCTION__,"local:%s server:%s",localMd5,serverMd5);
            needsUpgrade = strcmp(localMd5,serverMd5)!=0;
        } else {
            ESP_LOGW(__FUNCTION__,"Weird MD5s, local:%s server:%s",localMd5,serverMd5);
            needsUpgrade=strlen(serverMd5)==32;
        }
    } else {
        ESP_LOGW(__FUNCTION__,"No local MD5, upgrading");
        needsUpgrade=true;
    }
    if (needsUpgrade) {
        ESP_LOGI(__FUNCTION__,"Needs an upgrade...");
        if (theRest->DownloadFirmware(serverMd5)){
            const esp_partition_t * part  = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
            if (part != NULL){
                esp_err_t err = esp_ota_set_boot_partition(part);
                if (err == ESP_OK)
                {
                    ESP_LOGI(__FUNCTION__, "esp_ota_set_boot_partition succeeded");
                    esp_system_abort("Firmware Upgrade");
                } else {
                    ESP_LOGE(__FUNCTION__,"Cannot set OTA partition");
                }
            } else {
                ESP_LOGE(__FUNCTION__,"Cannot get factory partition");
            }
        } else {
            ESP_LOGE(__FUNCTION__,"Error downloading firmware");
        }
    } else {
        ESP_LOGI(__FUNCTION__,"No upgrade needed");
    }
    ldfree(url);
}
