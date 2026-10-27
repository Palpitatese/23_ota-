#ifndef CONFIG_MGR_H
#define CONFIG_MGR_H


//宏定义
#define WIFI_SSID_MAX_LEN 33    //wifi ssid最大长度（32个字符+1个结束符）
#define WIFI_PASS_MAX_LEN 65    //wifi 密码最大长度（64个字符+1个结束符）

//声明函数
void config_mgr_init(void); //init函数，初始化NVS，读取配置文件，如果没有配置文件就创建一个默认配置文件
void config_get_wifi(char *ssid, char *pass);   //获取wifi配置
void config_set_wifi(const char *ssid, const char *pass);//设置wifi配置
    //指针参数是和环形缓冲区的 read 一样——要把读到的值“写回”调用者的缓冲区。
    //const → 函数内部不改这两个字符串

    
#endif