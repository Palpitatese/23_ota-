#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ota_mgr.h"
#include "esp_log.h"
#include "esp_https_ota.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "OTA_MGR";

//这是一个运行在后台的独立任务，专门负责下载固件
//静态函数（static），外部调用不到，专门由后面的启动函数唤醒
static void ota_update_task(void *pvParameter)
{
    // 在这里接收传进来的 URL
    char *url = (char *)pvParameter;

    ESP_LOGW(TAG, "开始 OTA 升级，固件地址: %s", url);

    // 配置 HTTP 客户端
        esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 10000,
    };

    // 新增一个结构体，把 HTTP 配置装进去
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    // 调用官方封装好的 OTA 核心函数
     esp_err_t ret = esp_https_ota(&ota_config);//自动发起 HTTP GET 请求，一边接收数据，一边把数据写进备用分区（ota_1），同时还会自动做 CRC 校验

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA 升级成功！系统即将重启...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // 延迟1秒，让串口把这句话打印完
        esp_restart();                   // 重启设备
    } else {
        ESP_LOGE(TAG, "OTA 升级失败: %s", esp_err_to_name(ret));
    }
    
    // 任务结束前，释放 url 占用的内存，否则会内存泄漏
    free(url);
    
    // 任务执行完毕后，删除自己
    vTaskDelete(NULL);
}



//ota启动函数
void ota_mgr_start_task(const char *url)
{
    // 把传入的 URL 拷贝到堆内存（Heap）中。
    // 为什么不能直接传指针？因为外面（MQTT回调）的变量在函数结束后会被销毁。
    // 这里申请了 strlen(url) + 1 个字节的空间（+1 是留给 '\0' 结尾）。
    char *url_copy = malloc(strlen(url) + 1);
    if (url_copy != NULL) {
        strcpy(url_copy, url);
        // 创建一个独立任务去后台下载
        // 栈大小给了 8192，因为 OTA 涉及 TLS/HTTP 解析，极其吃栈。优先级设为 5。
        xTaskCreate(ota_update_task, "ota_task", 8192, (void *)url_copy, 5, NULL);
    } else {
        ESP_LOGE(TAG, "内存不足，无法启动 OTA 任务");
    }
}