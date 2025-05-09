/**
* @file logger.c
*/

#ifdef _WIN32
#define _CRT_SECURE_NO_DEPRECATE
#endif

#include "../logger.h"
#include "../defines.h"

#include <inttypes.h>
#include <stdio.h>
#include <time.h>

#define LOG_FORMAT_STRING "%" PRIu64 "[%s]: %s\n"

static int log_profile;
static FILE *log_file;

/**
 * @brief Prepare the logger use.
 * @param profile - int if running in debug or release mode
 * @return return 1 on success 0 on failure
 */
int log_open(int profile)
{
	log_profile = profile;
	log_file = fopen(LOG_FILE_PATH, "w");
	if (log_file == NULL) {
		log_write(LOG_TAG_ERR, "fatal unable to open log file.");
		return FUNC_FAILURE;
	}
	return FUNC_SUCCESS;
}

/**
 * @brief Convert the int log level to a string for output
 * @param tag - int representation of log level
 * @return const char * string representation of log level
 */
static const char *tag_to_string(int tag)
{
	switch (tag) {
	case LOG_TAG_INFO:
		return "INFO";
	case LOG_TAG_WARN:
		return "WARN";
	case LOG_TAG_ERR:
		return "ERR";
	default:
		return "tag";
	}
}

/**
 * @brief Write the message to stdout and log file if it's open
 * @param tag - int what level is this message
 * @param message - message to write to log
 */
void output(int tag, const char *message)
{
	const time_t now = time(NULL);
	const char *ctag = tag_to_string(tag);
	if (log_file != NULL)
		fprintf(log_file, LOG_FORMAT_STRING, now, ctag, message);
	printf(LOG_FORMAT_STRING, now, ctag, message);
}

/**
 * @brief Write a log message, if logging in debug mode all logs output to
 *	stdout and the log file, if in release mode only error messages get
 *	written to log file and stdout.
 * @param tag - int what level is this message
 * @param message - message to write to the log
 */
void log_write(int tag, const char *message)
{
	if (log_profile != LOG_RELEASE || tag == LOG_TAG_ERR)
		output(tag, message);
}

/**
 * @brief Clean up logging
 */
void log_close()
{
	if (log_file != NULL) {
		fclose(log_file);
		log_file = NULL;
	}
}
