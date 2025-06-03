/**
 * @file msg_queue.c
 */

#include "msg_queue.h"

#include <stdlib.h>

#include "../utilities/defines.h"
#include "../utilities/logger.h"

/**
 * @brief increment the read cursor for this queue, if we've reached the limit go back to 0, queue should be locked before calling
 * @param q pointer to msg_queue struct
 */
static void inc_read_cursor(struct msg_queue *q) {
    q->read_cursor++;
    if (q->read_cursor == MAX_MESSAGES)
        q->read_cursor = 0;
}

/**
 * @breif increment the write cursor for this queue, if we've reached the limit go back to 0, queue should be locked before calling
 * @param q pointer to msg_queue struct
 */
static void inc_write_cursor(struct msg_queue *q) {
    q->write_cursor++;
    if (q->write_cursor == MAX_MESSAGES)
        q->write_cursor = 0;
}

/**
 * @brief lock the given message queue
 * @param q pointer to msg_queue struct to lock
 */
static void lock_msg_queue(const struct msg_queue *q) {
    pthread_mutex_lock(q->lock);
}

/**
 * @brief unlock the given message queue
 * @param q pointer to msg_queue struct to unlock
 */
static void unlock_msg_queue(const struct msg_queue *q) {
    pthread_mutex_unlock(q->lock);
}

/**
 * @brief open a message queue, allocate memory initialize variables
 * @param q pointer to msg_queue struct representing q to open
 * @return 0 on success 1 on failure
 */
int msg_queue_open(struct msg_queue *q) {
    q->read_cursor = 0;
    q->write_cursor = 0;

    for (int i = 0; i < MAX_MESSAGES; i++) {
        q->queue[i] = NULL;
    }

    q->lock = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
    if (q->lock == NULL) {
        log_write(LOG_TAG_ERR, "couldn't allocate message queue lock");
        return FUNC_FAILURE;
    }

    if (pthread_mutex_init(q->lock, NULL) != 0) {
        free(q->lock);
        q->lock = NULL;
        log_write(LOG_TAG_ERR, "couldn't initialize message queue lock");
        return FUNC_FAILURE;
    }

    return FUNC_SUCCESS;
}

/**
 * @brief close a message queue, deallocating memory and clearing variables
 * @param q pointer to msg_queue struct representing q to close
 */
void msg_queue_close(struct msg_queue *q) {
    if (q == NULL) return;

    pthread_mutex_destroy(q->lock);
    free(q->lock);
    q->lock = NULL;

    for (int i = 0; i < MAX_MESSAGES; i++) {
        if (q->queue[i] != NULL) {
            free(q->queue[i]);
            q->queue[i] = NULL;
        }
    }
}

/**
 * @brief writes a new message to the queue, handling locking and cursor updates
 * @param q pointer to msg_queue to write to
 * @param msg pointer to char message to write to the queue
 */
void msg_queue_write(struct msg_queue *q, char *msg) {
    lock_msg_queue(q);
    q->queue[q->write_cursor] = msg;
    inc_write_cursor(q);
    unlock_msg_queue(q);
}

/**
 * @brief return message in queue at read cursor
 * @param q pointer to msg_queue to read from
 * @return pointer to char message at read cursor
 */
char * msg_queue_read(const struct msg_queue *q) {
    return q->queue[q->read_cursor];
}

/**
 * @brief free message at read cursor and advance read cursor
 * @param q pointer to msg_queue
 */
void msg_queue_flush(struct msg_queue *q) {
    if (q->queue[q->read_cursor] == NULL) return;
    lock_msg_queue(q);
    free(q->queue[q->read_cursor]);
    q->queue[q->read_cursor] = NULL;
    inc_read_cursor(q);
    unlock_msg_queue(q);
}
