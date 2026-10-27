#include <stdbool.h>
#include <stdio.h>
#include "led_ctrl.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"//核心头文件
#include "freertos/projdefs.h"
#include "freertos/task.h"//RTOS任务管理库
#include "freertos/queue.h"//rtos队列
#include "portmacro.h"


//引脚定义
#define LED1 5


//句柄定义
//队列句柄（项目A的LED指令队列）
static QueueHandle_t Queuehandle = NULL;
//任务句柄（项目A的LED指令任务）
static TaskHandle_t Task1handle = NULL;


//任务1创建
static void Task1_cmd(void *pvParameters){
    //配置LED1
    cmd_type_t cmd;
    //去掉volatile
    //volatile 挡不住竞争（其实本来也没有竞争），只会让编译器不敢把它放寄存器
    bool zt = false; 
    
    gpio_reset_pin(LED1);//复位引脚
    gpio_set_direction(LED1,GPIO_MODE_OUTPUT);//输出模式

    while (1) {
        xQueueReceive(Queuehandle,&cmd,portMAX_DELAY);//无限期等待，数据剪切至变量cmd
        if (cmd ==CMD_LED_ON && zt == false) {
            gpio_set_level(LED1, 1);
            printf("收到on指令,已开灯\n");
            zt = true;
        }else if (cmd ==CMD_LED_OFF && zt == true) {
            gpio_set_level(LED1,0);
            printf("收到off指令,已关灯\n");
            zt = false;
        }else  {
            printf("指令与状态重复，此次指令无效\n");    
        }
    }
}


//创建LED指令队列
esp_err_t led_ctrl_init(void){
    Queuehandle = xQueueCreate(4, sizeof(cmd_type_t));//创建队列，放入4个cmd_type_t；类型数据（枚举）
    //事件组和队列创建失败就直接停住，不要拿着 NULL 句柄往下跑
    if (Queuehandle == NULL) {
        printf("LED指令队列创建失败\n");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}


//往LED指令队列里塞一条指令
void led_ctrl_send_cmd(cmd_type_t cmd){
    xQueueSend(Queuehandle,&cmd,0);
}


//创建LED任务
void led_ctrl_start(void){
    xTaskCreate(Task1_cmd, "Task1", 2048, NULL, 4, &Task1handle);
}
