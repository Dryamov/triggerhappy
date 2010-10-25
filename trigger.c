#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eventnames.h"
#include "keystate.h"
#include "trigger.h"

trigger *TRIGGER_LIST = NULL;

static int triggers_are_enabled = 1;

void triggers_enabled( int status ) {
	triggers_are_enabled = status;
}

static int strint(const char *str) {
	if (str == NULL) {
		return -1;
	} else {
		return atoi(str);
	}
}

/* copy token */
static char* ct(char *token) {
	if (token == NULL) {
		return NULL;
	} else {
		return strdup(token);
	}
}

trigger* parse_trigger(char* line) {
	char *token = NULL;
	char *sptr = NULL;

	/* ignore everything behind # */
	char *comment = strchr(line, '#');
	if ( comment != NULL ) {
		*comment = '\0';
	}
	char *cp = strdup(line);

	char *delim = " \t\n";

	char *evdef = strtok_r(cp, delim, &sptr);
	/* extract modifier */
	char *sptr2 = NULL;
	char *evname = ct( strtok_r(cp, "+", &sptr2) );
	/* build array of modifier keycodes */
	trigger_modifier modifier;
	int n = 0;
	while (n < TRIGGER_MODIFIERS_MAX) {
		char *evmod = strtok_r(NULL, "+", &sptr2);
		modifier[n] = evmod ? lookup_event_code(evmod) : -1;
		n++;
	}
	int value = strint( strtok_r(NULL, delim, &sptr) );
	char *cmd = ct( strtok_r(NULL, "\n", &sptr) );
	free(cp);

	/* all fields filled? */
	if (evname && cmd && (value >= 0)) {
		trigger *et = malloc( sizeof(trigger) );
		et->type = lookup_event_type( evname );
		et->code = lookup_event_code( evname );
		et->value = value;
		et->cmdline = cmd;
		/* store keystate */
		memcpy(et->modifiers, modifier, sizeof(trigger_modifier));
		et->next = NULL;
		return et;
	} else {
		free(evname);
		free(cmd);
		return NULL;
	}
}

void append_trigger(trigger *t) {
	trigger **p = &TRIGGER_LIST;
	while (*p != NULL) {
		p = &( (*p)->next );
	}
	*p = t;
}

int read_triggerfile(const char *filename) {
	trigger **p = &TRIGGER_LIST;
        FILE *conf;
        int len = 0;
        char *line = NULL;
        ssize_t read;
        conf = fopen(filename, "r");
        if (conf == NULL) {
		return 1;
        }
        while ((read = getline(&line, &len, conf)) != -1) {
		trigger *t = parse_trigger( line );
		if (t) {
			append_trigger( t );
		} else if (strlen(line) > 0) {
			fprintf(stderr, "Unable to parse trigger line: %s\n", line);
		}
	}
	fclose(conf);
	free(line);
	return 0;
}

static int mods_equal( keystate_holder ksh, trigger_modifier tm ) {
	int n = 0;
	while ( n < TRIGGER_MODIFIERS_MAX ) {
		int code = tm[n]; /* this key must be pressed */
		if (code < 0) {
			break;
		} else if (ksh[code] <= 0) {
			return 0;
		}
		n++;
	}
	/* Now n is equal to the number of modifiers needed
	 * we can check whether any additional keys are pressed
	 * by counting the number of pressed keys instead of having
	 * to cross-examine every key against the modifier list
	 */
	int x;
	for (x=0; x<=KEY_MAX; x++) {
		if (ksh[x] > 0) {
			n--;
		}
	}
	/* if n is zero, we have the exact number of needed modifiers pressed */
	return (n == 0);
}

void run_triggers(int type, int code, int value, keystate_holder ksh) {
	if (triggers_are_enabled == 0) {
		return;
	}
	trigger *et = TRIGGER_LIST;
	while (et != NULL) {
		if ( type  == et->type &&
		     code  == et->code &&
		     value == et->value &&
		     mods_equal(ksh, et->modifiers)) {
			fprintf(stderr, "Executing trigger: %s\n", et->cmdline);
			int pid = fork();
			if (pid == 0 ) {
				/* adjust environment */
				setenv( "TH_KEYSTATE", get_keystate(ksh), 1 );
				char *en = lookup_event_name_i( et->type, et->code );
				setenv( "TH_EVENT", en, 1 );
				char ev[8];
				sprintf( &(ev[0]), "%d", et->value );
				setenv( "TH_VALUE", &(ev[0]), 1 );
				system(	et->cmdline );
				exit(0);
			}
		}
		et = et->next;
	}
}

int count_triggers( trigger **list ) {
	int n = 0;
	trigger *p = *list;
	while ( p != NULL ) {
		n++;
		p = p->next;
	}
	return n;
}

void clear_triggers() {
	trigger **p = &TRIGGER_LIST;
	while ( *p != NULL ) {
		trigger **n = &( (*p)->next );
		free( (*p)->cmdline );
		free( (*p) );
		*p = NULL;
		p = n;
	}
}
