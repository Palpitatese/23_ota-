#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "config_mgr.h"
#include <string.h>

/*
1. nvs_flash_init()             初始化NVS（全局只需一次）
2. nvs_open(namespace, ...)     打开命名空间，拿到句柄
3. nvs_get_str / nvs_set_str    读/写
4. nvs_commit()                 提交（set之后必须调）
5. nvs_close()                  关闭命名空间
*/

//宏定义
#define NVS_NAMESPACE "wifi_cfg"
#define KEY_SSID      "ssid"
#define KEY_PASS      "password"

// 默认WiFi配置（NVS里没有时用这个）
#define DEFAULT_SSID  "ay"
#define DEFAULT_PASS  "20061120"

//nvs初始化，读取配置文件
void config_mgr_init(void)
{
    nvs_handle_t handle;//nvs句柄
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);//成功了写进句柄handle
    if (err != ESP_OK) {
        printf("NVS打开失败: %s\n", esp_err_to_name(err));
        return;//失败了就打印错误信息并返回
    }

    char ssid[WIFI_SSID_MAX_LEN] = {0};//参数len传进去时：表示缓冲区大小，返回时：表示实际读到的长度，所以必须先把 len = sizeof(ssid)
    size_t len = sizeof(ssid);
    err = nvs_get_str(handle, KEY_SSID, ssid, &len);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        printf("NVS里没有WiFi配置，写入默认值\n");
        nvs_set_str(handle, KEY_SSID, DEFAULT_SSID);
        nvs_set_str(handle, KEY_PASS, DEFAULT_PASS);
        nvs_commit(handle);//调用生效，否则写入不生效
    } else {
        printf("NVS里已有WiFi配置，SSID=%s\n", ssid);
    }

    nvs_close(handle);//用完关闭，释放资源
}


//获取NVS配置，把NVS里的WiFi账号密码，读出来交给调用者
void config_get_wifi(char *ssid, char *pass)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("NVS打开失败: %s\n", esp_err_to_name(err));
        return;
    }

    size_t len = WIFI_SSID_MAX_LEN;
    err = nvs_get_str(handle, KEY_SSID, ssid, &len);//把键 "ssid" 的值读到 ssid 里，&len 传入缓冲区大小，返回实际长度
    if (err != ESP_OK) {
        printf("读取SSID失败: %s，使用默认值\n", esp_err_to_name(err));
        strncpy(ssid, DEFAULT_SSID, WIFI_SSID_MAX_LEN);
    }

    len = WIFI_PASS_MAX_LEN;
    err = nvs_get_str(handle, KEY_PASS, pass, &len);
    if (err != ESP_OK) {
        printf("读取密码失败: %s，使用默认值\n", esp_err_to_name(err));
        strncpy(pass, DEFAULT_PASS, WIFI_PASS_MAX_LEN);//strncpy 可以指定最大长度，防止溢出
    }

    nvs_close(handle);
}


//设置wifi配置，把调用者传来的账号密码，写进NVS里
void config_set_wifi(const char *ssid, const char *pass)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        printf("NVS打开失败: %s\n", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(handle, KEY_SSID, ssid);
    if (err != ESP_OK) {
        printf("写入SSID失败: %s\n", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_set_str(handle, KEY_PASS, pass);
    if (err != ESP_OK) {
        printf("写入密码失败: %s\n", esp_err_to_name(err));
        nvs_close(handle);
        return;
    }

    err = nvs_commit(handle);//不调这个，前面的 nvs_set_str 只是写在缓存里，掉电就丢,调了之后，才真正写进 Flash
    if (err != ESP_OK) {
        printf("NVS提交失败: %s\n", esp_err_to_name(err));
    } else {
        printf("WiFi配置已保存到NVS\n");
    }

    nvs_close(handle);
}