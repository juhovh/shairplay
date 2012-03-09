#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

#include "logger.h"

void
logger_init(logger_t *logger)
{
	assert(logger);

	logger->level = LOGGER_INFO;
	logger->callback = NULL;
}

void
logger_set_level(logger_t *logger, int level)
{
	assert(logger);

	logger->level = level;
}

void
logger_set_callback(logger_t *logger, logger_callback_t callback)
{
	assert(logger);

	logger->callback = callback;
}

static char *
logger_utf8_to_local(const char *str)
{
	char *ret = NULL;

/* FIXME: This is only implemented on Windows for now */
#if defined(_WIN32) || defined(_WIN64)
	int wclen, mblen;
	WCHAR *wcstr;
	BOOL failed;

	wclen = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	wcstr = malloc(sizeof(WCHAR) * wclen);
	MultiByteToWideChar(CP_UTF8, 0, str, -1, wcstr, wclen);

	mblen = WideCharToMultiByte(CP_ACP, 0, wcstr, wclen, NULL, 0, NULL, &failed);
	if (failed) {
		/* Invalid characters in input, conversion failed */
		free(wcstr);
		return NULL;
	}

	ret = malloc(sizeof(CHAR) * mblen);
	WideCharToMultiByte(CP_ACP, 0, wcstr, wclen, ret, mblen, NULL, NULL);
	free(wcstr);
#endif

	return ret;
}

void
logger_log(logger_t *logger, int level, const char *fmt, ...)
{
	char buffer[4096];
	va_list ap;

	if (level > logger->level)
		return;

	buffer[sizeof(buffer)-1] = '\0';
	va_start(ap, fmt);
	vsnprintf(buffer, sizeof(buffer)-1, fmt, ap);
	va_end(ap);

	if (logger->callback) {
		logger->callback(level, buffer);
	} else {
		char *local = logger_utf8_to_local(buffer);

		if (local) {
			fprintf(stderr, "%s", local);
			free(local);
		} else {
			fprintf(stderr, "%s", buffer);
		}
	}
}

