/**
 * @file frinfo.c
 */

#include "frinfo.h"
#include "../utilities/defines.h"
#include "../utilities/logger.h"
#include "../utilities/threading.h"
#include "serial_comm.h"
#include "ui.h"

/**
 * @brief start the application with the given config
 * @param config pointer to frinfo_config defines settings for app
 * @return 0 on success 1 on failure
 */
int frinfo_start(struct frinfo_config *config) {
    struct frinfo fi;
    fi.shutdown = false;
    fi.config = config;

    pthread_t comm_thread = 0;
    log_write(LOG_TAG_INFO, "launching serial communication thread");
    int rval = pthread_create(&comm_thread, NULL, serial_comm_loop, &fi);
    if (rval != FUNC_SUCCESS) {
        log_write(LOG_TAG_ERR, "failed to create serial communication thread");
        return FUNC_FAILURE;
    }

    // start the user interface

    pthread_join(comm_thread, NULL);

    return FUNC_SUCCESS;
}
