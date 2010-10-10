/*
 * thd.c - the triggerhappy daemon
 * by Stefan Tomanek <stefan.tomanek@wertarbyte.de>
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <stdlib.h>
#include <sys/wait.h>

#include "eventnames.h"
#include "reader.h"
#include "keystate.h"
#include "executer.h"

#ifndef NOTHREADS

#include <pthread.h>

// PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP looks like a GNU thing
#ifndef PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP
#define PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP \
{ { 0, 0, 0, PTHREAD_MUTEX_RECURSIVE_NP, 0, { 0 } } }
#endif

pthread_mutex_t keystate_mutex = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

#define LOCK(mutex) pthread_mutex_lock(&mutex)
#define UNLOCK(mutex) pthread_mutex_unlock(&mutex)

#else

#define LOCK(x) 
#define UNLOCK(x)

#endif // NOTHREADS

static char* script_basedir = NULL;
static int dump_events = 0;

char* lookup_event_name(struct input_event ev) {
	if (ev.type == EV_KEY) {
		return (KEY_MAX >= ev.code ? KEY_NAME[ ev.code ] : NULL);
	}
	if (ev.type == EV_SW) {
		return (SW_MAX >= ev.code ? SW_NAME[ ev.code ] : NULL);
	}
	return NULL;
}

/*
 * Look up event and key names and print them to STDOUT
 */
void print_event(struct input_event ev) {
	char *typename = EV_NAME[ ev.type ];
	char *evname = lookup_event_name( ev );
	if ( evname != NULL ) {
		printf( "%s\t%s\t%d\n", typename, evname, ev.value );
	} else {
		fprintf( stderr, "Unknown %s event id: %d (value %d)\n", typename, ev.code, ev.value );
	}
	fflush(stdout);
}

/*
 * Read events from device file, decode them and print them to STDOUT
 */
int read_events(char *devname) {
	int dev;
	dev = open(devname, O_RDONLY);
	if (dev < 0) {
		fprintf(stderr, "Unable to open device file '%s': %s\n", devname, strerror(errno));
		return 1;
	} else {
		struct input_event ev;
		while(1) {
			int n = read( dev, &ev, sizeof(ev) );
			if ( n != sizeof(ev) ) {
				fprintf(stderr, "Read error\n");
				return 1;
			}
			/* ignore all events except KEY and SW */
			if (ev.type == EV_KEY || ev.type == EV_SW) {
				LOCK(keystate_mutex);
				change_keystate( ev );
				if (dump_events) {
					print_event( ev );
					print_keystate();
				}
				if (script_basedir != NULL)
					launch_script( script_basedir, ev );
				UNLOCK(keystate_mutex);
			}
		}
		close(dev);
	}
	return 0;
}

#ifndef NOTHREADS
void* reader_thread(void* ptr) {
	char *devname;
	devname = (char *) ptr;
	read_events( devname );
}

void spawn_reader(char *dev, struct readerlist **list) {
	// append struct to list
	if (*list == NULL) {
		*list = malloc(sizeof(**list));
		(*list)->reader.devname = dev;
		(*list)->next = NULL;
		pthread_create( &((*list)->reader.thread), NULL, &reader_thread, (void *)dev );
	} else {
		spawn_reader( dev, &((*list)->next) );
	}
}

void join_readers(struct readerlist **list) {
	if (*list != NULL) {
		pthread_join( (*list)->reader.thread, NULL );
		join_readers( &( (*list)->next ) );
		free(*list);
	}
}

#endif

int main(int argc, char *argv[]) {
	signal(SIGCHLD, SIG_IGN);
	int c;
	while ((c = getopt(argc, argv, "ds:")) != -1) {
		switch (c) {
			case 'd':
				dump_events = 1;
				break;
			case 's':
				script_basedir = optarg;
				break;
			case '?':
				if (optopt == 's') {
					fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				} else if (isprint(optopt)) {
					fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				} else {
					fprintf(stderr, "Unknown option character\n");
				}
				return 1;
		}
	}
	return start_readers(argc, argv, optind);
}

int start_readers(int argc, char *argv[], int start) {
	if (argc-start < 1) {
		fprintf(stderr, "No input device file specified.\n");
		return 1;
	} else {
#ifndef NOTHREADS
		struct readerlist *readers;
		// create one thread for every device file supplied
		int i;
		for (i=start; i<argc; i++) {
			char *dev = argv[i];
			spawn_reader( dev, &readers );
		}
		for (i=start; i<argc; i++) {
			join_readers( &readers );
		}
#else
		// without threading, we only handle the first device file named
		read_events( argv[start] );
#endif
	}
	return 0;
}
