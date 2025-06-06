/*
 * @file frinfo.h
 */

#ifndef FRINFO_H
#define FRINFO_H

#include "msg_queue.h"
#include <stdbool.h>


/**
 * @struct frinfo_config
 * @brief Configuration settings for frinfo application
 */
struct frinfo_config {
    char *serial_path; //!< path to serial port to read current vehicle status
    int serial_baud_rate; //!< baud rate of serial port for vehicle status connection
    int serial_buffer_size; //!< buffer size for serial port messages
    int window_width; //!< width of window in pixels
    int window_height; //!< height of window in pixels
};

/**
 * @struct vehicle_status
 * @breif vehicle status container
 */
struct vehicle_status {
    uint16_t flags; //!< vehicle status flags bit field
    uint16_t rpm; //!< motor rotations per minute
};

/**
 * @struct frinfo
 * @brief top-level frinfo objects
 */
struct frinfo {
    bool shutdown; //!< should the threads shutdown
    struct frinfo_config *config; //!< pointer to configuration
    bool serial_connected; //!< did we make a serial connection with sensor network controller
    struct msg_queue *incoming_msg_queue; //!< pointer to incoming message queue
    struct msg_queue *outgoing_msg_queue; //!< pointer to outgoing message queue
    struct vehicle_status *vehicle_status; //!< pointer to vehicle status structure
};

extern int frinfo_start(struct frinfo_config *config);

#endif //FRINFO_H
