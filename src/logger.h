#ifndef LOGGER_H
#define LOGGER_H

/* Define syslog style log levels */
#define LOGGER_EMERG       0       /* system is unusable */
#define LOGGER_ALERT       1       /* action must be taken immediately */
#define LOGGER_CRIT        2       /* critical conditions */
#define LOGGER_ERR         3       /* error conditions */
#define LOGGER_WARNING     4       /* warning conditions */
#define LOGGER_NOTICE      5       /* normal but significant condition */
#define LOGGER_INFO        6       /* informational */
#define LOGGER_DEBUG       7       /* debug-level messages */

typedef void (*logger_callback_t)(int level, char *msg);

struct logger_s {
	int level;
	logger_callback_t callback;
};
typedef struct logger_s logger_t;

void logger_init(logger_t *logger);
void logger_set_level(logger_t *logger, int level);
void logger_set_callback(logger_t *logger, logger_callback_t callback);

void logger_log(logger_t *logger, int level, const char *fmt, ...);

#endif
