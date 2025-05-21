/**
 * @file frinfo.c
 */

#include "frinfo.h"
#include "../utilities/defines.h"

#include <stddef.h>

/**
 * @breif main method of UI thread
 * @param args void pointer arguments to UI thread
 * @return void pointer to result of thread
 */
void *frinfo_ui(void *args) {
    return NULL;
}

/**
 * @breif main method of serial communication thread
 * @param args void pointer arguments to serial communcation thread
 * @return void pointer to result of thread
 */
void *frinfo_serial(void *args) {
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
    return NULL;
}

/**
 * @brief start the application with the given config
 * @param config pointer to frinfo_config defines settings for app
 * @return 0 on success 1 on failure
 */
int frinfo_start(const struct frinfo_config *config) {
    // algorithm:
    // initialize frinfo object
    // create a serial port communication thread
    // create a UI thread
    // start the threads
    // wait for threads to finish
    return FUNC_SUCCESS;
}

/**
 * @breif close the application
 * @return 0 on success 1 on failure
 */
int frinfo_stop() {
    /*
     * algorithm:
     * if frinfo object exists free it
     * close serial connection if exists
     */
    return FUNC_SUCCESS;
}
