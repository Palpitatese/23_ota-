#include <stdio.h>
#include "esp_err.h"
#include "led_ctrl.h"
#include "wifi_sta.h"
#include "mqtt_app.h"
#include "bme280.h"
#include "sensor_task.h"
#include "nvs_flash.h"
#include "config_mgr.h"
//#include "sd_card.h"

//主函数入口
void app_main(void)
{   
    //printf("\n\n-------------------------------------\n");
    //printf("ota升级成功，现在是v2版本（ota测试用）\n");
    //printf("\n---------------------------------------\n\n");
    
    //创建LED指令队列
    if (led_ctrl_init() != ESP_OK) {
        return;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());  // 把旧NVS分区整个擦干净
        ret = nvs_flash_init();              // 重新初始化
    }
    ESP_ERROR_CHECK(ret);
    config_mgr_init();

    printf(">>> 准备调用wifi_init_sta\n");
    //创建事件组 + 初始化并启动WiFi STA
    if (wifi_sta_init() != ESP_OK) {
        return;
    }
    printf(">>> wifi_init_sta 调用完毕，开始等事件组\n");


    //等待事件位portMAX_DELAY无限期等，等不到不会往下走
    wifi_sta_wait_connected();

    printf(">>> 等到WiFi了,主任务继续\n");

    mqtt_app_init();//MQtt初始化
    //MQTT需要网络，网络需要IP，IP要等事件组置位。所以mqtt_init()必须放在xEventGroupWaitBits之后
    led_ctrl_start();//创建LED任务


    //---------- 项目B并入的流程 ----------
    //建I2C总线 → 挂BME280 → 发两条配置指令 → 等1秒 → 读校准参数
    //（原来这一段全摊在 app_main 里，现在收进 bme280_init）
    if (bme280_init() != ESP_OK) {
        printf("BME280 初始化失败，主任务继续运行\n");
    }

    //建传感器数据队列 + 创建生产者/消费者任务
    //（原来这一段也全摊在 app_main 里，现在收进 sensor_task_start）
    if (sensor_task_start() != ESP_OK) {
        printf("传感器任务创建失败，主任务继续运行\n");
    }  
}