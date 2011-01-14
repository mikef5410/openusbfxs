#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dahdi/version.h"

int main (void) {
    char *p, *maj, *min;
    char *version = strdup (DAHDI_VERSION);

    maj = version;
    for (p = maj; *p && *p != '.'; p++);
    *p = '\0';
    min = ++p;
    for (; *p && *p != '.'; p++);
    *p = '\0';
    if (*version) {
	printf (
	  "#define DAHDI_VERSION_MAJOR\t%s\n#define DAHDI_VERSION_MINOR\t%s\n",
	  maj, min);
    }
    exit (0);
}
