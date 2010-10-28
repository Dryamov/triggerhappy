#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <strings.h>

#include "devices.h"
#include "command.h"
#include "cmdsocket.h"

int main(int argc, char *argv[]) {
	char *op = NULL;
	char *dev = NULL;
	if (argc == 3 && (strcasecmp( "udev", argv[2] ) == 0) ) {
		/* gather params from udev environment */
		op = getenv("ACTION");
		dev = getenv("DEVNAME");
		if (op == NULL || dev == NULL) {
			return 1;
		}
	} else if (argc == 3 && (strcasecmp( "quit", argv[2] ) == 0) ) {
		op = "quit";
	} else if (argc == 3 && (strcasecmp( "enable", argv[2] ) == 0) ) {
		op = "enable";
	} else if (argc == 3 && (strcasecmp( "disable", argv[2] ) == 0) ) {
		op = "disable";
	} else if (argc == 4) {
		op = argv[2];
		dev = argv[3];
	} else {
		fprintf( stderr, "Use:\n");
		fprintf( stderr, "  th-cmd <socket> udev\n");
		fprintf( stderr, "  th-cmd <socket> add <device>\n");
		fprintf( stderr, "  th-cmd <socket> remove <device>\n");
		fprintf( stderr, "  th-cmd <socket> disable\n");
		fprintf( stderr, "  th-cmd <socket> enable\n");
		fprintf( stderr, "  th-cmd <socket> quit\n");
		return 1;
	}

	int s = connect_cmdsocket( argv[1] );
	if (s < 0) {
		perror("connect()");
		return 1;
	}
	int err;
	if ( strcasecmp( "add", op ) == 0 ) {
		err = send_command( s, CMD_ADD, dev );
	} else if ( strcasecmp( "remove", op ) == 0 ) {
		err = send_command( s, CMD_REMOVE, dev );
	} else if ( strcasecmp( "enable", op ) == 0 ) {
		err = send_command( s, CMD_ENABLE, NULL );
	} else if ( strcasecmp( "disable", op ) == 0 ) {
		err = send_command( s, CMD_DISABLE, NULL );
	} else if ( strcasecmp( "quit", op ) == 0 ) {
		err = send_command( s, CMD_QUIT, NULL );
	} else {
		fprintf( stderr, "Unknown command: %s\n", op);
	}
	if (err) {
		//fprintf( stderr, "Error sending command\n");
		perror("send_command()");
	}
	close(s);

}
