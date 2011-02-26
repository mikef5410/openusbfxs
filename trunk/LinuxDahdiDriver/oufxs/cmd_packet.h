#ifndef CMD_PACKET_H
#define CMD_PACKET_H
#ifndef __KERNEL__
typedef unsigned char __u8;
#endif

/* commands that the OpenUSBFXS board understands */
enum enum_oufxs_cmd {
    READ_VERSION	= 0x00,

    GET_FXS_VERSION	= 0x60,		/* get the FXS firmware version	*/
    WRITE_SERIAL_NO	= 0x61,		/* write serial # in eeprom	*/
    REBOOT_BOOTLOAD	= 0x62,		/* reboot in bootloader mode	*/
   	
    START_STOP_IOV2	= 0x7D,		/* cease/start PCM I/O vrsn.2	*/
    START_STOP_IO	= 0x7E,		/* cease/start PCM I/O		*/
    SOF_PROFILE		= 0x7F,		/* perform SOF profiling	*/

    PROSLIC_SCURRENT	= 0x80,		/* unimplemented		*/
    PROSLIC_RCURRENT	= 0x81,		/* unimplemented		*/
    PROSLIC_RDIRECT	= 0x82,		/* read direct register		*/
    PROSLIC_WDIRECT	= 0x83,		/* write direct register	*/
    PROSLIC_RDINDIR	= 0x84,		/* read indirect register	*/
    PROSLIC_WRINDIR	= 0x85,		/* write indirect register	*/
#ifdef DO_RESET_PROSLIC
    PROSLIC_RESET	= 0x8F,		/* reset ProSLIC		*/
#endif /* DO_RESET_PROSLIC */

    RESET		= 0xFF
};

union oufxs_packet {
#if 0
    struct {
    	__u8	cmd;
	__u8	len;
    }		rdvrsn_req;
#endif
    struct {
        __u8	cmd;
	__u8	len;
	__u8	mjr;
	__u8	mnr;
    }		rdvrsn_rpl;
    struct {
        __u8	cmd;
    }		fxsvsn_req;
    struct {
        __u8	cmd;
	__u8	rsv;
	__u8	maj;
	__u8	min;
	__u8	rev;
    }		fxsvsn_rpl;
    struct {
        __u8	cmd;
	__u8	rsv;
	__u8	str[4];
    }		serial_req, serial_rpl;
    struct {
        __u8	cmd;
    }		bootload_req; /* no reply for this, board just reboots */
    struct {
	__u8	cmd;
	__u8	reg;
    }		rdirect_req, rdindir_req;
    struct {
	__u8	cmd;
	__u8	reg;
	__u8	val;
    }		rdirect_rpl, wdirect_req, wdirect_rpl;
    struct {
        __u8	cmd;
	__u8	reg;
	__u16	val;
    }		rdindir_rpl, wrindir_req, wrindir_rpl;
    struct {
        __u8	cmd;
	__u8	val;
    }		strtstp_req;
    struct {
        __u8	cmd;
	__u8	val;
	__u8	seq;	/* DR setting sequence # */
    }		strtstpv2_req;
    struct {
	__u8	cmd;
    }		sofprof_req;
    struct {
    	__u8	cmd;
	__u8	rsv;
	__u16	sof[15];
    }		sofprof_rpl;
#ifdef DO_RESET_PROSLIC
    struct {
        __u8	cmd;
    }		slicrst_req;
    struct {
        __u8	cmd;
	__u8	rsv;
    }		slicrst_rpl;
#endif /* DO_RESET_PROSLIC */
#ifdef DO_RESET_BOARD
    struct {
        __u8	cmd;
    }		brdrst_req;
    /* as you can guess, there is no reply to this... */
#endif /* DO_RESET_BOARD */
};

#define GET_FXS_VERSION_REQ()		\
		{.fxsvsn_req={.cmd=GET_FXS_VERSION}}
#define WRITE_SERIAL_NO_REQ(s3,s2,s1,s0) \
		{.serial_req={.cmd=WRITE_SERIAL_NO, \
		 .str[0]=s3,.str[1]=s2,.str[2]=s1,.str[3]=s0}}
#define REBOOT_BOOTLOADER_REQ()		\
		{.bootload_req={.cmd=REBOOT_BOOTLOAD}}
#define	PROSLIC_RDIRECT_REQ(r)		\
		{.rdirect_req={.cmd=PROSLIC_RDIRECT,.reg=(r)}}
#define PROSLIC_WDIRECT_REQ(r,v)	\
		{.wdirect_req={.cmd=PROSLIC_WDIRECT,.reg=(r),.val=(v)}}
#define PROSLIC_RDINDIR_REQ(r)		\
		{.rdindir_req={.cmd=PROSLIC_RDINDIR,.reg=(r)}}
#define	PROSLIC_WRINDIR_REQ(r,v)	\
	  {.wrindir_req={.cmd=PROSLIC_WRINDIR,.reg=(r),.val=cpu_to_le16(v)}}
#ifdef DO_RESET_PROSLIC
#define PROSLIC_RESET_REQ()		\
		{.slicrst_req={.cmd=PROSLIC_RESET}}
#endif /* DO_RESET_PROSLIC */
#ifdef DO_RESET_BOARD
#define BOARD_RESET_REQ()		\
		{.brdrst_req={.cmd=RESET}}
#endif /* DO_RESET_BOARD */
#define START_STOP_IO_REQ(v)		\
		{.strtstp_req={.cmd=START_STOP_IO,.val=(v)}}
#define START_STOP_IOV2_REQ(v,s)	\
		{.strtstpv2_req={.cmd=START_STOP_IOV2,.val=(v),.seq=(s)}}
#define SOFPROFILE_REQ()			\
		{.sofprof_req={.cmd=SOF_PROFILE}}


#define PROSLIC_RDIRECT_RPV(p)	p.rdirect_rpl.val
#define PROSLIC_WDIRECT_RPV(p)	p.wdirect_rpl.val
#define PROSLIC_RDINDIR_RPV(p)	le16_to_cpu(p.rdindir_rpl.val)
#define PROSLIC_WRINDIR_RPV(p)	le16_to_cpu(p.wrindir_rpl.val)
#define SOFPROFILE_TMRVAL(p,i)	le16_to_cpu(p.sof[i])


#define OUFXS_DTHDR_SIZE	8
union oufxs_data {
    struct {
	__u8	header[OUFXS_DTHDR_SIZE];
	__u8	sample[OUFXS_CHUNK_SIZE];
    }		oblique;
    struct {
	__u8	unusd1[3];
	__u8	outseq;
	__u8	drsseq;
	__u8	drsreg;
	__u8	drsval;
	__u8	unusd2[1];
        __u8	sample[OUFXS_CHUNK_SIZE];
    }		outpack;
    struct {
	__u8	magic1;
	__u8	oddevn;		/* 0xdd for odd, 0xee for even packets */
	__u8	rsrvd1;
	__u8	moutsn;		/* mirror of former out-packet seq # */
	__u8	rsrvd2[4];
        __u8	sample[OUFXS_CHUNK_SIZE];
    }		in_pack;	/* yet unidentified in-packet */
    struct {
        __u8	magic1;		/* should always be 0xba */
	__u8	oddevn;		/* 0xdd for odd packets */
	__u8	hkdtmf;
	__u8	moutsn;
	__u16	tmr3lv;
	__u8	losses;
	__u8	inoseq;
        __u8	sample[OUFXS_CHUNK_SIZE];
    }		inopack;	/* odd in-packet */
    struct {
    	__u8	magic1;		/* should always be 0xba */
	__u8	oddevn;		/* 0xee for even packets */
	__u8	unusd1;
	__u8	moutsn;
	__u8	unusd3[3];
	__u8	ineseq;
        __u8	sample[OUFXS_CHUNK_SIZE];
    }		inepack;	/* even in-packet */
};
#define OUFXS_DPACK_SIZE	sizeof(union oufxs_data)


#if 0
#define	REQ_RESET()			\
    {.cmd=RESET,.len=2}
#endif /* 0 */

#endif /* CMD_PACKET_H */
