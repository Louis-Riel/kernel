#include "utils.h"

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
