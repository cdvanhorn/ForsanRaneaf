/**
 * @file frinfo.c
 */

#include "frinfo.h"

#include <stdlib.h>

#include "../utilities/defines.h"
#include "../utilities/logger.h"
#include "../utilities/threading.h"
#include "serial_comm.h"
#include "ui.h"

/**
 * @brief initialize given frinfo structure
 * @param fi pointer to struct frinfo that will be initialized
 * @param config pointer to struct frinfo_config config to save to frinfo struct
 * @return 0 on success 1 on failure
 */
static int frinfo_open(struct frinfo *fi, struct frinfo_config *config) {
    fi->shutdown = false;
    fi->serial_connected = false;
    fi->config = config;

    fi->incoming_msg_queue = NULL;
    fi->incoming_msg_queue = (struct msg_queue *)malloc(sizeof(struct msg_queue));
    if (fi->incoming_msg_queue == NULL) {
        log_write(LOG_TAG_ERR, "couldn't allocate memory for incoming message queue");
        return FUNC_FAILURE;
    }
    int rtv = msg_queue_open(fi->incoming_msg_queue);
    if (rtv == FUNC_FAILURE)
        return FUNC_FAILURE;

    fi->outgoing_msg_queue = NULL;
    fi->outgoing_msg_queue = (struct msg_queue *)malloc(sizeof(struct msg_queue));
    if (fi->outgoing_msg_queue == NULL) {
        log_write(LOG_TAG_ERR, "couldn't allocate memory for outgoing message queue");
        return FUNC_FAILURE;
    }
    rtv = msg_queue_open(fi->outgoing_msg_queue);
    if (rtv == FUNC_FAILURE) {
        msg_queue_close(fi->incoming_msg_queue);
        return FUNC_FAILURE;
    }

    fi->vehicle_status = NULL;
    fi->vehicle_status = (struct vehicle_status *)malloc(sizeof(struct vehicle_status));
    if (fi->vehicle_status == NULL) {
        log_write(LOG_TAG_ERR, "couldn't allocate memory for vehicle status");
        msg_queue_close(fi->outgoing_msg_queue);
        msg_queue_close(fi->incoming_msg_queue);
        return FUNC_FAILURE;
    }

    return FUNC_SUCCESS;
}

/**
 * @brief free any allocated memory in given frinfo structure
 * @param fi pointer to struct frinfo
 */
static void frinfo_close(struct frinfo *fi) {
    if (fi->incoming_msg_queue != NULL) {
        msg_queue_close(fi->incoming_msg_queue);
        free(fi->incoming_msg_queue);
        fi->incoming_msg_queue = NULL;
    }

    if (fi->outgoing_msg_queue != NULL) {
        msg_queue_close(fi->outgoing_msg_queue);
        free(fi->outgoing_msg_queue);
        fi->outgoing_msg_queue = NULL;
    }

    if (fi->vehicle_status != NULL) {
        free(fi->vehicle_status);
        fi->vehicle_status = NULL;
    }
}

/**
 * @brief start the application with the given config
 * @param config pointer to frinfo_config defines settings for app
 * @return 0 on success 1 on failure
 */
int frinfo_start(struct frinfo_config *config) {
    struct frinfo fi;
    frinfo_open(&fi, config);

    pthread_t comm_thread = 0;
    log_write(LOG_TAG_INFO, "launching serial communication thread");
    int rval = pthread_create(&comm_thread, NULL, serial_comm_loop, &fi);
    if (rval != FUNC_SUCCESS) {
        log_write(LOG_TAG_ERR, "failed to create serial communication thread");
        return FUNC_FAILURE;
    }

    log_write(LOG_TAG_INFO, "starting user interface");
    ui_loop(&fi);
    fi.shutdown = true;

    pthread_join(comm_thread, NULL);
    log_write(LOG_TAG_INFO, "joined communication thread");

    frinfo_close(&fi);
    return FUNC_SUCCESS;
}
