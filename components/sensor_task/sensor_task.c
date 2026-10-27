#include <stdio.h>
#include "sensor_task.h"
#include "bme280.h"
#include "mqtt_app.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"//核心头文件
#include "freertos/projdefs.h"
#include "freertos/task.h"//RTOS任务管理库
#include "freertos/queue.h"//rtos队列
#include "cJSON.h"//cJSO
#include "portmacro.h"


//句柄定义
//队列句柄（项目B的BME280数据队列）
static QueueHandle_t s_bme_queue_handle = NULL;
//任务句柄（项目B的生产者/消费者）
static TaskHandle_t s_bme_task1_handle = NULL;
static TaskHandle_t s_bme_task2_handle = NULL;


//--------------------------------------------任务创建-------------------------------
//传感器bme的生产任务task1
static void bme280_task1(void *pvParameters){
    WeatherData data;
    while (1) {
        //调用bme的计算与输出函数，把数据读经data里
        //【优化点4】读失败就跳过本轮，2秒后再来，别把假数据发出去
        esp_err_t read_ret = bme280_read_data(&data.temp, &data.press, &data.hum);
        if (read_ret != ESP_OK) {
            printf("BME280 数据读取失败:%s，本轮不上报\n",esp_err_to_name(read_ret));
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }
        //把data数据发给队列（portMAX_DELAY无限期等待）
        xQueueSend(s_bme_queue_handle,&data,portMAX_DELAY);

    //先把浮点数格式化，保留 1 位小数
    char temp_str[10], hum_str[10], press_str[10];
    snprintf(temp_str, sizeof(temp_str), "%.1f", data.temp);
    snprintf(hum_str, sizeof(hum_str), "%.1f", data.hum);
    snprintf(press_str, sizeof(press_str), "%.1f", data.press);

    //用 cJSON 创建对象
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "温度 ", temp_str);
    cJSON_AddStringToObject(root, "湿度 ", hum_str);
    cJSON_AddStringToObject(root, "气压 ", press_str);

    //打包成字符串
    char *payload = cJSON_PrintUnformatted(root);

    //通过 MQTT 发布出去
    //先看 MQTT 到底连上没有
    //把客户端 start 起来就立刻返回，TCP 还没连上，所以上线后第一条"阿逸/天气"必然丢失
    //publish 的返回值是 msg_id，小于0表示这次提交失败
    if (mqtt_app_is_connected()) {
        int msg_id = mqtt_app_publish("阿逸/天气", payload, 0, 1, 0);//订阅主题为“/阿逸天气”查看
        if (msg_id < 0) {
            printf("上报失败\n");
        }
    }else {
        printf("MQTT未连接，本轮不上报\n");
    }


    //释放内存（非常重要！）：cJSON_CreateObject()和 cJSON_PrintUnformatted()都会在堆上动态分配内存。
    //如果不free和Delete，每发一次消息就泄漏一次内存，ESP32就会因为内存耗尽而崩溃。
    cJSON_free(payload);
    cJSON_Delete(root);

    vTaskDelay(pdMS_TO_TICKS(2000));


    }
}

//传感器bme的消费者任务task2
static void bme_task2(void *pvParameters){
    WeatherData data;
    while (1) {
        //接收数据,无限期等待
        xQueueReceive(s_bme_queue_handle,&data,portMAX_DELAY);
        //拿到数据开始打印
        printf("温度：%.2f  气压：%.2f 湿度：%.2f\n",data.temp,data.press,data.hum);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


//启动BME280的两个任务
esp_err_t sensor_task_start(void){
    //创建队列
    s_bme_queue_handle = xQueueCreate(1,sizeof(WeatherData));//长度为一，放一个结构体类型
    //【优化点6】队列创建失败，生产者会在 xQueueSend 里对着 NULL 句柄无限期卡住
    if (s_bme_queue_handle == NULL) {
        printf("BME280数据队列创建失败\n");
        return ESP_ERR_NO_MEM;
    }

    xTaskCreate(bme280_task1,"Task2",4096,NULL,4,&s_bme_task1_handle);
    xTaskCreate(bme_task2,"Task3",4096,NULL,3,&s_bme_task2_handle);

    return ESP_OK;
}
