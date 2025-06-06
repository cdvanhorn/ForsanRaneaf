/**
 * @file msg_handler.c
 */

#include "msg_handler.h"
#include "../utilities/defines.h"
#include "../utilities/logger.h"
#include "turbob64.h"

#include <stdlib.h>
#include <string.h>

#define MSG_TYPE_VEHICLE_STATUS 1

struct msg_header {
    uint8_t msg_size;
    uint8_t msg_type;
};

/**
 * @brief decode message and perform validation
 * @param msg char pointer to base64 encoded message
 * @param decoded_msg unsigned char pointer to be filled with decoded bytes
 * @return 0 on success 1 on failure
 */
static int validate_msg(char *msg, unsigned char *decoded_msg) {
    const size_t len = strlen(msg);
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
 * @param msg const unsigned char pointer to message byte array
 * @param fi const pointer to frinfo structure
 */
static void handle_status_update(const unsigned char *msg, const struct frinfo *fi) {
    memcpy(fi->vehicle_status, msg + sizeof(struct msg_header), sizeof(struct vehicle_status));
}

/**
 * @brief look at the message type and take proper action
 * @param msg unsigned char pointer to message byte array
 * @param fi const pointer to frinfo structure
 */
static void process_msg(unsigned char *msg, const struct frinfo *fi) {
    struct msg_header *header = (struct msg_header *)msg;
    if (header->msg_type == MSG_TYPE_VEHICLE_STATUS) {
        handle_status_update(msg, fi);
    }
}

/**
 * @brief Go through messages in the incoming queue, validate them, then update vehicle status
 * @param fi const pointer to frinfo structure
 */
void msg_handler_handle(const struct frinfo *fi) {
    int ret = 0;
    unsigned char decoded_msg[fi->config->serial_buffer_size];
    char *msg = msg_queue_read(fi->incoming_msg_queue);
    while (msg) {
        ret = validate_msg(msg, decoded_msg);
        if (ret == FUNC_SUCCESS) {
            process_msg(decoded_msg, fi);
        }
        free(msg);
        msg = msg_queue_read(fi->incoming_msg_queue);
    }
}
