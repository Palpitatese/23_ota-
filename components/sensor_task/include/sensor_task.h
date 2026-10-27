#pragma once

#include "esp_err.h"


//启动BME280的两个任务：建数据队列 → 建生产者任务(Task2) → 建消费者任务(Task3)
//原来这一段是摊在 app_main 里的
esp_err_t sensor_task_start(void);
