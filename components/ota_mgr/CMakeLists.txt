idf_component_register(SRCS "ota_mgr.c"
                       INCLUDE_DIRS "."
                       REQUIRES app_update esp_https_ota esp_http_client)