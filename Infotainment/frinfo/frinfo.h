/*
 * @file frinfo.h
 */

#ifndef FRINFO_H
#define FRINFO_H

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
 * @struct frinfo
 * @brief top-level frinfo objects
 */
struct frinfo {
    bool shutdown; //!< should the threads shutdown
    struct frinfo_config *config; //!< pointer to configuration
};

extern int frinfo_start(struct frinfo_config *config);

#endif //FRINFO_H
