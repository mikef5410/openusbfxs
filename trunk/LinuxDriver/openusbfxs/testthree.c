# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>
# include "openusbfxs.h"

main () {
    int d, o, i;
    char c[8], n;
    int t = 0;
    int h;
    if ((d = open ("vm-options.ulaw", O_RDONLY)) < 0) {
        perror ("open vm-options.ulaw failed");
	exit (1);
    }
    if ((o = open ("/dev/openusbfxs0", O_WRONLY)) < 0) {
        perror ("open /dev/openusbfxs0 failed");
	exit (1);
    }
    if ((i = ioctl (o, OPENUSBFXS_IOCSRING, 1)) < 0) {
        perror ("IOCSRING (on) failed");
	exit (1);
    }
    sleep (1);
    if ((i = ioctl (o, OPENUSBFXS_IOCSRING, 0)) < 0) {
        perror ("IOCSRING (off) failed");
	exit (1);
    }
    if ((i = ioctl (o, OPENUSBFXS_IOCSLMODE, 1)) < 0) {
        perror ("IOCSLMODE (fwd active) failed");
	exit (1);
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
    while (read (d, &c[0], 8) == 8) {
        if ((n = write (o, &c[0], 8)) < 0) {
	    perror ("write failed");
	    exit (1);
	}
	if (n < 8) {
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
    }
    printf ("A total of %d bytes were written\n", t);
}
