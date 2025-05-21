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
};

/**
 * @struct frinfo
 * @brief top-level frinfo objects
 */
struct frinfo {
    bool shutdown; //!< should the threads shutdown
};

extern int frinfo_start(const struct frinfo_config *config);
extern int frinfo_stop();

#endif //FRINFO_H
