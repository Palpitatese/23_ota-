#pragma once

#include "esp_err.h"

//枚举
typedef enum{   //typedef->起别名
    CMD_LED_OFF = 0,
    CMD_LED_ON = 1,
}cmd_type_t;


//创建LED指令队列
esp_err_t led_ctrl_init(void);

//往LED指令队列里塞一条指令
void led_ctrl_send_cmd(cmd_type_t cmd);

//创建LED任务
void led_ctrl_start(void);
