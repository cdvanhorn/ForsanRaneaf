/**
* @file logger.h
*/

#ifndef LOGGER_H
#define LOGGER_H

#define LOG_DEBUG 0
#define LOG_RELEASE 1

#define LOG_TAG_INFO 0
#define LOG_TAG_WARN 1
#define LOG_TAG_ERR 2

#define LOG_FILE_PATH "log.txt"

extern int log_open(int profile);
extern void log_write(int tag, const char *message);
extern void log_close();

#endif //LOGGER_H
