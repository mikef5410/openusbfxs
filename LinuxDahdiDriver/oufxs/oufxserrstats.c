#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include "oufxs.h"

static char *me;
static char path [128];


static void usage(void)
{
    fprintf (stderr, "usage: %s <chanX>\n", me);
}

static void chanusage (void)
{
    usage ();
    fprintf (stderr, "\t<chanX> must be a number between 1 and 999\n");
}


int main (int argc, char **argv)
{
    me = argv[0];
    int fd;
    struct oufxs_errstats errstats;
    int channo;

    if (argc != 2) {
	usage ();
	exit (1);
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

    if (ioctl (fd, OUFXS_IOCGERRSTATS, &errstats) < 0) {
	perror ("OUFXS_IOCGERRSTATS failed");
	exit (1);
    }

    fprintf (stderr, "%s: error statistics for %s\n", me, path);
    fprintf (stderr, "  Total number of errors:         %lu\n",
      errstats.errors);
    fprintf (stderr, "  Last error operation:           %s\n",
      (errstats.lasterrop == none)? "none" :
      ((errstats.lasterrop == in__err)? "IN" : "OUT"));
    fprintf (stderr, "  Last IN  errno:                 %d\n",
      errstats.in__lasterr);
    fprintf (stderr, "  Last OUT errno:                 %d\n",
      errstats.out_lasterr);
    /* rest of stats are not implemented yet */
    exit (0);
}
