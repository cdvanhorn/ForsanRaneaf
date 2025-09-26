#ifndef COMP_CAN_NETWORK_H
#define COMP_CAN_NETWORK_H

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include <freertos/queue.h>

#include "esp_twai.h"
#include "esp_twai_onchip.h"

/**
 * @struct can_network_config
 * @brief network configuration
 */
struct can_network_config {
    uint8_t baud; //!< uint8_t baud rate
    QueueHandle_t *can_message_queue; //!<< pointer to queue handle received can messages will be placed in this queue
};

/**
 * @struct can_message
 * @brief can/twai network message
 */
struct can_message {
    twai_frame_t frame; //!< twai frame contains header with data
    uint8_t data[TWAI_FRAME_MAX_LEN]; //!< byte array to hold can data
};

extern void can_network_send_message(uint32_t id, uint8_t *buffer);
extern void can_network_start(void *pvParameter);
extern void can_network_stop();

#endif