#pragma once

#include "esp_err.h"


//wifi的初始化函数
//里面会创建事件组、初始化NVS/网络接口/事件循环、启动WiFi STA
esp_err_t wifi_sta_init(void);

//等待WiFi连上并拿到IP（无限期等）
void wifi_sta_wait_connected(void);
