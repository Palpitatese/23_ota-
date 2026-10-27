idf_component_register(SRCS "wifi_sta.c"
                       INCLUDE_DIRS "include"
                       REQUIRES mqtt_app esp_wifi nvs_flash esp_event esp_netif esp_timer config_mgr)
