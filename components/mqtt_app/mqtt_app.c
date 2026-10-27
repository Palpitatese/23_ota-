#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/_intsup.h>
#include "mqtt_app.h"
#include "led_ctrl.h"
#include "esp_err.h"
#include "mqtt_client.h"    //MQTT
#include "config_mgr.h"
#include "ota_mgr.h"


//添加标志位记录mqtt
static bool s_mqtt = false;
//专门记录"MQTT当前是否真的连在Broker上"，和上面 s_mqtt（只表示"启动过"）区分开
static bool s_mqtt_connected = false;

//句柄定义
//MQTT句柄
static esp_mqtt_client_handle_t s_mqtt_handle = NULL;   //mqtt的客户端类型句柄


//mqtt的回调函数
static void mqtt_event_handler(void *arg, esp_event_base_t base, 
                                int32_t event_id, void *event_data){
    //判断事件：如果MQTT连接了Broker                                
    if (event_id == MQTT_EVENT_CONNECTED) {
        printf("MQTT_EVENT_CONNECTED:MQTT连接成功\n");
        s_mqtt_connected = true;//broker真的连上了，允许上报
        esp_mqtt_client_subscribe(s_mqtt_handle, "阿逸/设备指令", 1);//订阅主题（该主题用于设备接收MQTTX的信息）
                                                                                   //Qos等级：0：每条信息最多发一次，可能会丢（适合高频传感器）
                                                                                   //         1：每条至少发一次，不会丢，但可能会重复（适合控制指令）  
                                                                                   //         2 ：每条恰好一次，开销大（计费，关键指令，不能丢，不能重复）     
                                                                                   //等级越高，握手次数越多，越慢，尽量少用等级2  

        printf("订阅完成\n");
        // 在 MQTT_EVENT_CONNECTED 回调里
        esp_mqtt_client_publish(s_mqtt_handle, "阿逸/设备状态", "设备已上线", 0, 1, true);//这里是发字符串，用0函数会自己算
    }else {
        //判断事件：如果MQTT与Broker断开了
        if (event_id ==MQTT_EVENT_DISCONNECTED ) {
            printf("MQTT与Broker断开\n");
            s_mqtt_connected = false;//断开了，禁止上报
            //这里没有重连逻辑原因是esp_mqtt_client_start函数。这个客户端函数自带有重连功能的
        }else {
            //判断如果是信息这个事情
            if (event_id == MQTT_EVENT_DATA) {
                esp_mqtt_event_handle_t event = event_data;//把系统传进来的数据强制转成MQTT事件结构体
                printf("收到指令信息\n");

                /*
                event->topic → 主题内容（比如 cmd/led）
                event->topic_len → 主题长度（比如 7）
                event->data → 消息内容（比如 on）
                event->data_len → 消息长度（比如 2）
                */ 

                //消息的主题和长度
                printf("指令主题：%.*s\n",event->topic_len, event->topic);//%.*s是指按指定长度打印，data和topic没有\0结尾也不会乱读
                //因为MQTT里的data和topic没有\0结尾，%s会一直往后读，越界读垃圾内存，打印乱码，严重的话会崩溃
                
                //信息的内容和长度
                printf("指令内容：%.*s\n", event->data_len, event->data);
                
                //strncmp是比较两个字符串，相等返回0
                cmd_type_t cmd_t;

                //on指令
                if (event->data_len == 2 && strncmp(event->data, "on", 2) == 0) {
                    cmd_t = CMD_LED_ON;
                    //如果信息收到是on，则执行以下指令（这里只有打印）
                    printf("收到 on 指令,正在处理\n");
                    led_ctrl_send_cmd(cmd_t);

                //off指令    
                }else if (event->data_len == 3 && strncmp(event->data, "off", 3) == 0){ //先判断长度，再判断内容
                    cmd_t = CMD_LED_OFF;
                    printf("收到 off 指令，正在处理\n");  
                    led_ctrl_send_cmd(cmd_t);   
                
                //ota指令    
                }else if(event->data_len > 4 && strncmp(event->data, "ota:", 4) == 0){
                // 提取 "ota:" 后面的网址
                char url_buf[256] = {0};
                int url_len = event->data_len - 4;
                
                if (url_len > 0 && url_len < sizeof(url_buf)) {
                    // 跳过前4个字符，把剩下的拷贝到 url_buf
                    memcpy(url_buf, event->data + 4, url_len);
                    printf("收到 OTA 指令，准备下载: %s\n", url_buf);
                    
                    // 调用我们刚刚写的 OTA 启动函数
                    ota_mgr_start_task(url_buf);
                    //调用 ota_mgr_start_task(url_buf)。创建一个新任务去下载，保证 MQTT 回调能返回，不耽误系统接收下一条消息。
                }    
                
                //WiFi配置修改指令    
                } else if (event->data_len >= 8 && strncmp(event->data, "setwifi:", 8) == 0) {
                    char payload[100] = {0};//存放指令内容
                    int copy_len = event->data_len - 8; //计算要复制的长度(跳过前8个字符)
                    if (copy_len > 0 &&copy_len < 100) {
                        memcpy(payload, event->data + 8, copy_len);//把指令内容复制到payload里

                    // 找逗号，分割 ssid 和 pass
                    char *comma = strchr(payload, ',');
                    if (comma != NULL) {
                        *comma = '\0'; // 把逗号变成字符串结束符，前半段就是ssid
                        char *new_ssid = payload;
                        char *new_pass = comma + 1; // 后半段是密码

                        printf("收到修改WiFi指令，SSID: %s, PASS: %s\n", new_ssid, new_pass);
                            
                        // 写入 NVS
                        config_set_wifi(new_ssid, new_pass);
                            
                        printf("新WiFi配置已保存，请重启设备生效\n");
                    }
                    }
                }
            }  
        }
    }
}


//MQTT初始化函数
esp_err_t mqtt_app_init(void){
    esp_mqtt_client_config_t mqtt_cfg = {   //准备配置
        .broker.address.uri = "mqtt://broker.emqx.io:1883",  //broker地址

        //配置设备下线遗嘱
        .session.last_will.topic = "阿逸/设备状态", //主题名（该主题与设备的接收主题不同，用于设备向MQTTX发消息）
        .session.last_will.msg = "设备已离线",      //内容
        .session.last_will.qos = 1,         //Qos等级，关键信息不能丢，这个消息也可以重复，所以用1
        .session.last_will.retain = true,   //true保留消息，即设备在监控订阅之后启动也能收到，防止监控程序后启动遗漏关键信息
                                            //如果为false，是不保留，如果监控程序启动在设备启动之后，则收不到
        .session.keepalive = 5,            //设备断电->Broker等不到心跳包->等够 1.5倍 keepalive 时间（180秒）->判断设备下线—>发布遗嘱 
                                            //这里把时间改为5（秒）                       
    };              

    s_mqtt_handle = esp_mqtt_client_init(&mqtt_cfg);
    s_mqtt = true;  //标志位，记录mqtt已经启动过了
    //用上面的配置创建MQTT客户端，返回MQTT客户端的句柄

    //注册MQTT类事件的回调
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt_handle,
                                                    ESP_EVENT_ANY_ID,
                                                    &mqtt_event_handler,
                                                    NULL));
    esp_mqtt_client_start(s_mqtt_handle);//启动MQTT客户端，在后台连 Broker、收发消息，默认自带有重连
    printf("mqtt_init 返回了\n");

    return ESP_OK;
}


//WiFi恢复后主动重连MQTT
void mqtt_app_reconnect(void){
    if (s_mqtt && s_mqtt_handle != NULL) {//wifi与mqtt断过，并且mqtt句柄不为空（初始化已启动）
        printf("wifi恢复,主动重连mqtt\n");
        esp_mqtt_client_reconnect(s_mqtt_handle);//主动重连mqtt
    }
}


//MQTT当前是否真的连在Broker上
bool mqtt_app_is_connected(void){
    return s_mqtt_connected;
}


//发一条消息
int mqtt_app_publish(const char *topic, const char *payload, int len, int qos, int retain){
    return esp_mqtt_client_publish(s_mqtt_handle, topic, payload, len, qos, retain);
}
