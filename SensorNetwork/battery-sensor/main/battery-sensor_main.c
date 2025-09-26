#include <esp_log.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "nvs_flash.h"

#include "can_network.h"
#include "network.h"

///////////////////////////////////////////////////////////////////////////////
// DEFINES
///////////////////////////////////////////////////////////////////////////////
static const char *LOG_TAG = "main"; //!< char pointer - logging group
#define CAN_MESSAGE_QUEUE_SIZE              200

///////////////////////////////////////////////////////////////////////////////
// COMPONENT VARIABLES
///////////////////////////////////////////////////////////////////////////////
static QueueHandle_t can_message_queue = NULL; //!< QueueHandle_t esp_now queue that will hole can messages to be processed

/**
 * @brief setup can interface and start monitoring a can bus
 * @param pvParameter void pointer will eventually contain any can bus config
 */
void can_task(void *pvParameter) {
    can_message_queue = xQueueCreate(CAN_MESSAGE_QUEUE_SIZE, sizeof(struct can_message));
    if (can_message_queue == NULL) {
        ESP_LOGE(LOG_TAG, "Create queue fail");
        goto graceful_exit;
    }

    struct can_network_config network_config;
    network_config.can_message_queue = &can_message_queue;
    can_network_start(&network_config);
    // ESP_LOGI(LOG_TAG, "Sending config request message");

    // create a queue to hold CAN messages
    // create call back on receive CAN message place it on the queue
    // check queue for un-processed messages and process them
    // keep internal data structure representing battery status
    // Every second send requests to get required MCU data
    // place battery update message on queue for ESPNOW task
    struct can_message msg;
    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        while (xQueueReceive(can_message_queue, &msg, 0) == pdTRUE) {
            ESP_LOGI(LOG_TAG, "RX: %x [%d] %x %x %x %x %x %x %x %x",
                     msg.frame.header.id,
                     msg.frame.header.dlc,
                     msg.frame.buffer[0],
                     msg.frame.buffer[1],
                     msg.frame.buffer[2],
                     msg.frame.buffer[3],
                     msg.frame.buffer[4],
                     msg.frame.buffer[5],
                     msg.frame.buffer[6],
                     msg.frame.buffer[7]);
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

graceful_exit: // cleanup
    can_network_stop();
    if (can_message_queue != NULL) {
        vQueueDelete(can_message_queue);
        can_message_queue = NULL;
    }
    vTaskDelete(NULL); // kill the task
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
}
