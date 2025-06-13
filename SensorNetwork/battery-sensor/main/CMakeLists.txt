idf_component_register(SRCS "battery-sensor_main.c"
                    PRIV_REQUIRES spi_flash esp_netif esp_wifi nvs_flash
                    INCLUDE_DIRS "")
