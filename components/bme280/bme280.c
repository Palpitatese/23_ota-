#include <stdio.h>
#include <stdint.h>
#include "bme280.h"
#include "driver/i2c_master.h"//EDP-IDF的新版I2C主机驱动库
#include "esp_err.h"
#include "freertos/FreeRTOS.h"//核心头文件
#include "freertos/projdefs.h"
#include "freertos/task.h"//RTOS任务管理库
#include "portmacro.h"


//引脚定义
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 6


//模块地址
#define BME280_ADDR  0x76


//句柄定义
static i2c_master_bus_handle_t bus_handle = NULL;//总线的句柄
static i2c_master_dev_handle_t bme_handle = NULL;//BME的句柄


//bme280传感器的校准参数变量
static uint16_t dig_T1, dig_P1;
static int16_t dig_T2, dig_T3, dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
static uint8_t dig_H1;
static int16_t dig_H2, dig_H3, dig_H4, dig_H5;
static int8_t dig_H6;
// 中间计算需要的变量
static int32_t t_fine;


//------------------------------------------------官方计算公式-------------------------------------------------
//温度补偿计算
static float compensate_temperature(int32_t adc_T) {
    int32_t var1, var2;
    var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    return (t_fine * 5 + 128) >> 8;
}
//气压补偿计算
static float compensate_pressure(int32_t adc_P) {
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) + ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0) return 0;
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);
    return (float)p / 256;
}
//湿度补偿计算
static float compensate_humidity(int32_t adc_H) {
    int32_t v_x1_u32r;
    v_x1_u32r = (t_fine - ((int32_t)76800));
    v_x1_u32r = (((((adc_H << 14) - (((int32_t)dig_H4) << 20) - (((int32_t)dig_H5) * v_x1_u32r)) + ((int32_t)16384)) >> 15) * (((((((v_x1_u32r * ((int32_t)dig_H6)) >> 10) * (((v_x1_u32r * ((int32_t)dig_H3)) >> 11) + ((int32_t)32768))) >> 10) + ((int32_t)2097152)) * ((int32_t)dig_H2) + 8192) >> 14));
    v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((int32_t)dig_H1)) >> 4));
    v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
    v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
    return (float)(v_x1_u32r >> 12) / 1024.0;
}


//bme传感器的读取与拼装任务
//校准参数读不到就要让上层知道，
//否则拿着全0的 dig_T1/dig_P1... 算出来的数据是毫无意义的
static esp_err_t bme_task1(void *pvParameters){
    //------------读取温度气压参数（从0x88开始的26个字节）------------------    
    //创建两个变量    
    uint8_t calib1[26] = {0};   //用一个数组接收26字节数据
    uint8_t reg1 = 0x88;        //要读的寄存器地址
    //先写后读
    //参数：设备句柄，发送内容（地址），一次发送的长度（字节数）
    //用来存放读回数据(地址)
    //要读的字节数，超时时间（ms）
    esp_err_t ret1 = i2c_master_transmit_receive(bme_handle,&reg1,1,calib1,26,1000);     //数组名就已经代表一个地址了，无需加&取地址 
    
    if (ret1 == ESP_OK) {
        printf("温度气压参数： "); 
        for (int i =0; i < 26; i++) {
        printf("%02X  ",calib1[i]);
    }
        printf("\n");
        //温度气压参数： 2B  6F  D3  69  32  00  D9  92  AD  D6  D0  0B  CF  1A  36  00  F9  FF  AC  26  0A  D8  BD  10  00  4B  
    }else {
        printf("温度气压参数读取失败:%s\n",esp_err_to_name(ret1));
        //【优化点3】读失败就到此为止，不再往下拼装全0的校准参数
        return ret1;
    }
    

    // --------------------读取湿度参数（从0xE1开始的7个字节） ---------------------
    //创建变量
    uint8_t calib2[7] = {0};
    uint8_t reg2 = 0xE1;

    // 拼装温度、气压校准参数（官方参数）
    dig_T1 = (calib1[1] << 8) | calib1[0];
    dig_T2 = (calib1[3] << 8) | calib1[2];
    dig_T3 = (calib1[5] << 8) | calib1[4];
    dig_P1 = (calib1[7] << 8) | calib1[6];
    dig_P2 = (calib1[9] << 8) | calib1[8];
    dig_P3 = (calib1[11] << 8) | calib1[10];
    dig_P4 = (calib1[13] << 8) | calib1[12];
    dig_P5 = (calib1[15] << 8) | calib1[14];
    dig_P6 = (calib1[17] << 8) | calib1[16];
    dig_P7 = (calib1[19] << 8) | calib1[18];
    dig_P8 = (calib1[21] << 8) | calib1[20];
    dig_P9 = (calib1[23] << 8) | calib1[22];
    dig_H1 = calib1[25];

    esp_err_t ret2 = i2c_master_transmit_receive(bme_handle,&reg2,1,calib2,7,1000);
    if (ret2 == ESP_OK) {
        printf("湿度参数读取成功: ");
        for (int i=0; i<7; i++) {
        printf("%02X  ",calib2[i]);
    }
        printf("\n");
        //湿度参数： 85  01  00  10  22  03  1E
    }else {
        printf("湿度读取失败：%s\n",esp_err_to_name(ret2));
        //【优化点3】湿度校准参数读不到，一样不给它往下走
        return ret2;
    }
    

    // 拼装湿度校准参数（官方参数）
    dig_H2 = (calib2[1] << 8) | calib2[0];
    dig_H3 = calib2[2];
    dig_H4 = (calib2[3] << 4) | (calib2[4] & 0x0F);
    dig_H5 = (calib2[5] << 4) | (calib2[4] >> 4);
    dig_H6 = (int8_t)calib2[6];

    return ESP_OK;
}


//bme的计算与输出任务
//【优化点4】返回值由 void 改成 esp_err_t：I2C 读失败时里面的 raw_data 全是0，
//           算出来的温度气压湿度是假数据，不能让它混进队列和 MQTT
esp_err_t bme280_read_data(float *temp, float *press, float *hum) {
    //开始读取数据
           //定义数组和寄存器的地址
        uint8_t raw_data[8] = {0};      //用来存放读回来的8个字节数据
        //raw_data[0] ~ raw_data[2]：气压数据（3个字节）
        //raw_data[3] ~ raw_data[5]：温度数据（3个字节）
        //raw_data[6] ~ raw_data[7]：湿度数据（2个字节）
        uint8_t reg_data = 0xF7;    //告诉BME280传感器要从这个寄存器开始读

        //先写后读
        esp_err_t ret = i2c_master_transmit_receive(bme_handle,&reg_data,1,raw_data,8,1000);
        //【优化点4】读失败直接返回，不往下算
        if (ret != ESP_OK) {
            return ret;
        }


        //把散的数据用位运算拼成三个整数

        // 温度原始数据（20位）
        int32_t adc_T = (raw_data[3] << 12) | (raw_data[4] << 4) | (raw_data[5] >> 4);
        // 气压原始数据（20位）
        int32_t adc_P = (raw_data[0] << 12) | (raw_data[1] << 4) | (raw_data[2] >> 4);
        // 湿度原始数据（16位）
        int32_t adc_H = (raw_data[6] << 8) | raw_data[7];
   
        // 最后，把算出来的结果，通过指针赋给外部变量：
        *temp = compensate_temperature(adc_T) / 100.0;
        *press = compensate_pressure(adc_P) / 100.0;
        *hum = compensate_humidity(adc_H);

        return ESP_OK;
}


//BME280初始化：建I2C总线 → 挂设备 → 发两条配置指令 → 等1秒 → 读校准参数
esp_err_t bme280_init(void)
{
    //创建总线配置的结构体
    i2c_master_bus_config_t bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,    // 默认时钟源
        .i2c_port = I2C_NUM_0,                // 使用I2C0 端口
        .scl_io_num = I2C_SCL_PIN,            // SCL引脚（6）
        .sda_io_num = I2C_SDA_PIN,            // SDA引脚（项目A里5被LED占了，这里改成8）
        .glitch_ignore_cnt = 7,               // 滤波，默认是7
        .flags.enable_internal_pullup = true, // 开启内部上拉
    };

    //bme传感器设备的配置结构体
    i2c_device_config_t dev_bme = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7, // 7位地址（大多数设备的地址是7位）
        .device_address = BME280_ADDR,         // 0x76（bme传感器的设备地址）
        .scl_speed_hz = 100000,                // 100kHz（一个比较稳的频率）
    };


    //创建总线    
    //这个函数返回值类型是esp_err_t
    esp_err_t bus_ret = i2c_new_master_bus(&bus_config,&bus_handle);//参数是两个指针（总线配置的结构体；用来接收创建成功的总线句柄）
       if (bus_ret == ESP_OK) {
            printf("总线创建成功\n");
        }else {
            //如果直接打印ret，只能看到数字，例如0x101
            //打印esp_err_to_name(ret)则会返回对应的字符串
            printf("总线创建失败:%s\n",esp_err_to_name(bus_ret));
            //【优化点6】总线都没起来，后面的传感器指令和两个任务全是白跑，直接结束
            return bus_ret;
        }
       
    //将设备bme280挂载在总线上
    esp_err_t bme_ret = i2c_master_bus_add_device(bus_handle,&dev_bme,&bme_handle);
        if (bme_ret == ESP_OK) {
            printf("BME传感器挂载成功\n");
        }else {
            printf("BME传感器挂载失败%s\n",esp_err_to_name(bme_ret));
            //【优化点6】设备没挂上，一样没必要往下走
            return bme_ret;
        }


    //定义两个数组
    uint8_t cmd_hum[2] = {0xF2, 0x01};  //湿度控制
    uint8_t cmd_meas[2] = {0xF4,0x27};  //温度气压控制

    //只发（主机向从机发送数据），相当于发指令(启动测量任务)
    //【优化点6】这两条配置指令原来没看返回值，配置没成功的话后面读到的都是空
    esp_err_t cfg_ret = i2c_master_transmit(bme_handle,cmd_hum,2,200);
    if (cfg_ret != ESP_OK) {
        printf("湿度控制寄存器配置失败:%s\n",esp_err_to_name(cfg_ret));
        return cfg_ret;
    }
    cfg_ret = i2c_master_transmit(bme_handle,cmd_meas,2,200);
    if (cfg_ret != ESP_OK) {
        printf("温压控制寄存器配置失败:%s\n",esp_err_to_name(cfg_ret));
        return cfg_ret;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    //【优化点3】bme_task1 现在有返回值了，校准读不到就别建任务
    esp_err_t calib_ret = bme_task1(NULL);   // 读取校准参数
    if (calib_ret != ESP_OK) {
        printf("BME280 校准参数读取失败:%s，不启动采集任务\n",esp_err_to_name(calib_ret));
        return calib_ret;
    }

    return ESP_OK;
}
