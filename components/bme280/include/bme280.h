#pragma once

#include "esp_err.h"


//bme280数据的结构体
typedef struct{
    float temp;         //温度
    float press;        //气压
    float hum;          //湿度
} WeatherData;


//BME280初始化：建I2C总线 → 挂设备 → 发两条配置指令 → 等1秒 → 读校准参数
//原来这一段是摊在 app_main 里的
esp_err_t bme280_init(void);

//读一次传感器并算好温湿度气压
esp_err_t bme280_read_data(float *temp, float *press, float *hum);
