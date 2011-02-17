#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <dahdi/user.h>
#include "oufxs.h"

static char *me;
static char path [128];


static void usage()
{
    fprintf (stderr, "usage: %s <chan1> <serial>\n", me);
}

static void chanusage ()
{
    usage ();
    fprintf (stderr, "\t<chanX> must be a number between 1 and 999\n");
}

static void serusage () 
{
    usage ();
    fprintf (stderr, "\t<serial> must consist of exactly eight hex digits\n");
}


main (int argc, char **argv)
{
    me = argv[0];
    int fd;
    unsigned long serial;
    unsigned int b3 = 0, b2 = 0, b1 = 0, b0 = 0;
    int channo;
    int i;

    if (argc != 3) {
	usage ();
	exit (1);
    }

    if (strcmp (argv[argc - 1], "-r") && strlen (argv [argc - 1]) != 8) {
        serusage ();
	exit (1);
    }
    else {
	for (i = 0; i < 8; i++) {
	    if (!isxdigit (argv [argc - 1][i])) {
		serusage ();
		exit (1);
	    }
	}
    }
    sscanf (argv[argc - 1], "%2x%2x%2x%2x", &b3, &b2, &b1, &b0);
    fprintf (stderr, "%2X%2X%2X%2X\n", b3, b2, b1, b0);
    serial = (b3 & 0xff) << 24 |
             (b2 & 0xff) << 16 |
	     (b1 & 0xff) <<  8 |
	     (b0 & 0xff);

    if (!serial) {
    	if (!strcmp (argv[argc - 1], "-r")) {
	    serial = 0xEEEEEEEE;
	}
	else {
	    usage ();  
	    exit (1);
	}
    }

    channo = atoi (argv[1]);
    if (channo <= 0 || channo > 999) {
	chanusage ();
	exit (1);
    }
    snprintf (path, 64, "/dev/dahdi/%d", channo);
    if ((fd = open (path, O_RDWR)) < 0) {
	perror (path);
	exit (1);
    }

    if (ioctl (fd, OUFXS_IOCBURNSN, &serial) < 0) {
	perror ("OUFXS_IOCBURNSN failed");
	exit (1);
    }

    fprintf (stderr, "%s: serial set to %ld\n", me, serial);
}
