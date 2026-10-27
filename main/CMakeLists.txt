idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES led_ctrl wifi_sta mqtt_app bme280 sensor_task esp_driver_gpio esp_driver_i2c json esp_wifi nvs_flash esp_event esp_netif mqtt esp_timer)
