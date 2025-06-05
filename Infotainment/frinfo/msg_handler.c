/**
 * @file msg_handler.c
 */

#include "msg_handler.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utilities/defines.h"
#include "turbob64.h"
#include "../utilities/logger.h"

/**
 * @brief decode message and perform validation
 * @param msg char pointer to base64 encoded message
 * @param decoded_msg unsigned char pointer to be filled with decoded bytes
 * @return 0 on success 1 on failure
 */
static int validate_msg(char *msg, unsigned char *decoded_msg) {
    size_t len = strlen(msg);
    const size_t ret = tb64dec((unsigned char *)msg, len, decoded_msg);
    if (ret == 0) {
        log_write(LOG_TAG_WARN, "base64 message decode failed");
        return FUNC_FAILURE;
    }
    decoded_msg[ret] = 0; // null terminate message
    if (decoded_msg[0] != ret) {
        log_write(LOG_TAG_WARN, "serial message has incorrect length");
        return FUNC_FAILURE;
    }
    return FUNC_SUCCESS;
}

/**
 * @brief handle status update message which updates vehicle status
 * @param msg unsigned char pointer to message byte array
 */
static void handle_status_update(unsigned char *msg) {
}

/**
 * @brief look at the message type and take proper action
 * @param msg unsigned char pointer to message byte array
 */
static void process_msg(unsigned char *msg) {
    // look at the message type (2nd byte)
    // call appropriate function for the message type
}

/**
 * @brief Go through messages in the incoming queue, validate them, then update vehicle status
 * @param fi pointer to frinfo structure
 */
void msg_handler_handle(struct frinfo *fi) {
    int ret = 0;
    unsigned char decoded_msg[fi->config->serial_buffer_size];
    char *msg = msg_queue_read(fi->incoming_msg_queue);
    while (msg) {
        ret = validate_msg(msg, decoded_msg);
        if (ret == FUNC_SUCCESS) {
            process_msg(decoded_msg);
            // 00001100 10110010
            // 12 178
            // 3250
        }
        free(msg);
        msg = msg_queue_read(fi->incoming_msg_queue);
    }
}
