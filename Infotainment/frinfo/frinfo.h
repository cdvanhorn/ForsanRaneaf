/*
 * @file frinfo.h
 */

#ifndef FRINFO_H
#define FRINFO_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_INCOMING_MESSAGES 256
#define MAX_OUTGOING_MESSAGES 256

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
 * @struct frinfo
 * @brief top-level frinfo objects
 */
struct frinfo {
    bool shutdown; //!< should the threads shutdown
    struct frinfo_config *config; //!< pointer to configuration
    bool serial_connected; //!< did we make a serial connection with sensor network controller
    char *incoming_messages[MAX_INCOMING_MESSAGES]; //!< array of char pointers each is an incoming message
    uint16_t incoming_message_write_cursor; //!< index where writing to incoming message array
    uint16_t incoming_message_read_cursor; //!< index where reading from incoming message array
    char *outgoing_messages[MAX_OUTGOING_MESSAGES]; //!< array of char pointers each is an outgoing message to be sent on serial connection
    uint16_t outgoing_message_write_cursor; //!< index where writing to outgoing message array
    uint16_t outgoing_message_read_cursor; //!< index where reading from outgoing message array
};

//TODO: add lock for incoming message parts of frinfo
//TODO: add lock for outgoing message parts of frinfo

extern int frinfo_start(struct frinfo_config *config);

#endif //FRINFO_H
