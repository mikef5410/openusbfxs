/*
 *  oufxs.h: DAHDI-compatible Linux kernel headers for the Open USB FXS board
 *  Copyright (C) 2010  Angelos Varvitsiotis
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
 */

// MISSING: macro documentation

#ifndef OUFXS_H



/* our debug level and default facility */
#define OUFXS_DBGNONE		0
#define OUFXS_DBGTERSE		1
#define OUFXS_DBGVERBOSE	2
#define OUFXS_DBGDEBUGGING	3 /* may clog console/syslog, use at own risk */
#define OUFXS_FACILITY		KERN_INFO

#define	DEBUGGING	/* turns on verbose debugging: undef in production */

/* various states a board may be in	*/
#define OUFXS_STATE_IDLE	0	/* not initialized		*/
#define OUFXS_STATE_INIT	1	/* initializing			*/
#define OUFXS_STATE_WAIT	2	/* waiting for device to respond*/
#define OUFXS_STATE_OK		3	/* initialized			*/
#define OUFXS_STATE_ERROR	4	/* in error			*/
#define OUFXS_STATE_UNLOAD 	5	/* driver unloading		*/

/* chunk size (8 bytes = 1ms) */
#define OUFXS_CHUNK_BITS	3
#define OUFXS_CHUNK_SIZE	(1 << OUFXS_CHUNK_BITS)

/* our vendor and product ids (note: sub-licensed from Microchip) */
#define OUFXS_VENDOR_ID		0x04D8
#define OUFXS_PRODUCT_ID	0xFCF1

#ifdef __KERNEL__
#include <linux/usb.h>
#include <linux/ioctl.h>

/* max # of boards supported per system */
#define OUFXS_MAX_BOARDS	 128
#define MAX_DUMMYCHANS		1024

#if 0
#define OUFXS_MAXURB		   8	/* maximum number of isoc URBs */
#define OUFXS_INFLIGHT	 	   4	/* number of isoc URBs in flight */
#define OUFXS_MAXINFLIGHT	   4	/* max isoc URBs in flight */
#define OUFXS_MAXPCKPERURB	  32	/* max number of packets per isoc URB */
#else
#define OUFXS_MAXURB		  16	/* maximum number of isoc URBs */
#define OUFXS_INFLIGHT	 	  16	/* number of isoc URBs in flight */
#define OUFXS_MAXINFLIGHT	  16	/* max isoc URBs in flight */
#define OUFXS_MAXPCKPERURB	  16	/* max number of packets per isoc URB */
#endif

#if (OUFXS_MAXURB * OUFXS_MAXPCKPERURB > 256)
#error "The product of OUFXS_MAXURB and OUFXS_MAXPCKPERURB must be <= 256"
#endif


/* debug macros for userland and kernel */
#ifdef DEBUGGING
#define OUFXS_DEBUG(level, fmt, args...)	\
    if (debuglevel >= (level)) \
      printk (OUFXS_FACILITY KBUILD_MODNAME ": " fmt "\n", ## args)
#define OUFXS_ERR(fmt, args...) \
    printk (KERN_ERR KBUILD_MODNAME ": " fmt "\n", ## args)
#define OUFXS_WARN(fmt, args...) \
    printk (KERN_WARNING KBUILD_MODNAME ": " fmt "\n", ## args)
#define OUFXS_INFO(fmt, args...) \
    printk (KERN_INFO KBUILD_MODNAME ": " fmt "\n", ## args)
#else /* DEBUGGING */
#define OUFXS_DEBUG(level, fmt, args...)
#endif /* DEBUGGING */

#else /* __KERNEL__ */
#include <sys/ioctl.h>

#ifdef OUFXS_DEBUG_SYSLOG
#define OUFXS_DEBUG(level, fmt, args...)	/* TODO: write a syslog macro */
#else /* OUFXS_DEBUG_SYSLOG */
#define OUFXS_DEBUG(level, fmt, args...)	\
    if (debuglevel >= (level)) fprintf (stderr, fmt, ## args)
#endif /* OUFXS_DEBUG_SYSLOG */


#endif /* __KERNEL__ */

/* returned by OUFXS_IOCGSTATS */
struct oufxs_errstats {
    ulong errors;
    enum {none, in__err, out_err} lasterrop;
    int in__lasterr;
    int out_lasterr;
    ulong in_overruns;
    ulong in_missed;
    ulong in_badframes;
    ulong in_subm_failed;
    ulong out_underruns;
    ulong out_missed;
    ulong out_subm_failed;
};

/* oufxs magic ioctl number */
#define OUFXS_IOC_MAGIC	'X'

/* reset and reinitialize board */
#define OUFXS_IOCRESET	_IO(OUFXS_IOC_MAGIC, 0)
/* cause a register dump (requires compilation with DEBUG set to VERBOSE) */
#define OUFXS_IOCREGDMP	_IO(OUFXS_IOC_MAGIC, 1)
/* set ringing on/off: zero sets ring off, non-zero sets ring on */
#define OUFXS_IOCSRING	_IO(OUFXS_IOC_MAGIC, 2)
/* set linefeed mode: zero is 'open' mode; non-zero is 'forward active' mode */
#define OUFXS_IOCSLMODE	_IO(OUFXS_IOC_MAGIC, 3)
/* get hook state: returns zero if on-hook, one if off-hook */
#define OUFXS_IOCGHOOK	_IOR(OUFXS_IOC_MAGIC, 4, int)
/* generic event (hook state or DTMF): 0 is no event, others ??? */
#define OUFXS_IOCGDTMF	_IOR(OUFXS_IOC_MAGIC, 5, int)
/* get and reset statistics on errors etc. */
#define OUFXS_IOCGERRSTATS	_IOR(OUFXS_IOC_MAGIC, 6, struct oufxs_errstats)
/* burn a serial number */
#define OUFXS_IOCBURNSN	_IO(OUFXS_IOC_MAGIC, 7)
/* reboot in bootloader mode */
#define OUFXS_IOCBOOTLOAD _IO(OUFXS_IOC_MAGIC, 8)
/* get the firmware version */
#define OUFXS_IOCGVER	_IOR(OUFXS_IOC_MAGIC, 9, int)
/* get the serial number */
#define OUFXS_IOCGSN	_IOR(OUFXS_IOC_MAGIC, 10, int)
#define OUFXS_MAX_IOCTL	10

#endif /* OUFXS_H */
