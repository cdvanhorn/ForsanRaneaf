/**
 * @file serial_comm.c
 */

#include "serial_comm.h"
#include "../utilities/logger.h"
#include "frinfo.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "../utilities/defines.h"

#define DELIM_LENGTH 4

static struct frinfo *frinfo;

static char *buffer = NULL;
static int serial_fd = 0;

static char *current_message = NULL;
static int current_message_index = 0;
static int plus_count = 0;
static int minus_count = 0;

/**
 * @brief Open serial port
 * @param cfg pointer to frinfo_config which will define the serial connection
 * @return 1 on failure 0 on success
 */
static int open_serial(struct frinfo_config *cfg) {
    // Open the serial port
    serial_fd = open(cfg->serial_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial_fd < 0) {
        log_write(LOG_TAG_ERR, "could not open serial port");
        return FUNC_FAILURE;
    }

    struct termios tty;
    // Get the current serial port settings
    if (tcgetattr(serial_fd, &tty) != 0) {
        log_write(LOG_TAG_ERR, "could not get serial port attributes");
    	goto func_failure;
    }

    // Configure the serial port
    tty.c_cflag = CS8 | CLOCAL | CREAD; // data bits, and enable receiver
    if (cfg->serial_baud_rate == 115200) {
        tty.c_cflag |= B115200;
    } else if (cfg->serial_baud_rate == 9600) {
        tty.c_cflag |= B9600;
    } else {
        log_write(LOG_TAG_ERR, "unknown baud rate");
        goto func_failure;
    }
    tty.c_iflag = IGNPAR; // Ignore parity errors
    tty.c_oflag = 0; // Raw output
    tty.c_lflag = 0; // Non-canonical mode

    // Apply the new settings
    if (tcsetattr(serial_fd, TCSANOW, &tty) != 0) {
    	log_write(LOG_TAG_ERR, "could not set serial port attributes");
    	goto func_failure;
    }

    return FUNC_SUCCESS;
func_failure:
    close(serial_fd);
    return FUNC_FAILURE;
}

/**
 * @brief create a new message while parsing serial input
 * @return 1 on failure 0 on success
 */
static int new_message() {
    if (current_message != NULL) {
        log_write(LOG_TAG_WARN, "invalid serial message previous message didn't close");
        free(current_message);
        current_message = NULL;
    }
    current_message_index = 0;
    current_message = (char *)malloc(frinfo->config->serial_buffer_size * sizeof(char));
    if (current_message == NULL)
        return FUNC_FAILURE;
    return FUNC_SUCCESS;
}

/**
 * @brief place message on incoming message buffer
 */
static void close_message() {
    if (current_message == NULL)
        log_write(LOG_TAG_WARN, "invalid serial message closed before opened");
    else {
        msg_queue_write(frinfo->incoming_msg_queue, current_message);
        current_message = NULL;
    }
}

/**
 * @brief parse serial input buffer placing complete messages on incoming message buffer
 * @param bytes size_t number of bytes in buffer ready to be parsed
 * @return 1 on failure 0 on success
 */
static int parse_buffer(const size_t bytes) {
    for (int i = 0; i < bytes; i++) {
        // printf("%c\n", buffer[i]);
        if (buffer[i] == '+') {
            plus_count++;
            if (plus_count == DELIM_LENGTH) {
                if (new_message() == FUNC_FAILURE)
                    return FUNC_FAILURE;
                plus_count = 0;
            }
        } else if (buffer[i] == '-') {
            minus_count++;
            if (minus_count == DELIM_LENGTH ) {
                close_message();
                minus_count = 0;
            }
        } else {
            plus_count = 0;
            minus_count = 0;
            current_message[current_message_index] = buffer[i];
            current_message_index++;
        }
    }
    return FUNC_SUCCESS;
}

/* Message Format
I'm not going to implement a CRC check at this time.
Message Opening Marker
    ++++ in ASCII

Message Closing Marker
    ---- in ASCII

Between the opening and closing marker is a base64 encoded message
1 byte message length in bytes including this length byte
1 byte message type
2 byte status bit field (message type 1)
2 byte RPM (message type 1)
*/

/**
 * @brief Read data from serial connection and place in buffer
 * @return 1 on failure 0 on success
 */
static int read_serial() {
    const ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
    if (bytes_read < 0 && errno != EAGAIN) {
        log_write(LOG_TAG_ERR, "could not read from serial port");
        return FUNC_FAILURE;
    }
    if (bytes_read > 0) {
        if (parse_buffer(bytes_read) == FUNC_FAILURE) {
            log_write(LOG_TAG_ERR, "could not parse serial message");
            return FUNC_FAILURE;
        }
    }
    return FUNC_SUCCESS;
}

/**
 * @brief Clean up any allocated memory or resources used in this file
 */
static void cleanup() {
    if (serial_fd >= 0)
        close(serial_fd);
    if (buffer != NULL) {
        free(buffer);
        buffer = NULL;
    }
    if (current_message != NULL) {
        free(current_message);
        current_message = NULL;
    }
}

/**
 * @brief check for messages to write on serial connection
 * @return 1 on failure 0 on success
 */
static int write_serial() {
    const char *message = msg_queue_read(frinfo->outgoing_msg_queue);
    if (message != NULL) {
        log_write(LOG_TAG_INFO, "writing message to serial connection");
        msg_queue_flush(frinfo->outgoing_msg_queue);
    }
    return FUNC_SUCCESS;
}

/**
 * @brief main method of serial communication thread
 * @param args void pointer arguments to serial communication thread
 * @return void pointer to result of thread
 */
// ReSharper disable once CppDFAConstantFunctionResult
void *serial_comm_loop(void *args) {
    frinfo = (struct frinfo *)args;

    buffer = (char *)malloc(frinfo->config->serial_buffer_size * sizeof(char));
    if (buffer == NULL) {
        log_write(LOG_TAG_ERR, "could not allocate buffer");
        goto func_failure;
    }

    int rtv = open_serial(frinfo->config);
    if (rtv == FUNC_FAILURE) {
        frinfo->serial_connected = false;
        goto func_failure;
    }
    frinfo->serial_connected = true;

    while (1) {
        if (frinfo->shutdown) {
            log_write(LOG_TAG_INFO, "serial communication received shutdown signal");
            break;
        }

        if (write_serial() == FUNC_FAILURE)
            break;

        if (read_serial() == FUNC_FAILURE)
            break;
    }

    cleanup();
    return NULL;

func_failure:
    cleanup();
    return NULL;
}
