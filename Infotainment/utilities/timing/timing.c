/**
* @file timing.c
*/

#include "../timing.h"

#include "../defines.h"

#include <stdint.h>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __linux__
#include <time.h>
#endif

#include <math.h>

/**
 * @brief get time in ms since system boot
 * @return time in ms since system startup
 */
uint64_t timing_get_time()
{
#ifdef _WIN32
	return GetTickCount64();
#elif __linux__
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000 +
	       round(ts.tv_nsec /
		     1000000); // convert nanoseconds to milliseconds
#endif
}

/**
 * @brief sleep for specified number of milliseconds
 * @param ms unsigned 32 bit int milliseconds to sleep
 * @return 0 on success
 */
int timing_msleep(uint32_t ms)
{
#ifdef _WIN32
	Sleep(ms);
	return FUNC_SUCCESS;
#elif __linux__
	struct timespec ts;
	ts.tv_nsec = ms * 1000000;
	ts.tv_sec = 0;
	return nanosleep(&ts, NULL);
#endif
}
