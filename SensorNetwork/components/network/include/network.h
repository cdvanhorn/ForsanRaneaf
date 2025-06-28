#ifndef COMP_NETWORK_H
#define COMP_NETWORK_H

#include <stdint.h>

#define NETWORK_MODE_CLIENT             0
#define NETWORK_MODE_CONTROLLER         1

#define NETWORK_CLIENT_COUNT            3
#define NETWORK_CLIENT_COOLANT          0
#define NETWORK_CLIENT_VCU              1
#define NETWORK_CLIENT_BATTERY          2
#define NETWORK_CLIENT_CONTROLLER       14

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