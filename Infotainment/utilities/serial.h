/**
 * @file serial.h
 */

#ifndef SERIAL_H
#define SERIAL_H

extern int serial_open(const char *path, int baud_rate, int buffer_size);
extern void serial_read(char **data);
extern void serial_close();

#endif //SERIAL_H
