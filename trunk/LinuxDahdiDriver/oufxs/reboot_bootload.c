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
    fprintf (stderr, "usage: %s <chan1>\n", me);
}

static void chanusage ()
{
    usage ();
    fprintf (stderr, "\t<chanX> must be a number between 1 and 999\n");
}

main (int argc, char **argv)
{
    me = argv[0];
    int fd;
    int channo;
    int i;

    if (argc != 2) {
	usage ();
	exit (1);
    }

    channo = atoi (argv[1]);
    if (channo < 0 || channo > 999) {
	chanusage ();
	exit (1);
    }
    snprintf (path, 64, "/dev/dahdi/%d", channo);
    if ((fd = open (path, O_RDWR)) < 0) {
	perror (path);
	exit (1);
    }

    if (ioctl (fd, OUFXS_IOCBOOTLOAD) < 0) {
	perror ("OUFXS_IOCBOOTLOAD failed");
	exit (1);
    }

    fprintf (stderr, "%s: board %s should now be in bootloader mode\n", me, path);
}
