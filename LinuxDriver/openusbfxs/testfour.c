# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>
# include <unistd.h>
# include "openusbfxs.h"

main () {
    int d, o, i;
    char c[8], n;
    int t = 0;
    int h;
    int k;
    struct openusbfxs_stats s;

    if ((d = open ("testfour.ulaw", O_CREAT|O_WRONLY|O_TRUNC, 0644)) < 0) {
        perror ("open testfour.ulaw failed");
	exit (1);
    }
    if ((o = open ("/dev/openusbfxs0", O_RDONLY)) < 0) {
        perror ("open /dev/openusbfxs0 failed");
	exit (1);
    }
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

    while (read (o, &c[0], 8) == 8) {
        if ((n = write (d, &c[0], 8)) < 0) {
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
	/* every 8192 bytes (~1 sec) print out statistics */
	if (!(t & 0x1fff)) {
	    if ((i = ioctl (o, OPENUSBFXS_IOCGSTATS, &s)) < 0) {
		perror ("IOCGSTATS failed");
		exit (1);
	    }
	    printf (
	      "IN OVR: %d, IN_MSS: %d, IN_BAD: %d, OUTUND: %d, OUTMSS: %d\n",
	      s.in_overruns, s.in_missed, s.in_badframes,
	      s.out_underruns, s.out_missed);
	}
    }
    printf ("A total of %d bytes were read and saved\n", t);
}
