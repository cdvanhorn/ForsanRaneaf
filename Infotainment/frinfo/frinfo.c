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
 */
static void frinfo_open(struct frinfo *fi, struct frinfo_config *config) {
    fi->shutdown = false;
    fi->serial_connected = false;
    fi->config = config;
    fi->incoming_message_write_cursor = 0;
    fi->incoming_message_read_cursor = 0;
    fi->outgoing_message_write_cursor = 0;
    fi->outgoing_message_read_cursor = 0;

    for (int i = 0; i < MAX_INCOMING_MESSAGES; i++) {
        fi->incoming_messages[i] = NULL;
    }

    for (int i = 0; i < MAX_OUTGOING_MESSAGES; i++) {
        fi->outgoing_messages[i] = NULL;
    }
}

/**
 * @brief free any allocated memory in given frinfo structure
 * @param fi pointer to struct frinfo
 */
static void frinfo_close(struct frinfo *fi) {
    for (int i = 0; i < MAX_INCOMING_MESSAGES; i++) {
        if (fi->incoming_messages[i] != NULL) {
            free(fi->incoming_messages[i]);
            fi->incoming_messages[i] = NULL;
        }
    }
    for (int i = 0; i < MAX_OUTGOING_MESSAGES; i++) {
        if (fi->outgoing_messages[i] != NULL) {
            free(fi->outgoing_messages[i]);
            fi->outgoing_messages[i] = NULL;
        }
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
