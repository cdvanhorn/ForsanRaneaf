/**
 * @file serial_comm.c
 */

#include "serial_comm.h"

#include <stddef.h>

/**
 * @brief main method of serial communication thread
 * @param args void pointer arguments to serial communication thread
 * @return void pointer to result of thread
 */
void *serial_comm_loop(void *args) {
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
