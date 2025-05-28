/**
 * @file serial_comm.c
 */

#include "serial_comm.h"
#include "../utilities/logger.h"
#include "frinfo.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <errno.h>

#include "../utilities/defines.h"

char *buffer = NULL;
int serial_fd = 0;

/**
 * @brief Open serial port
 * @param cfg pointer to frinfo_config which will define the serial connection
 * @return 0 on failure 1 on success
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
 * @brief Read data from serial connection and place in buffer
 * @return 0 on failure 1 on success
 */

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

Example Message:
DEAD 0000 0100 0000 0001 0000 1100 1011 0010 CRC BEEF
 */
static int read_serial() {
    ssize_t bytes_read = read(serial_fd, buffer, sizeof(buffer));
    if (bytes_read < 0 && errno != EAGAIN) {
        log_write(LOG_TAG_ERR, "could not read from serial port");
        return FUNC_FAILURE;
    }
    if (bytes_read > 0)
        printf("Received data: %.*s\n", (int)bytes_read, buffer);
    return FUNC_SUCCESS;
}

/**
 * @brief main method of serial communication thread
 * @param args void pointer arguments to serial communication thread
 * @return void pointer to result of thread
 */
void *serial_comm_loop(void *args) {
    struct frinfo *frinfo = (struct frinfo *)args;

    buffer = (char *)malloc(frinfo->config->serial_buffer_size * sizeof(char));
    if (buffer == NULL) {
        log_write(LOG_TAG_ERR, "could not allocate buffer");
        goto func_failure;
    }

    int rtv = open_serial(frinfo->config);
    if (rtv == FUNC_FAILURE)
        goto func_failure;

    while (1) {
        if (frinfo->shutdown) {
            log_write(LOG_TAG_INFO, "serial communication received shutdown signal");
            break;
        }

        rtv = read_serial();
        if (rtv == FUNC_FAILURE)
            frinfo->shutdown = true;

    }

    // TODO: check for data to be written to serial port

    /* algorithm:
     * initialize serial connection
     *
     * loop:
     * check if data to send out on serial connection
     * if data to send lock send array
     * send data on serial connection
     * remove data form send array
     * unlock send array
     *
     * check if data to read from serial connection
     * if so read and parse data
     * lock vehicle state
     * update vehicle state
     * unlock vehicle state
     *
     * close serial connection
     */


    if (serial_fd >= 0)
        close(serial_fd);
    free(buffer);
    return NULL;

func_failure:
    if (serial_fd >= 0)
        close(serial_fd);
    if (buffer != NULL)
        free(buffer);
    return NULL;
}
