#include "route.h"
#include "mbedtls/md.h"
#include "esp_ota_ops.h"

bool TheRest::GetLocalMD5(char* ccmd5) {
    FILE *fw = NULL;
    bool retVal=false;
    ESP_LOGI(__PRETTY_FUNCTION__, "Reading MD5 RAM:%d...", esp_get_free_heap_size());
    if ((fw = fopenCd("/lfs/firmware/current.bin.md5", "r", true)) != NULL)
    {
        uint32_t len = 0;
        if ((len = fread((void *)ccmd5, 1, 33, fw)) > 0)
        {
            ccmd5[32] = 0;
            ESP_LOGI(__PRETTY_FUNCTION__, "Local MD5:%s", ccmd5);
            retVal=true;
        }
        else
        {
            ESP_LOGE(__PRETTY_FUNCTION__, "Error with weird md5 len %d", len);
        }
        fclose(fw);
    }
    else
    {
        ESP_LOGE(__PRETTY_FUNCTION__, "Failed in opeing md5.");
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
        ESP_LOGI(__PRETTY_FUNCTION__,"Firmware len:%zu", fwLen);
        char *newFw = (char*) dmalloc(fwLen);
        sprintf(url, "http://%s/lfs/firmware/current.bin", gwAddr);
        plen = fwLen;
        SendRequest(url,HTTP_METHOD_GET,&plen,newFw);
        if (newFw && (plen == fwLen))
        {
            uint8_t srvmd[32];
            char ssrvmd[70];

            mbedtls_md_context_t ctx;
            mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

            mbedtls_md_init(&ctx);
            mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
            mbedtls_md_starts(&ctx);
            mbedtls_md_update(&ctx, (uint8_t *)newFw, plen);
            uint8_t shaResult[32];
            mbedtls_md_finish(&ctx, shaResult);
            mbedtls_md_free(&ctx);
            for (uint8_t i = 0; i < sizeof(shaResult); ++i)
            {
                sprintf(&ssrvmd[i * 2], "%02x", (unsigned int)srvmd[i]);
            }

            if (strcmp(srvMd5, ssrvmd) == 0)
            {
                ESP_LOGI(__PRETTY_FUNCTION__, "MD5 matched, writing FW");
                sprintf(url, "/lfs/firmware/current.bin");
                FILE *newFWf = fopen(url, "w");
                if (newFWf)
                {
                    if (fwrite(newFw, sizeof(uint8_t), plen, newFWf) == plen)
                    {
                        fclose(newFWf);
                        sprintf(url, "/lfs/firmware/tobe.bin.md5");
                        FILE *toBeMd5 = fopen(url, "w");
                        if (toBeMd5)
                        {
                            if (fwrite(srvMd5, sizeof(uint8_t), 32, toBeMd5) == 32)
                            {
                                fclose(toBeMd5);
                                ESP_LOGI(__PRETTY_FUNCTION__, "Updated Firmware File");
                                return true;
                            }
                            else
                            {
                                ESP_LOGE(__PRETTY_FUNCTION__, "Error openeing to be md5");
                            }
                            fclose(toBeMd5);
                        }
                        else
                        {
                            ESP_LOGE(__PRETTY_FUNCTION__, "Error openeing to be md5");
                        }
                    }
                    else
                    {
                        ESP_LOGE(__PRETTY_FUNCTION__, "Failed to write firmware");
                    }
                    fclose(newFWf);
                }
            }
            else
            {
                ESP_LOGE(__PRETTY_FUNCTION__, "Missmatch in MD5 reported md5(%zu):%s content MD5(%zu):%s",fwLen, srvMd5, plen, ssrvmd);
            }

            ldfree(newFw);
        } else {
            ESP_LOGE(__PRETTY_FUNCTION__,"%s downloaed len:%zu reported len:%zu",fwFile==NULL?"Malloc error":"Size missmatch", plen, fwLen);
        }
    }
    else
    {
        ESP_LOGE(__PRETTY_FUNCTION__,"weird fw stat");
    }

    if (fwFile)
        cJSON_Delete(fwFile);
    ldfree(url);
    return false;
}

void TheRest::CheckUpgrade(void* param){
    ESP_LOGI(__PRETTY_FUNCTION__,"Checking firmware");
    char localMd5[70];
    char serverMd5[70];
    char* url =(char*)dmalloc(266);
    bool needsUpgrade = false;
    memset(localMd5,0,70);
    memset(serverMd5,0,70);
    size_t len;
    TheRest* restInstance = TheRest::GetServer();

    if (restInstance->GetLocalMD5(localMd5)) {
        len=70;
        sprintf(url,"http://%s/ota/getmd5",restInstance->gwAddr);
        restInstance->SendRequest(url,HTTP_METHOD_POST,&len,serverMd5);
        if (strlen(serverMd5) == strlen(localMd5)) {
            ESP_LOGI(__PRETTY_FUNCTION__,"local:%s server:%s",localMd5,serverMd5);
            needsUpgrade = strcmp(localMd5,serverMd5)!=0;
        } else {
            ESP_LOGW(__PRETTY_FUNCTION__,"Weird MD5s, local:%s server:%s",localMd5,serverMd5);
            needsUpgrade=strlen(serverMd5)==32;
        }
    } else {
        ESP_LOGW(__PRETTY_FUNCTION__,"No local MD5, upgrading");
        needsUpgrade=true;
    }
    if (needsUpgrade) {
        ESP_LOGI(__PRETTY_FUNCTION__,"Needs an upgrade...");
        if (restInstance->DownloadFirmware(serverMd5)){
            const esp_partition_t * part  = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
            if (part != NULL){
                esp_err_t err = esp_ota_set_boot_partition(part);
                if (err == ESP_OK)
                {
                    ESP_LOGI(__PRETTY_FUNCTION__, "esp_ota_set_boot_partition succeeded");
                    esp_system_abort("Firmware Upgrade");
                } else {
                    ESP_LOGE(__PRETTY_FUNCTION__,"Cannot set OTA partition");
                }
            } else {
                ESP_LOGE(__PRETTY_FUNCTION__,"Cannot get factory partition");
            }
        } else {
            ESP_LOGE(__PRETTY_FUNCTION__,"Error downloading firmware");
        }
    } else {
        ESP_LOGI(__PRETTY_FUNCTION__,"No upgrade needed");
    }
    ldfree(url);
}
