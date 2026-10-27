#ifndef OTA_MGR_H
#define OTA_MGR_H

#include "esp_err.h"

// 声明启动 OTA 升级任务函数
// url 参数为固件的下载地址，例如 "http://192.168.1.100:8000/firmware.bin"
void ota_mgr_start_task(const char *url);

#endif