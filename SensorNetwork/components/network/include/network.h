#ifndef COMP_NETWORK_H
#define COMP_NETWORK_H

#include <stdint.h>

#define NETWORK_CLIENT_MODE             0
#define NETWORK_CONTROLLER_MODE         1

#define NETWORK_NUM_CLIENTS             3
#define NETWORK_COOLANT_CLIENT          0
#define NETWORK_VCU_CLIENT              1
#define NETWORK_BATTERY_CLIENT          2
#define NETWORK_CONTROLLER_CLIENT       14

/**
 * @struct network_config
 * @brief network configuration
 */
struct network_config {
    uint8_t mode; //!< uint8_t network mode client or controller
    uint8_t client_id; //!< uint8_t network client id, coolant, vcu, battery or controller
};

extern void network_task(void *pvParameter);

#endif