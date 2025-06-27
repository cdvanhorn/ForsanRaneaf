
idf_component_register(SRCS "network.c"
        PRIV_REQUIRES spi_flash esp_netif esp_wifi
        INCLUDE_DIRS "include")