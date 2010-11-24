// logging.c

#include "evlogging.h"

#include <assert.h>
#include <event.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>


// #if (_EVENT_NUMERIC_VERSION < 0x02000100)
// #error "Libevent needs to be version 2.0.1-alpha or higher"
// #endif


// Initialise the logging system with all the info that we need,
void log_init(logging_t *log, char *logfile, short int loglevel)
{
	assert(log);
	assert(loglevel >= 0);

	log->evbase = NULL;
	log->log_event = NULL;
	log->filename = logfile;
	log->loglevel = loglevel;

	log->outbuf = (expbuf_t *) malloc(sizeof(expbuf_t));
	expbuf_init(log->outbuf, 0);
	
	log->buildbuf = (expbuf_t *) malloc(sizeof(expbuf_t));
	expbuf_init(log->buildbuf, 64);

	log->tmpbuf = (expbuf_t *) malloc(sizeof(expbuf_t));
	expbuf_init(log->tmpbuf, 32);
	
}

void log_free(logging_t *log)
{
	assert(log);

	assert(log->log_event == NULL);
	
	assert(log->outbuf);
	expbuf_free(log->outbuf);
	free(log->outbuf);
	log->outbuf = NULL;
	
	assert(log->buildbuf);
	expbuf_free(log->buildbuf);
	free(log->buildbuf);
	log->buildbuf = NULL;

	assert(log->tmpbuf);
	expbuf_free(log->tmpbuf);
	free(log->tmpbuf);
	log->tmpbuf = NULL;
}



//-----------------------------------------------------------------------------
// Add the evbase to the log structure so that we can buffer the output.
void log_buffered(logging_t *log, struct event_base *evbase)
{
	assert(log);
	assert(evbase);

	assert(log->evbase == NULL);
	log->evbase = evbase;
}



//-----------------------------------------------------------------------------
// actually write the contents of a buffer to the log file.  It will need to
// create the file if it is not there, otherwise it will append to it.
static void log_print(logging_t *log, expbuf_t *buf)
{
	FILE *fp;
	
	assert(log);
	assert(buf);
	
	assert(buf->length > 0);
	assert(buf->data);
	
	assert(log->filename);
	fp = fopen(log->filename, "a");
	if (fp) {
		fputs(expbuf_string(buf), fp);
		fclose(fp);
	}
}

//-----------------------------------------------------------------------------
// Mark the logging system so that it no longer uses events to buffer the
// output.  it will do this by simply removing the evbase pointer.
void log_direct(logging_t *log)
{
	assert(log);
	assert(log->evbase);

	// Delete the log event if there is one.
	if (log->log_event) {
		#if (_EVENT_NUMERIC_VERSION < 0x02000000)
			event_del(log->log_event);
			free(log->log_event);
		#else 
			event_free(log->log_event);
		#endif
		log->log_event = NULL;
	}

	// remove the base from the log structure.
	log->evbase = NULL;

	// if we having pending data to write, then we should write it now (because
	// there wont be any more events, and there might not be any more log
	// entries to push it out)
	if (log->outbuf->length > 0) {
		log_print(log, log->outbuf);
		expbuf_clear(log->outbuf);
	}
}


//-----------------------------------------------------------------------------
// callback function that is fired after 5 seconds from when the first log
// entry was made.  It will write out the contents of the outbuffer.
static void log_handler(int fd, short int flags, void *arg)
{
	logging_t *log;

	assert(fd < 0);
	assert(arg);

	log = (logging_t *) arg;

	// clear the timeout event.
	#if (_EVENT_NUMERIC_VERSION < 0x02000000)
		event_del(log->log_event);
		free(log->log_event);
	#else 
		event_free(log->log_event);
	#endif
	log->log_event = NULL;
	
	assert(log->outbuf->length > 0);

	log_print(log, log->outbuf);
	expbuf_clear(log->outbuf);
}



//-----------------------------------------------------------------------------
// either log directly to the file (slow), or buffer and set an event.
void logger(logging_t *log, short int level, const char *format, ...)
{
	char buffer[30], timebuf[48];
	struct timeval tv;
	time_t curtime;
	va_list ap;
	int redo;
	int n;
	struct timeval t = {.tv_sec = DEFAULT_LOG_TIMER, .tv_usec = 0};

	
	assert(log);
	assert(level >= 0);
	assert(format);

	// first check if we should be logging this entry
	if (level <= log->loglevel && log->filename) {

		// calculate the time.
		gettimeofday(&tv, NULL);
		curtime=tv.tv_sec;
		strftime(buffer, 30, "%Y-%m-%d %T.", localtime(&curtime));
		snprintf(timebuf, 48, "%s%06ld ", buffer, tv.tv_usec);

		assert(log->buildbuf);
		assert(log->buildbuf->length == 0);
		assert(log->buildbuf->max > 0);

		// process the string. Apply directly to the buildbuf.  If buildbuf is not
		// big enough, increase the size and do it again.
		redo = 1;
		while (redo) {
			va_start(ap, format);
			n = vsnprintf(log->tmpbuf->data, log->tmpbuf->max, format, ap);
			va_end(ap);

			assert(n > 0);
			if (n > log->tmpbuf->max) {
				// there was not enough space, so we need to increase it, and try again.
				expbuf_shrink(log->tmpbuf, n + 1);
			}
			else {
				assert(n <= log->tmpbuf->max);
				log->tmpbuf->length = n;
				redo = 0;
			}
		}

		// we now have the built string in our tmpbuf.  We need to add it to the complete built buffer.
		assert(log->tmpbuf->length > 0);
		assert(log->buildbuf->length == 0);
			
		expbuf_set(log->buildbuf, timebuf, strlen(timebuf));
		expbuf_add(log->buildbuf, log->tmpbuf->data, log->tmpbuf->length);
		expbuf_add(log->buildbuf, "\n", 1);

		// if evbase is NULL, then we will need to write directly.
		if (log->evbase == NULL) {
			if (log->outbuf->length > 0) {
				log_print(log, log->outbuf);
				expbuf_free(log->outbuf);
			}
			
			log_print(log, log->buildbuf);
		}
		else {
			// we have an evbase, so we need to add our build data to the outbuf.
			expbuf_add(log->outbuf, log->buildbuf->data, log->buildbuf->length);
		
			// if the log_event is null, then we need to set the timeout event.
			if (log->log_event == NULL) {
				#if (_EVENT_NUMERIC_VERSION < 0x02000000)
					log->log_event = calloc(1, sizeof(*log->log_event));
					evtimer_set(log->log_event, log_handler, (void *) log);
					event_base_set(log->evbase, log->log_event);
				#else
					log->log_event = evtimer_new(log->evbase, log_handler, (void *) log);
				#endif
				assert(log->log_event);
				evtimer_add(log->log_event, &t);
			}
		}
		
		
		
		
		expbuf_clear(log->buildbuf);
	}
}


void log_inclevel(logging_t *log)
{
	assert(log);
	log->loglevel ++;
}

void log_declevel(logging_t *log)
{
	assert(log);
	if (log->loglevel > 0)
		log->loglevel --;
}

inline short int log_getlevel(logging_t *log)
{
	assert(log);
	assert(log->loglevel >= 0);
	return(log->loglevel);
}


