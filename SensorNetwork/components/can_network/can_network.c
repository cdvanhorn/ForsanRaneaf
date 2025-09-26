#include "can_network.h"

#include <esp_log.h>

///////////////////////////////////////////////////////////////////////////////
// COMPONENT VARIABLES
///////////////////////////////////////////////////////////////////////////////
static const char *LOG_TAG = "can_network"; //!< char pointer - logging group
static twai_node_handle_t node_hdl = NULL;

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
 * @brief Send a message on the can bus
 * @param id unsigned 32-bit integer - id of can message
 * @param buffer byte buffer data to send in can message
 */
void send_message(uint32_t id, uint8_t *buffer) {
    // let's see if we can send a can message
    // uint8_t send_buff[8];
    // send_buff[0] = 0x11;
    twai_frame_t tx_msg = {
        //.header.id = 0x213,           // Message ID
        .header.id = id,
        .header.ide = false,         // Use 29-bit extended ID format
        .buffer = buffer,        // Pointer to data to transmit
        .buffer_len = sizeof(buffer),  // Length of data to transmit
    };
    twai_node_transmit(node_hdl, &tx_msg, 0);
}

/**
 * @brief confiture and start can network
 * @param pvParameter void pointer should point to a can_network_config struct to configure the can connection
 */
void can_network_start(void *pvParameter) {
    struct can_network_config *config = (struct can_network_config *)pvParameter;
    twai_onchip_node_config_t node_config = {
        .io_cfg.tx = 4,             // TWAI TX GPIO pin
        .io_cfg.rx = 5,             // TWAI RX GPIO pin
        .bit_timing.bitrate = 500000,  // 500 kbps bitrate
        .tx_queue_depth = 5,        // Transmit queue depth set to 5
    };
    // Create a new TWAI controller driver instance
    ESP_ERROR_CHECK(twai_new_node_onchip(&node_config, &node_hdl));
    twai_event_callbacks_t const callbacks = {
        .on_rx_done = twai_listener_rx_callback,
        .on_tx_done = twai_sender_tx_done_callback,
        .on_error = twai_sender_on_error_callback,
    };
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &callbacks,  config->can_message_queue));
    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));
    ESP_LOGI(LOG_TAG, "CAN bus TWAI controller started!");
}

/**
 * @brief clean up can network connection
 */
void can_network_stop() {

}