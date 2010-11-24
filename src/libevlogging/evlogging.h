#ifndef __EVLOGGING_H
#define __EVLOGGING_H

#include <expbuf.h>
#include <event.h>

#define EVLOGGING_VERSION		0x0001

#if (_EVENT_NUMERIC_VERSION < 0x02000100)
	#error "Libevent needs to be version 2.0.1-alpha or higher"
#endif

#define DEFAULT_LOG_TIMER 1

typedef struct {
	struct event_base *evbase;
	struct event *log_event;
	char *filename;
	short int loglevel;
	expbuf_t *outbuf, *buildbuf, *tmpbuf;
} logging_t;

void log_init(logging_t *log, char *logfile, short int loglevel);
void log_free(logging_t *log);

void log_buffered(logging_t *log, struct event_base *evbase);
void log_direct(logging_t *log);

void log_setlevel(logging_t *log, short int loglevel);
void log_inclevel(logging_t *log);
void log_declevel(logging_t *log);
short int log_getlevel(logging_t *log);

void logger(logging_t *log, short int level, const char *format, ...);


#endif


