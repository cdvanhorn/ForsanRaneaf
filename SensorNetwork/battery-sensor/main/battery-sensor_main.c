#include <esp_log.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include "esp_twai.h"
#include "esp_twai_onchip.h"

#include "network.h"

static const char *LOG_TAG = "can_network"; //!< char pointer - logging group

/**
 * @brief setup can interface and start monitoring a can bus
 * @param pvParameter void pointer will eventually contain any can bus config
 */
void can_task(void *pvParameter) {
    twai_node_handle_t node_hdl = NULL;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = 4,             // TWAI TX GPIO pin
        .io_cfg.rx = 5,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 500000,  // 500 kbps bitrate
        .tx_queue_depth = 5,        // Transmit queue depth set to 5
    };
    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));
    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));
    ESP_LOGI(LOG_TAG, "CAN bus TWAI controller started!");

    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        // TODO: Check if someone says we should shutdown



        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

// graceful_exit: // cleanup
//     vTaskDelete(NULL); // kill the task
}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    struct network_config ncfg;
    ncfg.mode = NETWORK_MODE_CLIENT;
    ncfg.client_id = NETWORK_CLIENT_BATTERY;

    // TODO: Setup queue to communicate between tasks, the can task will need to send messages on the esp-now network
    // the esp-now task will need to tell can task to shutdown
    // can task will need to tell esp-now task to shutdown

    TaskHandle_t networkTaskHandle = NULL;
    xTaskCreate(network_task, "network_task", 8192, &ncfg, 4, &networkTaskHandle);

    TaskHandle_t canTaskHandle = NULL;
    xTaskCreate(can_task, "can_task", 8192, NULL, 4, &canTaskHandle);

    // TODO: infinite loop to monitor tasks

    // TODO: find peek memory usage by esp-now task so can give proper heap size
}
