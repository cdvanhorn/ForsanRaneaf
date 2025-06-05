/**
 * @file msg_queue.h
 */

#ifndef MSG_QUEUE_H
#define MSG_QUEUE_H

#define MAX_MESSAGES 256

#include "../utilities/threading.h"
#include <stdint.h>

struct msg_queue {
    char *queue[MAX_MESSAGES]; //!< array of char pointers each is a message
    uint16_t write_cursor; //!< index where writing messages
    uint16_t read_cursor; //!< index where reading messages
    pthread_mutex_t *lock; //!< pointer to mutex used to lock queue
};

extern int msg_queue_open(struct msg_queue *q);
extern void msg_queue_close(struct msg_queue *q);
extern void msg_queue_write(struct msg_queue *q, char *msg);
extern char *msg_queue_read(struct msg_queue *q);

#endif //MSG_QUEUE_H
