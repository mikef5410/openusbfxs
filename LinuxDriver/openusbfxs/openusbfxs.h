// MISSING: license, macro documentation

#ifndef OPENUSBFXS_H



/* our debug level and default facility */
#define OPENUSBFXS_DBGNONE	0
#define OPENUSBFXS_DBGTERSE	1
#define OPENUSBFXS_DBGVERBOSE	2
#define OPENUSBFXS_DBGDEBUGGING	3 /* may clog console/syslog, use at own risk */
#define OPENUSBFXS_FACILITY	KERN_INFO

#define	DEBUGGING	/* turns on verbose debugging: undefine in production */

/* various states a board may be in	*/
#define OPENUSBFXS_STATE_IDLE	0	/* not initialized		*/
#define OPENUSBFXS_STATE_INIT	1	/* initializing			*/
#define OPENUSBFXS_STATE_WAIT	2	/* waiting for device to respond*/
#define OPENUSBFXS_STATE_OK	3	/* initialized			*/
#define OPENUSBFXS_STATE_ERROR	4	/* in error			*/
#define OPENUSBFXS_STATE_UNLOAD 5	/* driver unloading		*/

/* chunk size (8 bytes = 1ms) */
#define OPENUSBFXS_CHUNK_BITS	3
#define OPENUSBFXS_CHUNK_SIZE	(1 << OPENUSBFXS_CHUNK_BITS)

#ifdef __KERNEL__
#include <linux/usb.h>
#include <linux/ioctl.h>

/* our vendor and product ids (note: sub-licensed from Microchip) */
#define OPENUSBFXS_VENDOR_ID	0x04D8
#define OPENUSBFXS_PRODUCT_ID	0xFCF1

/* minor device number base number (kernel may change this while registering) */
#define OPENUSBFXS_MINOR_BASE	192	/* TODO: check with kernel devs	*/	

#define OPENUSBFXS_MAXURB	16	/* maximum number of isoc URBs */
#define OPENUSBFXS_INFLIGHT	 4	/* number of isoc URBs in flight */
#define OPENUSBFXS_MAXINFLIGHT	 8	/* max isoc URBs in flight */
#define OPENUSBFXS_MAXPCKPERURB	32	/* max number of packets per isoc URB */


/* debug macros for userland and kernel */
#ifdef DEBUGGING
#define OPENUSBFXS_DEBUG(level, fmt, args...)	\
    if (debuglevel >= (level)) \
      printk (OPENUSBFXS_FACILITY KBUILD_MODNAME ": " fmt "\n", ## args)
#define OPENUSBFXS_ERR(fmt, args...) \
    printk (KERN_ERR KBUILD_MODNAME ": " fmt "\n", ## args)
#define OPENUSBFXS_WARN(fmt, args...) \
    printk (KERN_WARNING KBUILD_MODNAME ": " fmt "\n", ## args)
#define OPENUSBFXS_INFO(fmt, args...) \
    printk (KERN_INFO KBUILD_MODNAME ": " fmt "\n", ## args)
#else /* DEBUGGING */
#define OPENUSBFXS_DEBUG(level, fmt, args...)
#endif /* DEBUGGING */

#else /* __KERNEL__ */
#include <sys/ioctl.h>

#ifdef OPENUSBFXS_DEBUG_SYSLOG
#define OPENUSBFXS_DEBUG(level, fmt, args...)	/* TODO: write a syslog macro */
#else /* OPENUSBFXS_DEBUG_SYSLOG */
#define OPENUSBFXS_DEBUG(level, fmt, args...)	\
    if (debuglevel >= (level)) fprintf (stderr, fmt, ## args)
#endif /* OPENUSBFXS_DEBUG_SYSLOG */


#endif /* __KERNEL__ */

/* returned by OPENUSBFXS_IOCGSTATS */
struct openusbfxs_stats {
    int errors;
    enum {none, err_in, err_out} lasterrop;
    ulong in_overruns;
    ulong in_missed;
    ulong in_badframes;
    ulong out_underruns;
    ulong out_missed;
};

/* openusbfxs magic ioctl number */
#define OPENUSBFXS_IOC_MAGIC	'X'

/* reset and reinitialize board */
#define OPENUSBFXS_IOCRESET	_IO(OPENUSBFXS_IOC_MAGIC, 0)
/* cause a register dump (requires compilation with DEBUG set to VERBOSE) */
#define OPENUSBFXS_IOCREGDMP	_IO(OPENUSBFXS_IOC_MAGIC, 1)
/* set ringing on/off: zero sets ring off, non-zero sets ring on */
#define OPENUSBFXS_IOCSRING	_IO(OPENUSBFXS_IOC_MAGIC, 2)
/* set linefeed mode: zero is 'open' mode; non-zero is 'forward active' mode */
#define OPENUSBFXS_IOCSLMODE	_IO(OPENUSBFXS_IOC_MAGIC, 3)
/* get hook state: returns zero if on-hook, one if off-hook */
#define OPENUSBFXS_IOCGHOOK	_IOR(OPENUSBFXS_IOC_MAGIC, 4, int)
/* generic event (hook state or DTMF): 0 is no event, others ??? */
#define OPENUSBFXS_IOCGDTMF	_IOR(OPENUSBFXS_IOC_MAGIC, 5, int)
/* get and reset statistics on errors etc. */
#define OPENUSBFXS_IOCGSTATS	_IOR(OPENUSBFXS_IOC_MAGIC, 6, struct openusbfxs_stats)
#define OPENUSBFXS_MAX_IOCTL	6

#endif /* OPENUSBFXS_H */
