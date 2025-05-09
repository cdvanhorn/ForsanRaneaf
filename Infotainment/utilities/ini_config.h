/**
 * @file ini_config.h
 */

#ifndef INI_CONFIG_H
#define INI_CONFIG_H

#include <stdint.h>

#define INICFG_CONFIG_FILE_PATH "config.ini"

extern int inicfg_open();
extern void inicfg_getstring(const char *section, const char *key,
			     char **value);
extern void inicfg_getint(const char *section, const char *key, int *value);
extern void inicfg_getuint8_t(const char *section, const char *key,
			      uint8_t *value);
extern void inicfg_close();

#endif //INI_CONFIG_H
