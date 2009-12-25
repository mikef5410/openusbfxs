# include <fcntl.h>
# include <stdlib.h>
# include <stdio.h>

main () {
    int d, o;
    char c, n;
    int t = 0;
    if ((d = open ("vm-options.ulaw", O_RDONLY)) < 0) {
        perror ("open vm-options.ulaw failed");
	exit (1);
    }
    if ((o = open ("/dev/openusbfxs0", O_WRONLY)) < 0) {
        perror ("open /dev/openusbfxs0 failed");
	exit (1);
    }
    while (read (d, &c, 1) == 1) {
        if ((n = write (o, &c, 1)) < 0) {
	    perror ("write failed");
	    exit (1);
	}
	if (n == 0) {
	    break;
	}
	t++;
    }
    printf ("A total of %d bytes were written\n", t);
}
