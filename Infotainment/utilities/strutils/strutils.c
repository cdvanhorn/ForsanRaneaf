/**
* @file strutils.c
*/

#include "../strutils.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

char *strtrim(char *str)
{
	size_t size;
	char *end;

	size = strlen(str);

	if (!size)
		return str;

	end = str + size - 1;
	while (end >= str && isspace(*end))
		end--;
	*(end + 1) = '\0';

	while (*str && isspace(*str))
		str++;

	return str;
}
