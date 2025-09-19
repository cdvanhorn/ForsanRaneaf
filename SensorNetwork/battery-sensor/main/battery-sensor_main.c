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
#define POLL_DEPTH              200

typedef struct {
    twai_frame_t frame;
    uint8_t data[TWAI_FRAME_MAX_LEN];
} twai_listener_data_t;

typedef struct {
    twai_node_handle_t node_hdl;
    twai_listener_data_t *rx_pool;
    SemaphoreHandle_t free_pool_semaphore;
    SemaphoreHandle_t rx_result_semaphore;
    int write_idx;
    int read_idx;
} twai_listener_ctx_t;

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
    BaseType_t woken;
    twai_listener_ctx_t *ctx = (twai_listener_ctx_t *)user_ctx;

    if (xSemaphoreTakeFromISR(ctx->free_pool_semaphore, &woken) != pdTRUE) {
        ESP_EARLY_LOGI(LOG_TAG, "Pool full, dropping frame");
        return (woken == pdTRUE);
    }
    if (twai_node_receive_from_isr(handle, &ctx->rx_pool[ctx->write_idx].frame) == ESP_OK) {
        ctx->write_idx = (ctx->write_idx + 1) % POLL_DEPTH;
        xSemaphoreGiveFromISR(ctx->rx_result_semaphore, &woken);
    }
    return (woken == pdTRUE);
}

/**
 * @brief setup can interface and start monitoring a can bus
 * @param pvParameter void pointer will eventually contain any can bus config
 */
void can_task(void *pvParameter) {
    // Create semaphore for receive notification
    twai_listener_ctx_t twai_listener_ctx = {0};
    twai_listener_ctx.free_pool_semaphore = xSemaphoreCreateCounting(POLL_DEPTH, POLL_DEPTH);
    twai_listener_ctx.rx_result_semaphore = xSemaphoreCreateCounting(POLL_DEPTH, 0);
    assert(twai_listener_ctx.free_pool_semaphore != NULL);
    assert(twai_listener_ctx.rx_result_semaphore != NULL);

    twai_listener_ctx.rx_pool = calloc(POLL_DEPTH, sizeof(twai_listener_data_t));
    assert(twai_listener_ctx.rx_pool != NULL);
    for (int i = 0; i < POLL_DEPTH; i++) {
        twai_listener_ctx.rx_pool[i].frame.buffer = twai_listener_ctx.rx_pool[i].data;
        twai_listener_ctx.rx_pool[i].frame.buffer_len = sizeof(twai_listener_ctx.rx_pool[i].data);
    }
    ESP_LOGI(LOG_TAG, "Buffer initialized: %d slots for burst data", POLL_DEPTH);

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
    ESP_ERROR_CHECK(twai_node_register_event_callbacks(node_hdl, &callbacks,  &twai_listener_ctx));
    // Start the TWAI controller
    ESP_ERROR_CHECK(twai_node_enable(node_hdl));
    ESP_LOGI(LOG_TAG, "CAN bus TWAI controller started!");

    // let's see if we can send a can message
    // uint8_t send_buff[8];
    //send_buff[0] = send_buff[1] = send_buff[2] = send_buff[3] = send_buff[4] = send_buff[5] = send_buff[6] = send_buff[7] = 0x00;
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

    // while (1) {
    //     // TODO: Check if someone says we should shutdown
    //
    //
    //
    //     vTaskDelay(250 / portTICK_PERIOD_MS);
    // }
    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        if (xSemaphoreTake(twai_listener_ctx.rx_result_semaphore, portMAX_DELAY) == pdTRUE) {
            twai_frame_t *frame = &twai_listener_ctx.rx_pool[twai_listener_ctx.read_idx].frame;
            ESP_LOGI(LOG_TAG, "RX: %x [%d] %x %x %x %x %x %x %x %x", \
                     frame->header.id, frame->header.dlc, frame->buffer[0], frame->buffer[1], frame->buffer[2], frame->buffer[3], frame->buffer[4], frame->buffer[5], frame->buffer[6], frame->buffer[7]);
            twai_listener_ctx.read_idx = (twai_listener_ctx.read_idx + 1) % POLL_DEPTH;
            xSemaphoreGive(twai_listener_ctx.free_pool_semaphore);
        }
    }

    // Cleanup
    vSemaphoreDelete(twai_listener_ctx.rx_result_semaphore);
    vSemaphoreDelete(twai_listener_ctx.free_pool_semaphore);
    free(twai_listener_ctx.rx_pool);
    ESP_ERROR_CHECK(twai_node_disable(twai_listener_ctx.node_hdl));
    ESP_ERROR_CHECK(twai_node_delete(twai_listener_ctx.node_hdl));

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
}
