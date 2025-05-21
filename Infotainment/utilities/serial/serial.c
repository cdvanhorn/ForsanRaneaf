/**
 * @file serial.c
 */

#include "../serial.h"
#include "../defines.h"

/**
 * @brief Open serial connection defined by passed in arguments
 * @param path serial port path (e.g. /dev/ttyUSB)
 * @param baud_rate baud rate of serial connection
 * @param buffer_size how big message buffer should be
 * @return 0 on success 1 on failure
 */
int serial_open(const char *path, int baud_rate, int buffer_size) {
    return FUNC_SUCCESS;
}

/**
 * @breif read a message off the serial connection and place data in given pointer
 * @param data pointer to char pointer that the function will populate with the last serial message will be NULL
 *      if a message is not ready
 */
void serial_read(char **data) {

}

/**
 * @breif close serial connection
 */
void serial_close() {

}
