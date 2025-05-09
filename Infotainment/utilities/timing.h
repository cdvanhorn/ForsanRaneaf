/**
* @file timing.h
*/

#ifndef TIMING_H
#define TIMING_H
#include <stdint.h>

extern uint64_t timing_get_time();
extern int timing_msleep(uint32_t ms);

#endif //TIMING_H
