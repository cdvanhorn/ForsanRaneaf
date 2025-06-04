/**
 * @file msg_handler.c
 */

#include "msg_handler.h"

#include "../utilities/defines.h"

/**
 * @brief decode message and perform validation
 * @param msg char pointer to base64 encoded message
 * @return 0 on success 1 on failure
 */
static int validate_msg(char *msg, char *decoded_msg) {
    return FUNC_SUCCESS;
}

/**
 * @brief handle status update message which updates vehicle status
 * @param msg char pointer to plain text message
 * @return 0 on success 1 on failure
 */
static int handle_status_update(char *msg) {
    return FUNC_SUCCESS;
}

/**
 * @brief look at the message type and take proper action
 * @param msg char pointer to plain text message
 * @return 0 on success 1 on failure
 */
static int process_msg(char *msg) {
    return FUNC_SUCCESS;
}

/**
 * @brief Go through messages in the incoming queue, validate them, then update vehicle status
 * @param fi pointer to frinfo structure
 * @return 0 on success 1 on failure
 */
int msg_handler_handle(struct frinfo *fi) {
    int ret = 0;
    char decoded_msg[fi->config->serial_buffer_size];
    char *msg = msg_queue_read(fi->incoming_msg_queue);
    while (msg) {
        ret = validate_msg(msg, decoded_msg);
        if (ret == FUNC_SUCCESS) {
            // valid message update status
        } else {
            // invalid message, flush and move on
        }
    }
    return FUNC_SUCCESS;
}
