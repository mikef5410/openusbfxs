/*
 *  openusbfxs.c: Linux kernel driver for the Open USB FXS board
 *  Copyright (C) <2009-2010>  Angelos Varvitsiotis
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
static char *driverversion = "0.0";

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
#include "openusbfxs.h"
#include "cmd_packet.h"

/* local #defines */

#define OPENUSBFXS_MAXIBUFLEN	(rpacksperurb * OPENUSBFXS_DPACK_SIZE)
#define OPENUSBFXS_MAXOBUFLEN	(wpacksperurb * OPENUSBFXS_DPACK_SIZE)

/* Note: under normal operation, the driver retries forever to initialize a
 * failing board; when debugging is turned on, this can generate tons of
 * warnings in case the board does not initialize properly; a default
 * hint is provided here with the RETONFAIL macro, so that, if debugging is
 * turned on, openusbfx_setup will return immediately on any board failure,
 * leaving the board in its current state (which btw is very helpful when
 * debugging the board's hardware). To override this behavior, various
 * retonXXXfail module parameters are provided, so that the driver can be
 * instructed to retry initialization even when debugging is turned on,
 * despite the large volume of debugging information thus logged;
 */
#ifdef DEBUGGING
#define RETONFAIL 1
#else  /* DEBUGGING */
#define RETONFAIL 0
#endif /* DEBUGGING */

#ifndef WPACKSPERURB	/* any value from 2 to OPENUSBFXS_MAXPCKPERURB */
#define WPACKSPERURB	4
#endif /* WPACKSPERURB */

#ifndef RPACKSPERURB	/* any value from 2 to OPENUSBFXS_MAXPCKPERURB */
#define RPACKSPERURB	4
#endif /* RPACKSPERURB */

#ifndef WURBSINFLIGHT	/* any value from 2 to OPENUSBFXS_MAXINFLIGHT */
#define WURBSINFLIGHT	OPENUSBFXS_INFLIGHT
#endif /* WURBSINFLIGHT */

#ifndef RURBSINFLIGHT	/* any value from 2 to OPENUSBFXS_MAXINFLIGHT */
#define RURBSINFLIGHT	OPENUSBFXS_INFLIGHT
#endif /* RURBSINFLIGHT */

/* module parameters (lots of debugging ones) */
static int  debuglevel	= OPENUSBFXS_DBGVERBOSE;
static uint expdr11	= 51;			/* value to expect in DR11 */
static int retoncnvfail= RETONFAIL;		/* quit on DC-DC conv fail */
static int retoncalfail= RETONFAIL;		/* quit on calibration1 fail */
static int retonlbcfail= RETONFAIL;		/* quit on lbcalibration fail */
static int wpacksperurb= WPACKSPERURB;		/* write packets per urb */
static int wurbsinflight=WURBSINFLIGHT;		/* # of write urbs in-flight */
static int rpacksperurb= RPACKSPERURB;		/* read packets per urb */
static int rurbsinflight=RURBSINFLIGHT;		/* # of read urbs in-flight */

module_param(debuglevel, int, S_IWUSR|S_IRUGO);
module_param(expdr11, uint, S_IRUGO);
module_param(retoncnvfail, bool, S_IWUSR|S_IRUGO);
module_param(retoncalfail, bool, S_IWUSR|S_IRUGO);
module_param(retonlbcfail, bool, S_IWUSR|S_IRUGO);
module_param(wpacksperurb, int, S_IRUGO);
module_param(wurbsinflight, int, S_IRUGO);
module_param(rpacksperurb, int, S_IRUGO);
module_param(rurbsinflight, int, S_IRUGO);

/* table of devices that this driver handles */
static const struct usb_device_id openusbfxs_dev_table [] = {
    {USB_DEVICE (OPENUSBFXS_VENDOR_ID, OPENUSBFXS_PRODUCT_ID)},
    { }		/* terminator entry */
};

static __u8	trash8;		/* variable to dump useless values	*/

/* openusbfxs device structure */
struct openusbfxs_dev {
    /* USB core stuff */
    struct usb_device		*udev;	/* usb device for this device	*/
    struct usb_interface	*intf;	/* usb interface for this device*/

    /* device descriptors and respective packet sizes */
    __u8			ep_bulk_in;	/* bulk IN  EP address	*/
    __u8			ep_bulk_out;	/* bulk OUT EP address	*/
    __u8			ep_isoc_in;	/* ISOC IN  EP address	*/
    __u8			ep_isoc_out;	/* ISOC OUT EP address	*/
    __u8			bulk_in_size;	/* bulk IN  packet size	*/
    __u8			bulk_out_size;	/* bulk OUT packet size	*/
    __u8			isoc_in_size;	/* ISOC IN  packet size	*/
    __u8			isoc_out_size;	/* ISOC OUT packet size	*/

    struct isocbuf {
        spinlock_t		lock;
	struct urb		*urb;
	char			*buf;
	enum state_enum {
	    /* the write/OUT state cycle is
	     *   empty->[writing->{written}]->submitting->submitted->empty
	     * the read/IN state cycle is
	     *   submitting->submitted->ready->[reading->empty]->submitting
	     * states enclosed in [] are optional: if not entered, this will
	     * result in data overrun (read) or underrun (write); the written
	     * state (enclosed in {}) is also optional: if not entered, the
	     * submit/callback thread will take the partially written
	     * buffer away from write(), without this resulting in an
	     * underrun condition (rather, it will result in a "short buffer"
	     * situation)
	     */
	    st_empty	= 0,	/* free to write or ("read" cycle) submit */
	    st_wrtng	= 1,	/* taken by write(), partially written */
	    st_wrttn	= 2,	/* fully written, ready to be submitted */
	    st_sbmng	= 3,	/* taken by submit */
	    st_sbmtd	= 4,	/* submitted for transmission */
	    st_ready	= 5,	/* ready for reading */
	    st_rding	= 6	/* taken by read() and being read */
	}			state;
	int			len;
	struct openusbfxs_dev	*dev;
    }				outbufs[OPENUSBFXS_MAXURB],
    				in_bufs[OPENUSBFXS_MAXURB];
    int writenext;				/* next buffer for write() */
    int outsubmit;				/* next wr buffer to submit */
    spinlock_t outbuflock;			/* short-term lock for above */
    wait_queue_head_t		outwqueue;	/* for write() to wait */
    char			tinywbuf[OPENUSBFXS_CHUNK_SIZE]; /* if < chunk*/
    int				twbcount;	/* bytes stored in tinywbuf */
    __u8			outseqno;	/* sequence number */

    int read_next;				/* next buffer for read() */
    int in_submit;				/* next rd buffer to submit */
    spinlock_t in_buflock;			/* short-term lock for above */
    wait_queue_head_t		in_wqueue;	/* for read() to wait */
    char			tinyrbuf[OPENUSBFXS_CHUNK_SIZE]; /* if < chunk*/
    int				trbcount;	/* bytes stored in tinyrbuf */
    int				trboffst;	/* start position in tinyrbuf */
    __u8			in_moutsn;	/* our outseqno mirrored */
    __u32			in_oofseq;	/* # of out-of-seq packets */
    __u8			in_seqevn;	/* even sequence # from device*/
    __u8			in_seqodd;	/* odd sequence # from device */

    struct openusbfxs_stats	stats;		/* from last request	*/

    /* anchor for submitted/pending urbs */
    struct usb_anchor		submitted;

    // TODO: add more stuff here as needed

    /* locks and reference counts */
    int				opencnt;/* number of openers		*/
    spinlock_t			statslck;/* locked while updating stats	*/
    spinlock_t			statelck;/* locked during state changes	*/
    struct kref			kref;	/* kernel/usb reference counts	*/
    struct mutex		iomutex;/* locked during fs I/O ops	*/
    struct mutex		rdmutex;/* locked during read I/O ops	*/
    struct mutex		wrmutex;/* locked during write I/O ops	*/

    /* wait queues */

    /* board state */
    int				state;	/* device state-see openusbfxs.h*/
    __u8			hook;	/* 0=on-hook, non-0=off-hook	*/
    __u8			dtmf;	/* 0=no dtmf, non-0=dtmf event	*/

    /* initialization worker thread pointer and queue */
    struct workqueue_struct	*iniwq;	/* initialization workqueue	*/
    struct work_struct		iniwt;	/* initialization worker thread	*/
};

static char slic_dtmf_table[] = {
  'D', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '*', '#', 'A', 'B', 'C'
};

static int start_stop_io (struct openusbfxs_dev *, __u8);
static int read_direct (struct openusbfxs_dev *, __u8, __u8 *);
static int write_direct (struct openusbfxs_dev *, __u8, __u8, __u8 *);

/* usb_driver info (and fwd definitions of our function pointers therein) */
static int openusbfxs_probe (struct usb_interface *,
  const struct usb_device_id *);
static void openusbfxs_disconnect (struct usb_interface *);
static struct usb_driver openusbfxs_driver = {
    .name	= "openusbfxs",
    .id_table	= openusbfxs_dev_table,
    .probe	= openusbfxs_probe,
    .disconnect	= openusbfxs_disconnect,
};

/* file ops structure (and forward function definitions) */
static int openusbfxs_open (struct inode *, struct file *);
static int openusbfxs_ioctl (struct inode *, struct file *, unsigned int,
  unsigned long);
static ssize_t openusbfxs_read (struct file *, char * __user, size_t, loff_t *);
static ssize_t openusbfxs_write (struct file *, const char * __user, size_t, loff_t *);
static int openusbfxs_release (struct inode *, struct file *);
static int openusbfxs_flush (struct file *, fl_owner_t id);

static struct file_operations openusbfxs_fops = {
    .owner	= THIS_MODULE,
    .open	= openusbfxs_open,
    .read	= openusbfxs_read,
    .write	= openusbfxs_write,
    .release	= openusbfxs_release,
    .flush	= openusbfxs_flush,
    .ioctl	= openusbfxs_ioctl
};

/* USB class driver information */
static struct usb_class_driver openusbfxs_class = {
    .name	= "openusbfxs%d",
    .fops	= &openusbfxs_fops,
    .minor_base	= OPENUSBFXS_MINOR_BASE
};

/* macros and functions for manipulating and printing Si3210 register values */

#define dr_read(s,r,t,l)						\
  do {									\
    if ((s = read_direct (dev, r, &t)) != 0) {				\
      if (s != -ETIMEDOUT)						\
	OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,				\
	  "%s: err %d reading reg %d", __func__, s, r);			\
      ssleep (2);							\
      goto l;								\
    }									\
  } while(0)
#define dr_read_chk(s,r,v,t,l)						\
  do {									\
    if ((s = read_direct (dev, r, &t)) != 0) {				\
      if (s != -ETIMEDOUT)						\
	OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,				\
	  "%s: err %d reading reg %d", __func__, s, r);			\
      ssleep (2);							\
      goto l;								\
    }									\
    if (t != v) {							\
      OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,"%s: rdr %d: exp %d, got %d",\
       __func__, r, v, t);						\
      ssleep (2);							\
      goto l;								\
    }									\
  } while(0)
#define dr_write(s,r,v,t,l)						\
  do {									\
    if ((s = write_direct (dev, r, v, &t)) != 0) {			\
      if (s != -ETIMEDOUT)						\
	OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,				\
	  "%s: err %d writing reg %d", __func__, s, r);			\
      ssleep (2);							\
      goto l;								\
    }									\
  } while(0)
#define dr_write_check(s,r,v,t,l)					\
  do {									\
    if ((s = write_direct (dev, r, v, &t)) != 0) {			\
      if (s != -ETIMEDOUT)						\
	OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,				\
	  "%s: err %d writing reg %d", __func__, s, r);			\
      ssleep (2);							\
      goto l;								\
    }									\
    if (t != v) {							\
      OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,"%s: wdr %d: exp %d, got %d",\
       __func__, r, v, t);						\
      ssleep (2);							\
      goto l;								\
    }									\
  } while(0)
#define ir_write(s,r,v,t,l)						\
  do {									\
    if ((s = write_indirect (dev, r, v, &t)) != 0) {			\
      if (s != -ETIMEDOUT)						\
	OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,				\
	  "%s: err %d writing ireg %d", __func__, s, r);		\
      ssleep (2);							\
      goto l;								\
    }									\
  } while(0)


/* "destructor" function: frees "dev" and all its associated memory */
static void openusbfxs_delete (struct kref *kr)
{
    struct openusbfxs_dev *dev = container_of (kr, struct openusbfxs_dev, kref);
    int i;
    unsigned long flags;	/* irqsave flags */

    /* tell our board setup worker thread to exit */
    spin_lock_irqsave (&dev->statelck, flags);
    dev->state = OPENUSBFXS_STATE_UNLOAD;
    spin_unlock_irqrestore (&dev->statelck, flags);

    /* wake up any blocked reads or writes */
    wake_up_interruptible (&dev->in_wqueue);
    wake_up_interruptible (&dev->outwqueue);

    /* destroy board setup work queue */
    destroy_workqueue (dev->iniwq);	/* kills worker thread as well */
    dev->iniwq = NULL;

    /* free static buffers and urbs (is this needed or does put_dev do it?) */
    for (i = 0; i < OPENUSBFXS_MAXURB; i++) {
        usb_buffer_free (dev->udev, OPENUSBFXS_MAXOBUFLEN,
	  dev->outbufs[i].buf, dev->outbufs[i].urb->transfer_dma);
	usb_free_urb (dev->outbufs[i].urb);

        usb_buffer_free (dev->udev, OPENUSBFXS_MAXIBUFLEN,
	  dev->in_bufs[i].buf, dev->in_bufs[i].urb->transfer_dma);
	usb_free_urb (dev->in_bufs[i].urb);
    }

    /* return usb device to core and free any associated memory etc. */
    usb_put_dev (dev->udev);

    //TODO: make sure we have kfree()d everything in dev->
    
    kfree (dev);
}


#if 0 // currently unused
/* "drainout" function: waits for anchored URBs, and eventually cancels them */
static void openusbfxs_drain_urbs (struct openusbfxs_dev *dev)
{
    int time;
    
    /* wait one second to let pending urbs drain, then cancel
     * any remaining pending writes
     */
    time = usb_wait_anchor_empty_timeout (&dev->submitted, 1000);
    if (!time) {
        usb_kill_anchored_urbs (&dev->submitted);
    }
}
#endif

/* open() implementation */
static int openusbfxs_open (struct inode *inode, struct file *file)
{
    struct openusbfxs_dev *dev;
    struct usb_interface *intf;
    int minor;
    unsigned long flags;	/* irqsave flags */
    int retval = -ENODEV;

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING, "open()");

    minor = iminor (inode);
    intf = usb_find_interface (&openusbfxs_driver, minor);
    if (!intf) { /* e.g. if a minor was left un-deregistered? */
        OPENUSBFXS_ERR ("%s error: cannot find device for minor %d",
	  __func__, minor);
	goto open_error;
    }
    dev = usb_get_intfdata (intf);
    if (!dev) {	/* might happen while we are executing openusb_disconnect() */
    	OPENUSBFXS_ERR ("%s error: no device data for minor %d - disconnected?",
	  __func__, minor);
	goto open_error;
    }

    /* return -EAGAIN if the device is there but is not ready (yet) */
    if (dev->state != OPENUSBFXS_STATE_OK) {
	return (dev->state == OPENUSBFXS_STATE_ERROR)? -EIO : -EAGAIN;
    }

    /* increment (driver-side) usage count for the device */
    kref_get (&dev->kref);

    /* lock IO mutex in order to access dev atomically (we may sleep) */
    mutex_lock (&dev->iomutex);

    /* implement exclusive open (probably insufficient if multithreading) */
    if (dev->opencnt) {
    	retval = -EBUSY;
	mutex_unlock (&dev->iomutex);
	kref_put (&dev->kref, openusbfxs_delete);
	goto open_error;
    }
    dev->opencnt++;

    /* save dev in file's private structure, unlock and return success */
    file->private_data = dev;
    mutex_unlock (&dev->iomutex);

    /* set linefeed to forward active mode */
    // retval = write_direct (dev, 64, 0x01, &trash8);
    retval = 0;

    /* reset stats; this has to be here and not in dev initialization,
     * because initial mismatch in sequence numbers etc. causes a first
     * set of packets to be reported as errors (missed etc.);
     */
    spin_lock_irqsave (&dev->statslck, flags);
    memset (&dev->stats, 0, sizeof (struct openusbfxs_stats));
    spin_unlock_irqrestore (&dev->statslck, flags);

open_error:
    return retval;
}

static int openusbfxs_ioctl (struct inode *inode, struct file *file,
  unsigned int cmd, unsigned long arg)
{
    struct openusbfxs_dev *dev;
#if 0
    __u8 drval;
#endif
    int retval = 0;

    dev = file->private_data;

    /* consistency checks */
    if (_IOC_TYPE(cmd) != OPENUSBFXS_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > OPENUSBFXS_MAX_IOCTL) return -ENOTTY;

    if (_IOC_DIR(cmd) & _IOC_READ) {
        if (!access_ok (VERIFY_WRITE, (void __user *) arg, _IOC_SIZE(cmd))) {
	    return -EFAULT;
	}
    }

    mutex_lock (&dev->iomutex);	/* avoid unload while we are working */
    
    if (dev->state != OPENUSBFXS_STATE_OK) {	/* return immediately on error*/
        mutex_unlock (&dev->iomutex);
	return -ENODEV;		/* device has gone away */
    }

    switch (cmd) {
      // TODO: implement unimplemented IOCRESET and IOCREGDMP ioctls
      case OPENUSBFXS_IOCRESET:
      case OPENUSBFXS_IOCREGDMP:
	OPENUSBFXS_WARN ("%s: ioctl %d is not yet implemented", __func__, cmd);
	break;
      case OPENUSBFXS_IOCSRING:
        if (arg) {
	    retval = write_direct (dev, 64, 0x04, &trash8);
	}
	else {
	    retval = write_direct (dev, 64, 0x01, &trash8);
	}
	break;
      case OPENUSBFXS_IOCSLMODE:
        if (arg) {
	    retval = write_direct (dev, 64, 0x01, &trash8);
	}
	else {
	    retval = write_direct (dev, 64, 0x00, &trash8);
	}
	break;
      case OPENUSBFXS_IOCGHOOK:
#if 0
	retval = read_direct (dev, 68, &drval);
	if (retval < 0) break;	/* pass on errors from read_direct */
        retval = __put_user ((drval & 0x01), (int __user *) arg);
#else
        retval = __put_user (dev->hook? 1:0, (int __user *) arg);
#endif
	break;
      case OPENUSBFXS_IOCGDTMF:
	if (dev->dtmf && (dev->dtmf != 0xff)) {
	    retval = __put_user (
	      (int) slic_dtmf_table[dev->dtmf & 0xf],
	      (int __user *) arg);
	    /* avoid re-issuing the same digit until user releases key */
	    dev->dtmf = 0xff;
	}
	else {
	    retval = __put_user (0, (int __user *) arg);
	}
	break;
      case OPENUSBFXS_IOCGSTATS:
        if (copy_to_user ((struct openusbfxs_stats __user *) arg,
	  &dev->stats, sizeof (struct openusbfxs_stats))) {
	    retval = -EFAULT;
	    OPENUSBFXS_ERR ("%s: copy_to_user failed", __func__);
	}
        break;
      default:
	retval = -ENOTTY;
	break;	/* will return 'inappropriate ioctl for device' */
    }

    mutex_unlock (&dev->iomutex);
    return retval;
}

static ssize_t openusbfxs_read (struct file *file, char __user *ubuf,
  size_t count, loff_t *ppos)
{
    struct openusbfxs_dev *dev;	/* our device structure */
    int retval = 0;		/* # of bytes read or error */
    char *buf = NULL;		/* allocated buffer to hold copy of data */
    size_t actual;		/* bytes actually read */
    struct isocbuf *ourbuf;	/* sampled current buffer */
    int buflen;			/* sampled buffer length */
    unsigned long flags;	/* irqsave flags */
    char *ptr;
    size_t subchunk;

    dev = file->private_data;

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING, "read(fp,buf,%d)", (int) count);

    /* make sure board state is still OK */
    if (dev->state != OPENUSBFXS_STATE_OK) return -EIO;

    /* prevent others from attempting concurrent read()s (we may sleep) */
    mutex_lock (&dev->rdmutex);

    /* finish off quickly with the zero-count case */
    if (!count) goto read_exit;

    /* deal with the trivial case where a small amount of data is requested
     * which is already pre-buffered in tinyrbuf
     */
    if (count <= dev->trbcount) {
        if (copy_to_user (ubuf, dev->tinyrbuf + dev->trboffst, count)) {
	    retval = -EFAULT;
	    OPENUSBFXS_ERR ("%s: copy_to_user failed", __func__);
	    goto read_exit;
	}
	dev->trbcount -= count;
	dev->trboffst += count;
	dev->trboffst %= sizeof (dev->tinyrbuf);
	retval = count;
	goto read_exit;
    }

    /* note: from this point on we know that count > dev->trbcount */

    /* allocate a local buffer for de-packetization of data */
    buf = kmalloc (count, GFP_KERNEL);
    if (!buf) {
        retval = -ENOMEM;
	OPENUSBFXS_ERR ("%s: out of memory", __func__);
	goto read_exit;
    }

    if (dev->trbcount) {
	memcpy (buf, dev->tinyrbuf + dev->trboffst, dev->trbcount);
    }
    count -= dev->trbcount;		/* subtract "debt" we already payed */

    ptr = buf + dev->trbcount;		/* initialize destination pointer */
    actual = dev->trbcount;		/* initialize # of data already read */

    dev->trbcount = 0;			/* reset tinyrbuf count and index */
    dev->trboffst = 0;

read_findbuf:

    /* account for data read during previous rounds (this is needed
     * here only for the O_NONBLOCK case where we return the
     * current retval immediately)
     */
    retval += actual;
    count -= actual;

    actual = 0;				/* reinitialize for the new round */

    /* we are going to need a new buffer here anyway; submit() may have
     * moved the 'readnext' index since last round, so we (re-)sample the
     * currently-indexed read buffer into ourbuf; no locking is
     * necessary, since we check the buffer state later on using locking
     */
    ourbuf = &dev->in_bufs[dev->read_next];

    /* we know there is no data if we (have wrapped around and) hit
     * a buffer which is either empty* or taken by submit; in this
     * case, we either return immediately (if O_NONBLOCK is set),
     * or block waiting for the urb completion callback to wake us
     * when some data become available
     * ---
     * * : the "empty" case may occur only in error situations
     */
    if (file->f_flags & O_NONBLOCK) {
        if (ourbuf->state < st_ready) {
	    /* return zero or the number of bytes already read */
	    goto read_copy_exit;
	}
    }
    else {
        if (wait_event_interruptible (dev->in_wqueue,
	  ((ourbuf->state >= st_ready) ||
	   (dev->state != OPENUSBFXS_STATE_OK) ||
	   (ourbuf != &dev->in_bufs[dev->read_next])))) {
	    retval = -ERESTARTSYS;
	    goto read_exit;
	}

	/* make sure we are not unloading or something */
	if (dev->state != OPENUSBFXS_STATE_OK) {
	    retval = -ENODEV;	/* device has gone away */
	    goto read_exit;
	}

	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
	  "%s: after wait_event", __func__);

	/* if submit() has moved the read index, resample */
	if (ourbuf != &dev->in_bufs[dev->read_next]) {
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
	      "%s: read index moved", __func__);
	    goto read_findbuf;
	}
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
	  "%s: buffer %d state %d", __func__, 
	    (int) (ourbuf - &dev->in_bufs[0]),
	    ourbuf->state);
    }

read_newchunk:
    /* account for data read during previous newchunk round */
    retval += actual;
    count -= actual;

    actual = 0;				/* reinitialize for the new round */
    subchunk = (count < OPENUSBFXS_CHUNK_SIZE)? count : OPENUSBFXS_CHUNK_SIZE;

    /* lock our current buffer to keep submit() from messing with it */
    spin_lock_irqsave (&ourbuf->lock, flags);

    /* make sure we have won the race for buffer ownership against submit */
    if (ourbuf->state < st_ready) {	/* submit has taken our buffer */
        spin_unlock_irqrestore (&ourbuf->lock, flags);
	goto read_findbuf;
    }

    /* because we handle chunks of data, we copy a chunk's worth of data
     * each time, locking and unlocking the spinlock to avoid holding it
     * for too long; if we have less than a full chunk of data to copy,
     * we keep the rest in tinyrbuf;
     */

    /* adjust buffer state, copy a single chunk (at most) */
    ourbuf->state = st_rding;
    memcpy (ptr,
      ((union openusbfxs_data *) &ourbuf->buf[ourbuf->len])->in_pack.sample,
      subchunk);
    /* if less than a full chunk was requested, save the rest in tinyrbuf */
    if (count < OPENUSBFXS_CHUNK_SIZE) {
	dev->trbcount = OPENUSBFXS_CHUNK_SIZE - count;
	/* dev->trboffst = 0;	// is already zero */
	memcpy (dev->tinyrbuf, (char *)
	  (((union openusbfxs_data *)&ourbuf->buf[ourbuf->len])->in_pack.sample)
	    + count,
	  dev->trbcount);
    }
    ourbuf->len += OPENUSBFXS_DPACK_SIZE;
    buflen = ourbuf->len;		/* sample for use outside atomic rgn */
    if (buflen == OPENUSBFXS_MAXIBUFLEN) {
        ourbuf->state = st_empty;	/* mark buffer as fully read */
    }
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    /* account for the amount of data we just read */
    actual += subchunk;
    ptr += subchunk;

    if (buflen == OPENUSBFXS_MAXIBUFLEN) {
        /* lock to ensure consistency of dev->read_next */
	spin_lock_irqsave (&dev->in_buflock, flags);
	/* advance read_next only if submit() has not done so already */
	if (&dev->in_bufs [dev->read_next] == ourbuf) { /* sampled bufaddr */
	    dev->read_next = (dev->read_next + 1) & (OPENUSBFXS_MAXURB - 1);
	}
	spin_unlock_irqrestore (&dev->in_buflock, flags);
    }

    /* check if we are done (including case where < CHUNK_SIZE bytes remain) */
    if (count <= OPENUSBFXS_CHUNK_SIZE) {
        OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
	  "%s: about to return %d and keep %d in trbcount",
	  __func__, (int) (retval + actual),
	  (int) (OPENUSBFXS_CHUNK_SIZE - count));
	retval += actual;
	goto read_copy_exit;
    }

    /* if our buffer has more data, go back and test again */
    if (buflen < OPENUSBFXS_MAXIBUFLEN)  goto read_newchunk;

    /* otherwise find the next buffer */
    goto read_findbuf;

read_copy_exit:
    if (copy_to_user (ubuf, buf, retval)) {
	retval = -EFAULT;
	OPENUSBFXS_ERR ("%s: copy_to_user failed", __func__);
    }

read_exit:
    mutex_unlock (&dev->rdmutex);
    if (buf) kfree (buf);
    return retval;
}




static ssize_t openusbfxs_write (struct file *file, const char __user *ubuf,
  size_t count, loff_t *ppos)
{
    struct openusbfxs_dev *dev;	/* our device structure */
    int retval = 0;		/* # of bytes written or error */
    char *buf = NULL;		/* allocated buffer to hold copy of user data */
    char *ptr;			/* source pointer for copying into outbufs */
    size_t actual;		/* bytes actually written */
    struct isocbuf *ourbuf;	/* sampled current buffer */
    int buflen;			/* sampled buffer length */
    unsigned long flags;	/* irqsave flags */

    dev = file->private_data;

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING, "write(fp,buf,%d)", (int) count);

    /* make sure board is still OK */
    
    if (dev->state != OPENUSBFXS_STATE_OK) return -EIO; // or should -EAGAIN?
    /* note: currently we do not alter the state after initial setup, but this
     * may change in the future, so be prepared */

    /* prevent others from attempting concurrent write()s (we may sleep) */
    mutex_lock (&dev->wrmutex);

    /* finish off with the zero-count (probe) case */
    if (!count) goto write_exit;

    /* copy user data to a locally-allocated buffer; if data from previous
     * short writes exist (dev->twbcount > 0), prepend them to the newly-
     * supplied user data
     */
    buf = kmalloc (count + dev->twbcount, GFP_KERNEL);
    if (!buf) {
        retval = -ENOMEM;
	OPENUSBFXS_ERR ("%s: out of memory", __func__);
	goto write_exit;
    }
    if (dev->twbcount) memcpy (buf, dev->tinywbuf, dev->twbcount);
    if (copy_from_user (buf + dev->twbcount, ubuf, count)) {
        retval = -EFAULT;
	OPENUSBFXS_ERR ("%s: copy_from_user failed", __func__);
	goto write_exit;
    }

    count += dev->twbcount;	/* count previous "debt" into owed count */

    ptr = buf;			/* initialize source pointer */
    actual = 0;			/* initialize actually written bytes */

write_findbuf:

    /* account for data written in previous round */
    retval += actual;
    count -= actual;

    actual = 0;			/* initialize for the new round */

    /* check if we are done (including case where < CHUNK_SIZE bytes remain) */
    if (count < OPENUSBFXS_CHUNK_SIZE) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
	  "%s: about to return %d and keep %d in twbcount",
	  __func__, (int) (retval + count - dev->twbcount), (int) count);
	retval += count;		/* report everything was written */
        retval -= dev->twbcount;	/* subtract the "debt" that we payed */

					/* buffer outstanding data in tinywbuf*/
	if (count) memcpy (dev->tinywbuf, ptr, count);
	dev->twbcount = count;		/* and mark their length in twbcount */
	goto write_exit;
    }

    /* submit() may advance anytime the 'writenext' index, so we (re-)sample
     * the currently-indexed write buffer into ourbuf;  no locking is necessary,
     * because we check the buffer state later on using locking
     */
    ourbuf = &dev->outbufs[dev->writenext];

    /* we know we have no more space if we have (wrapped around and)
     * hit a buffer which is either (fully) written, or taken by submit;
     * in this case, we either return immediately (if O_NONBLOCK is set),
     * or block waiting for the urb completion callback to wake us when
     * more space is made available
     */
    if (file->f_flags & O_NONBLOCK) {
        if (ourbuf->state >= st_wrttn) {
	    /* return zero or the number of bytes already written */
	    goto write_exit;
	}
    }
    else {
	if (wait_event_interruptible (dev->outwqueue,
	  ((ourbuf->state <= st_wrtng) ||
	   (dev->state != OPENUSBFXS_STATE_OK) ||
	   (ourbuf != &dev->outbufs[dev->writenext])))) {
	    retval = -ERESTARTSYS;
	    goto write_exit;
	}

	/* make sure we are not unloading or something */
	if (dev->state != OPENUSBFXS_STATE_OK) {
	    retval = -ENODEV;	/* device has gone away */
	    goto write_exit;
	}

	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
	  "%s: after wait_event", __func__);

	/* if submit() has moved the write index, re-sample */
	if (ourbuf != &dev->outbufs[dev->writenext]) {
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
	      "%s: write index moved", __func__);
	    goto write_findbuf;
	}
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
	  "%s: buffer %d state %d", __func__, 
	    (int) (ourbuf - &dev->outbufs[0]),
	    ourbuf->state);
    }

write_newchunk:
    /* note: the last chunk may be shorter than normal */
    if (count - actual < 16) OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
      "%s: count now is %d", __func__, (int) (count - actual));

    /* make sure we really have something to write into an outbuf now */
    if (count - actual < OPENUSBFXS_CHUNK_SIZE) {
        retval -= dev->twbcount;	/* subtract "debt" that we payed */
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING,
	  "%s: about to return %d and keep %d in twbcount",
	  __func__, (int) (retval + actual), (int) (count - actual));
	if (count) memcpy (dev->tinywbuf, ptr, count - actual);
					/* buffer outstanding data in tinywbuf*/
	dev->twbcount = count - actual;	/* and mark their length in twbcount */
	actual = count;			/* now we have "written" everything */

	retval += actual;		/* account for it */
	goto write_exit;		/* and exit */
    }

    /* lock our current buffer to keep submit() from messing with it */
    spin_lock_irqsave (&ourbuf->lock, flags);

    /* make sure we have won the race for buffer ownership against submit() */
    if (ourbuf->state > st_wrtng) {
	spin_unlock_irqrestore (&ourbuf->lock, flags);
	goto write_findbuf;
    }

    /* because we need to packetize, we copy a chunk's worth of data each
     * time, locking and unlocking the spinlock in order to avoid holding
     * it for too long
     */

    /* adjust buffer state, copy a single chunk */
    ourbuf->state = st_wrtng;
    memcpy (
      ((union openusbfxs_data *) &ourbuf->buf[ourbuf->len])->outpack.sample,
      ptr, OPENUSBFXS_CHUNK_SIZE);
    ourbuf->len += OPENUSBFXS_DPACK_SIZE; /* even if we may have written less */
    buflen = ourbuf->len;		/* sample for use outside atomic regn */
    if (buflen == OPENUSBFXS_MAXOBUFLEN) {
        ourbuf->state = st_wrttn;	/* mark buffer as fully written */
    }
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    /* account for the chunk just copied */
    actual += OPENUSBFXS_CHUNK_SIZE;
    ptr += OPENUSBFXS_CHUNK_SIZE;

    if (buflen == OPENUSBFXS_MAXOBUFLEN) {
	/* lock to ensure consistency of dev->writenext */
	spin_lock_irqsave (&dev->outbuflock, flags);
	/* advance writenext only if submit() has not done so already */
	if (&dev->outbufs [dev->writenext] == ourbuf) { /* use sampled bufaddr*/
	    dev->writenext = (dev->writenext + 1) & (OPENUSBFXS_MAXURB - 1);
	}
	spin_unlock_irqrestore (&dev->outbuflock, flags);

	goto write_findbuf;
    }
    
    /* our buffer has more room, so go back and test again if we have
     * more to write and if we can do so
     */
    goto write_newchunk;

write_exit:
    mutex_unlock (&dev->wrmutex);
    if (buf) kfree (buf);
    return retval;
}

static int openusbfxs_release (struct inode *inode, struct file *file)
{
    struct openusbfxs_dev *dev;

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING, "release()");

    dev = (struct openusbfxs_dev *) file->private_data;
    if (dev == NULL) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "cannot get device from file->private_data!");
    	return -ENODEV;
    }

    /* set linefeed to open mode */
    // write_direct (dev, 64, 0x0, &trash8);

    /* lock, decrement openers count, then unlock */
    mutex_lock (&dev->iomutex);
    dev->opencnt--;
    mutex_unlock (&dev->iomutex);

    /* decrement (driver-side) reference count */
    kref_put (&dev->kref, openusbfxs_delete);

    return 0;
}

static int openusbfxs_flush (struct file *file, fl_owner_t id)
{
    struct openusbfxs_dev *dev;
    int retval;
    unsigned long flags;	/* irqsave flags */

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING, "flush()");
    dev = (struct openusbfxs_dev *) file->private_data;
    if (dev == NULL) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "cannot get device from file->private_data!");
        return -ENODEV;
    }

    /* wait till others finish with (fs) IO like read, write, etc. */
    /* (note: since we do exclusive open, is this really needed?) */
    mutex_lock (&dev->iomutex);

    /* drain was removed because it kills queued isochronous OUT urbs */
    // TODO: substitute with something that will wait until queued OUT
    //	     urbs are transmitted - that is, successfully or not
#if 0
    /* wait for pending writes to drain or cancel them */
    openusbfxs_drain_urbs (dev);
#endif

    /* reset tiny buffer(s) */
    dev->twbcount = 0;

#if 0
    // this is stupid, because sequence numbers are a USB link-level
    // issue and have nothing to do with the open()ed file state
    /* reset sequence number(s) */
    dev->outseqno = 0;
#endif

    /* read error and clean error/stats for subsequent opens to find it clear */
    spin_lock_irqsave (&dev->statslck, flags);
    /* pass on EPIPE, convert others to EIO */
    retval = (dev->stats.errors)?
      ((dev->stats.errors == -EPIPE)? -EPIPE : -EIO) : 0;
    dev->stats.errors = 0;
    memset (&dev->stats, 0, sizeof (struct openusbfxs_stats));
    spin_unlock_irqrestore (&dev->statslck, flags);

    /* let others in */
    mutex_unlock (&dev->iomutex);

    return retval;
}

/* tell board to start/stop PCM I/O */
static int start_stop_io (struct openusbfxs_dev *dev, __u8 val)
{
    union openusbfxs_packet req = START_STOP_IO_REQ(val);
    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int length;
    int rlngth;
    int retval;

    rlngth = sizeof(req.strtstp_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) wrote %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    return 0;
}

/* ProSLIC register I/O implementations */

 /* read_direct read a ProSLIC DR
 * @dev the struct usb_device instance
 * @reg the register to read
 * @value pointer to the return value
 * @return 0 on success, non-zero on failure
 */
static int read_direct (struct openusbfxs_dev *dev, __u8 reg, __u8 *value)
{
    union openusbfxs_packet req = PROSLIC_RDIRECT_REQ(reg);
    union openusbfxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.rdirect_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
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
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
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
static int write_direct (struct openusbfxs_dev *dev, __u8 reg, __u8 value,
  __u8 *actval)
{
    union openusbfxs_packet req = PROSLIC_WDIRECT_REQ(reg, value);
    union openusbfxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.wdirect_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
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
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    *actval = PROSLIC_WDIRECT_RPV (rpl);

    return 0;
}

/* write_indirect write a ProSLIC IR
 * @dev the struct usb_device instance
 * @reg the register to read
 * @value value to set
 * @actval pointer to the return value
 * @return 0 on success, non-zero on failure
 */
static int write_indirect (struct openusbfxs_dev *dev, __u8 reg, __u16 value,
  __u16 *actval)
{
    union openusbfxs_packet req = PROSLIC_WRINDIR_REQ(reg, value);
    union openusbfxs_packet rpl;
    int rlngth;
    int length;
    int retval;

    int outpipe = usb_sndbulkpipe (dev->udev, dev->ep_bulk_out);
    int in_pipe = usb_rcvbulkpipe (dev->udev, dev->ep_bulk_in);

    rlngth = sizeof(req.wrindir_req);

    /* ??? locking? */
    retval = usb_bulk_msg (dev->udev, outpipe, &req, rlngth, &length, 1000);
    if (retval) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(out) returned %d", __func__, retval);
	if (retval == -ETIMEDOUT) {
	    return retval;
	}
        return -EIO;
    }
    if (length != rlngth) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
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
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) returned %d", __func__, retval);
        return -EIO;
    }
    if (length != rlngth) {
	OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	  "%s: usb_bulk_msg(in) read %d instead of %d bytes", __func__,
	  length, rlngth);
        return -EIO;
    }

    *actval = PROSLIC_WRINDIR_RPV (rpl);

    return 0;
}

/* used in register dumps */
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


static void dump_direct_regs (char *msg, struct openusbfxs_dev *dev)
{
    int c = 0;
    __u8 regval;
    char line [80];
    int status;
    __u8 i;

    /* spare us all the trouble if we are not going to produce any output */
    if (debuglevel < OPENUSBFXS_DBGVERBOSE) return;

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
      "----3210 direct register hex dump (%s)----", msg);
    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
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
		sprintf (&line [8+(c<<2)], " %02X ", regval);
	    }
	}
	else {
	    sprintf (&line [8+(c<<2)], "    ");
	}
	if (c++ == 9) {
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE, "%s", line);
	    c = 0;
	}
    }
    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE, "----end of register dump----");
}

static int openusbfxs_isoc_out_submit (struct openusbfxs_dev *dev, int memflag){
    int retval = 0;
    struct isocbuf *ourbuf;
    unsigned long flags;	/* irqsave flags */
    union openusbfxs_data *p;
    enum state_enum oldst;

    /* make sure write() does not alter our dev state while we are working */
    spin_lock_irqsave (&dev->outbuflock, flags);

    /* if the buffer indexed by outsubmit is current for write(), tell
     * write() to advance to next buffer */
    if (dev->writenext == dev->outsubmit) {
        dev->writenext = (dev->writenext + 1) & (OPENUSBFXS_MAXURB - 1);
    }
    ourbuf = &dev->outbufs[dev->outsubmit];
    dev->outsubmit = (dev->outsubmit + 1) & (OPENUSBFXS_MAXURB - 1);
    spin_unlock_irqrestore (&dev->outbuflock, flags);

    /* from now on we work with this buffer only */
    spin_lock_irqsave (&ourbuf->lock, flags);
    oldst = ourbuf->state;
    ourbuf->state = st_sbmng;		/* tell write() this buffer is ours */
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    /* check for buffer underrun */
    if (ourbuf->len >= OPENUSBFXS_DPACK_SIZE) { /* must contain >= 1 packet */
	ourbuf->urb->number_of_packets = ourbuf->len / OPENUSBFXS_DPACK_SIZE;
	ourbuf->urb->transfer_buffer_length =	/* trim to packet size */
	  ourbuf->urb->number_of_packets * OPENUSBFXS_DPACK_SIZE;
	/* plant the right sequence numbers */
	for (p = (union openusbfxs_data *)ourbuf->buf;
	  p < (union openusbfxs_data *)(ourbuf->buf + ourbuf->len); p++) {
	    p->outpack.outseq = dev->outseqno++;
	}
    }
    else {
	/* underrun; send out a full buffer's worth of 0xFF's */
	ourbuf->urb->transfer_buffer_length = OPENUSBFXS_MAXOBUFLEN;
	ourbuf->urb->number_of_packets = wpacksperurb;
	/* plant the right sequence numbers */
	for (p = (union openusbfxs_data *)ourbuf->buf;
	  p < (union openusbfxs_data *)(ourbuf->buf + OPENUSBFXS_MAXOBUFLEN);
	  p++) {
	    p->outpack.outseq = dev->outseqno++;
	}
	/* account for underrun */
	spin_lock_irqsave (&dev->statslck, flags);
	dev->stats.out_underruns++;
	spin_unlock_irqrestore (&dev->statslck, flags);
    }
    ourbuf->urb->dev = dev->udev;	// TODO: is this needed every time?
    if (!dev->udev) {			/* disconnect called? */
        retval = -ENODEV;
	goto isoc_out_submit_error;
    }
    usb_anchor_urb (ourbuf->urb, &dev->submitted);

    /* memflag must be GFP_ATOMIC when we execute in handler context!
     * in that case, make sure that the device is not unloading, or a
     * race will occur (note that it's safe to hold a spinlock since
     * we are using GFP_ATOMIC)
     */
    if (memflag == GFP_ATOMIC) spin_lock_irqsave (&dev->statelck, flags);
    if (dev->state == OPENUSBFXS_STATE_OK) {
	retval = usb_submit_urb (ourbuf->urb, memflag);
    }
    if (memflag == GFP_ATOMIC) spin_unlock_irqrestore (&dev->statelck, flags);

    if (retval != 0) {
        usb_unanchor_urb (ourbuf->urb);
	goto isoc_out_submit_error;
    }
    ourbuf->state = st_sbmtd;
    return 0;

isoc_out_submit_error:

    /* mark the buffer as being free again */
    spin_lock_irqsave (&ourbuf->lock, flags);
    memset (ourbuf->buf, 0, OPENUSBFXS_MAXOBUFLEN);
    ourbuf->len = 0;
    ourbuf->state = st_empty;
    spin_unlock_irqrestore (&ourbuf->lock, flags);
    return (retval);
}

static int openusbfxs_isoc_in__submit (struct openusbfxs_dev *dev, int memflag){
    int retval = 0;
    struct isocbuf *ourbuf;
    unsigned long flags;
    enum state_enum oldst;

    /* make sure read() does not alter buffer state while we are working */
    spin_lock_irqsave (&dev->in_buflock, flags);
    
    /* if the buffer indexed by in_submit is current for read(), tell
     * read() to advance to the next buffer (this means data is overrun) */
    if (dev->read_next == dev->in_submit) {
        dev->read_next = (dev->read_next + 1) & (OPENUSBFXS_MAXURB - 1);
    }
    ourbuf = &dev->in_bufs[dev->in_submit];
    dev->in_submit = (dev->in_submit + 1) & (OPENUSBFXS_MAXURB - 1);
    spin_unlock_irqrestore (&dev->in_buflock, flags);

    /* from now on we are working with this buffer only */
    spin_lock_irqsave (&ourbuf->lock, flags);
    oldst = ourbuf->state;
    ourbuf->state = st_sbmng;	/* tell read() this buffer is ours */
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    /* check for overrun and keep count */
    if (oldst != st_empty) {	/* read() didn't fully drain this buffer */
	spin_lock_irqsave (&dev->statslck, flags);
	dev->stats.in_overruns++;
	spin_unlock_irqrestore (&dev->statslck, flags);
    }

    ourbuf->urb->dev = dev->udev;	// TODO: is this needed every time?
    if (!dev->udev) {			/* disconnect called? */
        retval = -ENODEV;
	goto isoc_in__submit_error;
    }
    usb_anchor_urb (ourbuf->urb, &dev->submitted);

    /* memflag must be GFP_ATOMIC when we execute in handler context!
     * in that case, make sure the device is not unloading, or a race
     * will occur
     */
    if (memflag == GFP_ATOMIC) spin_lock_irqsave (&dev->statelck, flags);
    if (dev->state == OPENUSBFXS_STATE_OK) {
        retval = usb_submit_urb (ourbuf->urb, memflag);
    }
    if (memflag == GFP_ATOMIC) spin_unlock_irqrestore (&dev->statelck, flags);
    if (retval != 0) {
        usb_unanchor_urb (ourbuf->urb);
	goto isoc_in__submit_error;
    }
    ourbuf->state = st_sbmtd;
    return 0;


isoc_in__submit_error:

    /* mark the buffer as being free again */
    spin_lock_irqsave (&ourbuf->lock, flags);
    ourbuf->len = 0;
    ourbuf->state = st_empty;
    spin_unlock_irqrestore (&ourbuf->lock, flags);
    return (retval);
}



/* isochronous callback function for OUT packets */
static void openusbfxs_isoc_out_cbak (struct urb *urb)
{
    struct openusbfxs_dev *dev;
    struct isocbuf *ourbuf;
    unsigned long flags;	/* irqsave flags */
    int ret;

    ourbuf = urb->context;
    dev = ourbuf->dev;

    /* do a couple of consistency checks; if they fail, probably we 're
     * veery screwed up, so we skip and do not submit anything further
     */
    if (ourbuf->state != st_sbmtd) {
        OPENUSBFXS_ERR ("%s: inconsistent state %d, bailing out",
	  __func__, ourbuf->state);
	/* serious error -- don't submit new urbs in this situation */
	goto isoc_out_cbak_exit;
    }
    spin_lock_irqsave (&ourbuf->lock, flags);
    memset (ourbuf->buf, 0xff, OPENUSBFXS_MAXOBUFLEN);
    ourbuf->len = 0;
    ourbuf->state = st_empty;
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    // TODO: wake_up_interruptible_sync?? (normally not needed, scheduler
    // "knows" we are running in IRQ context and will let us finish first)
    wake_up_interruptible (&dev->outwqueue);

    /* submit a new urb (for now, ignore the return value) */
    ret = openusbfxs_isoc_out_submit (dev, GFP_ATOMIC);
    if (ret) {
	spin_lock_irqsave (&dev->statslck, flags);
	dev->stats.errors = ret;
	dev->stats.lasterrop = err_out;
	spin_unlock_irqrestore (&dev->statslck, flags);
    }

isoc_out_cbak_exit:
    return;
}

/* isochronous callback function for IN packets */
static void openusbfxs_isoc_in__cbak (struct urb *urb)
{
    struct openusbfxs_dev *dev;
    struct isocbuf *ourbuf;
    unsigned long flags;	/* irqsave */
    int i;
    int ret;
    char in_missed;
    union openusbfxs_data *p;

    ourbuf = urb->context;
    dev = ourbuf->dev;

    /* check consistency, quiesce down if test fails */
    if (ourbuf->state != st_sbmtd) {
        OPENUSBFXS_ERR ("%s: inconsistent state %d, bailing out",
	  __func__, ourbuf->state);
	/* we won't submit a new urb in this case */
	goto isoc_in__cbak_exit;
    }

    // FIX THIS COMMENT
    /* scan for hook state and dtmf events now, so that reporting these
     * to the user gets decoupled from the time a read() is issued
     */
    for (i = 0; i < ourbuf->urb->number_of_packets; i++) {
	/* consider only packets that were received fully & correctly */
        if (ourbuf->urb->iso_frame_desc[i].status == 0 &&
	    ourbuf->urb->iso_frame_desc[i].actual_length ==
	      OPENUSBFXS_DPACK_SIZE) {

	    in_missed = 0;
	    /* use p as handy packet pointer */
	    p = ((union openusbfxs_data *)ourbuf->buf) + i;

	    /* hook state and dtmf are only reported in odd packets */
	    if (p->in_pack.oddevn == 0xdd) {	/* odd packet */
		__u8 hkdtmf = p->inopack.hkdtmf;
	        /* hook state is hkdtmf bit 7 */
		dev->hook = hkdtmf & 0x80;
		/* dtmf status is bit 4 and digit is in bits 3, 2, 1 and 0 */
		hkdtmf &= 0x1f;
		/* implement a simple one-digit "latch" to hold dtmf value */
		if (hkdtmf & 0x10) {		/* if digit is being pressed */
		    if (!dev->dtmf) {		/* do it just once per digit */
		        dev->dtmf = hkdtmf;
		    }
		}
		else {
		    dev->dtmf = 0;		/* reset if nothing pressed */
		}
		if (p->inopack.inoseq != dev->in_seqodd++) {
		    spin_lock_irqsave (&dev->statslck, flags);
		    dev->stats.in_missed++;
		    spin_unlock_irqrestore (&dev->statslck, flags);
		    dev->in_seqodd = p->inopack.inoseq + 1;
		}
	    }
	    else {				/* even packet */
		if (p->inepack.ineseq != dev->in_seqevn++) {
		    spin_lock_irqsave (&dev->statslck, flags);
		    dev->stats.in_missed++;
		    spin_unlock_irqrestore (&dev->statslck, flags);
		    dev->in_seqevn = p->inepack.ineseq + 1;
		}
	    }
	    /* check if we missed a mirrored sequence #; if so, this means
	     * that one output packet of ours got lost or delayed in the
	     * way, but in any case it was missed by the board
	     */
	    if (!in_missed & (p->in_pack.moutsn != dev->in_moutsn++)) {
		spin_lock_irqsave (&dev->statslck, flags);
		dev->stats.out_missed++;
		spin_unlock_irqrestore (&dev->statslck, flags);
		/*
		if (!dev->in_oofseq++) {
		    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
		      "%s: expected %d, got %d", __func__, dev->in_moutsn - 1,
		      p->in_pack.moutsn);
		}
		*/
		dev->in_moutsn = p->in_pack.moutsn + 1;
	    }
	    /*
	    else if (dev->in_oofseq) {
		if (dev->in_oofseq > 1) {
		    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
		      "%s: a total of %d packets were received ouf of sequence",
		      __func__, dev->in_oofseq);
		}
	        dev->in_oofseq = 0;
	    }
	    */
	}
	else {
	    /* report a bad frame (if we don't see the expected sequence in
	     * the next correctly-received packet, we 'll also increase
	     * stats.missing)
	     */
	    spin_lock_irqsave (&dev->statslck, flags);
	    dev->stats.in_badframes++;
	    spin_unlock_irqrestore (&dev->statslck, flags);
	}
    }
    ourbuf->len = 0;	/* in read bufs, .len is the # of data already read */

    spin_lock_irqsave (&ourbuf->lock, flags);
    ourbuf->state = st_ready;
    spin_unlock_irqrestore (&ourbuf->lock, flags);

    // TODO: wake_up_interruptible_sync?? (normally not needed, scheduler
    // "knows" we are running in IRQ context and will let us finish first)
    wake_up_interruptible (&dev->in_wqueue);

    /* submit a new urb (for now, ignore the return value) */
    ret = openusbfxs_isoc_in__submit (dev, GFP_ATOMIC);
    if (ret) {
	spin_lock_irqsave (&dev->statslck, flags);
	dev->stats.errors = ret;
	dev->stats.lasterrop = err_in;
	spin_unlock_irqrestore (&dev->statslck, flags);
    }

isoc_in__cbak_exit:
    return;
}

/* background board setup thread (runs as a worker thread in a workqueue) */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
static void openusbfxs_setup (struct work_struct *work)
{
    struct openusbfxs_dev *dev =
      container_of (work, struct openusbfxs_dev, iniwt);
#else
static void openusbfxs_setup (void *data)
{
    struct openusbfxs_dev *dev = data;
#endif

    int i;		/* generic counter */
    __u8 j;		/* counter, byte-sized */
    __u8 drval;		/* return value for DRs */
    __u16 irval;	/* return value for IRs */
    int sts = 0;	/* status returned */
    __u16 timeouts = 0;	/* # times board did not respond (initially) */
    __u16 dcdcfail = 0;	/* # times DC-DC converter failed */
    __u16 calfail = 0;	/* # times ADC calibration failed */
    __u16 lbcfail = 0;	/* # times longitudinal balance calibration failed */
    unsigned long flags;	/* irqsave flags */

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
      "%s: starting board setup procedure", __func__);

    /* wait for USB initialization to settle */
    ssleep (1);

    spin_lock_irqsave (&dev->statelck, flags);
    /* never proceed unless in IDLE state (just taking precautions against
     * future bugs if I ever think re-initializing a board is a good idea)
     */
    if (dev->state != OPENUSBFXS_STATE_IDLE) {
	spin_unlock_irqrestore (&dev->statelck, flags);
        return;
    }
    dev->state = OPENUSBFXS_STATE_INIT;
    dev->hook  = 0;
    dev->dtmf  = 0;
    spin_unlock_irqrestore (&dev->statelck, flags);

init_restart:
    /* return if driver is unloading */
    if (dev->state == OPENUSBFXS_STATE_UNLOAD) {
	OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,"%s: returning on driver unload",
	  __func__);
	return;
    }

    if (sts == -ETIMEDOUT) timeouts++;
    if (timeouts == 5) {
	OPENUSBFXS_ERR ("%s: openusbfxs#%d not responding, will keep trying",
	  __func__, dev->intf->minor);
    }

    /* loop until board replies and reports a reasonable value for DR11 */
    dr_read_chk (sts, 11, expdr11, drval, init_restart);
    if (timeouts) {
	OPENUSBFXS_DEBUG(OPENUSBFXS_DBGTERSE,"%s: board responded OK",
	  __func__);
    }
    timeouts = 0;

    dump_direct_regs ("at initialization", dev);

    /* advice to passers-by: initialization procedure follows closely
     * the guidelines of Silabs application note 35 (AN35.pdf); the
     * casual reader is advised to consult AN35 before attempting a
     * codewalk; although I 've tried to provide as many comments as
     * feasible, I am afraid that understanding what the code does
     * without having read the AN is rather impossible;
     */

    /* initialization step #1: quiesce down board (if re-initializing) */

    /* take DC-DC converter down */
    dr_write_check (sts, 14, 0x10, drval, init_restart);
    /* set linefeed to open mode */
    dr_write_check (sts, 64, 0x00, drval, init_restart);

    /* initialization step #2: initialize all indirect registers */

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
    // TODO: loop close threshold may need adjustment?
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
     * to 0x1000 as "DC-DC extra" (extra what?)
    ir_write (sts, 42, 0x1000, irval, init_restart); */
    // TODO: loop close threshold may need adjustment?
    ir_write (sts, 43, 0x1000, irval, init_restart); /* loop close thres. low */

    /* initialization step #3: set up DC-DC converter parameters */

    dr_write_check (sts, 0x08, 0, drval, init_restart); /* exit dig. loopback */
    dr_write_check (sts, 108, 0xeb, drval, init_restart); /* rev.E features */
    dr_write (sts, 66, 0x01, drval, init_restart); /* Vov=low, Vbat~Vring */
    /* following six parameter values taken from SiLabs Excel formula sheet
     * (with a fixed inductor value of 100uH, NREN=1, dist=1000)
     */
    dr_write_check (sts, 92, 202, drval, init_restart); /* PWM period=12.33us */
    dr_write_check (sts, 93, 12, drval, init_restart); /* min off time=732ns */
    dr_write_check (sts, 74, 44, drval, init_restart); /* Vbat(high)=-66V */
    dr_write_check (sts, 75, 40, drval, init_restart); /* Vbat(low)=-60V */
    dr_write_check (sts, 71, 0, drval, init_restart); /* Cur. max=20mA (dflt) */
    /* already done above, so it's commented out here
    ir_write (sts, 40, 0x0000, irval, init_restart); /@ cmmode bias ringing */

    /* initialization step #4: bring up DC-DC converter (sigh!...) */

    dr_write_check (sts, 14, 0, drval, init_restart); /* bring up DC-DC conv */
    for (i = 0; i < 10; i++) {
	dr_read (sts, 82, drval, init_restart);	/* read sensed voltage	*/
	drval = drval * 376 / 1000;		/* convert to volts	*/
	if (drval >= 60) goto init_dcdc_ok;
	OPENUSBFXS_WARN ("%s: measured dc voltage is %d V", __func__, drval);
    }
    dump_direct_regs ("dc-dc converter failed", dev);

    if (dcdcfail++ == 2) {
	if (retoncnvfail) {
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	      "%s: early exit (leaving converter ON for debugging)",
	      __func__);
	    return;
	}
	OPENUSBFXS_ERR (
	  "%s: openusbfxs#%d dc-dc converter failure, will keep trying",
	  __func__, dev->intf->minor);
    }
    goto init_restart;

init_dcdc_ok:
    dcdcfail = 0;

    /* initialization step #5: perform calibrations */

    dr_write_check (sts, 21, 0, drval, init_restart); /* disable intrs in IE1 */
    dr_write_check (sts, 22, 0, drval, init_restart); /* disable intrs in IE2 */
    dr_write_check (sts, 23, 0, drval, init_restart); /* disable intrs in IE3 */
    dr_write_check (sts, 64, 0, drval, init_restart); /* set "open mode" LF */

    /* calibration part 1: ADC calibration */
    /* monitor ADC calibration 1&2 but don't do DAC/ADC/balance calibration */
    dr_write_check (sts, 97, 0x1e, drval, init_restart); /* set cal. bits */
    /* start differential DAC, common-mode and I-LIM calibrations */
    dr_write_check (sts, 96, 0x47, drval, init_restart);
    /* wait for calibration to finish */
    for (i = 0; i < 10; i++) {
	dr_read (sts, 96, drval, init_restart);	/* read calibration result */
	if (drval == 0) goto init_clb1_ok;
	msleep (200);
    }
    dump_direct_regs ("calibration failed", dev);

    if (calfail++ == 2) {
	if (retoncnvfail) {
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE, "%s: early exit", __func__);
	    return;
	}
	OPENUSBFXS_ERR (
	  "%s: openusbfxs#%d calibration1 failure, will keep trying",
	  __func__, dev->intf->minor);
    }
    goto init_restart;

init_clb1_ok:
    calfail = 0;

    /* calibration part 2: manual Q5/Q6 current calibration (required:Si3210) */
    /* Q5 current */
    for (j = 0x1f; j >= 0; j--) {
	dr_write (sts, 98, j, drval, init_restart); /* adjust... */
	msleep (40); /* ...give it some time to settle... */
	dr_read (sts, 88, drval, init_restart);	/* ... and read current */
	if (drval == 0) break;
    }
    if (drval != 0) {
	OPENUSBFXS_WARN (
	  "%s: openusbfxs#%d Q5 cur. manual calibration failed, DR88=%d",
	  __func__, dev->intf->minor, drval);
	goto init_restart;
    }
    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE, "%s: Q5 calibration OK at %d",
      __func__, j);
    /* Q6 current */
    for (j = 0x1f; j >= 0; j--) {
	dr_write (sts, 99, j, drval, init_restart); /* adjust... */
	msleep (40); /* ...give it some time to settle... */
	dr_read (sts, 89, drval, init_restart);	/* ... and read current */
	if (drval == 0) break;
    }
    if (drval != 0) {
	OPENUSBFXS_WARN (
	  "%s: openusbfxs#%d Q6 cur. manual calibration failed, DR89=%d",
	  __func__, dev->intf->minor, drval);
	goto init_restart;
    }
    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE, "%s: Q6 calibration OK at %d",
      __func__, j);

    /* calibration part 3: longitudinal balance calibration */
    /* enable interrupt logic for on/off hook mode change during calibration */
    dr_write (sts, 23, 0x04, drval, init_restart);

    /* make sure equipment is on-hook */
    dr_write (sts, 64, 0x01, drval, init_restart); /* fwd active LF mode */
    ssleep (1);				 /* give ciruitry time to settle*/
    dr_read (sts, 68, drval, init_restart);	/* read hook state */
    if (!(drval & 0x01)) goto init_onhook;
    dr_write (sts, 64, 0x00, drval, init_restart); /* reset LF to open mode */

    if (lbcfail++ == 2) {
	if (retonlbcfail) {
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE, "%s: early exit", __func__);
	    return;
	}
	OPENUSBFXS_ERR (
	  "%s: openusbfxs#%d off-hook during calibration, will keep trying",
	  __func__, dev->intf->minor);
    }
    goto init_restart;

init_onhook:
    lbcfail = 0;

    /* perform actual longitudinal balance calibration*/
    dr_write (sts, 97, 0x01, drval, init_restart); /* set CALCM bit */
    dr_write (sts, 96, 0x40, drval, init_restart); /* start calibration */
    while (1) {	/* loop waiting for calibration to end */
        dr_read (sts, 96, drval, init_restart);
	if (drval == 0) break;
	msleep (200);
    }

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
      "%s: longitudinal mode calibration OK", __func__);
    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
      "%s: all calibrations completed successfully", __func__);

    /* initialization step #6: miscellaneous initializations */

    /* flush energy accumulators */
    for (j = 88; j <= 95; j++) {
	ir_write (sts, j, 0, irval, init_restart);
    }
    // ! BUG!! ir_write (sts, j, 97, irval, init_restart);
    ir_write (sts, 97, 0, irval, init_restart);
    for (j = 193; j <= 211; j++) {
	ir_write (sts, j, 0, irval, init_restart);
    }

    /* clear all pending interrupts while no interrupts are enabled */
    dr_write (sts, 18, 0xff, drval, init_restart);
    dr_write (sts, 19, 0xff, drval, init_restart);
    dr_write (sts, 20, 0xff, drval, init_restart);
    /* enable selected interrupts */
    dr_write_check (sts, 21, 0x00, drval, init_restart); /* none here */
    dr_write_check (sts, 22, 0xff, drval, init_restart); /* all here */
    // dr_write_check (sts, 22, 0x03, drval, init_restart); /* only lcip/rtip */
    dr_write_check (sts, 23, 0x01, drval, init_restart); /* only dtmf here */

    /* set read and write PCM clock slots */
    for (j = 2; j <= 5; j++) {
        dr_write (sts, j, 0, drval, init_restart);
    }

    /* set DRs 63, 67, 69, 70 -- currently, all set to their default values */
    /* DR 63 (loop closure debounce interval for ringing silent period) */
    dr_write_check (sts, 63, 0x54, drval, init_restart);	/* 105 ms */
    /* DR 67 (automatic/manual control) */
    dr_write_check (sts, 67, 0x1f, drval, init_restart);	/* all auto */
    /* DR 69 (loop closure debounce interval) */
    dr_write_check (sts, 69, 0x0a, drval, init_restart);	/* 12.5 ms */
    /* DR 70 (ring trip debounce interval) */
    dr_write_check (sts, 70, 0x0a, drval, init_restart);	/* 12.5 ms */

    /* set DRs 65-66, 71-73 */
    /* 65 (external bipolar transistor control) left to default 0x61 */
    /* 66 (battery feed control) Vov/Track set during DC-DC converter powerup */
    /* 71 (loop current limit) left to default 0x00 (20mA) */
    /* 72 (on-hook line voltage) left to default 0x20 (48V) */
    /* 73 (common-mode voltage) left to default 0x02 (3V) [6V in Zaptel?] */

    /* write indirect registers 35--39 */
    ir_write (sts, 35, 0x8000, irval, init_restart); /* loop closure filter */
    ir_write (sts, 36, 0x0320, irval, init_restart); /* ring trip filter */
    /* IRs 37--39 are set as per AN47 p.4 */
    ir_write (sts, 37, 0x0010, irval, init_restart); /* therm lp pole Q1Q2 */
    ir_write (sts, 38, 0x0010, irval, init_restart); /* therm lp pole Q3Q4 */
    ir_write (sts, 39, 0x0010, irval, init_restart); /* therm lp pole Q5Q6 */
    
    /* select PCM ulaw, enable PCM I/O and set txs/rxs */
    dr_write_check (sts, 1, 0x28, drval, init_restart);
#if 1
    /* already set to zero, #if'ed in to set to 1 */
    dr_write_check (sts, 2, 0x01, drval, init_restart); /* txs lowb set to 1 */
    dr_write_check (sts, 4, 0x01, drval, init_restart); /* rxs lowb set to 1 */
#endif

    /* set line mode to forward active */
    dr_write (sts, 64, 0x01, drval, init_restart);

    /* that's it, we 're done! */

    dump_direct_regs ("finally", dev);

    /* mark our state as OK */
    spin_lock_irqsave (&dev->statelck, flags);
    dev->state = OPENUSBFXS_STATE_OK;
    spin_unlock_irqrestore (&dev->statelck, flags);

    /* tell the board to start PCM I/O */
    start_stop_io (dev, 0);

    /* start isochronous reads/writes by spawning as many urbs as requested */
    // TODO: move this somewhere else, like in a "start/stop" ioctl
    for (i = 0; i < rurbsinflight; i++) {
        int __ret;
	__ret = openusbfxs_isoc_in__submit (dev, GFP_KERNEL);
	if (__ret < 0) {
	  OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	    "%s: isoc_submit (in) returns %d", __func__, __ret);
	}
    }

    for (i = 0; i < wurbsinflight; i++) {
	int __ret;
	__ret = openusbfxs_isoc_out_submit (dev, GFP_KERNEL);
	if (__ret < 0) {
	  OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
	    "%s: isoc_submit (out) returns %d", __func__, __ret);
	}
    }

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE,
      "%s: board setup completed successfully", __func__);

}


/* probe function: called whenever kernel sees our device plugged in (again) */
static int openusbfxs_probe (struct usb_interface *intf,
  const struct usb_device_id *id)
{
    struct openusbfxs_dev *dev;
    struct usb_host_interface *intf_desc;
    struct usb_endpoint_descriptor *epd;
    size_t pcktsize;
    char wqname[40];
    struct urb *urb;
    // TODO: add more stuff here as needed
    int i, j;
    int retval = -ENODEV;

    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGDEBUGGING, "probe()");

    /* allocate memory for our device state */
    dev = kzalloc (sizeof (*dev), GFP_KERNEL);	/* (note: may sleep) */
    if (!dev) {
    	OPENUSBFXS_ERR ("Out of memory while allocating device state");
	goto probe_error;
    }

    /* initialize  device state reference and locking structures */
    kref_init (&dev->kref);
    mutex_init (&dev->iomutex);
    mutex_init (&dev->rdmutex);
    mutex_init (&dev->wrmutex);
    spin_lock_init (&dev->statslck);
    spin_lock_init (&dev->statelck);

    /* initialize wait queues */

    /* initialize buffers (apart from the usb-specific DMA buffers) */

    /* initialize state */
    dev->state = OPENUSBFXS_STATE_IDLE;

    /* initialize USB stuff */
    init_usb_anchor (&dev->submitted);
    dev->udev = usb_get_dev (interface_to_usbdev (intf));
    dev->intf = intf;

    /* setup endpoint information */

    /* TODO (some day): contrary to what the USB standard mandates, current
     * OpenUSBFXS descriptor lists isochronous endpoints with non-zero packet
     * sizes in the main configuration; to conform, we ought to list zero-size
     * packets for these endpoints in the main configuration and include one
     * alternate configuration with non-zero packet sizes, in which case, we
     * should also arrange this loop to look for the other alternative
     * configuration(s).
     * [NB: the purpose of this requirement in the standard is to eliminate
     * the possibility of the host failing the device in the enumeration
     * stage because of bandwidth lack in the overall USB bus; but since
     * OpenUSBFXS just eats 128kbps of isochronous bandwidth, chances that
     * this will happen are really very limited].
     */

    /* loop around EPs in current int, noting EP addresses and sizes */
    intf_desc = intf->cur_altsetting;
    for (i = 0; i < intf_desc->desc.bNumEndpoints; i++) {
    	epd = &intf_desc->endpoint[i].desc;

	/* if we haven't yet a bulk IN EP and found one, mark it as ours */
	if (!dev->ep_bulk_in && usb_endpoint_is_bulk_in (epd)) {
	    pcktsize = le16_to_cpu (epd->wMaxPacketSize);
	    dev->ep_bulk_in = epd->bEndpointAddress;
	    dev->bulk_in_size = pcktsize;
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
	      "bulk IN  endpoint found, EP#%d, size:%d",
	      dev->ep_bulk_in, dev->bulk_in_size);
	    // TODO: allocate a buffer for this, goto error if alloc fails
	}

	/* same for a bulk OUT EP */
	else if (!dev->ep_bulk_out && usb_endpoint_is_bulk_out (epd)) {
	    pcktsize = le16_to_cpu (epd->wMaxPacketSize);
	    dev->ep_bulk_out = epd->bEndpointAddress;
	    dev->bulk_out_size = pcktsize;
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
	      "bulk OUT endpoint found, EP#%d, size:%d",
	      dev->ep_bulk_out, dev->bulk_out_size);
	    // TODO: allocate a buffer for this, goto error if alloc fails (??)
	}

	/* same for an isochronous IN EP */
	else if (!dev->ep_isoc_in && usb_endpoint_is_isoc_in (epd)) {
	    pcktsize = le16_to_cpu (epd->wMaxPacketSize);
	    dev->ep_isoc_in = epd->bEndpointAddress;
	    dev->isoc_in_size = pcktsize;
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
	      "isoc IN  endpoint found, EP#%d, size:%d",
	      dev->ep_isoc_in, dev->isoc_in_size);
	    // TODO: probably I 'll fix this to 16 bytes, because leaving it
	    // variable is too much trouble and does not make sense anyway.
	    // so it would be something like:
	    // if (pkctsize != sizeof (some_struct_in_dot_h_file)) { err...}
	}

	/* same for an isochronous OUT EP */
	else if (!dev->ep_isoc_out && usb_endpoint_is_isoc_out (epd)) {
	    pcktsize = le16_to_cpu (epd->wMaxPacketSize);
	    dev->ep_isoc_out = epd->bEndpointAddress;
	    dev->isoc_out_size = pcktsize;
	    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
	      "isoc OUT endpoint found, EP#%d, size:%d",
	      dev->ep_isoc_out, dev->isoc_out_size);
	    // TODO: checks, size and buffer: see above TODO note
	}

	/* complain (but don't fail) on other EPs */
	else {
	    OPENUSBFXS_ERR ("Unexpected endpoint #%d", epd->bEndpointAddress);
	}
    }

    /* at end of loop, make sure we have found all four required endpoints */
    if (!(dev->ep_bulk_in && dev->ep_bulk_out &&
      dev->ep_isoc_in && dev->ep_isoc_out)) {
        OPENUSBFXS_ERR (
	  "Board does not support all required bulk/isoc endpoints??");
	goto probe_error;
    }

    /* tell USB core's PM we do not want to have the device autosuspended */
    retval = usb_autopm_get_interface (intf);
    if (retval) {
        OPENUSBFXS_ERR ("Call to autopm_get_interface failed with %d", retval);
	goto probe_error;
    }

    /* save back-pointer to our dev structure within usb interface */
    usb_set_intfdata (intf, dev);

    /* try registering the device with the USB core */
    retval = usb_register_dev (intf, &openusbfxs_class);
    if (retval) {
    	OPENUSBFXS_ERR ("Unable to register device and get a minor");
	usb_set_intfdata (intf, NULL);
	goto probe_error;
    }

    /* initialize all isochronous buffers, urbs, locks etc. */
    dev->outsubmit = 0;
    dev->writenext = 0;
    spin_lock_init (&dev->outbuflock);
    init_waitqueue_head (&dev->outwqueue);
    dev->twbcount = 0;
    for (i = 0; i < OPENUSBFXS_MAXURB; i++) {
        spin_lock_init (&dev->outbufs[i].lock);
	dev->outbufs[i].state = st_empty;
	dev->outbufs[i].dev = dev;	/* back-pointer (for context) */
	/* allocate urb */
	dev->outbufs[i].urb = usb_alloc_urb (wpacksperurb, GFP_KERNEL);
	if (dev->outbufs[i].urb == NULL) {
	    OPENUSBFXS_ERR (
	      "Out of memory while allocating isochronous OUT urbs");
	    retval = -ENOMEM;
	    goto probe_error;
	}
	/* allocate buffer for urb */
	dev->outbufs[i].buf = usb_buffer_alloc (dev->udev,OPENUSBFXS_MAXOBUFLEN,
	  GFP_KERNEL, &dev->outbufs[i].urb->transfer_dma);
	if (dev->outbufs[i].buf == NULL) {
	    OPENUSBFXS_ERR (
	      "Out of memory while allocating isochronous OUT buffers");
	    retval = -ENOMEM;
	    goto probe_error;
	}
	dev->outbufs[i].len = 0;
	/* pre-initialize urb */
	urb = dev->outbufs[i].urb;
	urb->interval = 1;	/* send at every microframe */
	urb->dev = dev->udev;	/* chain to this USB device */
	urb->pipe = usb_sndisocpipe (dev->udev, dev->ep_isoc_out);
	urb->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
	urb->transfer_buffer = dev->outbufs[i].buf;
	urb->transfer_buffer_length = 0;	/* to be set just-in-time */
	urb->complete = openusbfxs_isoc_out_cbak;
	urb->context = &dev->outbufs[i];	/* unusual, but we need outbuf*/
	urb->start_frame = 0;
	urb->number_of_packets = 0;		/* to be set just-in-time */
	for (j = 0; j < wpacksperurb; j++) {
	    urb->iso_frame_desc[j].offset = j * OPENUSBFXS_DPACK_SIZE;
	    urb->iso_frame_desc[j].length = OPENUSBFXS_DPACK_SIZE;
	}
    }

    /* same for read() structures */
    dev->in_submit = 0;
    dev->read_next = 0;
    spin_lock_init (&dev->in_buflock);
    init_waitqueue_head (&dev->in_wqueue);
    dev->trbcount = 0;
    dev->trboffst = 0;
    for (i = 0; i < OPENUSBFXS_MAXURB; i++) {
        spin_lock_init (&dev->in_bufs[i].lock);
	dev->in_bufs[i].state = st_empty;
	dev->in_bufs[i].dev = dev;
	/* allocate urb */
	dev->in_bufs[i].urb = usb_alloc_urb (rpacksperurb, GFP_KERNEL);
	if (dev->in_bufs[i].urb == NULL) {
	    OPENUSBFXS_ERR (
	      "Out of memory while allocating isochronous IN urbs");
	    retval = -ENOMEM;
	    goto probe_error;
	}
	/* allocate buffer for urb */
	dev->in_bufs[i].buf = usb_buffer_alloc (dev->udev,OPENUSBFXS_MAXIBUFLEN,
	  GFP_KERNEL, &dev->in_bufs[i].urb->transfer_dma);
	if (dev->in_bufs[i].buf == NULL) {
	    OPENUSBFXS_ERR (
	      "Out of memory while allocating isochronous IN buffers");
	    retval = -ENOMEM;
	    goto probe_error;
	}
	dev->in_bufs[i].len = 0;
	/* pre-initialize urb */
	urb = dev->in_bufs[i].urb;
	urb->interval = 1;	/* send at every microframe */
	urb->dev = dev->udev;	/* chain to this USB device */
	urb->pipe = usb_rcvisocpipe (dev->udev, dev->ep_isoc_in);
	urb->transfer_flags = URB_ISO_ASAP | URB_NO_TRANSFER_DMA_MAP;
	urb->transfer_buffer = dev->in_bufs[i].buf;
	urb->transfer_buffer_length = OPENUSBFXS_MAXIBUFLEN;
	urb->complete = openusbfxs_isoc_in__cbak;
	urb->context = &dev->in_bufs[i];	/* unusual, but we need in_buf*/
	urb->start_frame = 0;
	urb->number_of_packets = rpacksperurb;	/* to be set just-in-time (?) */
	for (j = 0; j < rpacksperurb; j++) {
	    urb->iso_frame_desc[j].offset = j * OPENUSBFXS_DPACK_SIZE;
	    urb->iso_frame_desc[j].length = OPENUSBFXS_DPACK_SIZE;
	}
    }

    /* create a unique name for our workqueue, then create the queue itself */
    sprintf (wqname, "openusbfxs%d", intf->minor);
    dev->iniwq = create_singlethread_workqueue (wqname);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20)
    /* workqueue kernel API changed in 2.6.20 */
    INIT_WORK (&dev->iniwt, openusbfxs_setup);
#else
    INIT_WORK (&dev->iniwt, openusbfxs_setup, dev);
#endif
    if (!queue_work (dev->iniwq, &dev->iniwt)) {
	OPENUSBFXS_ERR (
	  "in %s, queue_work() returns zero (already queued)", __func__);
    }

    OPENUSBFXS_INFO ("OpenUSBFXS device attached to openusbfxs#%d", intf->minor);
    return 0;

probe_error:
    for (i = 0; i < OPENUSBFXS_MAXURB; i++) {
	/* first free buffers */
	if (dev->outbufs[i].buf) {
	    usb_buffer_free (dev->udev, OPENUSBFXS_MAXOBUFLEN,
	      dev->outbufs[i].buf, dev->outbufs[i].urb->transfer_dma);
	}
	if (dev->in_bufs[i].buf) {
	    usb_buffer_free (dev->udev, OPENUSBFXS_MAXIBUFLEN,
	      dev->in_bufs[i].buf, dev->in_bufs[i].urb->transfer_dma);
	}

	/* then free urbs */
        if (dev->outbufs[i].urb) {
	    usb_free_urb (dev->outbufs[i].urb);
	}
	if (dev->in_bufs[i].urb) {
	    usb_free_urb (dev->in_bufs[i].urb);
	}
    }
    if (dev) {
    	/* decrement reference count and call our "destructor" function */
	kref_put (&dev->kref, openusbfxs_delete);
    }
    return retval;
}

static void openusbfxs_disconnect (struct usb_interface *intf)
{
    struct openusbfxs_dev *dev;
    int minor = intf->minor;
    unsigned long flags;	/* irqsave flags */

    /* get us a pointer to dev, then clear it from the USB interface */
    dev = usb_get_intfdata (intf);
    usb_set_intfdata (intf, NULL);

    /* tell USB core's PM that we don't need autosuspend turned off anymore */
    usb_autopm_put_interface (intf);

    /* note: this is needed here, although the functionality also exists
     * in openusbfxs_delete(), because _delete() is not called until the
     * last kref from open()ing the device is release()d. Therefore, we
     * need to alter the state to tell read(), write() and ioctl() that
     * they should fail
     */
    spin_lock_irqsave (&dev->statelck, flags);
    dev->state = OPENUSBFXS_STATE_UNLOAD;
    spin_unlock_irqrestore (&dev->statelck, flags);
    wake_up_interruptible (&dev->in_wqueue);
    wake_up_interruptible (&dev->outwqueue);

    /* return our minor dev back to the kernel (via USB core) */
    usb_deregister_dev (intf, &openusbfxs_class);

    /* prevent any filesystem-based I/O from starting, unset link to USB */
    mutex_lock (&dev->iomutex);
    dev->intf = NULL;
    mutex_unlock (&dev->iomutex);

    usb_kill_anchored_urbs(&dev->submitted);
    kref_put (&dev->kref, openusbfxs_delete);

    OPENUSBFXS_INFO ("openusbfxs#%d is now disconnected", minor);
}

/* interesting compiler tags:
  __init 
  __initdata [for data only]
  __exit
*/

static int __init openusbfxs_init(void)
{
    int retval;

    printk (KERN_INFO "openusbfxs driver v%s loading\n", driverversion);
    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGTERSE, "debug level is %d", debuglevel);

    // TODO: wpacksperurb == 1 behaves strangely, I have to check why;
    // meanwhile, this value is disabled here
    if (wpacksperurb < 2 || wpacksperurb > OPENUSBFXS_MAXPCKPERURB) {
        printk (KERN_ERR
	  "parameter error: wpacksperurb must be between 2 and %d\n",
	  OPENUSBFXS_MAXPCKPERURB);
        return -EINVAL;
    }
    if (wurbsinflight < 2 || wurbsinflight > OPENUSBFXS_MAXINFLIGHT) {
        printk (KERN_ERR
	  "parameter error: wurbsinflight must be between 2 and %d\n",
	  OPENUSBFXS_INFLIGHT);
	return -EINVAL;
    }
    if (rpacksperurb < 2 || rpacksperurb > OPENUSBFXS_MAXPCKPERURB) {
        printk (KERN_ERR
	  "parameter error: rpacksperurb must be between 2 and %d\n",
	  OPENUSBFXS_MAXPCKPERURB);
        return -EINVAL;
    }
    if (rurbsinflight < 2 || rurbsinflight > OPENUSBFXS_MAXINFLIGHT) {
        printk (KERN_ERR
	  "parameter error: wurbsinflight must be between 2 and %d\n",
	  OPENUSBFXS_INFLIGHT);
	return -EINVAL;
    }

    retval = usb_register (&openusbfxs_driver);
    if (retval) {
        OPENUSBFXS_ERR ("usb_register failed, error=%d", retval);
    }

    // return values other than 0 are errors, see <linux/errno.h>

    return retval;
}

static void __exit openusbfxs_exit(void)
{
    OPENUSBFXS_DEBUG (OPENUSBFXS_DBGVERBOSE,
      "deregistering driver from usb core");
    usb_deregister (&openusbfxs_driver);
    printk (KERN_INFO "openusbfxs driver unloaded\n");
}

module_init(openusbfxs_init);
module_exit(openusbfxs_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Angelos Varvitsiotis <avarvit-at-gmail-dot-com>");
MODULE_DESCRIPTION("Open USB FXS driver module");
//MODULE_ALIAS
MODULE_DEVICE_TABLE (usb, openusbfxs_dev_table);
