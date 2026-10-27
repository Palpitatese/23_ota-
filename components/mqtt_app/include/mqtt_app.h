#pragma once    //告诉预处理器：这个文件在一次编译里只展开一次，第二次遇到直接跳过

#include <stdbool.h>
#include "esp_err.h"


//MQTT初始化函数
esp_err_t mqtt_app_init(void);

//WiFi恢复后主动重连MQTT
//（原来是 wifi_event_handle 里 "s_wifi && s_mqtt && s_mqtt_handle != NULL" 那一段，
//  其中 s_mqtt 和 s_mqtt_handle 是MQTT模块内部变量，所以判断搬到这里面来了）
void mqtt_app_reconnect(void);

//MQTT当前是否真的连在Broker上（传感器任务上报前用它判断）
bool mqtt_app_is_connected(void);

//发一条消息（原来传感器任务是直接调用 esp_mqtt_client_publish 的）
int mqtt_app_publish(const char *topic, const char *payload, int len, int qos, int retain);
