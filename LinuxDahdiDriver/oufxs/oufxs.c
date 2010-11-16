/*
 *  oufxs.c: DAHDI-compatible Linux kernel driver for the Open USB FXS board
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

// TODO: (a decent) version, function documentation
static char *driverversion = "0.1-dahdi";


/* includes */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/moduleparam.h>
#include <linux/mod_devicetable.h>
#include <linux/mutex.h>
#include <linux/usb.h>
#include <linux/workqueue.h>
#include <linux/kfifo.h>
#include <linux/wait.h>
#include <linux/ioctl.h>
#include <linux/version.h>
#include <asm/uaccess.h>

/* Open USB FXS - specific includes */
#include "oufxs.h"
#include "cmd_packet.h"
#include "../proslic.h"

/* dahdi-specific includes */
#include <dahdi/kernel.h>
/* normally, we would include here something like a "oufxs_user.h"
 * header, where we would define our own structures and ioctls for
 * oufxs; however, it is much more convenient to have dahdi-tools
 * programs like "fxstest.c" talk to our board without having to
 * reinvent the wheel (especially given that the differences
 * between the wctdm.c and our module from a user's perspective
 * are few - mostly vmwi capabilities, which we are lacking)
 */
#include <dahdi/wctdm_user.h>


/* local #defines */
	/* define to get HW-based DTMF detection in dev->dtmf */
	/* (note: Asterisk does its own DTMF detection, so this
	 * is useless for Asterisk)
	 */
#undef HWDTMF
	/* define to get instantaneous hook state in dev->hook */
#undef HWHOOK
	/* define to print debugging messages related to seq#->buf mapping */
#undef DEBUGSEQMAP

/* initialization macro; assumes target 's' is a char array memset to 0 */
#define safeprintf(s, args...)	snprintf (s, sizeof(s) - 1, ## args)

/* immediately return on failure, leaving the board in its current state */
#ifdef DEBUGGING
#define DEFDEBUGLEVEL OUFXS_DBGVERBOSE
#define RETONFAIL 1
#else /* DEBUGGING */
#define DEFDEBUGLEVEL OUFXS_NONE
#define RETONFAIL 0
#endif /* DEBUGGING */

/* maximum buffer lengths */
#define OUFXS_MAXIBUFLEN	(rpacksperurb * OUFXS_DPACK_SIZE)
#define OUFXS_MAXOBUFLEN	(wpacksperurb * OUFXS_DPACK_SIZE)

/* default values for urb-related module parameters */
#ifndef WPACKSPERURB	/* any value from 2 to OUFXS_MAXPCKPERURB */
#define WPACKSPERURB	4
#endif /* WPACKSPERURB */

#ifndef RPACKSPERURB	/* any value from 2 to OUFXS_MAXPCKPERURB */
#define RPACKSPERURB	4
#endif /* RPACKSPERURB */

#ifndef WURBSINFLIGHT	/* any value from 2 to OUFXS_MAXINFLIGHT */
#define WURBSINFLIGHT	OUFXS_INFLIGHT
#endif /* WURBSINFLIGHT */

#ifndef RURBSINFLIGHT	/* any value from 2 to OUFXS_MAXINFLIGHT */
#define RURBSINFLIGHT	OUFXS_INFLIGHT
#endif /* RURBSINFLIGHT */

/* OHT_TIMER specifies how long to maintain on-hook-transfer after ring */
#define OHT_TIMER	6000	/* in milliseconds */

/* module parameters */

/* generic */
static int debuglevel	= DEFDEBUGLEVEL;	/* actual debug level */
/* initialization failure handling */
static int retoncnvfail	= RETONFAIL;		/* quit on dc-dc conv fail */
static int retonadcfail	= RETONFAIL;		/* quit on adc calbr. fail */
static int retonq56fail	= RETONFAIL;		/* quit on q56 calbr. fail */
/* urb etc. dimensioning */
static int wpacksperurb	= WPACKSPERURB;		/* # of write packets per urb */
static int wurbsinflight= WURBSINFLIGHT;	/* # of write urbs in-flight */
static int rpacksperurb	= RPACKSPERURB;		/* # of read packets per urb */
static int rurbsinflight= RURBSINFLIGHT;	/* # of read urbs in-flight */
static int sofprofile = 0;			/* SOF profiling mode */
/* telephony-related */
static int alawoverride	= 0;			/* use a-law instead of mu-law*/
static int reversepolarity = 0;			/* use reversed polarity */
static int loopcurrent = 20;			/* loop current */
static int lowpower = 0;			/* set on-hook voltage to 24V */
static int hifreq = 1;				/* ~80kHz, for L1=100uH */
#ifdef DEBUGSEQMAP
static int complaintimes = 0;			/* how many times to complain */
#endif	/* DEBUGSEQMAP */
static int availinerror = 0;			/* make available even when
						 * in error condition */

module_param(debuglevel, int, S_IWUSR|S_IRUGO);
module_param(retoncnvfail, bool, S_IWUSR|S_IRUGO);
module_param(retonadcfail, bool, S_IWUSR|S_IRUGO);
module_param(retonq56fail, bool, S_IWUSR|S_IRUGO);
module_param(wpacksperurb, int, S_IRUGO);
module_param(wurbsinflight, int, S_IRUGO);
module_param(rpacksperurb, int, S_IRUGO);
module_param(rurbsinflight, int, S_IRUGO);
module_param(sofprofile, int, S_IWUSR|S_IRUGO);
module_param(alawoverride, int, S_IWUSR|S_IRUGO);
module_param(reversepolarity, int, S_IWUSR|S_IRUGO);
module_param(loopcurrent, int, S_IWUSR|S_IRUGO);
module_param(lowpower, int, S_IWUSR|S_IRUGO);
module_param(hifreq, int, S_IWUSR|S_IRUGO);
#ifdef DEBUGSEQMAP
module_param(complaintimes, int, S_IWUSR|S_IRUGO);
#endif	/* DEBUGSEQMAP */
module_param(availinerror, bool, S_IWUSR|S_IRUGO);

/* our device structure */

struct oufxs_dahdi {

    int				slot;    	/* slot in boards[] below */
    /* USB core stuff */
    struct usb_device		*udev;		/* usb device for this board */
    struct usb_interface	*intf;		/* usb intface for this board */

    /* usb endpoint descriptors and respective packet sizes */
    __u8			ep_bulk_in;	/* bulk IN  EP address	*/
    __u8			ep_bulk_out;	/* bulk OUT EP address	*/
    __u8			ep_isoc_in;	/* ISOC IN  EP address	*/
    __u8			ep_isoc_out;	/* ISOC OUT EP address	*/
    __u8			bulk_in_size;	/* bulk IN  packet size	*/
    __u8			bulk_out_size;	/* bulk OUT packet size	*/

    /* isochronous buffer/urb structures */
    struct isocbuf {
	spinlock_t		lock;		/* protect this buffer	*/
        struct urb		*urb;		/* urb to be submitted	*/
	char			*buf;		/* actual buffer	*/
	enum state_enum {
	    st_free	= 0,	/* free to be submitted	*/
	    st_subm	= 1	/* submitted to usb core*/
	}			state;		/* current buffer state	*/
	char			inconsistent;	/* bad state was found	*/
	// int			len;		/* buffer length	*/
	struct oufxs_dahdi	*dev;		/* back-pointer to us	*/
    }				outbufs[OUFXS_MAXURB];
    int				outsubmit;	/* next out buffer to submit */
    spinlock_t			outbuflock;	/* short-term lock for above */
    struct isocbuf		in_bufs[OUFXS_MAXURB];
    int				in_submit;	/* next in buffer to submit */
    spinlock_t			in_buflock;	/* short-term lock for above */
    char			*prevrchunk;	/* previous read chunk	*/
    __u8			*seq2chunk[256];/* seqno->sample map	*/

    /* sequence numbers */
    __u8			outseqno;	/* out sequence number	*/
    __u8			in_moutsn;	/* in mirrored outseqno	*/
    __u32			in_oofseq;	/* # of out-of-seq packets */
    __u8			in_seqevn;	/* even seq# from device*/
    __u8			in_seqodd;	/* odd seq# from device	*/

    /* ProSLIC DR-setting related stuff (protected by dev->outbuflock)	*/
    __u8			drsseq;		/* current DR-set seq	*/
    __u8			drsreg;		/* current DR-set reg	*/
    __u8			drsval;		/* current DR-set value	*/

    // TODO: remove/bypass urb anchoring for kernels earlier than 2.6.23
    /* anchor for submitted/pending urbs */
    struct usb_anchor		submitted;

    /* initialization worker thread pointer and queue */
    struct workqueue_struct	*iniwq;	/* initialization workqueue	*/
    struct work_struct		iniwt;	/* initialization worker thread	*/

    /* board state */
    int				state;	/* device state - see oufxs.h	*/
#ifdef HWHOOK	/* superseded by hook debouncing code */
    __u8			hook;	/* 0=on-hook, 1=off-hook	*/
#endif
    __u8			dtmf;	/* 0=no dtmf, non-0=dtmf event	*/

    /* locks and references */
    int				opencnt;/* number of openers		*/
    spinlock_t			statelck; /* locked during state changes*/
    struct kref			kref;	/* kernel/usb reference counts	*/
    struct mutex		iomutex;/* serializes I/O operations	*/
    // spinlock_t		lock;	/* generic per-card lock	*/

    /* stuff replicated from wctdm.c */
    int oldrxhook;
    int debouncehook;
    int lastrxhook;
    int debounce;
    int ohttimer;		/* remaining time for on-hook transfer in ms */
    int idletxhookstate;	/* default idle value for DR 64 */
    int lasttxhook;		/* last setting of DR 64 */
    int palarms;		/* # of power alarms so far */
    int reversepolarity;	/* reverse line for this board only */
#   if 0	/* not supported */
    int mwisendtype;
    struct dahdi_vmwi_info vmwisetting;
    int vmwi_active_messages;
    int vmwi_lrev:1;                /* mwi line reversal*/
    int vmwi_hvdc:1;                /* mwi high voltage DC idle line */
    int vmwi_hvac:1;                /* mwi neon high voltage AC idle line */
#   endif

    /* stuff for interfacing with dahdi core */
    struct dahdi_span		span;	/* span, see dahdi/kernel.h	*/
    struct dahdi_chan		*chans[1]; /* chan * to pass to dahdi fncts*/
    int				flags;	/* flags ???			*/
    /* nb: 'volatile' is probably needed here to avoid gcc optimizations,
     * because these two are manipulated by dahdi-base.c */
    volatile char		*readchunk;	/* r/w chunk pointers	*/
    volatile char		*writechunk;
};


/* initialization stages, see init_stage_str[] below for meanings */
enum init_stage {
    init_not_yet	=  0,
    sanity_check	=  1,
    quiesce_down	=  2,
    init_indregs	=  3,
    setup_dcconv	=  4,
    dcconv_power	=  5,
    pwrleak_test	=  6,
    adccalibrate	=  7,
    q56calibrate	=  8,
    lblcalibrate	=  9,
    finalization	= 10,
    initializeok	= 11,
    /* error states (while waiting to restart) */
    dcconvfailed	= 12,
    adccalfailed	= 13,
    q5q6clfailed	= 14,
    strtiofailed	= 15,
};

/* initialization stage strings */
static char *init_stage_str[] = {
/*   123456789012345678			*/
    "setup not started",
    "SLIC sanity check",
    "initial powerdown",
    "set indirect regs",
    "set dc-dc convrtr",
    "dc-dc cnv powerup",
    "VBAT pwrleak test",
    "ADC calibration",
    "Q5/6 calibration",
    "LBAL calibration",
    "final step setup",
    "up-and-running",
    "dc-dc cnv failed",
    "ADC  calbr failed",
    "Q5/6 calbr failed",
    "start PCM failed",
};

/* static variables */

static struct oufxs_dahdi *boards[OUFXS_MAX_BOARDS];	/* active dev structs */
static spinlock_t boardslock;	/* lock while manipulating boards 	*/

/* tables etc. */

#ifdef HWDTMF
static char slic_dtmf_table[] = {
  'D', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '*', '#', 'A', 'B', 'C'
};
#endif


/* table of devices that this driver handles */
static const struct usb_device_id oufxs_dev_table[] = {
    {USB_DEVICE (OUFXS_VENDOR_ID, OUFXS_PRODUCT_ID)},
    { }	/* terminator entry */
};

/* usb driver info and forward definitions of our function pointers therein */
static int oufxs_probe (struct usb_interface *,
  const struct usb_device_id *);
static void oufxs_disconnect (struct usb_interface *);
static struct usb_driver oufxs_driver = {
    .name	= "oufxs",
    .id_table	= oufxs_dev_table,
    .probe	= oufxs_probe,
    .disconnect	= oufxs_disconnect,
};


/* other forward function definitions */

static int start_stop_iov2 (struct oufxs_dahdi *, __u8);
static int read_direct (struct oufxs_dahdi *, __u8, __u8 *);
static int write_direct (struct oufxs_dahdi *, __u8, __u8, __u8 *);
static int sof_profile (struct oufxs_dahdi *);

/* macros and functions for manipulating and printing Si3210 register values */

#define dr_read(s,r,t,l)						\
  do {									\
    if ((s = read_direct (dev, r, &t)) != 0) {				\
      if (s != -ETIMEDOUT)						\
	OUFXS_DEBUG(OUFXS_DBGTERSE,				\
	  "%s: err %d reading reg %d", __func__, s, r);			\
      goto l;								\
    }									\
  } while(0)
#define dr_read_chk(s,r,v,t,l)						\
  do {									\
    if ((s = read_direct (dev, r, &t)) != 0) {				\
      if (s != -ETIMEDOUT)						\
	OUFXS_DEBUG(OUFXS_DBGTERSE,				\
	  "%s: err %d reading reg %d", __func__, s, r);			\
      goto l;								\
    }									\
    if (t != v) {							\
      OUFXS_DEBUG(OUFXS_DBGTERSE,"%s: rdr %d: exp %d, got %d",\
       __func__, r, v, t);						\
      goto l;								\
    }									\
  } while(0)
#define dr_write(s,r,v,t,l)						\
  do {									\
    if ((s = write_direct (dev, r, v, &t)) != 0) {			\
      if (s != -ETIMEDOUT)						\
	OUFXS_DEBUG(OUFXS_DBGTERSE,				\
	  "%s: err %d writing reg %d", __func__, s, r);			\
      goto l;								\
    }									\
  } while(0)
#define dr_write_check(s,r,v,t,l)					\
  do {									\
    if ((s = write_direct (dev, r, v, &t)) != 0) {			\
      if (s != -ETIMEDOUT)						\
	OUFXS_DEBUG(OUFXS_DBGTERSE,					\
	  "%s: err %d writing reg %d", __func__, s, r);			\
      goto l;								\
    }									\
    if (t != v) {							\
      OUFXS_DEBUG(OUFXS_DBGTERSE,"%s: wdr %d: exp %d, got %d",\
       __func__, r, v, t);						\
      goto l;								\
    }									\
  } while(0)
#define ir_read(s,r,t,l)						\
  do {									\
    if ((s = read_indirect (dev, r, &t)) != 0) {			\
      if (s != -ETIMEDOUT)						\
	OUFXS_DEBUG(OUFXS_DBGTERSE,					\
	  "%s: err %d reading ireg %d", __func__, s, r);		\
      goto l;								\
    }									\
  } while(0)
#define ir_write(s,r,v,t,l)						\
  do {									\
    if ((s = write_indirect (dev, r, v, &t)) != 0) {			\
      if (s != -ETIMEDOUT)						\
	OUFXS_DEBUG(OUFXS_DBGTERSE,					\
	  "%s: err %d writing ireg %d", __func__, s, r);		\
      goto l;								\
    }									\
  } while(0)


/* function to write-direct-register via piggybacking data into OUT packets */

static inline void dr_write_piggyback (struct oufxs_dahdi *dev, __u8 reg,
  __u8 val) {
    unsigned long flags;

    spin_lock_irqsave (&dev->outbuflock, flags);
    dev->drsseq++;
    dev->drsreg = reg;
    dev->drsval = val;
    spin_unlock_irqrestore (&dev->outbuflock, flags);
}


/* tell board to start/stop PCM I/O */
static int start_stop_iov2 (struct oufxs_dahdi *dev, __u8 val)
{
    __u8 seq = jiffies & 0xff;		/* a random value */
    union oufxs_packet req = START_STOP_IOV2_REQ(val, seq);
    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int length;
    int rlngth;
    int retval;

    /* version 2 of the command is used to pass an initia sequence #
     * to the board, to synchronize piggyback-based DR setting; now,
     * initialize the DR-setting stuff (since the isochronous engine
     * is not running yet, no locking of dev->outbuflock is required)
     */
    dev->drsseq = seq;
    dev->drsreg = 0xff;		/* non-existing register */
    dev->drsval = 0xff;

    rlngth = sizeof(req.strtstpv2_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) wrote %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    return 0;
}


/* query board's firmware version number (firmware must be >= svn rev 27) */
static int get_fmwr_version (struct oufxs_dahdi *dev, unsigned long *version)
{
    union oufxs_packet req = GET_FXS_VERSION_REQ ();
    union oufxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.fxsvsn_req);
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }

    rlngth = sizeof(rpl.fxsvsn_rpl);
    retval = usb_bulk_msg (dev->udev, in_pipe, &rpl, rlngth, &length, 1000);
    if (retval) {
	/* avoid issuing warnings on ETIMEDOUT; if board doesn't respond,
	 * we 'll get to fill out message rings, syslog files, etc. without
	 * any reason
	 */
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: board is running firmware version %d.%d.%d\n",
      __func__, rpl.fxsvsn_rpl.maj,
      rpl.fxsvsn_rpl.min, rpl.fxsvsn_rpl.rev);

    *version =	((unsigned long) rpl.fxsvsn_rpl.maj) << 16 |
		((unsigned long) rpl.fxsvsn_rpl.min) <<  8 |
		((unsigned long) rpl.fxsvsn_rpl.rev);

    return 0;
}


/* write serial number to the board's eeprom */
static int burn_serial (struct oufxs_dahdi *dev, unsigned long serial)
{
    __u8 b3 = (serial & 0xff000000) >> 24;
    __u8 b2 = (serial & 0x00ff0000) >> 16;
    __u8 b1 = (serial & 0x0000ff00) >>  8;
    __u8 b0 = (serial & 0x000000ff);
    union oufxs_packet req = WRITE_SERIAL_NO_REQ (b3, b2, b1, b0);
    union oufxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.serial_req);
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }

    rlngth = sizeof(rpl.serial_rpl);
    retval = usb_bulk_msg (dev->udev, in_pipe, &rpl, rlngth, &length, 1000);
    if (retval) {
	/* avoid issuing warnings on ETIMEDOUT; if board doesn't respond,
	 * we 'll get to fill out message rings, syslog files, etc. without
	 * any reason
	 */
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: burnt serial %02X%02X%02X%02X (got back %02X%02X%02X%02X)",
      __func__, b3, b2, b1, b0, rpl.serial_rpl.str[0], rpl.serial_rpl.str[1],
      rpl.serial_rpl.str[2], rpl.serial_rpl.str[3]);

    return 0;
}


/* instruct board to reboot in bootloader mode */
static int reboot_bootload (struct oufxs_dahdi *dev)
{
    union oufxs_packet req = REBOOT_BOOTLOADER_REQ ();
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);

    rlngth = sizeof(req.serial_req);
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }

    /* at this point, the device should reboot, so we will lose it */

    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: rebooting device in bootloader mode", __func__);

    return 0;
}

/* ProSLIC register I/O implementations */

 /* read_direct read a ProSLIC DR
 * @dev the struct usb_device instance
 * @reg the register to read
 * @value pointer to the return value
 * @return 0 on success, non-zero on failure
 */
static int read_direct (struct oufxs_dahdi *dev, __u8 reg, __u8 *value)
{
    union oufxs_packet req = PROSLIC_RDIRECT_REQ(reg);
    union oufxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.rdirect_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) wrote %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    rlngth = sizeof (rpl.rdirect_rpl);
    retval = usb_bulk_msg (dev->udev, in_pipe, &rpl, rlngth, &length, 1000);
    if (retval) {
	/* avoid issuing warnings on ETIMEDOUT; if board doesn't respond,
	 * we 'll get to fill out message rings, syslog files, etc. without
	 * any reason
	 */
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    *value = PROSLIC_RDIRECT_RPV (rpl);

    return 0;
}

/* write_direct write a ProSLIC DR
 * @dev the struct usb_device instance
 * @reg the register to read
 * @value value to set
 * @actval pointer to the return value
 * @return 0 on success, non-zero on failure
 */
static int write_direct (struct oufxs_dahdi *dev, __u8 reg, __u8 value,
  __u8 *actval)
{
    union oufxs_packet req = PROSLIC_WDIRECT_REQ(reg, value);
    union oufxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.wdirect_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) wrote %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    rlngth = sizeof (rpl.rdirect_rpl);
    retval = usb_bulk_msg (dev->udev, in_pipe, &rpl, rlngth, &length, 1000);
    if (retval) {
	/* avoid issuing warnings on ETIMEDOUT */
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    *actval = PROSLIC_WDIRECT_RPV (rpl);

    return 0;
}

/* read_indirect read a ProSLIC IR
 * @dev the struct usb_device instance
 * @reg the register to read
 * @value pointer to the return value
 * @return 0 on success, non-zero on failure
 */
static int read_indirect (struct oufxs_dahdi *dev, __u8 reg, __u16 *value)
{
    union oufxs_packet req = PROSLIC_RDINDIR_REQ(reg);
    union oufxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.rdindir_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) wrote %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    rlngth = sizeof (rpl.rdindir_rpl);
    retval = usb_bulk_msg (dev->udev, in_pipe, &rpl, rlngth, &length, 1000);
    if (retval) {
	/* avoid issuing warnings on ETIMEDOUT */
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    *value = PROSLIC_RDINDIR_RPV (rpl);

    return 0;

}
  

/* write_indirect write a ProSLIC IR
 * @dev the struct usb_device instance
 * @reg the register to write
 * @value value to set
 * @actval pointer to the return value
 * @return 0 on success, non-zero on failure
 */
static int write_indirect (struct oufxs_dahdi *dev, __u8 reg, __u16 value,
  __u16 *actval)
{
    union oufxs_packet req = PROSLIC_WRINDIR_REQ(reg, value);
    union oufxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.wrindir_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) wrote %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    rlngth = sizeof (rpl.wrindir_rpl);
    retval = usb_bulk_msg (dev->udev, in_pipe, &rpl, rlngth, &length, 1000);
    if (retval) {
	/* avoid issuing warnings on ETIMEDOUT */
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    *actval = PROSLIC_WRINDIR_RPV (rpl);

    return 0;
}

/* used in register dumps (3210-specific) */
static int is_valid_direct_register (const __u8 b)
{
    switch (b) {
      case 07: case 12: case 13: case 16: case 17: case 25: case 26: case 27:
      case 28: case 29: case 30: case 31: case 53: case 54: case 55: case 56:
      case 57: case 58: case 59: case 60: case 61: case 62: case 90: case 91:
      case 95: return false;
      default:
	if (b > 108) return false;
	return true;
    }
}

static int is_valid_indirect_register (const __u8 b)
{
    if (b <= 43) return true;
    if (b >= 99 && b <= 104) return true;
    return false;
}

















static int sof_profile (struct oufxs_dahdi *dev)
{
    int i;
    union oufxs_packet req = SOFPROFILE_REQ();
    union oufxs_packet rpl;
    int rlngth;
    int length;
    int retval;
    char *printbuf;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    printbuf = (char *) kzalloc (256, GFP_KERNEL);
    if (!printbuf) {
	OUFXS_ERR ("%s: out of memory", __func__);
        return -ENOMEM;
    }

    rlngth = sizeof(req.sofprof_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) wrote %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    rlngth = sizeof (rpl.sofprof_rpl);
    retval = usb_bulk_msg (dev->udev, in_pipe, &rpl, rlngth, &length, 1000);
    if (retval) {
	/* avoid issuing warnings on ETIMEDOUT */
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OUFXS_DEBUG (OUFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    OUFXS_INFO (
     " 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 | 09 | 10 | 11 | 12 | 13 | 14 | 15"
     );
    for (i = 1; i < 15; i++) {
        snprintf (&printbuf[5 * (i - 1)], 6, (i == 14)? "%04X ":"%04X|",
	  (__u16) (SOFPROFILE_TMRVAL(rpl.sofprof_rpl, i) -
	  SOFPROFILE_TMRVAL(rpl.sofprof_rpl, i - 1)));
    }
    OUFXS_INFO ("%s", printbuf);

    kfree (printbuf);
    return 0;
}
















static void dump_direct_regs (struct oufxs_dahdi *dev, char *msg)
{
    int c = 0;
    __u8 regval;
    char line[80];
    int status;
    __u8 i;

    /* spare us all the trouble if we are not going to produce any output */
    if (debuglevel < OUFXS_DBGVERBOSE) return;

    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
      "----3210 direct register hex dump (%s)----", msg);
    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
      "          0   1   2   3   4   5   6   7   8   9");

    for (i = 0; i < 110; i++) {
	if (c == 0) sprintf (line, "%2d0     ", i / 10);
	if (is_valid_direct_register (i)) {
	    status = read_direct (dev, i, &regval);
	    if (status != 0) {
		sprintf (&line[8+(c<< 2)], " XX\n");
		break;
	    }
	    else {
		sprintf (&line[8+(c<<2)], " %02X ", regval);
	    }
	}
	else {
	    sprintf (&line[8+(c<<2)], "    ");
	}
	if (c++ == 9) {
	    OUFXS_DEBUG (OUFXS_DBGVERBOSE, "%s", line);
	    c = 0;
	}
    }
    OUFXS_DEBUG (OUFXS_DBGVERBOSE, "----end of direct register dump----");
}

static void dump_indirect_regs (struct oufxs_dahdi *dev, char *msg)
{
    int c = 0;
    __u16 regval;
    char line[80];
    int status;
    __u8 i;

    /* spare us all the trouble if we are not going to produce any output */
    if (debuglevel < OUFXS_DBGVERBOSE) return;

    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
      "----3210 indirect register hex dump (%s)----", msg);
    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
      "          0     1     2     3     4     5     6     7     8     9");

    for (i = 0; i < 110; i++) {
	if (c == 0) sprintf (line, "%2d0     ", i / 10);
	if (is_valid_indirect_register (i)) {
	    status = read_indirect (dev, i, &regval);
	    if (status != 0) {
		sprintf (&line[8+(c<< 2)], " XXXX\n");
		break;
	    }
	    else {
		sprintf (&line[8+(c<<2)], " %04X ", regval);
	    }
	}
	else {
	    sprintf (&line[8+(c<<2)], "    ");
	}
	if (c++ == 9) {
	    OUFXS_DEBUG (OUFXS_DBGVERBOSE, "%s", line);
	    c = 0;
	}
    }
    OUFXS_DEBUG (OUFXS_DBGVERBOSE, "----end of indirect register dump----");
}


/* isochronous out packet submission */
static inline int oufxs_isoc_out_submit (struct oufxs_dahdi *dev, int memflag)
{
    int retval = 0;
    struct isocbuf *ourbuf;
    union oufxs_data *p;
    unsigned long flags;	/* irqsave */
    __u8  drsseq, drsreg, drsval;	/* sampled from dev-> equivalents */

    /* lock outbuf to update the state and sample drsxxx values */
    spin_lock_irqsave (&dev->outbuflock, flags);
    ourbuf = &dev->outbufs[dev->outsubmit];
    dev->outsubmit = (dev->outsubmit + 1) & (OUFXS_MAXURB - 1);
    /* sample drsxxx while we hold the lock */
    drsseq = dev->drsseq;
    drsreg = dev->drsreg;
    drsval = dev->drsval;
    spin_unlock_irqrestore (&dev->outbuflock, flags);

    /* from now on we are going to work with this buffer */
    spin_lock_irqsave (&ourbuf->lock, flags);

    /* refuse to re-submit if inconsistent (should *never* happen) */
    if (ourbuf->state != st_free) {
	if (!ourbuf->inconsistent) {
	    OUFXS_ERR("%s: oufxs%d: inconsistent state on outbuf %ld; aborting",
	      __func__, dev->slot + 1, (ourbuf - &dev->outbufs[0]));
	}
	ourbuf->inconsistent++;	/* only print message once per outbuf */
        spin_unlock_irqrestore (&ourbuf->lock, flags);
	return -EIO;
    }
    /* mark buffer as ours, so nobody else messes with it */
    ourbuf->state = st_subm;
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    for (p = (union oufxs_data *) ourbuf->buf;
      p < ((union oufxs_data *) ourbuf->buf) + wpacksperurb; p++) {
#ifdef DEBUGSEQMAP	/* make sure to remove in production */
	if (dev->seq2chunk [dev->outseqno] != p->outpack.sample) {
	    if (complaintimes) {
	        complaintimes--;
	        OUFXS_ERR (
		  "SERIOUS: seqno %d maps to %lx, found in %lx",
		  dev->outseqno,
		  (long unsigned int) dev->seq2chunk[dev->outseqno],
		  (long unsigned int) p->outpack.sample);
	    }
	}
#endif
        p->outpack.outseq = dev->outseqno++;
	dev->chans[0]->writechunk = p->outpack.sample;
	dahdi_transmit (&dev->span);
	p->outpack.drsseq = drsseq;
	p->outpack.drsreg = drsreg;
	p->outpack.drsval = drsval;
    }

    /* set the urb device (TODO: check if this is needed every time) */
    ourbuf->urb->dev = dev->udev;
    /* check if disconnect() has been called */
    if (!dev->udev) {
        retval = -ENODEV;
	goto isoc_out_submit_error;
    }

    // TODO: remove/bypass urb anchoring for kernels earlier than 2.6.23
    /* anchor the urb */
    usb_anchor_urb (ourbuf->urb, &dev->submitted);

    /* if executing in handler context, make sure device won't unload
     * while we are submitting urb to the usb core (we do this by
     * locking the device state lock, statelck); at the same time,
     * if device state is not OK, let the isochronous engine quiesce
     * down by not submitting any further urbs; note that it is safe
     * to hold a spinlock while submitting the urb in GFP_ATOMIC
     * context
     */
    if (memflag == GFP_ATOMIC) spin_lock_irqsave (&dev->statelck, flags);
    if (dev->state == OUFXS_STATE_OK) {
        retval = usb_submit_urb (ourbuf->urb, memflag);
    }
    if (memflag == GFP_ATOMIC) spin_unlock_irqrestore (&dev->statelck, flags);

    if (retval != 0) {
	// TODO: remove/bypass urb anchoring for kernels earlier than 2.6.23
        usb_unanchor_urb (ourbuf->urb);
	goto isoc_out_submit_error;
    }
    return 0;

isoc_out_submit_error:
    /* reset ourbuf->state to free again */
    spin_lock_irqsave (&ourbuf->lock, flags);
    ourbuf->state = st_free;
    spin_unlock_irqrestore (&ourbuf->lock, flags);
    return retval;
}

static inline int oufxs_isoc_in__submit (struct oufxs_dahdi *dev, int memflag)
{
    int retval = 0;
    struct isocbuf *ourbuf;
    unsigned long flags;	/* irqsave */

    /* lock outbuf to update the state */
    spin_lock_irqsave (&dev->in_buflock, flags);
    ourbuf = &dev->in_bufs[dev->in_submit];
    dev->in_submit = (dev->in_submit + 1) & (OUFXS_MAXURB - 1);
    spin_unlock_irqrestore (&dev->in_buflock, flags);

    /* from now on we are going to work with this buffer */
    spin_lock_irqsave (&ourbuf->lock, flags);

    /* refuse to re-submit if inconsistent (should *never* happen) */
    if (ourbuf->state != st_free) {
        if (!ourbuf->inconsistent) {
	    OUFXS_ERR("%s: oufxs%d: inconsistent state on in_buf %ld; aborting",
	      __func__, dev->slot + 1, (ourbuf - &dev->in_bufs[0]));
	}
	ourbuf->inconsistent++;
	spin_unlock_irqrestore (&ourbuf->lock, flags);
	return -EIO;
    }
    /* mark buffer as ours, so nobody else messes with it */
    ourbuf->state = st_subm;
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    /* set the urb device (TODO: check if this is needed every time) */
    ourbuf->urb->dev = dev->udev;
    /* check if disconnect() has been called */
    if (!dev->udev) {
        retval = -ENODEV;
	goto isoc__in_submit_error;
    }

    // TODO: remove/bypass urb anchoring for kernels earlier than 2.6.23
    /* anchor the urb */
    usb_anchor_urb (ourbuf->urb, &dev->submitted);

    /* if executing in handler context, make sure device won't unload
     * while we are submitting urb to the usb core (we do this by
     * locking the device state lock, statelck); at the same time,
     * if device state is not OK, let the isochronous engine quiesce
     * down by not submitting any further urbs; note that it is safe
     * to hold a spinlock while submitting the urb in GFP_ATOMIC
     * context
     */
    if (memflag == GFP_ATOMIC) spin_lock_irqsave (&dev->statelck, flags);
    if (dev->state == OUFXS_STATE_OK) {
        retval = usb_submit_urb (ourbuf->urb, memflag);
    }
    if (memflag == GFP_ATOMIC) spin_unlock_irqrestore (&dev->statelck, flags);

    if (retval != 0) {
        // TODO: remove/bypass urb anchoring for kernels earlier than 2.6.23
	usb_unanchor_urb (ourbuf->urb);
	goto isoc__in_submit_error;
    }
    return 0;

isoc__in_submit_error:
    /* reset ourbuf->state to free again */
    spin_lock_irqsave (&ourbuf->lock, flags);
    ourbuf->state = st_free;
    spin_unlock_irqrestore (&ourbuf->lock, flags);
    return retval;
}

/* isochronous out packet completion callback */
static void oufxs_isoc_out_cbak (struct urb *urb)
{
    struct oufxs_dahdi *dev;
    struct isocbuf *ourbuf;
    unsigned long flags;	/* irqsave */
    int ret;
    
    /* retrieve current outbuf and dev from context */
    ourbuf = urb->context;
    dev = ourbuf->dev;

    spin_lock_irqsave (&ourbuf->lock, flags);
    /* refuse to proceed if inconsistent (should *never* happen) */
    if (ourbuf->state != st_subm) {
	if (!ourbuf->inconsistent) {
	    OUFXS_ERR("%s: oufxs%d: inconsistent state on outbuf %ld; aborting",
	      __func__, dev->slot + 1, (ourbuf - &dev->outbufs[0]));
	}
	ourbuf->inconsistent++;	/* only print message once per outbuf */
	spin_unlock_irqrestore (&ourbuf->lock, flags);
	goto isoc_out_cbak_exit;
    }

    /* mark this buffer as free again */
    ourbuf->state = st_free;
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    /* re-submit a new urb */
    ret = oufxs_isoc_out_submit (dev, GFP_ATOMIC);
#if 0
    if (ret) {
        //TODO: add statistics for error counting here
    }
#endif

    /* note that this code executes once per wpacksperurb milliseconds */

    /* if ringing, set the 'idle' mode to oht (idle mode will be
     * applied when ringing stops -- see DAHDI_TXSIG_{ON,OFF}HOOK
     * hooksigs)
     */
    if (unlikely (dev->lasttxhook == 0x04)) {	/* ringing? */
	/* arm ohttimer */
        dev->ohttimer = OHT_TIMER / wpacksperurb;
	/* preset idle state linefeed mode to oht */
	dev->idletxhookstate = (reversepolarity ^ dev->reversepolarity)?
    	  0x06 : 0x02; 			/* rev/fwd oht linefeed	*/
    }
    /* if not ringing, then oht mode has been applied when ringing ended;
     * if timer is still running, decrement and check if it expired; if
     * so, revert the oht mode to normal rev/fwd active
     */
    else if (unlikely (dev->ohttimer)) {
	/* if ohttimer has expired, revert to normal rev/fwd active mode */
    	if (! --dev->ohttimer) {
	    dev->idletxhookstate = (reversepolarity ^ dev->reversepolarity)?
	      0x05 : 0x01;
	    /* if needed, apply the change to the effective linefeed mode */
	    if (dev->lasttxhook == 0x06 || dev->lasttxhook == 0x02) {
	        dev->lasttxhook = dev->idletxhookstate;
		dr_write_piggyback (dev, 64, dev->lasttxhook);
	    }
	}
    }

isoc_out_cbak_exit:
    return;
}


static void oufxs_isoc_in__cbak (struct urb *urb)
{
    struct oufxs_dahdi *dev;
    struct isocbuf *ourbuf;
    struct urb *oururb;
    unsigned long flags; /* irqsave */
    union oufxs_data *p;
    __u8 hook;
    int i;
    int ret;

    /* retrieve current in_buf and dev from context */
    ourbuf = urb->context;
    dev = ourbuf->dev;

    spin_lock_irqsave (&ourbuf->lock, flags);
    /* refuse to proceed if inconsistent (should *never* happen) */
    if (ourbuf->state != st_subm) {
        if (!ourbuf->inconsistent) {
	    OUFXS_ERR("%s: oufxs%d: inconsistent state on in_buf %ld; aborting",
	      __func__, dev->slot + 1, (ourbuf - &dev->in_bufs[0]));
	}
	ourbuf->inconsistent++; /* only print message once per in_buf */
	spin_unlock_irqrestore (&ourbuf->lock, flags);
	goto isoc_in__cbak_exit;
    }

    oururb = ourbuf->urb;
    for (i = 0; i < rpacksperurb; i++) {
	/* make sure packet was received fully and correctly before using */
	if (i < oururb->number_of_packets &&
	  oururb->iso_frame_desc[i].status == 0 &&
	  oururb->iso_frame_desc[i].actual_length == OUFXS_DPACK_SIZE) {

	    /* use p as a handy packet pointer */
	    p = ((union oufxs_data *)ourbuf->buf) + i;

	    /* prepare to pass new sample to dahdi */
	    dev->chans[0]->readchunk = p->in_pack.sample;

	    /* pass sample to echo canceller, matching it to the respective
	     * transmitted sample based on the mirrored sequence #
	     */
	    dahdi_ec_chunk (dev->chans[0], dev->chans[0]->readchunk,
	      dev->seq2chunk[p->in_pack.moutsn]);

	    /* perform the actual receive in dahdi */
	    dahdi_receive (&dev->span);
	    dev->prevrchunk = p->in_pack.sample;

	    /* hook/dtmf state only exists in odd packets */
	    if (p->in_pack.oddevn == 0xdd) {	/* odd packet */
	        __u8 hkdtmf = p->inopack.hkdtmf;
		/* hook state is hkdtmf bit 7 */
		hook = hkdtmf & 0x80;

		if (unlikely (hook != dev->lastrxhook)) {
		    /* hook states are different: start the debouncer */
		    dev->debounce = 32;		/* time in milliseconds */
		}
		else {
		    /* check if we are already debouncing a hook state change */
		    if (unlikely (dev->debounce > 0)) {
			dev->debounce--;	/* we are invoked once per ms */
			if (dev->debounce == 0) {	/* counted down to 0? */
			    dev->debouncehook = hook;
			}
		    }
		    /* if (debounced) hook state has changed, signal upstream */
		    if (unlikely (dev->oldrxhook != dev->debouncehook)) {
			if (dev->debouncehook) {
			    OUFXS_DEBUG (OUFXS_DBGDEBUGGING,
			      "oufxs%d going off-hook", dev->slot + 1);
			    dahdi_hooksig (dev->chans[0], DAHDI_RXSIG_OFFHOOK);
			}
			else {
			    dahdi_hooksig (dev->chans[0], DAHDI_RXSIG_ONHOOK);
			    OUFXS_DEBUG (OUFXS_DBGDEBUGGING,
			      "oufxs%d going on-hook", dev->slot + 1);
			}
			dev->oldrxhook = dev->debouncehook;
		    }
		}
		dev->lastrxhook = hook;
#ifdef HWHOOK
		dev->hook = hook;
#endif

		// TODO: check missing mirrored sequences, incr. out_missed

#ifdef HWDTMF	/* hardware DTMF detection is not used in Asterisk */

		/* dtmf status is bit 4 and digit is in bits 3, 2, 1 and 0 */
		hkdtmf &= 0x1f;
		/* implement a simple one-digit "latch" to hold dtmf value */
		if (unlikely (hkdtmf & 0x10)) {	/* if digit is being pressed */
		    if (!dev->dtmf) {
		        dev->dtmf = hkdtmf;
		    }
		}
		else {			/* reset if nothing is being pressed */
		    dev->dtmf = 0;
		}
#endif	/* HWDTMF */

		// TODO: check if we missed an input packet, update stats if so
	    }
	    else {			/* even packet */
		// TODO: check if we missed an input packet, update stats if so
	    }

	    // TODO: sequence checking, update statistics

	}
	/* we did not receive a correct packet: repeat previous one */
	else {
	    /* we must return something in any case; return previous sample */
	    dev->chans[0]->readchunk = dev->prevrchunk; /* needed?? */
	    dahdi_receive (&dev->span);
	    // TODO: update statistics
	}
    }

    /* mark this buffer as free again */
    ourbuf->state = st_free;
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    /* re-submit a new urb */
    ret = oufxs_isoc_in__submit (dev, GFP_ATOMIC);
#if 0
    if (ret) {
        //TODO: add statistics for error counting here
    }
#endif

isoc_in__cbak_exit:
    return;
}



/* destructor function: frees dev and associated memory, resets boards[] slot */
static void oufxs_delete (struct kref *kr)
{
    struct oufxs_dahdi *dev = container_of(kr, struct oufxs_dahdi, kref);
    int i;
    unsigned long flags;	/* irqsave flags */

    OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s(): starting", __func__);

    /* set state to UNLOAD, so others will see it */
    spin_lock_irqsave (&dev->statelck, flags);
    dev->state = OUFXS_STATE_UNLOAD;
    spin_unlock_irqrestore (&dev->statelck, flags);

    /* this should never happen, or else our module becomes a zombie */
    if (dev->opencnt) {
        OUFXS_WARN ("%s: called with opencnt non-zero!", __func__);
    }

    /* we are not expecting anything from dahdi, so unregister our span */
    dahdi_unregister (&dev->span);

    /* destroy board initialization thread workqueue, killing worker thread */
    if (dev->iniwq) {
	destroy_workqueue (dev->iniwq);
    }
    dev->iniwq = NULL;

    // TODO: remove/bypass urb anchoring for kernels earlier than 2.6.23
    /* kill anchored urbs */
    usb_kill_anchored_urbs (&dev->submitted);

    /* free static buffers and urbs */
    for (i = 0; i < OUFXS_MAXURB; i++) {
	if (dev->outbufs[i].buf) {
	    usb_buffer_free (dev->udev, OUFXS_MAXOBUFLEN,
	      dev->outbufs[i].buf, dev->outbufs[i].urb->transfer_dma);
	}
	if (dev->outbufs[i].urb) {
	    usb_free_urb (dev->outbufs[i].urb);
	}

	if (dev->in_bufs[i].buf) {
	    usb_buffer_free (dev->udev, OUFXS_MAXIBUFLEN,
	      dev->in_bufs[i].buf, dev->in_bufs[i].urb->transfer_dma);
	}
	if (dev->in_bufs[i].urb) {
	    usb_free_urb (dev->in_bufs[i].urb);
	}
    }

    /* return usb device to the usb core (usb code will also free mem. etc.) */
    if (dev->udev) {
	usb_put_dev (dev->udev);
    }
    dev->udev = NULL;

    /* clear slot in boards[] */
    spin_lock_irqsave (&boardslock, flags);
    boards[dev->slot] = NULL;
    spin_unlock_irqrestore (&boardslock, flags);

    /* kfree various stuff in dev */
    if (dev->chans[0]) kfree (dev->chans[0]);

    /* just a note to myself: each time I read this code I 'd better recheck
     * to make sure that EVERYTHING in dev-> has been kfree()d
     */

    kfree (dev);

    OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: finished", __func__);
}

static void inline at_init_stage (struct oufxs_dahdi *dev, enum init_stage x)
{
    safeprintf (dev->span.desc, "Open USB FXS board %d: %s", dev->slot + 1,
      init_stage_str[x]);
}

/* background work thread; this thread initializes board and exits when
 * everything goes right; if things fail, and depending on the compile
 * flags and retonXXX flags, it may loop forever trying to bring up or
 * calibrate a board
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
/* workqueue thread kernel API changed in 2.6.20 */
static void oufxs_setup (struct work_struct *work)
#else
static void oufxs_setup (void *data)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
    struct oufxs_dahdi *dev = container_of (work, struct oufxs_dahdi, iniwt);
#else
    struct oufxs_dahdi *dev = data;
#endif
    int	i;		/* generic counter */
    __u8 j;		/* counter, byte-sized */
    __u8 drval;		/* return value for DRs */
    __u16 irval;	/* return value for IRs */
    __u8 dr71;		/* for loopcurrent */
    int sts = 0;	/* status returned by {d,i}r_{read,write} */
    __u16 timeouts = 0;	/* # times board did not respond (initially) */
    __u16 cnvfail = 0;	/* # times dc-dc converter failed to powerup */
    __u16 adcfail = 0;	/* # times adc calibration failed */
    __u16 q56fail = 0;	/* # times q5/q6 calibration failed */
    unsigned long flags;	/* irqsave flags */
    unsigned long jifsthen;	/* jiffies */

    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
      "%s: oufxs%d: starting board setup procedure", __func__, dev->slot + 1);

    /* wait for USB initialization to settle */
    ssleep (1);


    /* handle serial numbers, burn one if required */
    if (!dev->udev->serial) {
        OUFXS_WARN ("%s: oufxs%d: old-version firmware not reporting serial",
	  __func__, dev->slot + 1);
	OUFXS_WARN ("%s: please upgrade firmware on board and replug",
	  __func__);
	return;
    }
    else if (!strncmp (dev->udev->serial, "EEEEEEEE", 8)) {
        OUFXS_WARN ("%s: oufxs%d: no serial on device's eeprom, burning one",
	  __func__, dev->slot + 1);
	// FIXME: locking
	sts = burn_serial (dev, jiffies); /* should be random enough */
        OUFXS_WARN ("%s: oufxs%d: serial %swritten OK, re-plug to %s",
	  __func__, dev->slot+1,
	  sts? "NOT":"", sts? "retry operation":"activate new serial");
	/* will not progress anyway */
	return;
    }
    else {
        OUFXS_INFO ("%s: oufxs%d: serial number is %s", __func__, dev->slot + 1,
	  dev->udev->serial);
    }


    /* never proceed with normal board initialization unless in idle state */
    spin_lock_irqsave (&dev->statelck, flags);
    if (dev->state != OUFXS_STATE_IDLE) {
        spin_unlock_irqrestore (&dev->statelck, flags);
	return;
    }

    /* flag we are starting with initialization */
    dev->state = OUFXS_STATE_INIT;
    dev->lastrxhook = 0;
#ifdef HWHOOK
    dev->hook = 0;
#endif
#ifdef HWDTMF
    dev->dtmf = 0;
#endif
    spin_unlock_irqrestore (&dev->statelck, flags);

init_restart:

    /* return if driver is unloading */
    if (dev->state == OUFXS_STATE_UNLOAD) {
        OUFXS_INFO ("%s: oufxs%d: returning on driver unload", __func__,
	  dev->slot + 1);
	return;
    }

    /* initialization step #1: board and ProSLIC sanity check */
    at_init_stage (dev, sanity_check);

    /* report consecutive timeouts */
    if (sts == -ETIMEDOUT) timeouts++;
    if (timeouts == 5) {
        OUFXS_ERR ("%s: oufxs%d: board not responding, will keep trying",
	  __func__, dev->slot + 1);
    }
    /* unless board responds and reports a sane DR11 value, loop back */
    dr_read_chk (sts, 11, 51, drval, init_restart);
    if (timeouts) {
        OUFXS_INFO ("%s: oufxs%d: board now responds OK again", __func__,
	  dev->slot + 1);
    }
    timeouts = 0;

    /* now we know board responds, handle the special case of SOF profiling */
    if (sofprofile > 0) {
        OUFXS_INFO ("%s: in (early) SOF profiling mode", __func__);
	for (i = 0; i < sofprofile; i++) {
	    sts = sof_profile (dev);
	    if (sts < 0) {
	        OUFXS_ERR ("%s: sof_profile() returned %d - profiling halted",
		  __func__, sts);
		break;
	    }
	}
	OUFXS_INFO ("%s: will not do further initializations in profile mode",
	  __func__);
	OUFXS_INFO ("%s: to initialize, set sofprofile to 0 and re-plug",
	  __func__);
	return;
    }



    /* advice to passers-by: initialization procedure follows closely
     * the guidelines of Silabs application note 35 (AN35.pdf); the
     * casual reader is advised to consult AN35 before attempting a
     * codewalk; although I 've tried to provide as many comments as
     * feasible, I am afraid that understanding what the code does
     * without having read the AN is rather impossible;
     */

    /* initialization step #2: quiesce down board */
    at_init_stage (dev, quiesce_down);

    /* take dc-dc converter down */
    dr_write_check (sts, 14, 0x10, drval, init_restart);
    /* set linefeed to open mode */
    dr_write_check (sts, 64, 0x00, drval, init_restart);
    
    /* initialization step #3: initialize all indirect registers */
    at_init_stage (dev, init_indregs);

    /* DTMF control registers */
    ir_write (sts,  0, 0x55c2, irval, init_restart); /* DTMF row 0 peak */
    ir_write (sts,  1, 0x51e6, irval, init_restart); /* DTMF row 1 peak */
    ir_write (sts,  2, 0x4b85, irval, init_restart); /* DTMF row 2 peak */
    ir_write (sts,  3, 0x4937, irval, init_restart); /* DTMF row 3 peak */
    ir_write (sts,  4, 0x3333, irval, init_restart); /* DTMF col 1 peak */
    ir_write (sts,  5, 0x0202, irval, init_restart); /* DTMF fwd twist */
    ir_write (sts,  6, 0x0202, irval, init_restart); /* DTMF reverse twist */
    ir_write (sts,  7, 0x0198, irval, init_restart); /* DTMF row ratio thresh.*/
    ir_write (sts,  8, 0x0198, irval, init_restart); /* DTMF col ratio thresh.*/
    ir_write (sts,  9, 0x0611, irval, init_restart); /* DTMF row 2nd arm */
    ir_write (sts, 10, 0x0202, irval, init_restart); /* DTMF col 2nd arm */
    ir_write (sts, 11, 0x00e5, irval, init_restart); /* DTMF power min thresh.*/
    ir_write (sts, 12, 0x0a1c, irval, init_restart); /* DTMF ot limit thresh.*/

    /* oscillator control registers */
    ir_write (sts, 13, 0x7b30, irval, init_restart); /* oscillator 1 coeff.*/
    ir_write (sts, 14, 0x0063, irval, init_restart); /* oscillator 1 X */
    ir_write (sts, 15, 0x0000, irval, init_restart); /* oscillator 1 Y */
    ir_write (sts, 16, 0x7870, irval, init_restart); /* oscillator 2 coeff.*/
    ir_write (sts, 17, 0x007d, irval, init_restart); /* oscillator 2 X */
    ir_write (sts, 18, 0x0000, irval, init_restart); /* oscillator 2 Y */
    ir_write (sts, 19, 0x0000, irval, init_restart); /* ring voltage off */
    ir_write (sts, 20, 0x7ef0, irval, init_restart); /* ring oscillator */
    ir_write (sts, 21, 0x0160, irval, init_restart); /* ring X */
    ir_write (sts, 22, 0x0000, irval, init_restart); /* ring Y */
    ir_write (sts, 23, 0x2000, irval, init_restart); /* pulse envelope */
    ir_write (sts, 24, 0x2000, irval, init_restart); /* pulse X */
    ir_write (sts, 25, 0x0000, irval, init_restart); /* pulse Y */

    /* digital programmable gain/attenuation control registers */
    ir_write (sts, 26, 0x2000, irval, init_restart); /* receive digital gain */
    ir_write (sts, 27, 0x4000, irval, init_restart); /* transmit digital gain */

    /* SLIC control registers */
    ir_write (sts, 28, 0x1000, irval, init_restart); /* loop close threshold */
    ir_write (sts, 29, 0x3600, irval, init_restart); /* ring trip threshold */
    ir_write (sts, 30, 0x1000, irval, init_restart); /* cmmode min. threshold */
    ir_write (sts, 31, 0x0200, irval, init_restart); /* cmmode max. threshold */
    /* power alarms are adapted for a 3201 board according to AN47, p.4 */
    ir_write (sts, 32, 0x0ff0, irval, init_restart); /* power alarm Q1Q2 */
    ir_write (sts, 33, 0x7f80, irval, init_restart); /* power alarm Q3Q4 */
    ir_write (sts, 34, 0x0ff0, irval, init_restart); /* power alarm Q5Q6 */
    /* by AN35, filter IRs 35--39 must be set to 0x8000 during this step */
    ir_write (sts, 35, 0x8000, irval, init_restart); /* loop closure filter */
    ir_write (sts, 36, 0x8000, irval, init_restart); /* ring trip filter */
    ir_write (sts, 37, 0x8000, irval, init_restart); /* thermal LP pole Q1Q2 */
    ir_write (sts, 38, 0x8000, irval, init_restart); /* thermal LP pole Q3Q4 */
    ir_write (sts, 39, 0x8000, irval, init_restart); /* thermal LP pole Q4Q5 */
    ir_write (sts, 40, 0x0000, irval, init_restart); /* cmmode bias ringing */
    ir_write (sts, 41, 0x0c00, irval, init_restart); /* DC-DC conv. min. volt.*/
    /* ?? marked as "reserved" in si3210 manual, but seen elsewhere to be set
     * to 0x1000 as "DC-DC extra" (extra what?); anyway, it's commented out here
    ir_write (sts, 42, 0x1000, irval, init_restart); */
    ir_write (sts, 43, 0x1000, irval, init_restart); /* loop close thres. low */

    /* initialization step #4: setup dc-dc converter parameters */
    at_init_stage (dev, setup_dcconv);

    dr_write_check (sts, 8, 0x00, drval, init_restart); /* exit dig. loopback */
    dr_write_check (sts, 108, 0xeb, drval, init_restart); /* rev.E features */
    dr_write (sts, 66, 0x01, drval, init_restart); /* Vov=low, Vbat~Vring */
    /* following six parameter values taken from SiLabs Excel formula sheet
     * (with a fixed inductor value of 100uH, NREN=1, dist=1000)
     */
    if (hifreq) {
        dr_write_check (sts, 92, 202, drval, init_restart); /* period=12.33us */
	dr_write_check (sts, 93, 0x0c, drval, init_restart);/* min off t=732ns*/
    }
    else {
	dr_write_check (sts, 92, 230, drval, init_restart); /* period=FIXME */
	dr_write_check (sts, 93, 0x19, drval, init_restart);/* min off t=FIXME*/
    }
    dr_write_check (sts, 74, 44, drval, init_restart); /* Vbat(high)=-66V */
    dr_write_check (sts, 75, 40, drval, init_restart); /* Vbat(low)=-60V */
    dr_write_check (sts, 71, 0, drval, init_restart); /* Cur. max=20mA (dflt) */
    /* ir 40 is already set above, so it's commented out here
    ir_write (sts, 40, 0x0000, irval, init_restart); /@ cmmode bias ringing */


    /* initialization step #5: power up dc-dc converter (sigh!...) */
    at_init_stage (dev, dcconv_power);

    dr_write_check (sts, 14, 0, drval, init_restart); /* bring up converter */
    for (i = 0; i < 10; i++) {
        dr_read (sts, 82, drval, init_restart); /* read sensed voltage */
	drval = drval * 376 / 1000;
	if (drval >= 60) goto init_dcdc_ok;
	OUFXS_DEBUG (OUFXS_DBGDEBUGGING,
	  "%s: oufxs%d: measured dc voltage is %d V", __func__,
	  dev->slot + 1, drval);
	msleep (8);	/* wait ~100ms 10x(8 here + 2 in dr_read()) */
	/* a short note containing wisdom (that was painfully gained):
	 * the voltage is sensed at the rate of the fsync pulse, and the
	 * converter circuitry uses the sensed value to update the dcdrv
	 * output of the chip; so, if for some reason, fsync does not
	 * pulse at the expected rate (32kHz), convergence can be much,
	 * much slower; thus, if the converter of an otherwise good board
	 * fails after some change in the firmware code, it's a good idea
	 * to make sure that the change did not somehow screw the fsync
	 * pulsing rate.
	 */
    }
    OUFXS_WARN ("%s: oufxs%d: converter did not power up", __func__,
      dev->slot + 1);
    /* converter failed to power up */
    if (cnvfail++ == 2) {
	dump_direct_regs (dev, "dc-dc converter failed");
        if (retoncnvfail) {
	    OUFXS_DEBUG (OUFXS_DBGTERSE,
	      "%s: oufxs%d: early exit on dc-dc converter failure (cnv=ON)",
	      __func__, dev->slot + 1);
	    spin_lock_irqsave (&dev->statelck, flags);
	    dev->state = OUFXS_STATE_ERROR;
	    spin_unlock_irqrestore (&dev->statelck, flags);
	    return;
	}
	OUFXS_ERR (
	  "%s: oufxs%d: dc-dc converter powerup failure, will keep trying",
	    __func__, dev->slot + 1);
    }
    /* powerdown immediately to avoid damaging the board */
    dr_write (sts, 14, 0x10, drval, init_restart);
    /* notify user by setting state */
    at_init_stage (dev, dcconvfailed);
    ssleep (2);
    goto init_restart;

init_dcdc_ok:
    cnvfail = 0;
    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
      "%s: oufxs%d: converter powerup OK", __func__, dev->slot + 1);

    /* initialization step #6: perform powerleak test */
    at_init_stage (dev, pwrleak_test);

    /* powerleak test consists in turning the converter off and monitoring
     * Vbat for 0.5 seconds; if the sensed voltage drops below 0.5V, this
     * probably means that there is a short circuit somewhere -- a very
     * useful test indeed; note that a failing leakage test does not
     * fail the board, only produces a warning
     */

    /* power down converter */
    dr_write (sts, 14, 0x10, drval, init_restart);

    /* note down current jiffies value */
    jifsthen = jiffies;

    do {
        dr_read (sts, 82, drval, init_restart); /* read sensed voltage	*/
	if (drval < 0x06) break;		/* 2.256 V		*/
	if ((jiffies - jifsthen) >= (HZ/2)) break;	/* 0.5 secs	*/
    } while (1);
    if (drval >= 0x06) {
        OUFXS_DEBUG (OUFXS_DBGVERBOSE,
	  "%s: oufxs%d: power leakage test passed with %d V",
	  __func__, dev->slot + 1, drval * 376 / 1000);
    }
    else {
        OUFXS_WARN("%s: oufxs%d: power leakage test failed - check for shorts!",
	  __func__, dev->slot + 1);
    }

    /* bring up converter again */
    dr_write_check (sts, 14, 0, drval, init_restart);
    msleep (100);		/* wait 100 ms for converter to come up */
    dr_read (sts, 82, drval, init_restart); /* read voltage, just in case... */
    drval = drval * 376 / 1000;
    if (drval < 60) {
        OUFXS_WARN("%s: oufxs%d: converter failed after leakage test?",
	  __func__, dev->slot + 1);
	goto init_restart;
    }


    /* prepare for calibrations: disable all interrupts */
    dr_write_check (sts, 21, 0, drval, init_restart); /* disable intrs in IE1 */
    dr_write_check (sts, 22, 0, drval, init_restart); /* disable intrs in IE2 */
    dr_write_check (sts, 23, 0, drval, init_restart); /* disable intrs in IE3 */
    dr_write_check (sts, 64, 0, drval, init_restart); /* set "open mode" LF */


    /* initialization step #7: perform ADC calibration */
    at_init_stage (dev, adccalibrate);

    /* monitor ADC calibration 1&2 but don't do DAC/ADC/balance calibration */
    dr_write_check (sts, 97, 0x1e, drval, init_restart); /* set cal. bits */
    /* start differential DAC, common-mode and I-LIM calibrations */
    dr_write_check (sts, 96, 0x47, drval, init_restart);
    /* wait for calibration to finish */
    for (i = 0; i < 10; i++) {
	dr_read (sts, 96, drval, init_restart); /* read calibration result */
	if (drval == 0) goto init_adccal_ok;
	msleep (200);
    }
    OUFXS_WARN ("%s: oufxs%d: ADC calibration did not finish", __func__,
      dev->slot + 1);

    if (adcfail++ == 2) {
	dump_direct_regs (dev, "adc calibration failed");
        if (retonadcfail) {
	    OUFXS_DEBUG (OUFXS_DBGTERSE,
	      "%s: oufxs%d: early exit on ADC calibration failure (cnv=ON)",
	      __func__, dev->slot);
	    spin_lock_irqsave (&dev->statelck, flags);
	    dev->state = OUFXS_STATE_ERROR;
	    spin_unlock_irqrestore (&dev->statelck, flags);
	    return;
	}
	else {
	    OUFXS_ERR ("%s: oufxs%d: ADC calibration failed, will keep trying",
	      __func__, dev->slot);
	}
    }
    /* notify user by setting state */
    at_init_stage (dev, adccalfailed);
    ssleep (2);
    goto init_restart;

init_adccal_ok:
    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
      "%s: oufxs%d: ADC calibration OK", __func__, dev->slot + 1);

    /* initialization step #8: perform q5/q6 current calibration
     * (note: since we are using the Si3210, manual calibration
     * is mandatory here)
     */
    at_init_stage (dev, q56calibrate);

    /* q5 current */
    for (j = 0x1f; j >= 0; j--) {
        dr_write (sts, 98, j, drval, init_restart);	/* adjust...	*/
        msleep (40);				/* ...let it settle...	*/
	dr_read (sts, 88, drval, init_restart);	/* ...and read current	*/
	if (drval == 0) break;			/* calibration ok	*/
    }
    if (drval != 0) {
        OUFXS_ERR ("%s: oufxs%d: q5 current calibration failed, dr88=%d",
	  __func__, dev->slot + 1, drval);
	goto init_q56cal_error;
    }
    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: oufxs%d: q5 current calibration ok at dr98=%d",
      __func__, dev->slot + 1, j);

    /* q6 current */
    for (j = 0x1f; j >= 0; j--) {
        dr_write (sts, 99, j, drval, init_restart);	/* adjust...	*/
        msleep (40);				/* ...let it settle...	*/
	dr_read (sts, 89, drval, init_restart);	/* ...and read current	*/
	if (drval == 0) break;			/* calibration ok	*/
    }
    if (drval != 0) {
        OUFXS_ERR ("%s: oufxs%d: q5 current calibration failed, dr89=%d",
	  __func__, dev->slot + 1, drval);
	goto init_q56cal_error;
    }
    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: oufxs%d: q5 current calibration ok at dr99=%d",
      __func__, dev->slot + 1, j);
    goto init_q56cal_ok;

init_q56cal_error:
    if (q56fail++ == 2) {
	dump_direct_regs (dev, "q5/q6 calibration failed");
        if (retonq56fail) {
	    OUFXS_DEBUG (OUFXS_DBGTERSE,
	      "%s: oufxs%d: early exit on Q5/Q6 calibration failure (cnv=ON)",
	      __func__, dev->slot);
	    spin_lock_irqsave (&dev->statelck, flags);
	    dev->state = OUFXS_STATE_ERROR;
	    spin_unlock_irqrestore (&dev->statelck, flags);
	    return;
	}
	else {
	    OUFXS_ERR("%s: oufxs%d: Q5/Q6 calibration failed, will keep trying",
	      __func__, dev->slot);
	}
    }
    /* notify user by setting state */
    at_init_stage (dev, q5q6clfailed);
    ssleep (2);
    goto init_restart;

init_q56cal_ok:

    /* initialization step #9: perform q5/q6 current calibration */
    at_init_stage (dev, lblcalibrate);

    /* dr64 is already 0 (open mode) */

    /* enable interrupt logic for on/off hook mode change during calibration */
    dr_write (sts, 23, 0x04, drval, init_restart);
    /* note: former openusbfxs driver included a test for hook state, which
     * always returned on-hook, even if the equipment was off-hook; thus, I
     * have removed this test from this version
     */
    // TODO: test is put back in to see if this is the cause for non-ringing
    dr_write (sts, 64, 0x01, drval, init_restart);	/* fwd active mode */
    ssleep (1);
    dr_read (sts, 68, drval, init_restart);		/* test hook state */
    if (drval & 0x01) {
        OUFXS_WARN ("__%s: oufxs%d: equipment is not on-hook?", __func__, 
	  dev->slot + 1);
    }
    dr_write (sts, 64, 0x00, drval, init_restart);	/* back to open mode */
    dr_write (sts, 97, 0x01, drval, init_restart);	/* set CALCM bit */
    dr_write (sts, 96, 0x40, drval, init_restart);	/* start calibration */
    while (1) {	/* loop waiting for calibration to complete */
        dr_read (sts, 96, drval, init_restart);
	if (drval == 0) break;
	msleep (200);
    }
    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: oufxs%d: longitu-dinal mode calibration OK", __func__, dev->slot + 1);

    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: oufxs%d: starting dc/dc calibration", __func__, dev->slot + 1);
    dr_write (sts, 93, hifreq? 0x8c:0x99, drval, init_restart);  /* calibrate */
    dr_read  (sts, 107, drval, init_restart);           /* check cal rslt */
    if (drval < 2 || drval > 13) {
        OUFXS_WARN (
          "%s: oufxs%d: dc/dc calibration yields dr107=%d!",
          __func__, dev->slot + 1, drval);
        dr_write (sts, 107, 0x08, drval, init_restart); /* write avg value */
    }
    OUFXS_DEBUG (OUFXS_DBGTERSE,
      "%s: oufxs%d: dc/dc calibration finished", __func__, dev->slot + 1);


    /* initialization step #10: perform final miscellaneous initializations */
    at_init_stage (dev, finalization);

    /* flush energy accumulators */
    for (j = 88; j <= 95; j++) {
        ir_write (sts, j, 0, irval, init_restart);
    }
    ir_write (sts, 97, 0, irval, init_restart);
    for (j = 194; j <= 211; j++) {
        ir_write (sts, j, 0, irval, init_restart);
    }

    /* clear all pending interrupts while no interrupts are enabled */
    dr_write (sts, 18, 0xff, drval, init_restart);
    dr_write (sts, 19, 0xff, drval, init_restart);
    dr_write (sts, 20, 0xff, drval, init_restart);

    /* enable selected interrupts */
    dr_write_check (sts, 21, 0x00, drval, init_restart); /* none here */
    dr_write_check (sts, 22, 0xff, drval, init_restart); /* all here */
    dr_write_check (sts, 23, 0x01, drval, init_restart); /* only dtmf here */

    /* set read and write PCM clock slots */
    for (j = 2; j <= 5; j++) {
        dr_write (sts, j, 0, drval, init_restart);
    }
    /* set DRs 63, 67, 69, 70 -- currently, all set to their default values */
    /* DR 63 (loop closure debounce interval for ringing silent period) */
    dr_write_check (sts, 63, 0x54, drval, init_restart);        /* 105 ms */
    /* DR 67 (automatic/manual control) */
    dr_write_check (sts, 67, 0x1f, drval, init_restart);        /* all auto */
    /* DR 69 (loop closure debounce interval) */
    dr_write_check (sts, 69, 0x0a, drval, init_restart);        /* 12.5 ms */
    /* DR 70 (ring trip debounce interval) */
    dr_write_check (sts, 70, 0x0a, drval, init_restart);        /* 12.5 ms */

    /* set DRs 65-66, 71-73 */
    /* 65 (external bipolar transistor control) left to default 0x61 */
    /* 66 (battery feed control) Vov/Track set during DC-DC converter powerup */
    /* 71 (loop current limit), defaults to 20mA */
    dr71 = (loopcurrent - 20) / 3;
    dr_write_check (sts, 71, dr71, drval, init_restart); /* max curr, 20-41mA */
    /* 72 (on-hook line voltage) left to default 0x20 (48V) */
    /* 73 (common-mode voltage) left to default 0x02 (3V) [0x04=6V in dahdi?]*/

    /* write indirect registers 35--39 */
    ir_write (sts, 35, 0x8000, irval, init_restart); /* loop closure filter */
    ir_write (sts, 36, 0x0320, irval, init_restart); /* ring trip filter */
    /* IRs 37--39 are set as per AN47 p.4 */
    ir_write (sts, 37, 0x0010, irval, init_restart); /* therm lp pole Q1Q2 */
    ir_write (sts, 38, 0x0010, irval, init_restart); /* therm lp pole Q3Q4 */
    ir_write (sts, 39, 0x0010, irval, init_restart); /* therm lp pole Q5Q6 */

    /* enable PCM I/O (PCME bit) and select PCM mu-law or a-law */
    dr_write_check (sts, 1, ((alawoverride)? 0x20 : 0x28), drval, init_restart);
    /* select slot 1 for TXS/RXS */
    dr_write_check (sts, 2, 0x01, drval, init_restart);	/* txs lowb set to 1 */
    dr_write_check (sts, 4, 0x01, drval, init_restart); /* rxs lowb set to 1 */

    /* fix low-power mode: on-hook voltage = 24V, ringer voltage = 50V */
    if (lowpower) {
	OUFXS_INFO ("%s: oufxs%d is being put in low-power mode", __func__,
	  dev->slot + 1);
        dr_write (sts, 72, 0x10, drval, init_restart);	/* on-hook V */
	ir_write (sts, 21, 0x0108, irval, init_restart);/* ringer peak*/
    }
    
    // TODO: add gain fixups and respective DR9 adjustments

    /* set default hook state mode (fwd/reverse active) */
    dev->idletxhookstate = (reversepolarity ^ dev->reversepolarity)?
      0x05 : 0x01; /* fwd/rev active */
    dev->lasttxhook = dev->idletxhookstate;
    dr_write (sts, 64, dev->lasttxhook, drval, init_restart);

    /* tell the board to start PCM I/O */
    sts = start_stop_iov2 (dev, 0);
    if (sts != 0) {
        OUFXS_ERR ("%s: start_stop_iov2 failed with %d", __func__, sts);
	at_init_stage (dev, strtiofailed);
	ssleep (2);
	goto init_restart;
    }

    dump_direct_regs (dev, "finally...");
    dump_indirect_regs (dev, "..finished");


    /* that's it, we 're done! */
    at_init_stage (dev, initializeok);


#if 0
    // DELETEME
    OUFXS_INFO ("%s: setting ALM1 loopback", __func__);
    dr_write_check (sts, 8, 0x00, drval, init_restart); /* exit dig. loopback */
#endif

    /* mark our state as OK, so others can open and use the device */
    spin_lock_irqsave (&dev->statelck, flags);
    dev->state = OUFXS_STATE_OK;
    spin_unlock_irqrestore (&dev->statelck, flags);

    /* start isochronous OUT transmits by spawining as many urbs as requested */
    for (i = 0; i < wurbsinflight; i++) {
        int __ret;
	__ret = oufxs_isoc_out_submit (dev, GFP_KERNEL);
	if (__ret < 0) {
	    OUFXS_ERR ("%s: isoc_submit(out) failed with %d", __func__, __ret);
	    // TODO: cancel possibly succeeded URBs and goto init_restart
	}
    }
    for (i = 0; i < rurbsinflight; i++) {
        int __ret;
	__ret = oufxs_isoc_in__submit (dev, GFP_KERNEL);
	if (__ret < 0) {
	    OUFXS_ERR ("%s: isoc_submit(in) failed with %d", __func__, __ret);
	    // TODO: cancel possibly succeeded URBs and goto init_restart
	}
    }

    /* now we have initialized board, handle special case of SOF profiling */
    if (sofprofile < 0) {
        OUFXS_INFO ("%s: in (late) SOF profiling mode", __func__);
	for (i = 0; i < -sofprofile; i++) {
	    sts = sof_profile (dev);
	    if (sts < 0) {
	        OUFXS_ERR ("%s: sof_profile() returned %d - profiling halted",
		  __func__, sts);
		break;
	    }
	}
	OUFXS_INFO ("%s: will not set state to OK in profile mode", __func__);
	OUFXS_INFO ("%s: to initialize, set sofprofile to 0 and re-plug",
	  __func__);
	return;
    }

    OUFXS_INFO ("%s: board %d has been fully setup", __func__, dev->slot + 1);
}


static int oufxs_open (struct dahdi_chan *chan)
{
    struct oufxs_dahdi *dev = chan->pvt;

    OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "open()");

    /* don't proceed unless channel points to a device */
    if (!dev) {	/* can this happen during oufxs_disconnect()? */
        OUFXS_ERR ("%s: no device data for chan %d", __func__, chan->chanpos);
	return -ENODEV;
    }

    /* don't proceed if the device state is not OK yet (or is unloading) */
    if (dev->state != OUFXS_STATE_OK) {
	if (dev->state == OUFXS_STATE_INIT) return -EAGAIN;
	if (!availinerror) {
	    return -EIO;
	}
    }

    /* increment driver-side usage count for device */
    kref_get (&dev->kref);

    /* we CANNOT use mutexes in here; dahdi_base calls us with the channel
     * lock held, so we risk going to sleep with a spinlock held and locking
     * up the kernel
    // mutex_lock (&dev->iomutex);
     */

    /* increment open count (safe, since we are called with the lock held) */
    dev->opencnt++;

    /* tell kernel our use count has increased, so as to prevent unloading us */
    try_module_get (THIS_MODULE);

    // TODO: reset usb I/O statistics

    return 0;
}

static int oufxs_close (struct dahdi_chan *chan)
{
    struct oufxs_dahdi *dev = chan->pvt;

    OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s(): starting", __func__);

    if (!dev) {	/* can this happen during oufxs_disconnect()? */
        OUFXS_ERR ("%s: no device data for chan %d", __func__, chan->chanpos);
	return -ENODEV;
    }

    /* help! does caller hold any sort of lock on us? */
    dev->opencnt--;

    /* decrement module use count */
    module_put (THIS_MODULE);
    
    /* reset idle tx hook state */
    dev->idletxhookstate = (reversepolarity ^ dev->reversepolarity)?
      0x05 : 0x01; /* fwd/rev active */

    /* decrement driver-side reference count */
    kref_put (&dev->kref, oufxs_delete);

    OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: finished", __func__);

    return (0);
}

/* handle PBX-originating hook state change signals; depending on the
 * signaling mode of the channel, we may set DR 64 to fwd/rev active
 * or to some other mode; note that I have removed the vmwi (voice
 * mail waiting indication) stuff to get this done with faster
 *
 * Important note: this function is called with the channel lock
 * held, so it must not sleep; hence, we use dr_write_piggyback
 * instead of dr_write
 */
static int oufxs_hooksig (struct dahdi_chan *chan, enum dahdi_txsig txsig)
{
    struct oufxs_dahdi *dev = chan->pvt;

    switch (txsig) {
      case DAHDI_TXSIG_ONHOOK:
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_TXSIG_ONHOOK", __func__);
	switch (chan->sig) {
	  case DAHDI_SIG_FXOKS:		/* kewl start	*/
	  case DAHDI_SIG_FXOLS:		/* loop start	*/
	  case DAHDI_SIG_EM:		/* ear & mouth	*/
	    dev->lasttxhook = dev->idletxhookstate;
	    break;
	  case DAHDI_SIG_FXOGS:		/* group start	*/
	    dev->lasttxhook = 3;	/* 3 is TIP open */
	    break;
	}
	break;
      case DAHDI_TXSIG_OFFHOOK:
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_TXSIG_OFFHOOK", __func__);
        switch (chan->sig) {
	  case DAHDI_SIG_EM:
	    dev->lasttxhook = 5;	/* 5 is reverse active */
	    break;
	  default:
	    dev->lasttxhook = dev->idletxhookstate;
	    break;
	}
	break;
      case DAHDI_TXSIG_START:
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_TXSIG_START", __func__);
        dev->lasttxhook = 4;
	break;
      case DAHDI_TXSIG_KEWL:
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_TXSIG_KEWL", __func__);
        dev->lasttxhook = 0;
	break;
      default:
        OUFXS_WARN ("canot set tx state to %d", txsig);
    }
    OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "setting fxs hook state to %d",
      dev->lasttxhook);
    dr_write_piggyback (dev, 64, dev->lasttxhook);
    return 0;
}

/* note: the path to our ioctl function is through the default:
 * label of dahdi_chanandpseudo_ioctl() in dahdi-base.c; in this
 * execution path, the channel lock is not held by dahdi-base, so
 * we are allowed to use mutexes or call sleeping macros like
 * dr_read()/dr_write() if needed (in any case, get_user() etc.
 * may sleep by themselves)
 */
static int oufxs_ioctl (struct dahdi_chan *chan, unsigned int cmd,
  unsigned long data)
{
    /* see note on including <wctdm_user.h> */
    struct wctdm_stats stats;
    struct wctdm_regs regs;
    struct oufxs_dahdi *dev;
    unsigned long version;
    unsigned long serial;
    int i;
    int sts;
    __u8 drval;
    int retval = 0;
    
    OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "ioctl()");

    dev = chan->pvt;
    if (!dev) {	/* can this happen during oufxs_disconnect()? */
        OUFXS_ERR ("%s: no device data for chan %d", __func__, chan->chanpos);
	return -ENODEV;
    }

    /* prevent disconnect() from proceeding while we are working; note
     * that this also synchronizes the dev->state check below, because
     * disconnect() changes the device state before attempting to lock
     * the mutex
     */
    mutex_lock (&dev->iomutex);

    /* check the device state and refuse to proceed if device is not yet
     * ready, is unloading, or is in error (unless availinerror is set) */
    if (!availinerror && (dev->state != OUFXS_STATE_OK)) {
	retval = (dev->state == OUFXS_STATE_INIT)? -EBUSY : -ENODEV;
	goto ioctl_exit;
    }

    switch (cmd) {
      /* dahdi-defined generic ioctls */

      case DAHDI_ONHOOKTRANSFER:	/* go into on-hook transfer mode */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_ONHOOKTRANSFER", __func__);
	/* data contains the milliseconds to hold oht mode */
        if (get_user (i, (__user int *) data)) {
	    retval = -EFAULT;
	    break;
	}
	dev->ohttimer = i;
	dev->idletxhookstate = (reversepolarity ^ dev->reversepolarity)?
    	  0x06 : 0x02; 			/* rev/fwd oht linefeed	*/
	/* if needed, apply the change to the effective linefeed mode */
	if (dev->lasttxhook == 0x05 || dev->lasttxhook == 0x01) {
	    dev->lasttxhook = dev->idletxhookstate;
	    dr_write (sts, 64, dev->lasttxhook, drval, ioctl_io_error);
	}
	break;


      case DAHDI_SETPOLARITY:		/* set line polarity to fwd/rev */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_SETPOLARITY", __func__);
	/* data is zero for normal and non-zero for reversepolarity */
        if (get_user (i, (__user int *) data)) {
	    retval = -EFAULT;
	    break;
	}
	i = (i)? 1 : 0;		/* normalize possible non-{0,1} values */

	/* make sure linefeed mode is not ringing or open */
	if (dev->lasttxhook == 0x04 || dev->lasttxhook == 0x00) {
	    retval = -EINVAL;
	    break;
	}
	dev->reversepolarity = i;
	if (reversepolarity ^ dev->reversepolarity) {
	    dev->lasttxhook |= 0x04;
	}
	else {
	    dev->lasttxhook &= ~0x04;
	}
	dr_write (sts, 64, dev->lasttxhook, drval, ioctl_io_error);
	break;


      case DAHDI_VMWI:		/* set visual message waiting indication */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_VMWI", __func__);
        /* we don't implement this */
	retval = -ENOTTY;
	break;


      case DAHDI_SET_HWGAIN:	/* set the hardware gain */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: DAHDI_SET_HWGAIN", __func__);
	/* this ioctl applies only to FXO modules */
	retval = -EINVAL;
	break;


      /* wctdm-specific ioctls, so we can cooperate with "fxstest" */

      case WCTDM_GET_STATS:	/* get statistics */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: WCTDM_GET_STATS", __func__);
	dr_read (sts, 80, drval,  ioctl_io_error);
	stats.tipvolt = (int) drval * -376;
	dr_read (sts, 81, drval, ioctl_io_error);
	stats.ringvolt= (int) drval * -376;
	dr_read (sts, 82, drval,  ioctl_io_error);
	stats.batvolt = (int) drval * -376;
	if (copy_to_user ((__user void *) data, &stats, sizeof (stats))) {
	    retval = -EFAULT;
	}
        break;


      case WCTDM_GET_REGS:	/* get values of all direct/indirect regs */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: WCTDM_GET_REGS", __func__);
	memset (&regs, 0, sizeof (regs));
        for (i = 0; i < NUM_REGS; i++) {	/* defined in "wctdm_user.h" */
	    if (is_valid_direct_register (i)) {
		dr_read (sts, i, regs.direct[i], ioctl_io_error);
	    }
	}
	for (i = 0; i < NUM_INDIRECT_REGS; i++) { /*defined in "wctdm_user.h" */
	    if (is_valid_indirect_register (i)) {
	        ir_read (sts, i, regs.indirect[i], ioctl_io_error);
	    }
	}
	if (copy_to_user ((__user void *) data, &regs, sizeof (regs))) {
	    retval = -EFAULT;
	}
	break;


      case WCTDM_SET_REG:	/* set a register to a given value */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: WCTDM_SET_REG", __func__);
        /* too dangerous to implement */
	retval = -ENOTTY;
	break;


      case WCTDM_SET_ECHOTUNE:	/* set echo tune */
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: WCTDM_SET_ECHOTUNE", __func__);
        /* only FXO modules have have that... */
        retval = -EINVAL;
	break;


      /* OUFXS-specific ioctls */

      case OUFXS_IOCRESET:	/* reset the board (unimplemented) */
        OUFXS_WARN ("%s: OUFXS_IOCRESET is not yet implemented", __func__);
	retval = -ENOTTY;
	break;


      case OUFXS_IOCREGDMP:	/* cause a register dump to be kprintf'ed */
	dump_direct_regs (dev, "ioctl-requested");
	dump_indirect_regs (dev, "ioctl-requested");
        break;


      case OUFXS_IOCSRING:	/* set ringing state (probably buggy) */
        if (data) {
	    oufxs_hooksig (dev->chans[0], DAHDI_TXSIG_START);
	}
	else {
	    oufxs_hooksig (dev->chans[0], DAHDI_TXSIG_OFFHOOK);
	}
	break;


      case OUFXS_IOCGHOOK:	/* get back the hook state */
#ifdef HWHOOK
        retval = __put_user (dev->hook? 1:0, (int __user *) data);
#else
        retval = __put_user (dev->oldrxhook? 1:0, (int __user *) data);
#endif
	break;


#ifdef HWDTMF
      case OUFXS_IOCGDTMF:	/* consume the last DTMF digit available */
        if (dev->dtmf && (dev->dtmf != 0xff)) {
	    retval = __put_user (
	      (int) slic_dtmf_table[dev->dtmf & 0xf],
	      (int __user *) data);
	    dev->dtmf = 0xff;	/* meaning, "digit consumed */
	}
	else {
	    retval = __put_user (0, (int __user *) data);
	}
	break;
#endif


      case OUFXS_IOCGVER:	/* get firmware version */
        retval = get_fmwr_version (dev, &version);
	if (retval == 0) {
	    retval = __put_user (version, (int __user *) data);
	}
	break;


      case OUFXS_IOCBURNSN:	/* burn a serial number on EEPROM */
	retval = burn_serial (dev, *((unsigned long *)data));
	break;


      case OUFXS_IOCBOOTLOAD:	/* reboot in bootloader mode */
        retval = reboot_bootload (dev);
	break;


      case OUFXS_IOCGSN:	/* get the current serial number */
	sscanf (dev->udev->serial, "%lx", &serial);
	retval = __put_user (serial, (int __user *) data);
	break;

      default:
        OUFXS_DEBUG (OUFXS_DBGDEBUGGING, "%s: %d", __func__, cmd);
        retval = -ENOTTY;
	break;
    }

ioctl_exit:
    mutex_unlock (&dev->iomutex);
    return retval;

ioctl_io_error:
    retval = -EIO;
    goto ioctl_exit;

}



static int oufxs_probe (struct usb_interface *intf,
  const struct usb_device_id *id)
{
    struct oufxs_dahdi *dev;
    struct usb_host_interface *intf_desc;
    struct usb_endpoint_descriptor *epd;
    size_t usbpcksize;
    char wqname[40];
    struct urb *urb;
    int retval = -ENODEV;
    int slot;
    int i, j;
    unsigned long flags;
    union oufxs_data *p;
    __u8 *pcm_silence;

    /* lock boards and search for an empty slot */
    spin_lock_irqsave (&boardslock, flags);	/* are usb probe()s atomic? */
    for (slot = 0; slot < OUFXS_MAX_BOARDS; slot++) {
        if (!boards[slot]) break;
    }
    if (slot == OUFXS_MAX_BOARDS) {
        OUFXS_WARN ("too many Open USB FXS boards!");
	spin_unlock_irqrestore (&boardslock, flags);
	return (-EIO);
    }
    /* mark slot as being used so other device instances won't touch
     * it and release lock before kzalloc; kzalloc may sleep, and holding a spinlock while sleeping is a bad idea
     */
    boards[slot] = (struct oufxs_dahdi *) 1;
    spin_unlock_irqrestore (&boardslock, flags);

    /* allocate a new struct oufxs_dahdi and point current boards slot to it */
    dev = kzalloc (sizeof (*dev), GFP_KERNEL);	/* we may be put to sleep */
    if (!dev) {
        OUFXS_ERR ("out of memory while allocating device state");
	goto probe_error;
    }
    boards[slot] = dev;
    dev->slot = slot;

    /* initialize device kernel ref count */
    kref_init (&dev->kref);
    
    /* initialize mutexes */
    mutex_init (&dev->iomutex);

    /* initialize other locks */
    spin_lock_init (&dev->statelck);

    /* initialize wait queues */

    /* initialize board's state variable */

    /* initialize usb stuff */
    // TODO: remove/bypass urb anchoring for kernels earlier than 2.6.23
    init_usb_anchor (&dev->submitted);
    dev->udev = usb_get_dev (interface_to_usbdev (intf));
    dev->intf = intf;

    /* setup endpoint information */

    /* TODO (some day): contrary to what the USB standard mandates, current
     * Open USB FXS descriptor lists isochronous endpoints with non-zero packet
     * sizes in the main configuration; to conform, we ought to list zero-size
     * packets for these endpoints in the main configuration and include one
     * alternate configuration with non-zero packet sizes, in which case, we
     * should also arrange this loop to look for the other alternative
     * configuration(s).
     * [NB: the purpose of this requirement in the standard is to eliminate
     * the possibility of the host failing the device in the enumeration
     * stage because of bandwidth lack in the overall USB bus; but since
     * Open USB FXS just eats 128kbps of isochronous bandwidth, chances that
     * this will happen are really very limited].
     */

    /* loop around endpoints in current int, noting their addresses and sizes */
    intf_desc = intf->cur_altsetting;
    for (i = 0; i < intf_desc->desc.bNumEndpoints; i++) {
        epd = &intf_desc->endpoint[i].desc;

	/* if we don't have yet a bulk IN EP and found one, mark it as ours */
	if (!dev->ep_bulk_in && usb_endpoint_is_bulk_in (epd)) {
	    usbpcksize = le16_to_cpu (epd->wMaxPacketSize);
	    dev->ep_bulk_in = epd->bEndpointAddress;
	    dev->bulk_in_size = usbpcksize;
	    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
	      "bulk IN  endpoint found, EP#%d, size:%d",
	      dev->ep_bulk_in, dev->bulk_in_size);
	}
	/* same for a bulk OUT EP */
	else if (!dev->ep_bulk_out && usb_endpoint_is_bulk_out (epd)) {
	    usbpcksize = le16_to_cpu (epd->wMaxPacketSize);
	    dev->ep_bulk_out = epd->bEndpointAddress;
	    dev->bulk_out_size = usbpcksize;
	    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
	      "bulk OUT endpont found, EP#%d, size:%d",
	      dev->ep_bulk_out, dev->bulk_out_size);
	}
	/* same for an isochronous IN EP */
	else if (!dev->ep_isoc_in && usb_endpoint_is_isoc_in (epd)) {
	    usbpcksize = le16_to_cpu (epd->wMaxPacketSize);
	    if (usbpcksize != sizeof (union oufxs_data)) {
	        OUFXS_ERR ("unexpected max packet size %ld in isoc-in ep",
		  usbpcksize);
		goto probe_error;
	    }
	    dev->ep_isoc_in = epd->bEndpointAddress;
	    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
	      "isoc IN  endpoint found, EP#%d, size:%ld",
	      dev->ep_isoc_in, usbpcksize);
	}
	/* same for an isochronous OUT EP */
	else if (!dev->ep_isoc_out && usb_endpoint_is_isoc_out (epd)) {
	    usbpcksize = le16_to_cpu (epd->wMaxPacketSize);
	    if (usbpcksize != sizeof (union oufxs_data)) {
	        OUFXS_ERR ("unexpected max packet size %ld in isoc-out ep",
		  usbpcksize);
		goto probe_error;
	    }
	    dev->ep_isoc_out = epd->bEndpointAddress;
	    OUFXS_DEBUG (OUFXS_DBGVERBOSE,
	      "isoc IN  endpoint found, EP#%d, size:%ld",
	      dev->ep_isoc_out, usbpcksize);
	}

	/* complain (but don't fail) on other, unknown EPs */
	else {
	    OUFXS_ERR ("Unexpected endpoint #%d", epd->bEndpointAddress);
	}
    }

    /* at the end of the loop, make sure we have found all four required EPs */
    if (!(dev->ep_bulk_in && dev->ep_bulk_out &&
      dev->ep_isoc_in && dev->ep_isoc_out )) {
        OUFXS_ERR ("board does not support all required bulk/isoc/int EPs??");
	goto probe_error;
    }

    /* tell to the power management of the kernel USB core that we don't
     * want to have the device autosuspended
     */
    retval = usb_autopm_get_interface (intf);
    if (retval) {
        OUFXS_ERR ("call to autopm_get_interface failed with %d", retval);
	goto probe_error;
    }

    /* save back-pointer to our oufxs_dahdi structure within usb interface */
    usb_set_intfdata (intf, dev);

    /* initialize all OUT isochronous buffers, urbs, etc. */
    dev->outsubmit = 0;
    spin_lock_init (&dev->outbuflock);
    for (i = 0; i < OUFXS_MAXURB; i++) {
	spin_lock_init (&dev->outbufs[i].lock);
        dev->outbufs[i].dev = dev;		/* back-pointer for context */
	/* allocate urb */
	dev->outbufs[i].urb = usb_alloc_urb (wpacksperurb, GFP_KERNEL);
	if (dev->outbufs[i].urb == NULL) {
	    OUFXS_ERR (
	      "%s: oufxs%d: out of memory while allocating isoc-out urbs",
	      __func__, dev->slot + 1);
	    retval = -ENOMEM;
	    goto probe_error;
	}
	/* allocate buffer for urb */
	dev->outbufs[i].buf = usb_buffer_alloc (dev->udev, OUFXS_MAXOBUFLEN,
	  GFP_KERNEL, &dev->outbufs[i].urb->transfer_dma);
	if (dev->outbufs[i].buf == NULL) {
	    OUFXS_ERR (
	      "%s: oufxs%d: out of memory while allocating isocs-out buffers",
	        __func__, dev->slot + 1);
	    retval = -ENOMEM;
	    goto probe_error;
	}
	memset (dev->outbufs[i].buf, 0, OUFXS_MAXOBUFLEN);
	dev->outbufs[i].state = st_free;
	dev->outbufs[i].inconsistent = 0;
	// dev->outbufs[i].len = 0;
	/* pre-initialize urb */
	urb = dev->outbufs[i].urb;
	urb->interval = 1;	/* send at every microframe */
	urb->dev = dev->udev;	/* chain urb to this device */
	urb->pipe = usb_sndisocpipe (dev->udev, dev->ep_isoc_out);
	urb->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
	urb->transfer_buffer = dev->outbufs[i].buf;
	urb->transfer_buffer_length = OUFXS_MAXOBUFLEN;
	urb->complete = oufxs_isoc_out_cbak;
	urb->context = &dev->outbufs[i]; 	/* unusual, but we need outbuf*/
	urb->start_frame = 0;
	urb->number_of_packets = wpacksperurb;
	for (j = 0; j < wpacksperurb; j++) {
	    urb->iso_frame_desc[j].offset = j * OUFXS_DPACK_SIZE;
	    urb->iso_frame_desc[j].length = OUFXS_DPACK_SIZE;
	}
    }

    /* initialize all IN isochronous buffers, urbs, etc. */
    dev->in_submit = 0;
    spin_lock_init (&dev->in_buflock);
    for (i = 0; i < OUFXS_MAXURB; i++) {
	spin_lock_init (&dev->in_bufs[i].lock);
        dev->in_bufs[i].dev = dev;		/* back-pointer for context */
	/* allocate urb */
	dev->in_bufs[i].urb = usb_alloc_urb (rpacksperurb, GFP_KERNEL);
	if (dev->in_bufs[i].urb == NULL) {
	    OUFXS_ERR (
	      "%s: oufxs%d: out of memory while allocating isoc-in urbs",
	      __func__, dev->slot + 1);
	    retval = -ENOMEM;
	    goto probe_error;
	}
	/* allocate buffer for urb */
	dev->in_bufs[i].buf = usb_buffer_alloc (dev->udev, OUFXS_MAXIBUFLEN,
	  GFP_KERNEL, &dev->in_bufs[i].urb->transfer_dma);
	if (dev->in_bufs[i].buf == NULL) {
	    OUFXS_ERR (
	      "%s: oufxs%d: out of memory while allocating isoc-in buffers",
	        __func__, dev->slot + 1);
	    retval = -ENOMEM;
	    goto probe_error;
	}
	memset (dev->in_bufs[i].buf, 0, OUFXS_MAXIBUFLEN);
	dev->outbufs[i].state = st_free;
	dev->outbufs[i].inconsistent = 0;
	// dev->in_bufs[i].len = 0;
	/* pre-initialize urb */
	urb = dev->in_bufs[i].urb;
	urb->interval = 1;	/* send at every microframe */
	urb->dev = dev->udev;	/* chain urb to this device */
	urb->pipe = usb_rcvisocpipe (dev->udev, dev->ep_isoc_in);
	urb->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
	urb->transfer_buffer = dev->in_bufs[i].buf;
	urb->transfer_buffer_length = OUFXS_MAXIBUFLEN;
	urb->complete = oufxs_isoc_in__cbak;
	urb->context = &dev->in_bufs[i]; 	/* unusual, but we need in_buf*/
	urb->start_frame = 0;
	urb->number_of_packets = rpacksperurb;
	for (j = 0; j < rpacksperurb; j++) {
	    urb->iso_frame_desc[j].offset = j * OUFXS_DPACK_SIZE;
	    urb->iso_frame_desc[j].length = OUFXS_DPACK_SIZE;
	}
    }

    for (i = j = 0, p = (union oufxs_data *)(&dev->outbufs[0])->buf;
      j <= 255; j++) {
	dev->seq2chunk [j] = p->outpack.sample;
	if ((++p - (union oufxs_data *)(&dev->outbufs[i])->buf)
	  == wpacksperurb) {
	    i = (i + 1) % OUFXS_MAXURB;
	    p = (union oufxs_data *)(&dev->outbufs[i])->buf;
	}
    }

    /* prepare a packet of "silence" to return to dahdi for as long as
     * we have not yet received data from the board (use the last
     * packet of the last IN urb to be submitted as a scratch area for
     * storing the actual sample)
     */

    pcm_silence = 
      ((union oufxs_data *) (&dev->in_bufs[OUFXS_MAXURB - 1].
      buf[(rpacksperurb - 1) * OUFXS_DPACK_SIZE]))->in_pack.sample;
    memset (pcm_silence, 0xff, OUFXS_CHUNK_SIZE);
    dev->prevrchunk = pcm_silence;

    /* initialize dahdi stuff */

    /* back-pointer to us */
    dev->span.pvt = dev;
    /* board identification and descriptions */
    safeprintf (dev->span.name, "OUFXS/%d", slot + 1);
    at_init_stage (dev, init_not_yet);	/* sets span.desc */
    safeprintf (dev->span.location, "USB %s device #%d",
      dev_name (&dev->udev->dev), dev->udev->devnum);
    dev->span.manufacturer = "Angelos Varvitsiotis";
    safeprintf (dev->span.devicetype, "Open USB FXS");
    /* a/mu-law */
    if (alawoverride) {
    	OUFXS_INFO ("oufxs%d: alawoverride; device will operate in A-law",
	  slot + 1);
	dev->span.deflaw = DAHDI_LAW_ALAW;
    }
    else {
    	dev->span.deflaw = DAHDI_LAW_MULAW;
    }
    /* channel initilization */
    dev->chans[0] = kzalloc (sizeof(struct dahdi_chan), GFP_KERNEL);
    safeprintf (dev->chans[0]->name, "OUFXS/%d", slot);
    dev->chans[0]->sigcap =
      DAHDI_SIG_FXOKS	| DAHDI_SIG_FXOLS	| DAHDI_SIG_FXOGS 	|
      DAHDI_SIG_SF	| DAHDI_SIG_EM		| DAHDI_SIG_CLEAR; 
    dev->chans[0]->chanpos = 1;
    dev->chans[0]->pvt = dev;
    dev->chans[0]->readchunk = pcm_silence;	/* just in case... */

    /* instead of registering a device driver under /dev through the USB
     * core (as we did in the original "openusbfxs" driver), at this point
     * we are registering our device with dahdi-base
     */
    dev->span.chans = dev->chans;
    dev->span.channels = 1;
    dev->span.hooksig = oufxs_hooksig;
    dev->span.open = oufxs_open;
    dev->span.close = oufxs_close;
    dev->span.flags = DAHDI_FLAG_RBS;	/* robbed bit signaling(dahdi-default)*/
    dev->span.ioctl = oufxs_ioctl;
    // can do without it? dev->span.watchdog = oufxs_watchdog;
    init_waitqueue_head (&dev->span.maintq);
    retval = dahdi_register (&dev->span, 0);
    if (retval) {
        OUFXS_ERR ("unable to register span with dahdi");
	goto probe_error;
    }

    /* create a unique name for our workqueue, then create the queue itself */
    sprintf (wqname, "oufxs%d", dev->slot + 1);
    dev->iniwq = create_singlethread_workqueue (wqname);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
    /* workqueue kernel API changed in 2.6.20 */
    INIT_WORK (&dev->iniwt, oufxs_setup);
#else
    INIT_WORK (&dev->iniwt, oufxs_setup, dev);
#endif
    if (!queue_work (dev->iniwq, &dev->iniwt)) {
        OUFXS_WARN ("%s: queue_work() returns 0 (already queued)??", __func__);
    }

    OUFXS_INFO ("Open USB FXS device %d starting", dev->slot);
    return (0);

probe_error:
    /* free static dma buffers and urbs */
    for (i = 0; i < OUFXS_MAXURB; i++) {
	if (dev->outbufs[i].buf) {
	    usb_buffer_free (dev->udev, OUFXS_MAXOBUFLEN,
	      dev->outbufs[i].buf, dev->outbufs[i].urb->transfer_dma);
	}
	if (dev->outbufs[i].urb) {
	    usb_free_urb (dev->outbufs[i].urb);
	}

	if (dev->in_bufs[i].buf) {
	    usb_buffer_free (dev->udev, OUFXS_MAXIBUFLEN,
	      dev->in_bufs[i].buf, dev->in_bufs[i].urb->transfer_dma);
	}
	if (dev->in_bufs[i].urb) {
	    usb_free_urb (dev->in_bufs[i].urb);
	}
    }

    if (dev) {
       /* decrement kernel ref count (& indirectly, call destructor function) */
       kref_put (&dev->kref, oufxs_delete);
    }

    /* reset current slot (needed here as well, if we set it to 1 before) */
    boards[slot] = NULL;	/* no locking; if race occurs it's not serious*/
    return retval;
}

static void oufxs_disconnect (struct usb_interface *intf)
{
    struct oufxs_dahdi *dev;
    unsigned long flags;		/* irqsave flags */
    int slot;
    
    OUFXS_DEBUG (OUFXS_DBGVERBOSE, "%s(): starting", __func__);

    /* get us a pointer to dev, then clear it from the USB interface */
    dev = usb_get_intfdata (intf);
    usb_set_intfdata (intf, NULL);

    /* save slot for the final printk (dev will have been freed/null by then)*/
    slot = dev->slot;

    /* tell USB core's power mgmt we don't need autosuspend turned off */
    usb_autopm_put_interface (intf);

    /* set state to UNLOAD, so others will see it */
    spin_lock_irqsave (&dev->statelck, flags);
    dev->state = OUFXS_STATE_UNLOAD;
    spin_unlock_irqrestore (&dev->statelck, flags);

    // TODO: wake up threads in wait queues
    // (see openusbfxs.c) so that they will see the new state and return
    // an error

    /* decrease the kref count to dev, eventually calling oufxs_delete */
    kref_put (&dev->kref, oufxs_delete);

    OUFXS_INFO ("oufxs device %d is now disconnected", slot + 1);
}

static int __init oufxs_init(void)
{
    int retval;
    int pwrof2;

    OUFXS_INFO ("oufxs driver v%s loading\n", driverversion);
    OUFXS_DEBUG (OUFXS_DBGTERSE, "debug level is %d", debuglevel);

    /* adjust the loop current */
    if (loopcurrent < 20 || loopcurrent > 41) {
        OUFXS_WARN ("loopcurrent=%d is out of range, resetting to 20mA",
	  loopcurrent);
	loopcurrent = 20;
    }
    else if (loopcurrent != 20) {	/* be quiet on default value */
        OUFXS_INFO ("loopcurrent set to %d mA", loopcurrent);
    }

    /* adjust {r,w}packsperurb to be powers of 2 */
    for (pwrof2 = 1; pwrof2 <= OUFXS_MAXPCKPERURB; pwrof2 <<= 1) {
	if (rpacksperurb > pwrof2) {
	    if (pwrof2 == OUFXS_MAXPCKPERURB) {
	        OUFXS_WARN ("rpacksperurb out of range, setting it to %d",
		  OUFXS_MAXPCKPERURB);
		rpacksperurb = OUFXS_MAXPCKPERURB;
		break;
	    }
	    continue;
	}
	if (rpacksperurb < pwrof2) {
	    OUFXS_INFO ("rpacksperurb adjusted to %d", pwrof2);
	    rpacksperurb = pwrof2;
	}
	/* the following break will also apply if rpacksperurb == pwrof2,
	 * only in that case we accept silently the user-supplied value
	 */
	break;
    }
    for (pwrof2 = 1; pwrof2 <= OUFXS_MAXPCKPERURB; pwrof2 <<= 1) {
	if (wpacksperurb > pwrof2) {
	    if (pwrof2 == OUFXS_MAXPCKPERURB) {
	        OUFXS_WARN ("wpacksperurb out of range, setting it to %d",
		  OUFXS_MAXPCKPERURB);
		wpacksperurb = OUFXS_MAXPCKPERURB;
		break;
	    }
	    continue;
	}
	if (wpacksperurb < pwrof2) {
	    OUFXS_INFO ("wpacksperurb adjusted to %d", pwrof2);
	    wpacksperurb = pwrof2;
	}
	/* the following break will also apply if wpacksperurb == pwrof2,
	 * only in that case we accept silently the user-supplied value
	 */
	break;
    }

    /* check/adjust the values of {r,w}urbsinflight */
    if (rurbsinflight < 2) {
        OUFXS_WARN ("rurbsinflight out of range (too low), setting it to 2");
	rurbsinflight = 2;
    }
    else if (rurbsinflight > OUFXS_MAXINFLIGHT) {
        OUFXS_WARN ("rurbsinflight out of range (too high), setting it to %d",
	  OUFXS_MAXINFLIGHT);
	rurbsinflight = OUFXS_MAXINFLIGHT;
    }
    if (wurbsinflight < 2) {
        OUFXS_WARN ("wurbsinflight out of range (too low), setting it to 2");
	wurbsinflight = 2;
    }
    else if (wurbsinflight > OUFXS_MAXINFLIGHT) {
        OUFXS_WARN ("wurbsinflight out of range (too high), setting it to %d",
	  OUFXS_MAXINFLIGHT);
	wurbsinflight = OUFXS_MAXINFLIGHT;
    }

    spin_lock_init (&boardslock);
    memset (boards, 0, sizeof (boards));

    retval = usb_register (&oufxs_driver);
    if (retval) {
        OUFXS_ERR ("usb_register failed, error=%d", retval);
    }

    // return values other than 0 are errors, see <linux/errno.h>

    return retval;
}

static void __exit oufxs_exit(void)
{
    usb_deregister (&oufxs_driver);
    OUFXS_INFO ("oufxs driver unloaded\n");
}

module_init(oufxs_init);
module_exit(oufxs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Angelos Varvitsiotis <avarvit-at-gmail-dot-com>");
MODULE_DESCRIPTION("Open USB FXS driver for dahdi telephony interface");
//MODULE_ALIAS
MODULE_DEVICE_TABLE (usb, oufxs_dev_table);
