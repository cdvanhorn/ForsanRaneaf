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

///////////////////////////////////////////////////////////////////////////////
// DEFINES
///////////////////////////////////////////////////////////////////////////////
#define CAN_MESSAGE_QUEUE_SIZE              200

///////////////////////////////////////////////////////////////////////////////
// COMPONENT DATA TYPES
///////////////////////////////////////////////////////////////////////////////
struct can_message {
    twai_frame_t frame;
    uint8_t data[TWAI_FRAME_MAX_LEN];
};

///////////////////////////////////////////////////////////////////////////////
// COMPONENT VARIABLES
///////////////////////////////////////////////////////////////////////////////
static const char *LOG_TAG = "can_network"; //!< char pointer - logging group
static QueueHandle_t can_message_queue = NULL; //!< QueueHandle_t esp_now queue that will hole can messages to be processed

///////////////////////////////////////////////////////////////////////////////
// CAN/TWAI CALLBACKS
///////////////////////////////////////////////////////////////////////////////
static IRAM_ATTR bool twai_sender_tx_done_callback(twai_node_handle_t handle, const twai_tx_done_event_data_t *edata, void *user_ctx)
{
    if (!edata->is_tx_success) {
        ESP_EARLY_LOGW(LOG_TAG, "Failed to transmit message, ID: 0x%X", edata->done_tx_frame->header.id);
    }
    return false; // No task wake required
}

// Bus error callback
static IRAM_ATTR bool twai_sender_on_error_callback(twai_node_handle_t handle, const twai_error_event_data_t *edata, void *user_ctx)
{
    ESP_EARLY_LOGW(LOG_TAG, "TWAI node error: 0x%x", edata->err_flags.val);
    return false; // No task wake required
}

static bool IRAM_ATTR twai_listener_rx_callback(twai_node_handle_t handle, const twai_rx_done_event_data_t *edata, void *user_ctx)
{
    QueueHandle_t *queue = (QueueHandle_t *)user_ctx;

    struct can_message msg;
    msg.frame.buffer = msg.data;
    msg.frame.buffer_len = sizeof(msg.data);
    if (twai_node_receive_from_isr(handle, &msg.frame) == ESP_OK) {
        xQueueSendFromISR(*queue, &msg, NULL);
    }
    return false;
}

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

    twai_node_handle_t node_hdl = NULL;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = 4,             // TWAI TX GPIO pin
        .io_cfg.rx = 5,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 500000,  // 500 kbps bitrate
        .tx_queue_depth = 5,        // Transmit queue depth set to 5
    };
    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));
    twai_event_callbacks_t callbacks = {
        .on_rx_done = twai_listener_rx_callback,
        .on_tx_done = twai_sender_tx_done_callback,
        .on_error = twai_sender_on_error_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &callbacks,  &can_message_queue));
    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));
    ESP_LOGI(LOG_TAG, "CAN bus TWAI controller started!");

    // let's see if we can send a can message
    uint8_t send_buff[8];
    send_buff[0] = 0x11;
    twai_frame_t tx_msg = {
        .header.id = 0x213,           // Message ID
        .header.ide = false,         // Use 29-bit extended ID format
        .buffer = send_buff,        // Pointer to data to transmit
        .buffer_len = sizeof(send_buff),  // Length of data to transmit
    };
    ESP_ERROR_CHECK(twai_node_transmit(node_hdl, &tx_msg, 0));
    ESP_LOGI(LOG_TAG, "Sending config request message");

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
            // ESP_LOGI(LOG_TAG, "Received message");
        }

        vTaskDelay(50 / portTICK_PERIOD_MS);
    }

graceful_exit: // cleanup
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
