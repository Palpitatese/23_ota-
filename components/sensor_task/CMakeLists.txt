idf_component_register(SRCS "sensor_task.c"
                       INCLUDE_DIRS "include"
                       REQUIRES bme280 mqtt_app json)
