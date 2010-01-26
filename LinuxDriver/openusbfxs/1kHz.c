# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>
# include <unistd.h>
# include "openusbfxs.h"
static char c[8] = {0x10, 0x20, 0x30, 0x40, 0x30, 0x20, 0x10, 0x00};
main () {
    int o, i;
    char c[8], n;
    int t = 0;
    int h;
    int k;
    if ((o = open ("/dev/openusbfxs0", O_WRONLY)) < 0) {
        perror ("open /dev/openusbfxs0 failed");
	exit (1);
    }
    sleep (1);
    if ((i = ioctl (o, OPENUSBFXS_IOCGHOOK, &h)) < 0) {
	perror ("IOCGHOOK failed");
	exit (1);
    }
    if (h) {
        printf ("Not ringing since set is off-hook\n");
    }
    else {
	if ((i = ioctl (o, OPENUSBFXS_IOCSRING, 1)) < 0) {
	    perror ("IOCSRING (on) failed");
	    exit (1);
	}
	sleep (1);
	if ((i = ioctl (o, OPENUSBFXS_IOCSRING, 0)) < 0) {
	    perror ("IOCSRING (off) failed");
	    exit (1);
	}
    }
    while (1) {
	if ((i = ioctl (o, OPENUSBFXS_IOCGHOOK, &h)) < 0) {
	    perror ("IOCGHOOK failed");
	    exit (1);
	}
	printf ("Phone is %s-hook\n", (h)? "off":"on");
	if (h) break;
        sleep (1);
    }
    while (1) {
        if ((n = write (o, &c[0], 8)) < 0) {
	    perror ("write failed");
	    exit (1);
	}
	if (n < 8) {
	    fprintf (stderr, "write returned %d\n", n);
	    break;
	}
	t += 8;
	if ((i = ioctl (o, OPENUSBFXS_IOCGHOOK, &h)) < 0) {
	    perror ("IOCGHOOK failed");
	    exit (1);
	}
	if (!h) {
	    printf ("Phone is %s-hook\n", (h)? "off":"on");
	    break;
	}
	if ((i = ioctl (o, OPENUSBFXS_IOCGDTMF, &k)) < 0) {
	    perror ("IOCGDTMF failed");
	    exit (1);
	}
	if (k) {
	    printf ("DTMF key pressed: %c\n", k);
	}
    }
    printf ("A total of %d bytes were written\n", t);
}
