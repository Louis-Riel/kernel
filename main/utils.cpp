#include "utils.h"

sdmmc_host_t host = SDSPI_HOST_DEFAULT();
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = true,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024};
sdmmc_card_t *card = NULL;
const char mount_point[] = "/sdcard";

sdmmc_host_t* getSDHost(){
  return &host;
}

const char* getErrorMsg(FRESULT errCode){
  switch (errCode)
  {
    case FR_OK: return "(0) Succeeded"; break;
    case FR_DISK_ERR: return "(1) A hard error occurred in the low level disk I/O layer"; break;
    case FR_INT_ERR: return "(2) Assertion failed"; break;
    case FR_NOT_READY: return "(3) The physical drive cannot work"; break;
    case FR_NO_FILE: return "(4) Could not find the file"; break;
    case FR_NO_PATH: return "(5) Could not find the path"; break;
    case FR_INVALID_NAME: return "(6) The path name format is invalid"; break;
    case FR_DENIED: return "(7) Access denied due to prohibited access or directory full"; break;
    case FR_EXIST: return "(8) Access denied due to prohibited access"; break;
    case FR_INVALID_OBJECT: return "(9) The file/directory object is invalid"; break;
    case FR_WRITE_PROTECTED: return "(10) The physical drive is write protected"; break;
    case FR_INVALID_DRIVE: return "(11) The logical drive number is invalid"; break;
    case FR_NOT_ENABLED: return "(12) The volume has no work area"; break;
    case FR_NO_FILESYSTEM: return "(13) There is no valid FAT volume"; break;
    case FR_MKFS_ABORTED: return "(14) The f_mkfs() aborted due to any problem"; break;
    case FR_TIMEOUT: return "(15) Could not get a grant to access the volume within defined period"; break;
    case FR_LOCKED: return "(16) The operation is rejected according to the file sharing policy"; break;
    case FR_NOT_ENOUGH_CORE: return "(17) LFN working buffer could not be allocated"; break;
    case FR_TOO_MANY_OPEN_FILES: return "(18) Number of open files > FF_FS_LOCK"; break;
    case FR_INVALID_PARAMETER: return "(19) Given parameter is invalid"; break;
    default:
      break;
  }
  return "Invalid error code";
}

bool initSDMMCSDCard(){
  ESP_LOGI(__FUNCTION__, "Using SDMMC peripheral");
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
  slot_config.gpio_cd=SDMMC_SLOT_NO_CD;

  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};
  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(__FUNCTION__, "Failed to mount filesystem. "
                             "If you want the card to be formatted, set format_if_mount_failed = true.");
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "Failed to initialize the card (%s). "
                             "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return false;
  }

  ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
  if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
    sdmmc_card_print_info(stdout, card);

  f_mkdir("/converted");
  f_mkdir("/kml");
  f_mkdir("/sent");

  return true;
}

bool initSPISDCard()
{
  ESP_LOGD(__FUNCTION__, "Using SPI peripheral");
  esp_err_t ret=0;

  spi_bus_config_t bus_cfg = {
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = PIN_NUM_MISO,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4000,
      .flags = 0,
      .intr_flags = 0
  };

  //if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
  spi_bus_initialize((spi_host_device_t)host.slot, &bus_cfg, 1);
  //}

  sdspi_device_config_t device_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    device_config.gpio_cs = PIN_NUM_CS;
    device_config.host_id = (spi_host_device_t)host.slot;


  ret=esp_vfs_fat_sdspi_mount(mount_point, &host, &device_config, &mount_config, &card);

  if (ret != ESP_OK)
  {
    if (ret == ESP_FAIL)
    {
      ESP_LOGE(__FUNCTION__, "Failed to mount filesystem. "
                             "If you want the card to be formatted, set format_if_mount_failed = true.");
    }
    else
    {
      ESP_LOGE(__FUNCTION__, "Failed to initialize the card (%s). "
                             "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return false;
  }

  ESP_LOGD(__FUNCTION__, "SD card mounted %d", (int)card);
  if (LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG)
    sdmmc_card_print_info(stdout, card);

  f_mkdir("/converted");
  f_mkdir("/kml");
  f_mkdir("/sent");

  return true;
}

bool moveFile(char* src, char* dest){
  FRESULT res;
  FILE* srcF = fopen(src,"r");
  if (srcF != 0) {
    FILE* destF = fopen(dest,"w");
    if (destF != NULL) {
      int ch=0;
      while ((ch=fgetc(srcF))!=EOF){
        fputc(ch,destF);
      }
      fclose(destF);
      fclose(srcF);
      if ((res=f_unlink(&src[8]))==0){
        ESP_LOGD(__FUNCTION__,"moved %s to %s",src,dest);
        return true;
      } else {
        ESP_LOGE(__FUNCTION__,"failed in deleting %s %s",src,getErrorMsg(res));
      }
    } else {
      ESP_LOGE(__FUNCTION__,"Cannot open dest %s",dest);
    }
  } else {
    ESP_LOGE(__FUNCTION__,"Cannot open source %s",src);
  }
  return false;
}
