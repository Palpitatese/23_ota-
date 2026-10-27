#include <stdbool.h>
#include <stdio.h>
#include "wifi_sta.h"
#include "mqtt_app.h"
#include "esp_err.h"
#include "esp_event_base.h"
#include "esp_newlib.h"
#include "esp_private/wifi_types.h"
#include "freertos/FreeRTOS.h"//核心头文件
#include "freertos/projdefs.h"
#include "freertos/event_groups.h"//包含有freeROTS的事件组
#include "nvs_flash.h" //Wi-Fi 需要把配置存到 NVS 里
#include "esp_wifi.h"   //Wi-Fi 驱动
#include "esp_event.h"  //事件循环，用来处理连接成功/断开。
#include "esp_netif.h"  //网络接口，用来获取 IP 地址
#include <string.h>
#include "config_mgr.h"

//定义一个宏
#define WIFI_CONNECTED_BIT BIT0

//添加标志位记录wifi
static bool s_wifi = false;//初始化为false，为未断开


//事件组句柄
static EventGroupHandle_t s_wifi_handle = NULL; //static 修饰后，该变量只能在其所在的.c文件访问


//wifi事件回调函数
//“event_base这一类事件里，具体发生了event_id这个事，附带的数据在event_data。”
//void* arg是自定义参数，这里不传
static void wifi_event_handle(void *arg,esp_event_base_t event_base,
                              int32_t event_id,void *event_data){
        //先处理连接事件                        
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        //判断如果是WIFI类的事件，并且是WiFi启动的这个事情
        printf("WIFI_EVENT_STA_START:开始连接\n");
        esp_wifi_connect();//连接AP
        //因为 esp_wifi_start() 只是把WiFi驱动启动起来
        //启动完成后系统会发一个 WIFI_EVENT_STA_START 事件
        //这时候才知道“WiFi驱动准备好了”，可以开始连接了
        //所以连接动作要放在这个事件回调里。
    }else{                         
        //处理断开重新连接
        if (event_base ==WIFI_EVENT&& event_id == WIFI_EVENT_STA_DISCONNECTED) {
        //判断“如果是WiFi类事件，并且是‘连接断开’这个事件”
        s_wifi = true;//wifi断开，此时置为true，代表wifi断开事件发生了，待处理
        printf("WIFI_EVENT_STA_DISCONNECTED: 断开，重连中\n");
        esp_wifi_connect(); //再连一次
    }else{
        //这里是拿IP的事件
        if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        //判断“如果是IP类事件，并且是‘拿到IP’这个事件” 
            ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
            //把系统传进来的数据，强制转换成“IP事件结构体”指针，这样才就能取里面的IP
            printf("IP_EVENT_STA_GOT_IP: 拿到IP " IPSTR "\n", IP2STR(&event->ip_info.ip));
            //把4字节的IP转成 192.168.xx.xx 这种可读格式，IPSTR 是配套的格式占位符
            xEventGroupSetBits(s_wifi_handle, WIFI_CONNECTED_BIT);
            //这是一个设置事件组的API
            //置位，通过事件组告诉主任务“WiFi连上了，可以往下走了”

            if (s_wifi) {//wifi与mqtt断过，并且mqtt句柄不为空（初始化已启动）
                mqtt_app_reconnect();//原来是直接判断 s_mqtt && s_mqtt_handle != NULL 再 esp_mqtt_client_reconnect
            };
            s_wifi = false;//放在外面，无论是否重新，都清理标志
            }
        }
    }             
}


//wifi的初始化函数
esp_err_t wifi_sta_init(void)
{
    s_wifi_handle = xEventGroupCreate();//创建事件组，返回一个句柄
    //【优化点6】事件组和队列创建失败就直接停住，不要拿着 NULL 句柄往下跑
    if (s_wifi_handle == NULL) {
        printf("事件组创建失败\n");
        return ESP_ERR_NO_MEM;
    }

    //ESP_ERROR_CHECK的意思是：“调用这个函数，如果返回值不是ESP_OK就打印错误信息并 abort()停住。”
    ESP_ERROR_CHECK(esp_netif_init());  //初始化网络接口层
    ESP_ERROR_CHECK(esp_event_loop_create_default());//创建默认事件循环，事件回调要靠它分发
    esp_netif_create_default_wifi_sta();//给WiFi创建一个STA模式的网络接口
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();//准备WiFi驱动的默认配置
                                                        //WIFI_INIT_CONFIG_DEFAULT()是一个宏，展开后是一堆默认参数
                                                        // （缓冲区大小、任务优先级等）
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));//用这份配置初始化WiFi驱动

    //注册wifi类事件的回调
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,//事件类型
                                                        ESP_EVENT_ANY_ID,//订阅关注wifi这一类的事件，包括了连接和断开等
                                                        &wifi_event_handle,//根据关注的信息，调用上面的回调函数，
                                                        NULL, NULL));
    //注册IP类事件的回调                                                    
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,//IP_EVENT_STA_GOT_IP是获取IP这一个事情
                                                        &wifi_event_handle,
                                                        NULL, NULL));
    //配置账号密码
    // 从NVS读取WiFi配置
    char ssid_buf[WIFI_SSID_MAX_LEN] = {0};//初始化为零，防止读到垃圾数据
    char pass_buf[WIFI_PASS_MAX_LEN] = {0};
    config_get_wifi(ssid_buf, pass_buf);//调用写的函数，把NVS里的配置读进这两个buf里

    printf("正在连接WiFi: SSID=%s\n", ssid_buf);

    wifi_config_t wifi_config = {0};
    // wifi_config.sta.ssid是uint8_t数组，需要用strncpy拷贝
    strncpy((char *)wifi_config.sta.ssid, ssid_buf, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, pass_buf, sizeof(wifi_config.sta.password) - 1);



    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));//设置为STA模式

    //把刚才填的账号密码交给WiFi驱动
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  
    ESP_ERROR_CHECK(esp_wifi_start());//启动wifi（只是启动，真正的连接动作在回调函数里）
    //启动成功会发出一个WIFI_EVENT_STA_START事件，回调函数接收到了后才会进行连接动作

    printf("wifi_init_sta 返回了\n");

    return ESP_OK;
}


//等待WiFi连上并拿到IP
void wifi_sta_wait_connected(void){
    //等待事件位portMAX_DELAY无限期等，等不到不会往下走
    xEventGroupWaitBits(s_wifi_handle, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);
}
