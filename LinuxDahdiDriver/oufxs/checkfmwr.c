/*
 *  checkfmwr.c: Firmware upgrade utility for the Open USB FXS board
 *  Copyright (C) 2010  Angelos Varvitsiotis & Rockbochs, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Acknowledgement:
 *  The author wishes to thank Rockbochs, Inc. for their support and
 *  for funding the development of this program.
 *
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/errno.h>
#include "oufxs.h"
#include <usb.h>	/* this is libusb's include file */

/* how many channels to fail opening before declaring end-of-work */
#define  MAXAPART	50

static char *me;	/* my own name */
static char opt_n = 0;	/* don't program the flash, show what would be done */
static char opt_v = 0;	/* be verbose */
static char opt_f = 0;	/* re-flash even if new image is identical to flash */

/* parts copied shamelessly from fsusb file rjlhex.h */

typedef struct _hex_record {
    unsigned char datlen;
    unsigned int addr;
    unsigned char type;
    unsigned char checksum;
    unsigned char data[0];
} hex_record;

typedef struct _hex_file {
    FILE *f;
    unsigned long addr;
} hex_file;

/*
 * 18fx455 (not selected)
 *
 * EEPROM: 0x00 - 0xff (special, currently unsupported)
 * Program memory: 0x0000 - 0x5fff
 * ID: 0x200000 - 0x200007
 * Config: 0x300000 - 0x30000d (byte-writable)
 * Devid: 0x3ffffe - 0x3fffff (read-only)
 */


/*
 * 18fx550 (default)
 * EEPROM: 0x00 - 0xff (special, currently unsupported)
 * Program memory: 0x0000 - 0x7fff
 * ID: 0x200000 - 0x200007
 * Config: 0x300000 - 0x30000d (byte-writable)
 * Devid: 0x3ffffe - 0x3fffff (read-only)
 */


#ifndef DEV_18FX455
#define DEV_18FX550
#endif /* DEV_18FX455 */

#define MI_EEPROM_BASE      0x00
#define MI_EEPROM_TOP       0xff

#define MI_PROGRAM_BASE   0x0000	/* membase for non-bootloaded programs*/
#define MI_BTLDPGM_BASE   0x0800	/* base for bootloaded program memory */

#ifdef DEV_18FX455
#define MI_PROGRAM_TOP    0x5fff
#endif /* DEV_18FX455 */
#ifdef DEV_18FX550
#define MI_PROGRAM_TOP    0x7fff
#endif /* DEV_18FX550 */

#define MI_ID_BASE      0x200000
#define MI_ID_TOP       0x200007

#define MI_CONFIG_BASE  0x300000
#define MI_CONFIG_TOP   0x30000d

#define MI_DEVID_BASE   0x3ffffe
#define MI_DEVID_TOP    0x3fffff

/* avarvit: here come fsusb's struct's; a memory image consists of
 * five "patches" (as the fsusb code calls these); each "patch" is
 * a memory area of the PIC, including the actual memory and a
 * "mask" part, which is zero for invalid and 0xff for valid data;
 * there are five of those memory areas in a hex file: "program",
 * "id", "config", "devid" and "eeprom" (I think the last one is
 * not supported by the fsusb code? anyway...);
 */
typedef unsigned char mi_byte_t;

typedef struct _mi_patch {
    unsigned long base;
    unsigned long top;
    mi_byte_t *contents;
    char *mask;
} mi_patch;

typedef struct _mi_image {
    mi_patch *program;
    mi_patch *id;
    mi_patch *config;
    mi_patch *devid;
    mi_patch *eeprom;
} mi_image;



/* various other typedefs moved here for prototypes */
typedef struct usb_dev_handle picdem_handle;
typedef int mi_cbak_t (picdem_handle *, int, int, mi_byte_t *, char *);
typedef unsigned char byte;

/* following code shamelessly copied from fsusb file "bootload.h" */

/*
 * Command packets:
 *
 * 0x00: command
 * 0x01: data length (usually; different action for some commands!)
 * 0x02: address bits 7..0
 * 0x03: address bits 15..8
 * 0x04: address bits 23..16 (upper bits always zero)
 * 0x05: data[0]
 * 0x06: data[1]
 * 0x07: data[2]
 * 0x??: ...
 * 0x3f: data[BL_DATA_LEN-1]
 */


#define BL_PACKET_LEN 64
#define BL_HEADER_LEN  5 // command, len, low, high, upper
#define BL_DATA_LEN   (BL_PACKET_LEN - BL_HEADER_LEN)


enum {
    READ_VERSION    = 0x00, // Works
    READ_FLASH      = 0x01, // Works
    WRITE_FLASH     = 0x02, // Works
    ERASE_FLASH     = 0x03, // Works
    READ_EEDATA     = 0x04, // NOT IMPLEMENTED
    WRITE_EEDATA    = 0x05, // NOT IMPLEMENTED
    READ_CONFIG     = 0x06, // NOT IMPLEMENTED
			    // (but in current firmware READ_FLASH works
    WRITE_CONFIG    = 0x07, // NOT TESTED
    UPDATE_LED      = 0x32, // NOT IMPLEMENTED
    RESET           = 0xFF  // Works
};

typedef union _bl_packet {
    byte _byte[64];
    struct {
	byte command;
	byte len;
	struct {
	    byte low;
	    byte high;
	    byte upper;
	} address;
	byte data[BL_DATA_LEN];
    };
} bl_packet;




/* prototypes (most are mine :-) */
static void usage(void);
static void version_usage(void);
hex_file *hex_open(FILE *f);	  /* create hex_file from already-open FILE * */
hex_record *hex_read(hex_file *, char *); /* read the next hex_record from f  */
hex_record *hex_raw_read (FILE *, char *);
mi_patch *mi_make_patch (unsigned long, unsigned long);
void mi_free_patch(mi_patch *);
void mi_free_image(mi_image *);
void mi_modify_patch (mi_patch *, int, int, mi_byte_t *);
mi_image *mi_load_hexfile (char *);
int mi_scan (picdem_handle *, mi_patch *, mi_cbak_t);
int usb_send (picdem_handle *, byte *, int);
int usb_recv (picdem_handle *, byte *, int);
int reset_bootloader (picdem_handle *);
int request_version (picdem_handle *, unsigned char *);
int read_flash (picdem_handle *, int, int, bl_packet *);
int write_block (picdem_handle *, int, byte *);
int verify_flash (picdem_handle *, int, int, mi_byte_t *, char *);
int program_flash (picdem_handle *, int, int, mi_byte_t *, char *);


/* following code copied shamelessly (and commented) from fsusb file memimg.h */


/* following code copied (and modified) from fsusb file rjlhex.c */


static int hfline;
/*
 * hex_open: Create a hex_file from a FILE *
 *
 * f is assumed to already be open for reading, with the read
 * pointer at the start of the file;
 */
hex_file *hex_open (FILE *f) {
    hex_file *r;

    if (f == NULL) {
	return NULL;
    }

    hfline = 0;

    r = malloc (sizeof (hex_file));
    if (r == NULL) {
	return NULL;
    }

    r->f = f;
    r->addr = 0;
    return r;
}

/*
 * hex_raw_read: Create a hex_record from the next line of f
 *
 * f is assumed to already be open for reading, with the read
 * pointer at the start of the line to parse
 */
hex_record *hex_raw_read (FILE *f, char *error) {
    hex_record *r;
    hex_record *tempr;
    char *s = NULL;
    size_t ssize = 0;
    char temps[10];
    int i;
    char *p;
    unsigned char check = 0;
    ssize_t ign;

    *error = 0;

    if (f == NULL) {
	*error = 1;
	return NULL;
    }

    r = malloc (sizeof (hex_record));
    if (r == NULL) {
	fprintf (stderr, "hex_raw_read(): malloc failed\n");
	*error = 1;
	return NULL;
    }

    ign = getline (&s, &ssize, f); /* malloc()s s, so we must free it later */
    hfline++;

    /* (translating from rjlhex.c), the format of a .hex file's line is
     *                :llaaaatt[dd...]cc
     * , where:
     *   :        is a mandatory delimiter,
     *   ll       is the length of the data part [dd...] in bytes
     *   aaaa     is a 16-byte hex address
     *   tt       is the type of the current record (see hex_read() below)
     *   [dd..]	  are one or more couple(s) of hex digits representing data
     *   cc       is a checksum
     */

    /* return without an error on EOF */
    if (strlen (s) == 0 || ign <= 0) {
        free (r);
	free (s);
	return (NULL);
    }

    p = s + strlen (s) - 1;
    while (p >= s && (*p == '\n' || *p == '\r')) {
        *p = '\0';
	p--;
    }

    /* : 
     * just make sure the line starts with a ':'
     */
    if (s[0] != ':') {
	fprintf (stderr, "hex_raw_read(): un-intelligible line %d \"%s\"\n",
	  hfline, s);
	*error = 1;
	free (r);
	free (s);
	return NULL;
    }

    /* ll
     * parse the hex-length ll and match against the line length (as
     * read by getline()); if all is OK, update checksum
     */
    if (strlen (s) < 3 || !isxdigit (s[1]) || !isxdigit (s[2])) {
	fprintf (stderr, "hex_raw_read(): invalid length, line %d \"%s\"\n",
	  hfline, s);
	*error = 1;
	free (r);
	free (s);
	return NULL;
    }
    sprintf (temps, "0x%c%c", s[1], s[2]);
    r->datlen = strtol (temps, NULL, 16);
    check += r->datlen;

    if (strlen (s) < r->datlen * 2 + 11) {
	fprintf (stderr, "hex_raw_read(): line %d \"%s\" is too short\n",
	  hfline, s);
	*error = 1;
	free (r);
	free (s);
	return NULL;
    }

    if (strlen (s) > r->datlen * 2 + 11) {
	fprintf (stderr, "hex_raw_read(): line %d \"%s\" is too long\n",
	  hfline, s);
	*error = 1;
	free (r);
	free (s);
	return NULL;
    }


    /* check the rest of the line for potential non-hex characters; if
     * everything looks OK, realloc() our hex_record structure to include
     * the necessary size for the data contained in the current line
     * (remember that hex_record contains the array of data within the
     * structure, declared as "char data[0]", which is extended as needed
     * by the realloc
     */
    
    for (i = 3; i < r->datlen * 2 + 11; i++) {
	if (!isxdigit (s[i])) {
	    fprintf (stderr,
	      "hex_raw_read(): non-hex digit character 0x%02x in \"%s\"\n",
	      s[i], s);
	    *error = 1;
	    free (r);
	    free (s);
	    return NULL;
	}
    }
    tempr = realloc (r, sizeof (hex_record) + r->datlen * 2);
    if (tempr == NULL) {
	fprintf (stderr,
	  "hex_raw_read(): realloc failed on line %d \"%s\"\n", hfline, s);
	*error = 1;
	free (r);
	free (s);
	return NULL;
    }
    r = tempr;

    /* aaaa
     * parse the 16-bit address into r->addr, updating the checksum as needed
     */

    sprintf (temps, "0x%c%c%c%c", s[3], s[4], s[5], s[6]);
    r->addr = strtol (temps, NULL, 16);

    sprintf (temps, "0x%c%c", s[3], s[4]);
    check += strtol (temps, NULL, 16);
    sprintf (temps, "0x%c%c", s[5], s[6]);
    check += strtol (temps, NULL, 16);

    /* tt
     * parse the line type, updating the checksum
     */

    sprintf (temps, "0x%c%c", s[7], s[8]);
    r->type = strtol (temps, NULL, 16);
    check += r->type;

    /* [dd...]
     * read in the actual data, updating the checksum
     */

    for (i = 0; i < r->datlen; i++) {
	sprintf (temps, "0x%c%c", s[9 + 2 * i], s[10 + 2 * i]);
	r->data[i] = strtol (temps, NULL, 16);
	check += r->data[i];
    }

    /* cc
     * read the checksum
     */
    sprintf (temps, "0x%c%c", s[r->datlen * 2 + 9], s[r->datlen * 2 + 10]);
    r->checksum = strtol (temps, NULL, 16);

    //  printf("check is %x, 2c of check is %x\n", check, (unsigned char)(-((int)check)));
    //  printf("checksum wanted is %x\n", r->checksum);

    /* we can dispose off the buffer from getline() now */
    free (s);

    /* verify the checksum */
    if ((unsigned char)(-((int)check)) != r->checksum) {
	fprintf (stderr,
	  "hex_raw_read(): line %d, BAD CHECKSUM: got 0x%02X, wanted 0x%02X\n",
	  hfline,
	  r->checksum, (unsigned char)(-((int)check)));
	*error = 1;
	free (r);
	return NULL;
    }

    return r;
}


/*
 * hex_read: Return the next hex_record from f, after processing the
 * "type" field of the record; apparently(avarvit) types 2 and 3
 * allow setting a 20- (hex86) or 32- (hex386) -bit address base
 * for the data to follow in the next lines; this address base is
 * stored in f->addr (hex file "base") and added to the 16-bit
 * address aaaa on each hex line by hex_raw_read()
 */
hex_record *hex_read (hex_file *f, char *error) {
    hex_record *r;

    if (f == NULL) {
	return NULL;
    }

    r = hex_raw_read (f->f, error);
    if (r == NULL) {
	return NULL;
    }

    switch (r->type) {
      case 0: // data
	r->addr += f->addr;
	break;

      case 1: // EOF
	/*
	 * Do nothing, although something could be done on these
	 *
	 * It'll only get more data past this on a funny file,
	 *  and the assumption is that user-supplied files are usually ok
	 *  (which may not be a good assumption)
	 */
	break;

      /* avarvit: I would like to know if these exist in hex files
       * we process, so I added two fprintf's here
       */
      case 2: // hex86 address
	f->addr = (r->data[0] << 12) + (r->data[1] << 4); // endianness?
	/*
	fprintf (stderr, "hex_read(): line %d, hex86 address 0x%lX\n",
	  hfline, f->addr);
	*/
	break;

      case 4: // hex386 address
	f->addr = (r->data[0] << 24) + (r->data[1] << 16); // endianness?
	/*
	fprintf (stderr, "hex_read(): line %d hex386 address 0x%lX\n",
	  hfline, f->addr);
	*/
	break;

      default: // ??
        fprintf (stderr,
	  "hex_read(): line %d, invalid memory type 0x%02X\n", hfline, r->type);
	*error = 1;
	free (r);
	return NULL;
    }

    return r;
}


/* parts copied (and modified) from fsusb file memimg.c */

/* mi_make_patch: Allocate and initialize a mi_patch; the code allocates
 * memory for the full range as indicated by top - base and memset()s it
 * to zero; same goes for the 'mask' part; note that memset()ting the
 * mask to 0 * makes the whole memory invalid; later calls to
 * mi_modify_patch() (see below) update the mask on the memory areas
 * that are actually specified in the .hex file into 0xff (valid)
 */
mi_patch *mi_make_patch (unsigned long base, unsigned long top)
{
    mi_patch *pat;

    pat = malloc (sizeof (mi_patch));
    if (pat == NULL) {
        fprintf (stderr, "mi_make_patch(): malloc failed\n");
	return NULL;
    }

    pat->base = base;
    pat->top = top;

    pat->contents = malloc (sizeof (mi_byte_t) * (top - base));
    if(pat->contents == NULL) {
	fprintf (stderr, "mi_make_patch(): malloc failed\n");
	free (pat);
	return NULL;
    }

    memset (pat->contents, 0xff, sizeof(mi_byte_t) * (top - base));

    pat->mask = malloc (sizeof(char) * (top - base));
    if (pat->mask == NULL) {
	fprintf (stderr, "mi_make_patch(): malloc failed\n");
	free (pat->contents);
	free (pat);
	return NULL;
    }

    memset (pat->mask, 0, sizeof(char) * (top - base));

    return pat;
}


/* mi_free_patch: Free a mi_patch and its buffers
 */
void mi_free_patch(mi_patch *p) {
    if (p == NULL) {
	return;
    }

    free (p->contents);
    free (p->mask);
    free (p);
}


/* mi_free_image: Free a mi_patch and its contents
 */
void mi_free_image(mi_image *i) {
    if (i == NULL) {
	return;
    }

    mi_free_patch (i->program);
    mi_free_patch (i->id);
    mi_free_patch (i->config);
    mi_free_patch (i->devid);
    mi_free_patch (i->eeprom);

    free(i);
}


/* mi_modify_patch: Modify patch contents, tagging it as changed;
 * actually this sets the data of the memory area [base...base+len-1]
 * to the data pointed to by 'data', as well as the respective
 * mask to 0xff (=valid memory)
 */
void mi_modify_patch (mi_patch *p, int base, int len, mi_byte_t *data) {
    int i;

    if (p == NULL) {
	return;
    }

    if (base < p->base || base + len - 1 > p->top) {
	fprintf (stderr, "*** mi_modify_patch(): patch out of range\n");
	return;
    }

    for (i=0; i < len; i++) {
	p->contents [base - p->base + i] = data [i];
	p->mask [base - p->base + i] = 0xff;
    }
}


/* mi_image: Create a mi_image from the contents of filename
 */
mi_image *mi_load_hexfile (char *filename) {
    mi_image *img;
    hex_record *r;
    FILE *f;
    hex_file *hf;
    char error;

    if (filename == NULL) {
	fprintf (stderr, "mi_load_hexfile(): null filename supplied\n");
	return NULL;
    }

    f = fopen (filename, "r");
    if (f == NULL) {
	fprintf (stderr, "mi_load_hexfile(): error opening ");
	perror (filename);
	return NULL;
    }

    hf = hex_open (f);
    if (hf == NULL) {
	/* no need to report errors here, hex_open will do so for us */
	fclose (f);
	return NULL;
    }

    img = malloc (sizeof(mi_image));
    if (img == NULL) {
	fprintf (stderr, "mi_load_hexfile(): malloc failed\n");
	fclose (f);
	free (hf);
	return NULL;
    }

    /* These nulls may not be required, but make me [av: this is the original
     * author of the code] feel safer when  using free_image() on an error
     */
    img->program = NULL;
    img->id = NULL;
    img->config = NULL;
    img->devid = NULL;
    img->eeprom = NULL;

    img->program = mi_make_patch (MI_PROGRAM_BASE, MI_PROGRAM_TOP);
    img->id	 = mi_make_patch (MI_ID_BASE, MI_ID_TOP);
    img->config  = mi_make_patch (MI_CONFIG_BASE, MI_CONFIG_TOP);
    img->devid	 = mi_make_patch (MI_DEVID_BASE, MI_DEVID_TOP);
    img->eeprom	 = mi_make_patch (MI_EEPROM_BASE, MI_EEPROM_TOP);

    if (img->program == NULL || img->id == NULL || img->config == NULL
      || img->devid == NULL || img->eeprom == NULL) {
	fclose (f);
	free (hf);
	mi_free_image (img);
	return NULL;
    }

    while ((r = hex_read (hf, &error))) {
	if (r->type == 0) {
	    /*
	    printf ("file: %.2i@0x%.8X:\t", r->datlen, r->addr);
	    for (i=0; i < r->datlen; i++) {
		printf ("%.2x", r->data[i]);
	    }
	    printf ("\n");
	    */

	    /*
	     * // if (r->addr >= MI_PROGRAM_BASE && r->addr <= MI_PROGRAM_TOP) {
	     * (see http://forum.sparkfun.com/viewtopic.php?t=5767 : this was
	     * changed to MI_BTLDPGM_BASE in order to ignore potential lines
	     * in the .hex file addressing memory below 0x800)
	     *
	     * (note (avarvit): I have the impression that the bootloader
	     * firmware itself ignores attempted writes below 0x800, but
	     * it certainly feels safer not to attempt such writes at all)
	     */

	    if (r->addr >= MI_PROGRAM_BASE && r->addr <  MI_BTLDPGM_BASE) {
		if (opt_v) {
		    fprintf (stderr,
		      "mi_load_hexfile(): "
		      "\"%s\", line %d: low-mem data @%04X ignored\n",
		      filename, hfline, r->addr);
		}
	    }

	    /* program memory */
	    if (r->addr >= MI_BTLDPGM_BASE && r->addr <= MI_PROGRAM_TOP) {
		mi_modify_patch (img->program, r->addr, r->datlen, r->data);
	    }

	    /* id memory */
	    if (r->addr >= MI_ID_BASE && r->addr <= MI_ID_TOP) {
		if (opt_v) {
		    fprintf (stderr,
		      "mi_load_hexfile(): "
		      "\"%s\", line %d: id-mem data @%06X ignored\n",
		      filename, hfline, r->addr);
		}
		/*
		mi_modify_patch(img->id, r->addr, r->datlen, r->data);
		*/
	    }

	    /* config memory */
	    if (r->addr >= MI_CONFIG_BASE && r->addr <= MI_CONFIG_TOP) {
		if (opt_v) {
		    fprintf (stderr,
		      "mi_load_hexfile(): "
		      "\"%s\", line %d: cfg data @%06X ignored\n",
		      filename, hfline, r->addr);
		}
		/*
		mi_modify_patch(img->config, r->addr, r->datlen, r->data);
		*/
	    }

	    /* devid memory */
	    if (r->addr >= MI_DEVID_BASE && r->addr <= MI_DEVID_TOP) {
		if (opt_v) {
		    fprintf (stderr,
		      "mi_load_hexfile(): "
		      "\"%s\", line %d: devid-mem data @%06X ignored\n",
		      filename, hfline, r->addr);
		}
		/*
		mi_modify_patch(img->devid, r->addr, r->datlen, r->data);
		*/
	    }
	}
	free(r);
	//    printf("\n");
    }

    free (hf);
    fclose (f);
    if (error) {
        fprintf (stderr,
	  "mi_load_hexfile(): \"%s\" contains errors"
	  " or is not a valid hex image\n",
	  filename);
	mi_free_image (img);
	return NULL;
    }
    return img;
}

/* following code copied and adapted from fsusb file main.c */

int mi_scan (picdem_handle *udev, mi_patch *p, mi_cbak_t cbak) {

    int b, i, active;
    int retval = 1;
    int cbak_ret;

    /* mi data are handled in 64-byte blocks; the code below scans
     * the memory image in such blocks and, for each of these, if
     * the block actually contains code, it performs some action by
     * calling the callback function "cbak"
     */
    for (b = 0; b <= p->top - p->base; b += 64) {
	active = 0;
	for (i = 0; i < 64 && b + i <= p->top - p->base; i++) {
	    if (p->mask [i + b]) {
		active = 1;
	    }
	}

	if (active) {
	    if (!(cbak_ret = cbak (
	      udev,
	      b + p->base,
	      (b + 63 + p->base > p->top)? p->top - p->base - b + 1 : 64,
	      p->contents + b,
	      p->mask + b)
	    )) {
	        /* something bad happened; however, we don't break out of
		 * the loop, since it may be better to go on (e.g., for
		 * the purpose of printing out all the differences between
		 * the memory image and the actual flash contents in a
		 * verify operation); instead, we just note down the error
		 * return value (zero) and we pass that to our caller
		 */
		retval = cbak_ret;
	    }
	    /*      printf("active %s block at %.8lx\n", ttt, b+p->base); */
	}
    }
    return retval;
}



/* following code copied and adapted from fsusb file fsusb.c */

# define picdem_ep__in	0x81
# define picdem_ep_out  0x01
# define picdem_timeout	1000

int usb_send (picdem_handle *udev, byte *buf, int len) {
    int ret;
    ret = usb_bulk_write(udev, picdem_ep_out, (char *)buf, len, picdem_timeout);
    if (ret != len) {
        fprintf (stderr,
	  "%s: usb_bulk_write() returned %d instead of the expected %d\n",
	  me, ret, len);
	return 0;
    }
    return ret;
}

int usb_recv (picdem_handle *udev, byte *buf, int len) {
    int ret;
    ret = usb_bulk_read (udev, picdem_ep__in, (char *)buf, len, picdem_timeout);
    if (ret != len) {
        fprintf (stderr,
	  "%s: usb_bulk_read() returned %d instead of the expected %d\n",
	  me, ret, len);
	return 0;
    }
    return ret;
}

/* reset the board */
int reset_bootloader (picdem_handle *udev) {
    if (!usb_send (udev, (byte *) "\377", 1)) {
        return 0;
    }
    return 1;
}

/* get the (picdem bootloader) version as (maj,min) into the two-byte
 * char array pointed to by ret
 */
int request_version (picdem_handle *udev, unsigned char *ret) {
    union {
	struct {
	    byte cmd;
	    byte len;
	    byte min;
	    byte maj;
	} packet;
	byte data[4];
    } buf;

    if (!usb_send (udev, (byte *) "\0\0\0\0\0", 5)) {
        return 0;
    }
    if (!usb_recv (udev, buf.data, 4)) {
        return 0;
    }
    ret[0] = buf.packet.maj;
    ret[1] = buf.packet.min;
    return 1;
}


/* read a block of bytes from the board's flash memory
 */
int read_flash (picdem_handle *udev, int offset, int len, bl_packet *rpack) {
    bl_packet spack;

    /*
    fprintf (stderr, "read_flash (udev, %08lX, %i, rpack);\n",
      (unsigned long) offset, len);
    */

    spack.command = READ_FLASH;
    spack.address.low   = (offset & 0x0000ff) >>  0;
    spack.address.high  = (offset & 0x00ff00) >>  8;
    spack.address.upper = (offset & 0x0f0000) >> 16;
    spack.len = len;

    if (!usb_send (udev, &spack._byte[0], 5)) {
        return 0;
    }
    if (!usb_recv (udev, &rpack->_byte[0], len + 5)) {
        return 0;
    }
    return 1;
}


/* first erase, then write a 64-byte block of (64-byte-aligned) data into the
 * board's flash memory at offset 'offset' (64-byte boundaries are needed
 * because of the way the flash can be erased -- as 64-byte blocks)
 */
int write_block (picdem_handle *udev, int offset, byte *data) {
    bl_packet spack;
    byte ret;
    unsigned int subblock = 0;

    if (offset & 0x3f) {
        fprintf (stderr, "write_block(): offset %06x is not 64-byte aligned\n",
	  offset);
	return 0;
    }

    /* the ERASE_FLASH operation erases a 64-byte block */
    spack.command = ERASE_FLASH;
    spack.address.low   = (offset & 0x0000ff) >>  0;
    spack.address.high  = (offset & 0x00ff00) >>  8;
    spack.address.upper = (offset & 0x0f0000) >> 16;
    spack.len = 1;

    if (!usb_send (udev, &spack._byte[0], 5)) {
        return 0;
    }
    /* see note in write_flash() above about the returned byte's contents */
    if (!usb_recv (udev, &ret, 1)) {
        return 0;
    }

    /* flash can be written in 16-byte, 16-byte-aligned chunks only */
    for (subblock = 0; subblock < 4; subblock++) {

	/*
	int i;
	char buf [33];
	for (i = 0; i < 16; i++) {
	    sprintf (&buf[2*i],
	      "%02x", (unsigned int) *(data + (subblock << 4) + i));
	}
	fprintf (stderr, "write_block: @%06X: %s\n",
	  (offset + (subblock << 4)), buf);
	*/

	spack.command = WRITE_FLASH;
	spack.address.low   = ((offset + (subblock << 4)) & 0x0000ff) >>  0;
	spack.address.high  = ((offset + (subblock << 4)) & 0x00ff00) >>  8;
	spack.address.upper = ((offset + (subblock << 4)) & 0x0f0000) >> 16;
	spack.len = 16;
	memcpy (spack.data, data + (subblock << 4), 16);

	if (!usb_send (udev, &spack._byte[0], 5 + 16)) {
	    return 0;
	}
	/* see note in write_flash() above about the returned byte's contents */
	if (!usb_recv (udev, &ret, 1)) {
	    return 0;
	}
    }
    return 1;
}



/* the following code is copied and adapted from fsusb file main.c */

int verify_flash (picdem_handle *udev, int addr, int len, mi_byte_t *data, char *mask) {
    int mb;
    int i;
    bl_packet bp;
    int ret = 1;
    /*  printf("verifying %i bytes at %.8x\n", len, addr); */

    /*
    printf ("data is ");
    for (i = 0; i < len; i++) {
	printf ("%.2x", data[i]);
    }
    printf ("\nmask is ");
    for (i = 0; i < len; i++) {
	printf ("%.2x", (mask[i])? 0xff: 0x00);
    }
    printf ("\n");
    */

    /* data is passed as (at-most-)64-byte chunks; this is split into
     * two 32-byte read_flash calls
     */
    for (mb = 0; mb < 64 && mb < len; mb += 32) {
	if (!read_flash (
	  udev, addr + mb, (len - mb >= 32)? 32: len - mb, &bp)) {
	    /* read_flash will only fail in case of a USB I/O error;
	     * in that case we return, since chances are that a serious
	     * error has occurred
	     */
	    return 0;
	}

	for (i = 0; i < 32 && mb + i < len; i++) {
	    if (mask [mb + i] && data [mb + i] != bp.data [i]) {
		ret = 0;
		fprintf (stderr,
		  "verify_flash(): mismatch in %i-byte chunk at 0x%.8x:\n",
		  (len - mb >= 32)? 32: len - mb, addr + mb);
		fprintf (stderr, "Hexfile: ");
		for (i = 0; i < 32 && mb + i < len; i++) {
		    if (mask [mb + i]) {
			fprintf (stderr, "%.2x", data [mb + i]);
		    } else {
			fprintf (stderr, "##");
		    }
		}
		fprintf (stderr, "\nDevice:  ");
		for (i = 0; i < 32 && mb + i < len; i++) {
		    fprintf (stderr, "%.2x", bp.data[i]);
		}
		fprintf (stderr, "\n");
	    }
	}
	/* we don't break out of the loop here, because it looks like
	 * the best thing to do is to go on and print out all the
	 * discrepancies between our image and the flash memory
	 */
    }
    return ret;
}


int program_flash (picdem_handle *udev, int addr, int len, mi_byte_t *data, char *mask) {

    if (len != 64) {
        fprintf (stderr, "program_flash(): error: passed a %d-byte block\n",
	  len);
	return 0;
    }
    if (opt_n) {
        return 0;
    }
    if (!write_block (udev, addr, data)) {
        return 0;
    }
    return 1;
}


/* the following code is mine :-) */

#define FSUSB_VENDOR_ID		0x04d8		/* Microchip Inc.	*/
#define FSUSB_PRODUCT_ID	0x000b		/* PICDEM-FS USB	*/

static char path[128];

static void usage() {
    fprintf (stderr, "usage: %s [-v][-n|-f] <hexfile> <version>\n", me);
    fprintf (stderr, "       -v: produce verbose output\n");
    fprintf (stderr,
      "       -n: do not write flash; show what would be done\n");
    fprintf (stderr, "       -f: program flash even if hex img is identical\n");
    fprintf (stderr, "           (without -f, flash is not re-programmed if\n");
    fprintf (stderr, "           it is found to be identical to hex image)\n");
}

static void version_usage () {
    usage ();
    fprintf (stderr,
      "       <version> is of the form \"x.y.z\", where x, y and z are\n"
      "       numbers from 0 to 255\n");
}

static int mystrncpy (char *dst, char *src, size_t size) {
    strncpy (dst, src, size);
    dst [size - 1] = '\0';
    return (strcmp (dst, src));
}


static int upgrade (int fd, unsigned long serial, mi_image *img) {
    struct usb_bus *bus;	/* libusb bus */
    struct usb_device *dev;	/* libusb device */
    char busstr[32], devstr[32];/* stored bus and dev strings of oufxs pers/ty*/
    usb_dev_handle *udev;	/* libusb usb device handle */
    char serstr[256];		/* serial number to look (string) */
    unsigned long ser;		/* serial number found (ulong) */
    unsigned char buf[2];	/* for querying the bootloader version # */

    dev = NULL; /* to keep gcc happy */
    fprintf (stderr,
      "  %s: looking for oufxs with serial#=%08lX\n", me, serial);

    /* refresh libusb's idea of devices on the system */
    usb_find_devices ();
    for (bus = usb_busses; bus != NULL; bus = bus->next) {
        for (dev = bus->devices; dev != NULL; dev = dev->next) {
	    udev = 0;
	    if (
	      dev->descriptor.idVendor  == OUFXS_VENDOR_ID &&
	      dev->descriptor.idProduct == OUFXS_PRODUCT_ID) {

		if (!dev->descriptor.iSerialNumber) {
		    /* could happen, if we have a very old board that does
		     * not report a serial at all
		     */
		    goto next_dev;
		}
		if (!(udev = usb_open (dev))) {
		    goto next_dev;
		}
		if (!usb_get_string_simple (udev, dev->descriptor.iSerialNumber, serstr, sizeof (serstr))) {
		    goto next_dev;
		}
		sscanf (serstr, "%lx", &ser);
		if (ser == serial) {
		    usb_close (udev);
		    goto outa_here;
		}
	    }

	  next_dev:
	    if (udev) {
	        usb_close (udev);
	    }
	}
    }

  outa_here:
    if (!bus || !dev) {
	fprintf (stderr,
	  "%s: unexpected error: no oufxs board with serial %8lX found!\n",
	  me, serial);
	return 0;
    }

    if (mystrncpy (busstr, bus->dirname, sizeof (busstr))) {
        fprintf (stderr,
	  "%s: unexpected error: bus name too long (%s)\n",
	  me, bus->dirname);
	return 0;
    }
    if (mystrncpy (devstr, dev->filename, sizeof (devstr))) {
        fprintf (stderr,
	  "%s: unexpected error: dev name too long (%s)\n",
	  me, dev->filename);
	return 0;
    }

    fprintf (stderr,
      "  %s: found board on bus/dev %s/%s, s/n %8lX; invoking bootloader\n",
      me, bus->dirname, dev->filename, ser);

    if (ioctl (fd, OUFXS_IOCBOOTLOAD) < 0) {
        perror ("ioctl (IOCBOOTLOAD) failed");
	return 0;
    }

    /* give the board a few moments to reboot and the usb bus some time to
     * re-settle
     */
    sleep (3);

    /* note: Linux's USB bus always increases the device number for
     * a new device on the system; hence, our newly appearing bootloader
     * board will have a device number greater than the oufxs board found
     * earlier; there is no bulletproof way to associate the new device
     * with the old board, but we pick the first PICDEMFS board on the
     * same bus with a device number greater than that of the formerly
     * found oufxs board (in the hope that nobody will run a true PICDEM
     * board or a bootloader-booted-oufxs board on the same machine on
     * the same bus at the same time trying to upgrade an fxs-mode-booted
     * oufxs board :-)
     */

    /* refresh libusb's idea of devices on the system */
    usb_find_devices ();
    for (bus = usb_busses; bus != NULL; bus = bus->next) {
	if (strcmp (bus->dirname, busstr)) {
	    /*
	    fprintf (stderr, "  skipping bus %s\n", bus->dirname);
	    */
	    continue;
	}
        for (dev = bus->devices; dev != NULL; dev = dev->next) {
	    if (strcmp (dev->filename, devstr) < 0) {
		/*
	        fprintf (stderr, "  skipping dev %s (< %s)\n",
		  dev->filename, devstr);
		*/
		continue;
	    }
	    if (
	      dev->descriptor.idVendor  != FSUSB_VENDOR_ID ||
	      dev->descriptor.idProduct != FSUSB_PRODUCT_ID) {
		/*
		fprintf (stderr, "  skipping dev %s (!FSUSB)\n", dev->filename);
		*/
		continue;
	    }
	    goto outa_here2;
	}
    }

  outa_here2:
    if (!bus || !dev) {
	fprintf (stderr,
	  "%s: unexpected error: no bootloader device on bus %s after reload\n",
	  me, busstr);
	return 0;
    }

    fprintf (stderr,
      "  %s: found bootloader on bus %s, dev %s\n",
      me, bus->dirname, dev->filename);

    // FIXME: should use READ_EEDATA to get the serial number and match it
    // against the one reported by the oufxs personality

    /* open the device */
    if (!(udev = usb_open (dev))) {
        fprintf (stderr,
	  "%s: usb_open() failed\n", me);
	goto failed;
    }

    /* choose configuration #1 */
    if (usb_set_configuration (udev, 1)) {
        fprintf (stderr,
	  "%s: usb_set_configuration() to 1 failed for bootloader\n", me);
	goto failed;
    }

    if (!request_version (udev, buf)) {
	/* no error is necessary, request_version() has printed one for us */
        goto failed;
    }

    if (buf [0] != 0x01u) {
        fprintf (stderr, "%s: unexpected bootloader major version %u\n",
	  me, buf[0]);
        fprintf (stderr, "%s: leaving board in bootloader mode\n", me);
	goto failed;
    }


    if (mi_scan (udev, img->program, verify_flash)) {
	if (opt_f) {
	    fprintf (stderr,
	     "  on-board flash is identical to hex file "
	     "(but reflashing anyway since -f given)\n");
	}
	else {
	    fprintf (stderr,
	     "  on-board flash is identical to hex file; "
	     "resetting board without flashing...\n");
	    reset_bootloader (udev);
	    goto failed;
	}
    }

    if (!mi_scan (udev, img->program, program_flash)) {
	if (opt_n) {
	    fprintf (stderr,
	      "  did not write flash (-n given); resetting board...\n");
	}
	else {
	    fprintf (stderr,
	      "%s: program flash failed; resetting board...\n", me);
	}
	reset_bootloader (udev);
	goto failed;
    }

    fprintf (stderr, "  %s: board's flash programmed, verifying...\n", me);
    if (!mi_scan (udev, img->program, verify_flash)) {
        fprintf (stderr,
	  "%s: failed verification! (not resetting, pls. program manually)\n",
	  me);
	goto failed;
    }
    fprintf (stderr, "  %s: verified OK; resetting board...\n", me);
    reset_bootloader (udev);
    usb_close (udev);

    return 1;

  failed:
    if (udev) {
        usb_close (udev);
    }
    return 0;
}

int main (int argc, char **argv)
{
    int fd;
    int i;
    int oufxscount = 0;
    int fmwrtooold = 0;
    int upgraded = 0;
    int apart = 0;
    unsigned long serial;
    unsigned long version;
    mi_image *img = NULL;

    unsigned int maj, min, rev, bmj, bmn, brv;

    /* set me to the basename of the invoked program */
    me = strrchr (argv[0], '/');
    me = me? me + 1 : argv[0];

    if (argc < 3) {
        usage ();
	exit (1);
    }

    if (argv[1][0] == '-') {
	switch (argv[1][1]) {
	  case 'n':
	    if (opt_f) {
		fprintf (stderr,
		  "%s: options -n and -f are mutually exclusive\n",
		  me);
		usage ();
		exit (1);
	    }
	    opt_n = 1;
	    argv++;
	    break;

	  case 'v':
	    opt_v = 1;
	    argv++;
	    break;

	  case 'f':
	    if (opt_n) {
		fprintf (stderr,
		  "%s: options -n and -f are mutually exclusive\n",
		  me);
		usage ();
		exit (1);
	    }
	    opt_f = 1;
	    argv++;
	    break;

	  default:
	    fprintf (stderr, "%s: unknown option %s\n", me, argv[1]);
	    usage();
	    exit (1);
	}
    }
    

    if ((img = mi_load_hexfile (argv [1])) == NULL) {
	exit (1);
    }

    if (sscanf (argv[2], "%d.%d.%d", &maj, &min, &rev) != 3) {
        version_usage ();
	exit (1);
    }
    if (maj < 0 || maj > 255 || min < 0 || min > 255 || rev < 0 || rev > 255) {
        version_usage ();
	exit (1);
    }
    fprintf (stderr, "%s: upgrading oufxs devices to version %d.%d.%d\n",
      me, maj, min, rev);

    if (opt_v) {
	if (setenv ("USB_DEBUG", "1", 1) < 0) {
	    perror ("setenv failed");
	}
    }
    usb_init ();
    /* assuming busses won't change throughout the program's lifetime,
     * calling usb_find_busses() just once here should be OK; in contrast,
     * usb_find_devices() is caled every time, to sync with board
     * reboots and personality changes;
     */
    usb_find_busses ();


    /* loop over all dahdi channels in the system */
    for (i = 1; ; i++) {
        snprintf (path, 64, "/dev/dahdi/%d", i);

	if (opt_v) {
	    fprintf (stderr, "  trying %s\n", path);
	}
	if ((fd = open (path, O_RDWR)) < 0) {
	    switch (errno) {
	      /* caveat: the following code just exits if the next /dev/dahdi/N
	       * fails with ENOENT; however, if e.g. /dev/dahdi/1 has been
	       * removed from the system while /dev/dahdi/2 is still active,
	       * this may not be what we wanted;
	       */
	      case ENOENT:
	        if (apart++ < MAXAPART) {
		    /* silently try the next device until we reach MAXAPART */
		    continue;
		}
	        fprintf (stderr,
		  "%s: %d dahdi devices tried, %d oufxs boards found, "
		  "%d too old, %d upgraded\n",
		  me, i - 1, oufxscount, fmwrtooold, upgraded);
		exit (0);

	      case ENXIO:
	        fprintf (stderr,
		  "%s: -ENXIO on opening %s; have you run dahdi_cfg yet?\n",
		  me, path);
		break;

	      case EIO:
	        fprintf (stderr,
		  "%s: -EIO on opening %s; please check board's health\n",
		  me, path);
		break;

	      case EAGAIN:
	        fprintf (stderr,
		  "%s: -EAGAIN on opening %s; re-run after board initializes\n",
		  me, path);
		break;

	      default:
	        fprintf (stderr,
		  "%s: unexpected error %d on opening %s\n",
		  me, errno, path);
		break;
	    }
	    goto next_dahdi_dev;
	}
	/* reset apart, so next time we start over */
	apart = 0;

	/* try to get the board's serial number [note: the magic number
	 * used by oufxs ioctls is not normally used by dahdi devices,
	 * so it is relatively safe to issue a OUFXS_IOCGSN (get serial
	 * number) ioctl in order to attempt to tell a oufxs board; OTOH,
	 * an unimplemented ioctl (ENOTTY) error cannot really be used
	 * to tell apart a oufxs device running older firmware from
	 * other devices; luckily, both cases (older oufxs devices and
	 * other devices) are to be handled alike, in that older oufxs
	 * boards cannot be flashed automagically by this program]
	 */
	if (ioctl (fd, OUFXS_IOCGSN, &serial) < 0) {
	    if (errno == ENOTTY) {
		/* try also a register dump (supported since old versions) */
	        if (ioctl (fd, OUFXS_IOCREGDMP) < 0) {
		    fprintf (stderr,
		      "%s: %s is likely not a oufxs device, ignoring it\n",
		      me, path);
		    goto next_dahdi_dev;
		}
		else {
		    oufxscount++;
		    fmwrtooold++;
		    fprintf (stderr,
		      "%s: cannot update %s (errno=%d): "
		      "firmware too old; must be > 1.27.0\n",
		      me, path, errno);
		}
	    }
	    else {
	        fprintf (stderr, "%s: unexpected errno (%d) for OUFXS_IOCGSN\n",
		  me, errno);
		goto next_dahdi_dev;
	    }
	}
	fprintf (stderr, "  found serial %08lX\n", serial);
	
	if (ioctl (fd, OUFXS_IOCGVER, &version) < 0) {
	    fprintf (stderr,
	      "%s: unexpected error (errno: %d) while getting version\n",
	      me, errno);
	    perror ("");
	    goto next_dahdi_dev;
	}

	bmj = (version & 0x00ff0000) >> 16;
	bmn = (version & 0x0000ff00) >>  8;
	brv = (version & 0x000000ff);
	if (maj > bmj ||
	  (maj == bmj && min > bmn) ||
	  (maj == bmj && min == bmn && rev > brv)) {
	    if (upgrade (fd, serial, img) == 1) {
		upgraded++;
	    }
	}
	else {
	    fprintf (stderr, "  %s: no need to upgrade %s, runs %d.%d.%d\n",
	      me, path, bmj, bmn, brv);
	}

      next_dahdi_dev:
        if (fd >= 0) {
	    close (fd);
	}
    }
}
