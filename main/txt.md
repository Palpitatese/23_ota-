2026-10-27  OTA空中升级

学习了什么
OTA原理：基于A/B双分区机制，Bootloader根据otadata标志位跳转执行新固件
新版ESP-IDF的esp_https_ota库用法，以及结构体嵌套配置
测试用Python快速搭建局域网HTTP服务器分发固件

做了什么
修改menuconfig，切分16MB Flash，配置双OTA分区表
新建ota_mgr组件，封装OTA后台下载任务
修改MQTT回调，解析"ota:http://..."指令，创建独立任务传参
修复NVS分区变动引发的崩溃，加上擦除重试逻辑
开启menuconfig允许HTTP OTA，解决安全拦截报错
成功通过MQTTX下发指令，设备自动从本地服务器下载固件，重启并运行V2版本


改完双OTA分区表编译报错，提示需要3.1MB但配置只有2MB
原因：flash大小没同步更新
解决：在menuconfig中把Flash size改回16MB，重新编译

修改分区表后设备不断重启，报ESP_ERR_NVS_NO_FREE_PAGES
原因：NVS分区位置变了，旧数据变成乱码，无法初始化
解决：在nvs_flash_init外面套一层判断，失败就擦除重试

OTA下载报错No option for server verification is enabled
原因：ESP-IDF，拦截了纯HTTP的固件下载
解决：在menuconfig的ESP HTTPS OTA中勾选Allow HTTP for OTA
