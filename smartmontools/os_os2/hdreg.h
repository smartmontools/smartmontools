#ifndef _LINUX_HDREG_H
#define _LINUX_HDREG_H

/*
 * This file contains some defines for the AT-hd-controller.
 * Various sources.  
 */

#define HD_IRQ 14		/* the standard disk interrupt */

/* ide.c has its own port definitions in "ide.h" */

/* Hd controller regs. Ref: IBM AT Bios-listing */
#define HD_DATA		0x1f0	/* _CTL when writing */
#define HD_ERROR	0x1f1	/* see err-bits */
#define HD_NSECTOR	0x1f2	/* nr of sectors to read/write */
#define HD_SECTOR	0x1f3	/* starting sector */
#define HD_LCYL		0x1f4	/* starting cylinder */
#define HD_HCYL		0x1f5	/* high byte of starting cyl */
#define HD_CURRENT	0x1f6	/* 101dhhhh , d=drive, hhhh=head */
#define HD_STATUS	0x1f7	/* see status-bits */
#define HD_FEATURE HD_ERROR	/* same io address, read=error, write=feature */
#define HD_PRECOMP HD_FEATURE	/* obsolete use of this port - predates IDE */
#define HD_COMMAND HD_STATUS	/* same io address, read=status, write=cmd */

#define HD_CMD		0x3f6	/* used for resets */
#define HD_ALTSTATUS	0x3f6	/* same as HD_STATUS but doesn't clear irq */

/* remainder is shared between hd.c, ide.c, ide-cd.c, and the hdparm utility */

/* Bits of HD_STATUS */
#define ERR_STAT	0x01
#define INDEX_STAT	0x02
#define ECC_STAT	0x04	/* Corrected error */
#define DRQ_STAT	0x08
#define SEEK_STAT	0x10
#define WRERR_STAT	0x20
#define READY_STAT	0x40
#define BUSY_STAT	0x80

/* Values for HD_COMMAND */
#define WIN_RESTORE		0x10
#define WIN_READ		0x20
#define WIN_WRITE		0x30
#define WIN_WRITE_VERIFY	0x3C
#define WIN_VERIFY		0x40
#define WIN_FORMAT		0x50
#define WIN_INIT		0x60
#define WIN_SEEK		0x70
#define WIN_DIAGNOSE		0x90
#define WIN_SPECIFY		0x91	/* set drive geometry translation */
#define WIN_IDLEIMMEDIATE	0xE1	/* force drive to become "ready" */
#define WIN_SETIDLE1		0xE3
#define WIN_SETIDLE2		0x97

#define WIN_STANDBYNOW1		0xE0
#define WIN_STANDBYNOW2		0x94
#define WIN_SLEEPNOW1		0xE6
#define WIN_SLEEPNOW2		0x99
#define WIN_CHECKPOWERMODE1	0xE5
#define WIN_CHECKPOWERMODE2	0x98

#define WIN_DOORLOCK		0xDE	/* lock door on removable drives */
#define WIN_DOORUNLOCK		0xDF	/* unlock door on removable drives */

#define WIN_MULTREAD		0xC4	/* read sectors using multiple mode */
#define WIN_MULTWRITE		0xC5	/* write sectors using multiple mode */
#define WIN_SETMULT		0xC6	/* enable/disable multiple mode */
#define WIN_IDENTIFY		0xEC	/* ask drive to identify itself	*/
#define WIN_IDENTIFY_DMA	0xEE	/* same as WIN_IDENTIFY, but DMA */
#define WIN_SETFEATURES		0xEF	/* set special drive features */
#define WIN_READDMA		0xC8	/* read sectors using DMA transfers */
#define WIN_WRITEDMA		0xCA	/* write sectors using DMA transfers */

#define WIN_QUEUED_SERVICE	0xA2	/* */
#define WIN_READDMA_QUEUED	0xC7	/* read sectors using Queued DMA transfers */
#define WIN_WRITEDMA_QUEUED	0xCC	/* write sectors using Queued DMA transfers */

#define WIN_READ_BUFFER		0xE4	/* force read only 1 sector */
#define WIN_WRITE_BUFFER	0xE8	/* force write only 1 sector */

#define WIN_SMART		0xB0	/* self-monitoring and reporting */

/* Additional drive command codes used by ATAPI devices. */
#define WIN_PIDENTIFY		0xA1	/* identify ATAPI device	*/
#define WIN_SRST		0x08	/* ATAPI soft reset command */
#define WIN_PACKETCMD		0xA0	/* Send a packet command. */

#define DISABLE_SEAGATE		0xFB
#define EXABYTE_ENABLE_NEST	0xF0

/* WIN_SMART sub-commands */

#define SMART_READ_VALUES	0xd0
#define SMART_READ_THRESHOLDS	0xd1
#define SMART_AUTOSAVE		0xd2
#define SMART_SAVE		0xd3
#define SMART_IMMEDIATE_OFFLINE	0xd4
#define SMART_READ_LOG_SECTOR	0xd5
#define SMART_WRITE_LOG_SECTOR	0xd6
#define SMART_ENABLE		0xd8
#define SMART_DISABLE		0xd9
#define SMART_STATUS		0xda
#define SMART_AUTO_OFFLINE	0xdb

/* WIN_SETFEATURES sub-commands */

#define SETFEATURES_EN_WCACHE	0x02	/* Enable write cache */
#define SETFEATURES_XFER	0x03	/* Set transfer mode */
#	define XFER_UDMA_7	0x47	/* 0100|0111 */
#	define XFER_UDMA_6	0x46	/* 0100|0110 */
#	define XFER_UDMA_5	0x45	/* 0100|0101 */
#	define XFER_UDMA_4	0x44	/* 0100|0100 */
#	define XFER_UDMA_3	0x43	/* 0100|0011 */
#	define XFER_UDMA_2	0x42	/* 0100|0010 */
#	define XFER_UDMA_1	0x41	/* 0100|0001 */
#	define XFER_UDMA_0	0x40	/* 0100|0000 */
#	define XFER_MW_DMA_2	0x22	/* 0010|0010 */
#	define XFER_MW_DMA_1	0x21	/* 0010|0001 */
#	define XFER_MW_DMA_0	0x20	/* 0010|0000 */
#	define XFER_SW_DMA_2	0x12	/* 0001|0010 */
#	define XFER_SW_DMA_1	0x11	/* 0001|0001 */
#	define XFER_SW_DMA_0	0x10	/* 0001|0000 */
#	define XFER_PIO_4	0x0C	/* 0000|1100 */
#	define XFER_PIO_3	0x0B	/* 0000|1011 */
#	define XFER_PIO_2	0x0A	/* 0000|1010 */
#	define XFER_PIO_1	0x09	/* 0000|1001 */
#	define XFER_PIO_0	0x08	/* 0000|1000 */
#	define XFER_PIO_SLOW	0x00	/* 0000|0000 */
#define SETFEATURES_DIS_DEFECT	0x04	/* Disable Defect Management */
#define SETFEATURES_EN_APM	0x05	/* Enable advanced power management */
#define SETFEATURES_DIS_MSN	0x31	/* Disable Media Status Notification */
#define SETFEATURES_DIS_RLA	0x55	/* Disable read look-ahead feature */
#define SETFEATURES_EN_RI	0x5D	/* Enable release interrupt */
#define SETFEATURES_EN_SI	0x5E	/* Enable SERVICE interrupt */
#define SETFEATURES_DIS_RPOD	0x66	/* Disable reverting to power on defaults */
#define SETFEATURES_DIS_WCACHE	0x82	/* Disable write cache */
#define SETFEATURES_EN_DEFECT	0x84	/* Enable Defect Management */
#define SETFEATURES_DIS_APM	0x85	/* Disable advanced power management */
#define SETFEATURES_EN_MSN	0x95	/* Enable Media Status Notification */
#define SETFEATURES_EN_RLA	0xAA	/* Enable read look-ahead feature */
#define SETFEATURES_PREFETCH	0xAB	/* Sets drive prefetch value */
#define SETFEATURES_EN_RPOD	0xCC	/* Enable reverting to power on defaults */
#define SETFEATURES_DIS_RI	0xDD	/* Disable release interrupt */
#define SETFEATURES_DIS_SI	0xDE	/* Disable SERVICE interrupt */

/* WIN_SECURITY sub-commands */

#define SECURITY_SET_PASSWORD		0xBA	/* 0xF1 */
#define SECURITY_UNLOCK			0xBB	/* 0xF2 */
#define SECURITY_ERASE_PREPARE		0xBC	/* 0xF3 */
#define SECURITY_ERASE_UNIT		0xBD	/* 0xF4 */
#define SECURITY_FREEZE_LOCK		0xBE	/* 0xF5 */
#define SECURITY_DISABLE_PASSWORD	0xBF	/* 0xF6 */

/* Bits for HD_ERROR */
#define MARK_ERR	0x01	/* Bad address mark */
#define TRK0_ERR	0x02	/* couldn't find track 0 */
#define ABRT_ERR	0x04	/* Command aborted */
#define MCR_ERR		0x08	/* media change request */
#define ID_ERR		0x10	/* ID field not found */
#define ECC_ERR		0x40	/* Uncorrectable ECC error */
#define	BBD_ERR		0x80	/* pre-EIDE meaning:  block marked bad */
#define	ICRC_ERR	0x80	/* new meaning:  CRC error during transfer */

struct hd_geometry {
      unsigned char heads;
      unsigned char sectors;
      unsigned short cylinders;
      unsigned long start;
};

/* hd/ide ctl's that pass (arg) ptrs to user space are numbered 0x030n/0x031n */
#define HDIO_GETGEO		0x0301	/* get device geometry */
#define HDIO_GET_UNMASKINTR	0x0302	/* get current unmask setting */
#define HDIO_GET_MULTCOUNT	0x0304	/* get current IDE blockmode setting */
#define HDIO_OBSOLETE_IDENTITY	0x0307	/* OBSOLETE, DO NOT USE: returns 142 bytes */
#define HDIO_GET_KEEPSETTINGS	0x0308	/* get keep-settings-on-reset flag */
#define HDIO_GET_32BIT		0x0309	/* get current io_32bit setting */
#define HDIO_GET_NOWERR		0x030a	/* get ignore-write-error flag */
#define HDIO_GET_DMA		0x030b	/* get use-dma flag */
#define HDIO_GET_NICE		0x030c	/* get nice flags */
#define HDIO_GET_IDENTITY	0x030d	/* get IDE identification info */

#define HDIO_DRIVE_RESET	0x031c	/* execute a device reset */
#define HDIO_TRISTATE_HWIF	0x031d	/* execute a channel tristate */
#ifndef __EMX__
#define HDIO_DRIVE_TASK		0x031e	/* execute task and special drive command */
#endif
#define HDIO_DRIVE_CMD		0x031f	/* execute a special drive command */

#define HDIO_DRIVE_CMD_AEB	HDIO_DRIVE_TASK

/* hd/ide ctl's that pass (arg) non-ptr values are numbered 0x032n/0x033n */
#define HDIO_SET_MULTCOUNT	0x0321	/* change IDE blockmode */
#define HDIO_SET_UNMASKINTR	0x0322	/* permit other irqs during I/O */
#define HDIO_SET_KEEPSETTINGS	0x0323	/* keep ioctl settings on reset */
#define HDIO_SET_32BIT		0x0324	/* change io_32bit flags */
#define HDIO_SET_NOWERR		0x0325	/* change ignore-write-error flag */
#define HDIO_SET_DMA		0x0326	/* change use-dma flag */
#define HDIO_SET_PIO_MODE	0x0327	/* reconfig interface to new speed */
#define HDIO_SCAN_HWIF		0x0328	/* register and (re)scan interface */
#define HDIO_SET_NICE		0x0329	/* set nice flags */
#define HDIO_UNREGISTER_HWIF	0x032a  /* unregister interface */

/* BIG GEOMETRY */
struct hd_big_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned int cylinders;
	unsigned long start;
};

/* hd/ide ctl's that pass (arg) ptrs to user space are numbered 0x033n/0x033n */
#define HDIO_GETGEO_BIG		0x0330	/* */
#define HDIO_GETGEO_BIG_RAW	0x0331	/* */

#define __NEW_HD_DRIVE_ID
/* structure returned by HDIO_GET_IDENTITY, as per ANSI ATA2 rev.2f spec */
struct hd_driveid {
	unsigned short	config;		/* lots of obsolete bit flags */
	unsigned short	cyls;		/* "physical" cyls */
	unsigned short	reserved2;	/* reserved (word 2) */
	unsigned short	heads;		/* "physical" heads */
	unsigned short	track_bytes;	/* unformatted bytes per track */
	unsigned short	sector_bytes;	/* unformatted bytes per sector */
	unsigned short	sectors;	/* "physical" sectors per track */
	unsigned short	vendor0;	/* vendor unique */
	unsigned short	vendor1;	/* vendor unique */
	unsigned short	vendor2;	/* vendor unique */
	unsigned char	serial_no[20];	/* 0 = not_specified */
	unsigned short	buf_type;
	unsigned short	buf_size;	/* 512 byte increments; 0 = not_specified */
	unsigned short	ecc_bytes;	/* for r/w long cmds; 0 = not_specified */
	unsigned char	fw_rev[8];	/* 0 = not_specified */
	unsigned char	model[40];	/* 0 = not_specified */
	unsigned char	max_multsect;	/* 0=not_implemented */
	unsigned char	vendor3;	/* vendor unique */
	unsigned short	dword_io;	/* 0=not_implemented; 1=implemented */
	unsigned char	vendor4;	/* vendor unique */
	unsigned char	capability;	/* bits 0:DMA 1:LBA 2:IORDYsw 3:IORDYsup*/
	unsigned short	reserved50;	/* reserved (word 50) */
	unsigned char	vendor5;	/* vendor unique */
	unsigned char	tPIO;		/* 0=slow, 1=medium, 2=fast */
	unsigned char	vendor6;	/* vendor unique */
	unsigned char	tDMA;		/* 0=slow, 1=medium, 2=fast */
	unsigned short	field_valid;	/* bits 0:cur_ok 1:eide_ok */
	unsigned short	cur_cyls;	/* logical cylinders */
	unsigned short	cur_heads;	/* logical heads */
	unsigned short	cur_sectors;	/* logical sectors per track */
	unsigned short	cur_capacity0;	/* logical total sectors on drive */
	unsigned short	cur_capacity1;	/*  (2 words, misaligned int)     */
	unsigned char	multsect;	/* current multiple sector count */
	unsigned char	multsect_valid;	/* when (bit0==1) multsect is ok */
	unsigned int	lba_capacity;	/* total number of sectors */
	unsigned short	dma_1word;	/* single-word dma info */
	unsigned short	dma_mword;	/* multiple-word dma info */
	unsigned short  eide_pio_modes; /* bits 0:mode3 1:mode4 */
	unsigned short  eide_dma_min;	/* min mword dma cycle time (ns) */
	unsigned short  eide_dma_time;	/* recommended mword dma cycle time (ns) */
	unsigned short  eide_pio;       /* min cycle time (ns), no IORDY  */
	unsigned short  eide_pio_iordy; /* min cycle time (ns), with IORDY */
	unsigned short	words69_70[2];	/* reserved words 69-70 */
	/* HDIO_GET_IDENTITY currently returns only words 0 through 70 */
	unsigned short	words71_74[4];	/* reserved words 71-74 */
	unsigned short  queue_depth;	/*  */
	unsigned short  words76_79[4];	/* reserved words 76-79 */
	unsigned short  major_rev_num;	/*  */
	unsigned short  minor_rev_num;	/*  */
	unsigned short  command_set_1;	/* bits 0:Smart 1:Security 2:Removable 3:PM */
	unsigned short  command_set_2;	/* bits 14:Smart Enabled 13:0 zero */
	unsigned short  cfsse;		/* command set-feature supported extensions */
	unsigned short  cfs_enable_1;	/* command set-feature enabled */
	unsigned short  cfs_enable_2;	/* command set-feature enabled */
	unsigned short  csf_default;	/* command set-feature default */
	unsigned short  dma_ultra;	/*  */
	unsigned short	word89;		/* reserved (word 89) */
	unsigned short	word90;		/* reserved (word 90) */
	unsigned short	CurAPMvalues;	/* current APM values */
	unsigned short	word92;		/* reserved (word 92) */
	unsigned short	hw_config;	/* hardware config */
	unsigned short  words94_125[32];/* reserved words 94-125 */
	unsigned short	last_lun;	/* reserved (word 126) */
	unsigned short	word127;	/* reserved (word 127) */
	unsigned short	dlf;		/* device lock function
					 * 15:9	reserved
					 * 8	security level 1:max 0:high
					 * 7:6	reserved
					 * 5	enhanced erase
					 * 4	expire
					 * 3	frozen
					 * 2	locked
					 * 1	en/disabled
					 * 0	capability
					 */
	unsigned short  csfo;		/* current set features options
					 * 15:4	reserved
					 * 3	auto reassign
					 * 2	reverting
					 * 1	read-look-ahead
					 * 0	write cache
					 */
	unsigned short	words130_155[26];/* reserved vendor words 130-155 */
	unsigned short	word156;
	unsigned short	words157_159[3];/* reserved vendor words 157-159 */
	unsigned short	words160_255[95];/* reserved words 160-255 */
};

/*
 * IDE "nice" flags. These are used on a per drive basis to determine
 * when to be nice and give more bandwidth to the other devices which
 * share the same IDE bus.
 */
#define IDE_NICE_DSC_OVERLAP	(0)	/* per the DSC overlap protocol */
#define IDE_NICE_ATAPI_OVERLAP	(1)	/* not supported yet */
#define IDE_NICE_0		(2)	/* when sure that it won't affect us */
#define IDE_NICE_1		(3)	/* when probably won't affect us much */
#define IDE_NICE_2		(4)	/* when we know it's on our expense */

#ifdef __KERNEL__
/*
 * These routines are used for kernel command line parameters from main.c:
 */
#include <linux/config.h>

#if defined(CONFIG_BLK_DEV_IDE) || defined(CONFIG_BLK_DEV_IDE_MODULE)
int ide_register(int io_port, int ctl_port, int irq);
void ide_unregister(unsigned int);
#endif /* CONFIG_BLK_DEV_IDE || CONFIG_BLK_DEV_IDE_MODULE */

#endif  /* __KERNEL__ */

#endif	/* _LINUX_HDREG_H */
