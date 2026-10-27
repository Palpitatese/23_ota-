idf_component_register(SRCS "mqtt_app.c"
                       INCLUDE_DIRS "include"
                       REQUIRES led_ctrl mqtt esp_timer ota_mgr config_mgr)
