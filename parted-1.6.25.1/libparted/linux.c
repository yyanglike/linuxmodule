/*
    libparted - a library for manipulating disk partitions
    Copyright (C) 1999 - 2005 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
*/

#include "config.h"

#include <parted/parted.h>
#include <parted/debug.h>
#include <parted/linux.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/utsname.h>	/* for uname() */
#include <scsi/scsi.h>

#include "blkpg.h"

#if ENABLE_NLS
#  include <libintl.h>
#  define _(String) dgettext (PACKAGE, String)
#else
#  define _(String) (String)
#endif /* ENABLE_NLS */

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#ifndef __NR__llseek
#define __NR__llseek 140
#endif

#ifndef SCSI_IOCTL_SEND_COMMAND
#define SCSI_IOCTL_SEND_COMMAND 1
#endif

/* from <linux/hdreg.h> */
#define HDIO_GETGEO		0x0301	/* get device geometry */
#define HDIO_GET_IDENTITY	0x030d	/* get IDE identification info */

struct hd_geometry {
	unsigned char heads;
	unsigned char sectors;
	unsigned short cylinders;
	unsigned long start;
};

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
	char		model[40];	/* 0 = not_specified */
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

/* from <linux/fs.h> */
#define BLKRRPART  _IO(0x12,95)	/* re-read partition table */
#define BLKGETSIZE _IO(0x12,96)	/* return device size */
#define BLKFLSBUF  _IO(0x12,97)	/* flush buffer cache */
#define BLKSSZGET  _IO(0x12,104) /* get block device sector size */
#define BLKGETLASTSECT  _IO(0x12,108) /* get last sector of block device */
#define BLKSETLASTSECT  _IO(0x12,109) /* set last sector of block device */
#define BLKGETSIZE64 _IOR(0x12,114,size_t)	/* return device size in bytes (u64 *arg) */

struct blkdev_ioctl_param {
	unsigned int block;
	size_t content_length;
	char * block_contents;
};

/* from <linux/major.h> */
#define IDE0_MAJOR		3
#define IDE1_MAJOR		22
#define IDE2_MAJOR		33
#define IDE3_MAJOR		34
#define IDE4_MAJOR		56
#define IDE5_MAJOR		57
#define SCSI_CDROM_MAJOR	11
#define SCSI_DISK0_MAJOR	8
#define SCSI_DISK1_MAJOR	65
#define SCSI_DISK2_MAJOR	66
#define SCSI_DISK3_MAJOR	67
#define SCSI_DISK4_MAJOR	68
#define SCSI_DISK5_MAJOR	69
#define SCSI_DISK6_MAJOR	70
#define SCSI_DISK7_MAJOR	71
#define COMPAQ_SMART2_MAJOR	72
#define COMPAQ_SMART2_MAJOR1	73
#define COMPAQ_SMART2_MAJOR2	74
#define COMPAQ_SMART2_MAJOR3	75
#define COMPAQ_SMART2_MAJOR4	76
#define COMPAQ_SMART2_MAJOR5	77
#define COMPAQ_SMART2_MAJOR6	78
#define COMPAQ_SMART2_MAJOR7	79
#define COMPAQ_SMART_MAJOR	104
#define COMPAQ_SMART_MAJOR1	105
#define COMPAQ_SMART_MAJOR2	106
#define COMPAQ_SMART_MAJOR3	107
#define COMPAQ_SMART_MAJOR4	108
#define COMPAQ_SMART_MAJOR5	109
#define COMPAQ_SMART_MAJOR6	110
#define COMPAQ_SMART_MAJOR7	111
#define DAC960_MAJOR		48
#define ATARAID_MAJOR		114
#define I2O_MAJOR1		80
#define I2O_MAJOR2		81
#define I2O_MAJOR3		82
#define I2O_MAJOR4		83
#define I2O_MAJOR5		84
#define I2O_MAJOR6		85
#define I2O_MAJOR7		86
#define I2O_MAJOR8		87
#define UBD_MAJOR               98

#define SCSI_BLK_MAJOR(M) (						\
		(M) == SCSI_DISK0_MAJOR 				\
		|| (M) == SCSI_CDROM_MAJOR				\
		|| ((M) >= SCSI_DISK1_MAJOR && (M) <= SCSI_DISK7_MAJOR))

static char* _device_get_part_path (PedDevice* dev, int num);
static int _partition_is_mounted_by_path (const char* path);

static int
_is_ide_major (int major)
{
	switch (major) {
		case IDE0_MAJOR:
		case IDE1_MAJOR:
		case IDE2_MAJOR:
		case IDE3_MAJOR:
		case IDE4_MAJOR:
		case IDE5_MAJOR:
			return 1;

		default:
			return 0;
	}
}

static int
_is_cpqarray_major (int major)
{
	return ((COMPAQ_SMART2_MAJOR <= major && major <= COMPAQ_SMART2_MAJOR7)
	     || (COMPAQ_SMART_MAJOR <= major && major <= COMPAQ_SMART_MAJOR7));
}

static int
_is_i2o_major (int major)
{
  	return (I2O_MAJOR1 <= major && major <= I2O_MAJOR8);
}

static int
_device_stat (PedDevice* dev, struct stat * dev_stat)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);

	while (1) {
		if (!stat (dev->path, dev_stat)) {
			return 1;
		} else {
			if (ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_RETRY_CANCEL,
				_("Could not stat device %s - %s."),
				dev->path,
				strerror (errno))
					!= PED_EXCEPTION_RETRY)
				return 0;
		}
	}
}

static int
_device_probe_type (PedDevice* dev)
{
	struct stat		dev_stat;
	int			dev_major;
	int			dev_minor;
	PedExceptionOption	ex_status;

	if (!_device_stat (dev, &dev_stat))
		return 0;

	if (!S_ISBLK(dev_stat.st_mode)) {
		dev->type = PED_DEVICE_FILE;
		return 1;
	}

	dev_major = major (dev_stat.st_rdev);
	dev_minor = minor (dev_stat.st_rdev);

	if (SCSI_BLK_MAJOR (dev_major) && (dev_minor % 0x10 == 0)) {
		dev->type = PED_DEVICE_SCSI;
	} else if (_is_ide_major (dev_major) && (dev_minor % 0x40 == 0)) {
		dev->type = PED_DEVICE_IDE;
	} else if (dev_major == DAC960_MAJOR && (dev_minor % 0x8 == 0)) {
		dev->type = PED_DEVICE_DAC960;
	} else if (dev_major == ATARAID_MAJOR && (dev_minor % 0x10 == 0)) {
		dev->type = PED_DEVICE_ATARAID;
	} else if (_is_i2o_major (dev_major) && (dev_minor % 0x10 == 0)) {
		dev->type = PED_DEVICE_I2O;
	} else if (_is_cpqarray_major (dev_major) && (dev_minor % 0x10 == 0)) {
		dev->type = PED_DEVICE_CPQARRAY;
	} else if (dev_major == UBD_MAJOR && (dev_minor % 0x10 == 0)) {
		dev->type = PED_DEVICE_UBD;
	} else {
		dev->type = PED_DEVICE_UNKNOWN;
	}

	return 1;
}

static int
_get_linux_version ()
{
	static int kver = -1;

	struct utsname uts;
	int major;
	int minor;
	int teeny;

	if (kver != -1)
		return kver;

	if (uname (&uts))
		return kver = 0;
	if (sscanf (uts.release, "%u.%u.%u", &major, &minor, &teeny) != 3)
		return kver = 0;

	return kver = KERNEL_VERSION (major, minor, teeny);
}

static int
_have_devfs ()
{
	static int have_devfs = -1;
	struct stat sb;

	if (have_devfs != -1)
		return have_devfs;

	/* the presence of /dev/.devfsd implies that DevFS is active */
	if (stat("/dev/.devfsd", &sb) < 0)
		return have_devfs = 0;

	return have_devfs = S_ISCHR(sb.st_mode) ? 1 : 0;
}

static int
_device_get_sector_size (PedDevice* dev)
{
	LinuxSpecific*	arch_specific = LINUX_SPECIFIC (dev);
	int		sector_size = PED_SECTOR_SIZE; /*makes Valgrind happy*/

	PED_ASSERT (dev->open_count, return 0);

	if (_get_linux_version() < KERNEL_VERSION (2,3,0))
		return PED_SECTOR_SIZE;
	if (ioctl (arch_specific->fd, BLKSSZGET, &sector_size))
		return PED_SECTOR_SIZE;

	if (sector_size != PED_SECTOR_SIZE) {
		if (ped_exception_throw (
			PED_EXCEPTION_BUG,
			PED_EXCEPTION_IGNORE_CANCEL,
			_("The sector size on %s is %d bytes.  Parted is known "
			"not to work properly with drives with sector sizes "
			"other than %d bytes."),
			dev->path,
			sector_size,
			PED_SECTOR_SIZE)
				== PED_EXCEPTION_IGNORE)
			return sector_size;
		else
			return PED_SECTOR_SIZE;
	}

	return sector_size;
}

static int
_kernel_has_blkgetsize64(void)
{
	int version = _get_linux_version();

	if (version >= KERNEL_VERSION (2,5,4)) return 1;
	if (version <  KERNEL_VERSION (2,5,0) &&
	    version >= KERNEL_VERSION (2,4,18)) return 1;
        return 0;
}

/* TODO: do a binary search if BLKGETSIZE doesn't work?! */
static PedSector
_device_get_length (PedDevice* dev)
{
	unsigned long		size;
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);
	uint64_t bytes=0;

	PED_ASSERT (dev->open_count > 0, return 0);

        if (_kernel_has_blkgetsize64()) {
                if (ioctl(arch_specific->fd, BLKGETSIZE64, &bytes) == 0) {
                        return bytes / PED_SECTOR_SIZE;
		}
	}

	if (ioctl (arch_specific->fd, BLKGETSIZE, &size)) {
		ped_exception_throw (
			PED_EXCEPTION_BUG,
			PED_EXCEPTION_CANCEL,
			_("Unable to determine the size of %s (%s)."),
			dev->path,
			strerror (errno));
		return 0;
	}

	return size;
}

static int
_device_probe_geometry (PedDevice* dev)
{
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);
	struct stat		dev_stat;
	struct hd_geometry	geometry;

	if (!_device_stat (dev, &dev_stat))
		return 0;
	PED_ASSERT (S_ISBLK (dev_stat.st_mode), return 0);

	dev->length = _device_get_length (dev);
	if (!dev->length)
		return 0;

	dev->sector_size = _device_get_sector_size (dev);
	if (!dev->sector_size)
		return 0;

	/* The GETGEO ioctl is no longer useful (as of linux 2.6.x).  We could
	 * still use it in 2.4.x, but this is contentious.  Perhaps we should
	 * move to EDD. */
	dev->bios_geom.sectors = 63;
	dev->bios_geom.heads = 255;
	dev->bios_geom.cylinders
	       	= dev->length / (63 * 255)
	       		/ (dev->sector_size / PED_SECTOR_SIZE);

	/* FIXME: what should we put here?  (TODO: discuss on linux-kernel) */
	if (!ioctl (arch_specific->fd, HDIO_GETGEO, &geometry)) {
		dev->hw_geom.sectors = geometry.sectors;
		dev->hw_geom.heads = geometry.heads;
		dev->hw_geom.cylinders
		       	= dev->length / (dev->hw_geom.heads
				         * dev->hw_geom.sectors)
				/ (dev->sector_size / PED_SECTOR_SIZE);
	} else {
		dev->hw_geom = dev->bios_geom;
	}

	return 1;
}

static char*
strip_name(char* str)
{
	int	i;
	int	end = 0;

	for (i = 0; str[i] != 0; i++) {
		if (!isspace (str[i])
		    || (isspace (str[i]) && !isspace (str[i+1]) && str[i+1])) {
			str [end] = str[i];
			end++;
		}
	}
	str[end] = 0;
	return strdup (str);
}

static int
init_ide (PedDevice* dev)
{
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);
	struct stat		dev_stat;
	int			dev_major;
	struct hd_driveid	hdi;
	PedExceptionOption	ex_status;
	char			hdi_buf[41];

	if (!_device_stat (dev, &dev_stat))
		goto error;

	dev_major = major (dev_stat.st_rdev);

	if (!ped_device_open (dev))
		goto error;

        if (ioctl (arch_specific->fd, HDIO_GET_IDENTITY, &hdi)) {
		ex_status = ped_exception_throw (
				PED_EXCEPTION_WARNING,
				PED_EXCEPTION_IGNORE_CANCEL,
				_("Could not get identity of device %s - %s"),
				dev->path, strerror (errno));
		switch (ex_status) {
			case PED_EXCEPTION_CANCEL:
				goto error_close_dev;

			case PED_EXCEPTION_UNHANDLED:
				ped_exception_catch ();
			case PED_EXCEPTION_IGNORE:
				dev->model = strdup(_("IDE"));
		}
	} else {
		/* hdi.model is not guaranteed to be NULL terminated */
		memcpy (hdi_buf, hdi.model, 40);
		hdi_buf[40] = '\0';
		dev->model = strip_name (hdi_buf);
	}

	if (!_device_probe_geometry (dev))
		goto error_close_dev;

	ped_device_close (dev);
	return 1;

error_close_dev:
	ped_device_close (dev);
error:
	return 0;
}

/* This function reads the /sys entry named "file" for device "dev". */
static char *
read_device_sysfs_file (PedDevice *dev, const char *file)
{
	FILE *f;
	char name_buf[128];
	char buf[256];

	snprintf (name_buf, 127, "/sys/block/%s/device/%s",
		  basename (dev->path), file);

	if ((f = fopen (name_buf, "r")) == NULL)
		return NULL;

	fgets (buf, 255, f);
	fclose (f);
	return strip_name (buf);
}

/* This function sends a query to a SCSI device for vendor and product
 * information.  It uses the deprecated SCSI_IOCTL_SEND_COMMAND to
 * issue this query.
 */
static int
scsi_query_product_info (PedDevice* dev, char **vendor, char **product)
{
	/* The following are defined by the SCSI-2 specification. */
	typedef struct _scsi_inquiry_cmd
	{
		uint8_t op;
		uint8_t lun;          /* bits 5-7 denote the LUN */
		uint8_t page_code;
		uint8_t reserved;
		uint8_t alloc_length;
		uint8_t control;
	} __attribute__((packed)) scsi_inquiry_cmd_t;

	typedef struct _scsi_inquiry_data
	{
		uint8_t peripheral_info;
		uint8_t device_info;
		uint8_t version_info;
		uint8_t _field1;
		uint8_t additional_length;
		uint8_t _reserved1;
		uint8_t _reserved2;
		uint8_t _field2;
		uint8_t vendor_id[8];
		uint8_t product_id[16];
		uint8_t product_revision[4];
		uint8_t vendor_specific[20];
		uint8_t _reserved3[40];
	} __attribute__((packed)) scsi_inquiry_data_t;

	struct scsi_arg
	{
		unsigned int inlen;
		unsigned int outlen;

		union arg_data
		{
			scsi_inquiry_data_t out;
			scsi_inquiry_cmd_t  in;
		} data;
	} arg;

	LinuxSpecific* arch_specific = LINUX_SPECIFIC (dev);
	char	buf[32];

	*vendor = NULL;
	*product = NULL;

	memset (&arg, 0x00, sizeof(struct scsi_arg));
	arg.inlen  = 0;
	arg.outlen = sizeof(scsi_inquiry_data_t);
	arg.data.in.op  = INQUIRY;
	arg.data.in.lun = dev->host << 5;
	arg.data.in.alloc_length = sizeof(scsi_inquiry_data_t);
	arg.data.in.page_code = 0;
	arg.data.in.reserved = 0;
	arg.data.in.control = 0;

	if (ioctl (arch_specific->fd, SCSI_IOCTL_SEND_COMMAND, &arg) < 0)
		return 0;

	memcpy (buf, arg.data.out.vendor_id, 8);
	buf[8] = '\0';
	*vendor = strip_name (buf);

	memcpy (buf, arg.data.out.product_id, 16);
	buf[16] = '\0';
	*product = strip_name (buf);

	return 1;
}

/* This function provides the vendor and product name for a SCSI device.
 * It supports both the modern /sys interface and direct queries
 * via the deprecated ioctl, SCSI_IOCTL_SEND_COMMAND.
 */
static int
scsi_get_product_info (PedDevice* dev, char **vendor, char **product)
{
	*vendor = read_device_sysfs_file (dev, "vendor");
	*product = read_device_sysfs_file (dev, "model");
	if (*vendor && *product)
		return 1;

	return scsi_query_product_info (dev, vendor, product);
}

static int
init_scsi (PedDevice* dev)
{
	struct scsi_idlun
	{
		uint32_t dev_id;
		uint32_t host_unique_id;
	} idlun;

	LinuxSpecific* arch_specific = LINUX_SPECIFIC (dev);
	char* vendor;
	char* product;

	if (!ped_device_open (dev))
		goto error;

        if (ioctl (arch_specific->fd, SCSI_IOCTL_GET_IDLUN, &idlun) < 0) {
		dev->host = 0;
		dev->did = 0;
		if (ped_exception_throw (
			PED_EXCEPTION_ERROR, PED_EXCEPTION_IGNORE_CANCEL,
			_("Error initialising SCSI device %s - %s"),
			dev->path, strerror (errno))
				!= PED_EXCEPTION_IGNORE)
			goto error_close_dev;
		if (!_device_probe_geometry (dev))
			goto error_close_dev;
		ped_device_close (dev);
		return 1;
        }

	dev->host = idlun.host_unique_id;
	dev->did  = idlun.dev_id;

	dev->model = (char*) ped_malloc (8 + 16 + 2);
	if (!dev->model)
		goto error_close_dev;

	if (scsi_get_product_info (dev, &vendor, &product)) {
		sprintf (dev->model, "%.8s %.16s", vendor, product);
		ped_free (vendor);
		ped_free (product);
	} else {
		strcpy (dev->model, "SCSI");
	}

	if (!_device_probe_geometry (dev))
		goto error_close_dev;

	ped_device_close (dev);
	return 1;

error_close_dev:
	ped_device_close (dev);
error:
	return 0;
}

static int
init_file (PedDevice* dev)
{
	struct stat	dev_stat;
 
	if (!_device_stat (dev, &dev_stat))
		goto error;
	if (!ped_device_open (dev))
		goto error;

	if (S_ISBLK(dev_stat.st_mode))
		dev->length = _device_get_length (dev);
	else
		dev->length = dev_stat.st_size / 512;
	if (dev->length <= 0) {
		ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_CANCEL,
			_("The device %s is zero-length, and can't possibly "
			  "store a file system or partition table.  Perhaps "
			  "you selected the wrong device?"),
			dev->path);
		goto error_close_dev;
	}

	ped_device_close (dev);

	dev->bios_geom.cylinders = dev->length / 4 / 32;
	dev->bios_geom.heads = 4;
	dev->bios_geom.sectors = 32;
	dev->hw_geom = dev->bios_geom;
	dev->sector_size = 512;
	dev->model = strdup ("");
	return 1;

error_close_dev:
	ped_device_close (dev);
error:
	return 0;
}

static int
init_generic (PedDevice* dev, char* model_name)
{
	struct stat		dev_stat;
	PedExceptionOption	ex_status;

	if (!_device_stat (dev, &dev_stat))
		goto error;

	if (!ped_device_open (dev))
		goto error;

	ped_exception_fetch_all ();
	if (_device_probe_geometry (dev)) {
		ped_exception_leave_all ();
	} else {
		/* hack to allow use of files, for testing */
		ped_exception_catch ();
		ped_exception_leave_all ();

		ex_status = ped_exception_throw (
				PED_EXCEPTION_WARNING,
				PED_EXCEPTION_IGNORE_CANCEL,
				_("Unable to determine geometry of "
				"file/device.  You should not use Parted "
				"unless you REALLY know what you're doing!"));
		switch (ex_status) {
			case PED_EXCEPTION_CANCEL:
				goto error_close_dev;

			case PED_EXCEPTION_UNHANDLED:
				ped_exception_catch ();
			case PED_EXCEPTION_IGNORE:
				; // just workaround for gcc 3.0
		}

		/* what should we stick in here? */
		dev->length = dev_stat.st_size / PED_SECTOR_SIZE;
		dev->bios_geom.cylinders = dev->length / 4 / 32;
		dev->bios_geom.heads = 4;
		dev->bios_geom.sectors = 32;
		dev->sector_size = PED_SECTOR_SIZE;
	}

	dev->model = strdup (model_name);

	ped_device_close (dev);
	return 1;

error_close_dev:
	ped_device_close (dev);
error:
	return 0;
}

static PedDevice*
linux_new (const char* path)
{
	PedDevice*	dev;

	PED_ASSERT (path != NULL, return NULL);

	dev = (PedDevice*) ped_malloc (sizeof (PedDevice));
	if (!dev)
		goto error;

	dev->path = strdup (path);
	if (!dev->path)
		goto error_free_dev;

	dev->arch_specific
		= (LinuxSpecific*) ped_malloc (sizeof (LinuxSpecific));
	if (!dev->arch_specific)
		goto error_free_path;

	dev->open_count = 0;
	dev->read_only = 0;
	dev->external_mode = 0;
	dev->dirty = 0;
	dev->boot_dirty = 0;

	if (!_device_probe_type (dev))
		goto error_free_arch_specific;

	switch (dev->type) {
	case PED_DEVICE_IDE:
		if (!init_ide (dev))
			goto error_free_arch_specific;
		break;

	case PED_DEVICE_SCSI:
		if (!init_scsi (dev))
			goto error_free_arch_specific;
		break;

	case PED_DEVICE_DAC960:
		if (!init_generic (dev, _("DAC960 RAID controller")))
			goto error_free_arch_specific;
		break;

	case PED_DEVICE_CPQARRAY:
		if (!init_generic (dev, _("Compaq Smart Array")))
			goto error_free_arch_specific;
		break;

	case PED_DEVICE_ATARAID:
		if (!init_generic (dev, _("ATARAID Controller")))
			goto error_free_arch_specific;
		break;

	case PED_DEVICE_I2O:
		if (!init_generic (dev, _("I2O Controller")))
			goto error_free_arch_specific;
		break;

	case PED_DEVICE_UBD:
		if (!init_generic (dev, _("User-Mode Linux UBD")))
			goto error_free_dev;
		break;

 	case PED_DEVICE_FILE:
		if (!init_file (dev))
			goto error_free_arch_specific;
		break;

	case PED_DEVICE_UNKNOWN:
		if (!init_generic (dev, _("Unknown")))
			goto error_free_arch_specific;
		break;

	default:
		ped_exception_throw (PED_EXCEPTION_NO_FEATURE,
				PED_EXCEPTION_CANCEL,
				_("ped_device_new()  Unsupported device type"));
		goto error_free_arch_specific;
	}
	return dev;

error_free_arch_specific:
	ped_free (dev->arch_specific);
error_free_path:
	ped_free (dev->path);
error_free_dev:
	ped_free (dev);
error:
	return NULL;
}

static void
linux_destroy (PedDevice* dev)
{
	ped_free (dev->arch_specific);
	ped_free (dev->path);
	ped_free (dev->model);
	ped_free (dev);
}

static int
linux_is_busy (PedDevice* dev)
{
	int	i;
	char*	part_name;

	if (_partition_is_mounted_by_path (dev->path))
		return 1;

	for (i = 0; i < 32; i++) {
		int status;

		part_name = _device_get_part_path (dev, i);
		if (!part_name)
			return 1;
		status = _partition_is_mounted_by_path (part_name);
		ped_free (part_name);

		if (status)
			return 1;
	}

	return 0;
}

/* we need to flush the master device, and all the partition devices,
 * because there is no coherency between the caches.
 * We should only flush unmounted partition devices, because:
 *  - there is never a need to flush them (we're not doing IO there)
 *  - flushing a device that is mounted causes unnecessary IO, and can
 * even screw journaling & friends up.  Even cause oopsen!
 */
static void
_flush_cache (PedDevice* dev)
{
	LinuxSpecific*	arch_specific = LINUX_SPECIFIC (dev);
	int		i;

	if (dev->read_only)
		return;
	dev->dirty = 0;

	ioctl (arch_specific->fd, BLKFLSBUF);

	for (i = 1; i < 16; i++) {
		char*		name;
		int		fd;

		name = _device_get_part_path (dev, i);
		if (!name)
			break;
		if (!_partition_is_mounted_by_path (name)) {
			fd = open (name, O_WRONLY, 0);
			if (fd > 0) {
				ioctl (fd, BLKFLSBUF);
				close (fd);
			}
		}
		ped_free (name);
	}
}

static int
linux_open (PedDevice* dev)
{
	LinuxSpecific*	arch_specific = LINUX_SPECIFIC (dev);

retry:
	arch_specific->fd = open (dev->path, O_RDWR);
	if (arch_specific->fd == -1) {
		char*	rw_error_msg = strerror (errno);

		arch_specific->fd = open (dev->path, O_RDONLY);
		if (arch_specific->fd == -1) {
			if (ped_exception_throw (
				PED_EXCEPTION_ERROR,
				PED_EXCEPTION_RETRY_CANCEL,
				_("Error opening %s: %s"),
				dev->path, strerror (errno))
					!= PED_EXCEPTION_RETRY) {
				return 0;
			} else {
				goto retry;
			}
		} else {
			ped_exception_throw (
				PED_EXCEPTION_WARNING,
				PED_EXCEPTION_OK,
				_("Unable to open %s read-write (%s).  %s has "
				  "been opened read-only."),
				dev->path, rw_error_msg, dev->path);
			dev->read_only = 1;
		}
	} else {
		dev->read_only = 0;
	}

	_flush_cache (dev);

	return 1;
}

static int
linux_refresh_open (PedDevice* dev)
{
	return 1;
}

static int
linux_close (PedDevice* dev)
{
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);

	if (dev->dirty)
		_flush_cache (dev);
	close (arch_specific->fd);
	return 1;
}

static int
linux_refresh_close (PedDevice* dev)
{
	if (dev->dirty)
		_flush_cache (dev);
	return 1;
}

#if SIZEOF_OFF_T < 8

static _syscall5(int,_llseek,
		 unsigned int, fd,
		 unsigned long, offset_high,
		 unsigned long, offset_low,
		 loff_t*, result,
		 unsigned int, origin)

loff_t
llseek (unsigned int fd, loff_t offset, unsigned int whence)
{
	loff_t result;
	int retval;

	retval = _llseek(fd,
			 ((unsigned long long)offset) >> 32,
			 ((unsigned long long)offset) & 0xffffffff,
			 &result,
			 whence);
	return (retval==-1 ? (loff_t) retval : result);
}

#endif /* SIZEOF_OFF_T < 8 */

static int
_device_seek (PedDevice* dev, PedSector sector)
{
	LinuxSpecific*	arch_specific;

	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);
	
	arch_specific = LINUX_SPECIFIC (dev);

#if SIZEOF_OFF_T < 8
	if (sizeof (off_t) < 8) {
		loff_t	pos = sector * PED_SECTOR_SIZE;
		return llseek (arch_specific->fd, pos, SEEK_SET) == pos;
	} else
#endif
	{
		off_t	pos = sector * PED_SECTOR_SIZE;
		return lseek (arch_specific->fd, pos, SEEK_SET) == pos;
	}
}

static int
_read_lastoddsector (PedDevice* dev, void* buffer)
{
	LinuxSpecific*			arch_specific;
	struct blkdev_ioctl_param	ioctl_param;

	PED_ASSERT(dev != NULL, return 0);
	PED_ASSERT(buffer != NULL, return 0);
	
	arch_specific = LINUX_SPECIFIC (dev);

retry:
	ioctl_param.block = 0; /* read the last sector */
	ioctl_param.content_length = dev->sector_size;
	ioctl_param.block_contents = buffer;
	
	if (ioctl(arch_specific->fd, BLKGETLASTSECT, &ioctl_param) == -1) {
		PedExceptionOption	opt;
		opt = ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_RETRY_IGNORE_CANCEL,
			_("%s during read on %s"),
			strerror (errno), dev->path);

		if (opt == PED_EXCEPTION_CANCEL)
			return 0;
		if (opt == PED_EXCEPTION_RETRY)
			goto retry;
	}

	return 1;
}

static int
linux_read (PedDevice* dev, void* buffer, PedSector start, PedSector count)
{
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);
	int			status;
	PedExceptionOption	ex_status;
	size_t			read_length = count * PED_SECTOR_SIZE;


	if (_get_linux_version() < KERNEL_VERSION (2,6,0)) {
		/* Kludge.  This is necessary to read/write the last
		   block of an odd-sized disk, until Linux 2.5.x kernel fixes.
		*/
		if (dev->type != PED_DEVICE_FILE && (dev->length & 1)
		    && start + count - 1 == dev->length - 1)
			return ped_device_read (dev, buffer, start, count - 1)
				&& _read_lastoddsector (
					dev, buffer + (count-1) * 512);
	}
	while (1) {
		if (_device_seek (dev, start))
			break;

		ex_status = ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_RETRY_IGNORE_CANCEL,
			_("%s during seek for read on %s"),
			strerror (errno), dev->path);

		switch (ex_status) {
			case PED_EXCEPTION_IGNORE:
				return 1;

			case PED_EXCEPTION_RETRY:
				break;

			case PED_EXCEPTION_UNHANDLED:
				ped_exception_catch ();
			case PED_EXCEPTION_CANCEL:
				return 0;
		}
	}

	while (1) {
		status = read (arch_specific->fd, buffer, read_length);
		if (status == count * PED_SECTOR_SIZE) break;
		if (status > 0) {
			read_length -= status;
			buffer += status;
			continue;
		}

		ex_status = ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_RETRY_IGNORE_CANCEL,
			_("%s during read on %s"),
			strerror (errno),
			dev->path);

		switch (ex_status) {
			case PED_EXCEPTION_IGNORE:
				return 1;

			case PED_EXCEPTION_RETRY:
				break;

			case PED_EXCEPTION_UNHANDLED:
				ped_exception_catch ();
			case PED_EXCEPTION_CANCEL:
				return 0;
		}
	}

	return 1;
}

static int
_write_lastoddsector (PedDevice* dev, const void* buffer)
{
	LinuxSpecific*			arch_specific;
	struct blkdev_ioctl_param	ioctl_param;

	PED_ASSERT(dev != NULL, return 0);
	PED_ASSERT(buffer != NULL, return 0);
	
	arch_specific = LINUX_SPECIFIC (dev);

retry:
	ioctl_param.block = 0; /* write the last sector */
	ioctl_param.content_length = dev->sector_size;
	ioctl_param.block_contents = (void*) buffer;
	
	if (ioctl(arch_specific->fd, BLKSETLASTSECT, &ioctl_param) == -1) {
		PedExceptionOption	opt;
		opt = ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_RETRY_IGNORE_CANCEL,
			_("%s during write on %s"),
			strerror (errno), dev->path);

		if (opt == PED_EXCEPTION_CANCEL)
			return 0;
		if (opt == PED_EXCEPTION_RETRY)
			goto retry;
	}

	return 1;
}

static int
linux_write (PedDevice* dev, const void* buffer, PedSector start,
	     PedSector count)
{
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);
	int			status;
	PedExceptionOption	ex_status;
	size_t			write_length = count * PED_SECTOR_SIZE;

	if (dev->read_only) {
		if (ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_IGNORE_CANCEL,
			_("Can't write to %s, because it is opened read-only."),
			dev->path)
				!= PED_EXCEPTION_IGNORE)
			return 0;
		else
			return 1;
	}

	if (_get_linux_version() < KERNEL_VERSION (2,6,0)) {
		/* Kludge.  This is necessary to read/write the last
		   block of an odd-sized disk, until Linux 2.5.x kernel fixes.
		*/
		if (dev->type != PED_DEVICE_FILE && (dev->length & 1)
		    && start + count - 1 == dev->length - 1)
			return ped_device_write (dev, buffer, start, count - 1)
				&& _write_lastoddsector (
					dev, buffer + (count-1) * 512);
	}
	while (1) {
		if (_device_seek (dev, start))
			break;

		ex_status = ped_exception_throw (
			PED_EXCEPTION_ERROR, PED_EXCEPTION_RETRY_IGNORE_CANCEL,
			_("%s during seek for write on %s"),
			strerror (errno), dev->path);

		switch (ex_status) {
			case PED_EXCEPTION_IGNORE:
				return 1;

			case PED_EXCEPTION_RETRY:
				break;

			case PED_EXCEPTION_UNHANDLED:
				ped_exception_catch ();
			case PED_EXCEPTION_CANCEL:
				return 0;
		}
	}

#ifdef READ_ONLY
	printf ("ped_device_write (\"%s\", %p, %d, %d)\n",
		dev->path, buffer, (int) start, (int) count);
#else
	dev->dirty = 1;
	while (1) {
		status = write (arch_specific->fd, buffer, write_length);
		if (status == count * PED_SECTOR_SIZE) break;
		if (status > 0) {
			write_length -= status;
			buffer += status;
			continue;
		}

		ex_status = ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_RETRY_IGNORE_CANCEL,
			_("%s during write on %s"),
			strerror (errno), dev->path);

		switch (ex_status) {
			case PED_EXCEPTION_IGNORE:
				return 1;

			case PED_EXCEPTION_RETRY:
				break;

			case PED_EXCEPTION_UNHANDLED:
				ped_exception_catch ();
			case PED_EXCEPTION_CANCEL:
				return 0;
		}
	}
#endif /* !READ_ONLY */
	return 1;
}

/* returns the number of sectors that are ok.
 */
static PedSector
linux_check (PedDevice* dev, void* buffer, PedSector start, PedSector count)
{
	LinuxSpecific*	arch_specific = LINUX_SPECIFIC (dev);
	PedSector	done = 0;
	int		status;

	if (!_device_seek (dev, start))
		return 0;

	for (done = 0; done < count; done += status / PED_SECTOR_SIZE) {
		status = read (arch_specific->fd, buffer,
			       (size_t) ((count-done) * PED_SECTOR_SIZE));
		if (status < 0)
			break;
	}

	return done;
}

static int
_do_fsync (PedDevice* dev)
{
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);
	int			status;
	PedExceptionOption	ex_status;

	while (1) {
		status = fsync (arch_specific->fd);
		if (status >= 0) break;

		ex_status = ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_RETRY_IGNORE_CANCEL,
			_("%s during write on %s"),
			strerror (errno), dev->path);

		switch (ex_status) {
			case PED_EXCEPTION_IGNORE:
				return 1;

			case PED_EXCEPTION_RETRY:
				break;

			case PED_EXCEPTION_UNHANDLED:
				ped_exception_catch ();
			case PED_EXCEPTION_CANCEL:
				return 0;
		}
	} 
	return 1;
}

static int
linux_sync (PedDevice* dev)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);

	if (dev->read_only)
		return 1;
	if (!_do_fsync (dev))
		return 0;
	_flush_cache (dev);
	return 1;
}

static int
linux_sync_fast (PedDevice* dev)
{
	PED_ASSERT (dev != NULL, return 0);
	PED_ASSERT (!dev->external_mode, return 0);

	if (dev->read_only)
		return 1;
	if (!_do_fsync (dev))
		return 0;
	/* no cache flush... */
	return 1;
}

static inline int
_compare_digit_state (char ch, int need_digit)
{
	return !!isdigit (ch) == need_digit;
}

/* matches the regexp "[^0-9]+[0-9]+[^0-9]+[0-9]+$".
 * Motivation: accept devices looking like /dev/rd/c0d0, but
 * not looking like /dev/hda1 and /dev/rd/c0d0p1
 */
static int
_match_rd_device (const char* name)
{
	const char* pos;
	int state;

	/* exclude directory names from test */
	pos = strrchr(name, '/') ?: name;

	/* states:
	 * 	0	non-digits
	 * 	1	digits
	 * 	2	non-digits
	 * 	3	digits
	 */
	for (state = 0; state < 4; state++) {
		int want_digits = (state % 2 == 1);
		do {
			if (!*pos)
				return 0;
			if (!_compare_digit_state (*pos, want_digits))
				return 0;
			pos++;
		} while (_compare_digit_state (*pos, want_digits));
	}

	return *pos == 0;
}

static int
_probe_proc_partitions ()
{
	FILE*		proc_part_file;
	int		major, minor, size;
	char		buf [512];
	char		part_name [256];
	char		dev_name [256];

	proc_part_file = fopen ("/proc/partitions", "r");
	if (!proc_part_file)
		return 0;

	fgets (buf, 256, proc_part_file);
	fgets (buf, 256, proc_part_file);

	while (fgets (buf, 512, proc_part_file)
	       && sscanf (buf, "%d %d %d %255s", &major, &minor, &size,
		          part_name) == 4) {
		/* Heuristic for telling partitions and devices apart
		 * Probably needs to be improved
		 */
		if (!_match_rd_device (part_name)
		    && isdigit (part_name [strlen (part_name) - 1]))
			continue;

		strcpy (dev_name, "/dev/");
		strcat (dev_name, part_name);
		_ped_device_probe (dev_name);
	}

	fclose (proc_part_file);
	return 1;
}

static int
_probe_standard_devices ()
{
	_ped_device_probe ("/dev/sda");
	_ped_device_probe ("/dev/sdb");
	_ped_device_probe ("/dev/sdc");
	_ped_device_probe ("/dev/sdd");
	_ped_device_probe ("/dev/sde");
	_ped_device_probe ("/dev/sdf");

	_ped_device_probe ("/dev/hda");
	_ped_device_probe ("/dev/hdb");
	_ped_device_probe ("/dev/hdc");
	_ped_device_probe ("/dev/hdd");
	_ped_device_probe ("/dev/hde");
	_ped_device_probe ("/dev/hdf");
	_ped_device_probe ("/dev/hdg");
	_ped_device_probe ("/dev/hdh");

	return 1;
}

static void
linux_probe_all ()
{
	_probe_proc_partitions ();

	/* we should probe the standard devs too, even with /proc/partitions,
	 * because /proc/partitions might return devfs stuff, and we might not
	 * have devfs available
	 */
	_probe_standard_devices ();
}

static char*
_device_get_part_path (PedDevice* dev, int num)
{
	int		path_len = strlen (dev->path);
	int		result_len = path_len + 16;
	char*		result;

	result = (char*) ped_malloc (result_len);
	if (!result)
		return NULL;

	/* Check for devfs-style /disc => /partN transformation
	   unconditionally; the system might be using udev with devfs rules,
	   and if not the test is harmless. */
	if (!strcmp (dev->path + path_len - 5, "/disc")) {
		/* replace /disc with /path%d */
		strcpy (result, dev->path);
	        snprintf (result + path_len - 5, 16, "/part%d", num);
	} else if (dev->type == PED_DEVICE_DAC960
			|| dev->type == PED_DEVICE_CPQARRAY
			|| dev->type == PED_DEVICE_ATARAID
			|| isdigit (dev->path[path_len - 1]))
	        snprintf (result, result_len, "%sp%d", dev->path, num);
	else
	        snprintf (result, result_len, "%s%d", dev->path, num);

	return result;
}

static char*
linux_partition_get_path (const PedPartition* part)
{
	return _device_get_part_path (part->disk->dev, part->num);
}

static dev_t
_partition_get_part_dev (const PedPartition* part)
{
	struct stat dev_stat;
	int dev_major, dev_minor;

	if (!_device_stat (part->disk->dev, &dev_stat))
		return 0;
	dev_major = major (dev_stat.st_rdev);
	dev_minor = minor (dev_stat.st_rdev);
	return makedev (dev_major, dev_minor + part->num);
}

static int
_mount_table_search (const char* file_name, dev_t dev)
{
	struct stat part_stat;
	char line[512];
	char part_name[512];
	FILE* file;
	int junk;

	file = fopen (file_name, "r");
	if (!file)
		return 0;
 	while (fgets (line, 512, file)) {
		junk = sscanf (line, "%s", part_name);
		if (stat (part_name, &part_stat) == 0) {
			if (part_stat.st_rdev == dev) {
				fclose (file);
				return 1;
			}
		}
 	}
 	fclose (file);
	return 0;
}

static int
_partition_is_mounted_by_dev (dev_t dev)
{
	return  _mount_table_search( "/proc/mounts", dev)
		|| _mount_table_search( "/proc/swaps", dev)
		|| _mount_table_search( "/etc/mtab", dev);
}

static int
_partition_is_mounted_by_path (const char *path)
{
	struct stat part_stat;
	if (stat (path, &part_stat) != 0)
		return 0;
	if (!S_ISBLK(part_stat.st_mode))
		return 0;
	return _partition_is_mounted_by_dev (part_stat.st_rdev);
}

static int
_partition_is_mounted (const PedPartition *part)
{
	dev_t dev;
	if (!ped_partition_is_active (part))
		return 0;
	dev = _partition_get_part_dev (part);
	return _partition_is_mounted_by_dev (dev);
}

static int
linux_partition_is_busy (const PedPartition* part)
{
	PedPartition*	walk;

	PED_ASSERT (part != NULL, return 0);

	if (_partition_is_mounted (part))
		return 1;
	if (part->type == PED_PARTITION_EXTENDED) {
		for (walk = part->part_list; walk; walk = walk->next) {
			if (linux_partition_is_busy (walk))
				return 1;
		}
	}
	return 0;
}

static int
_blkpg_part_command (PedDevice* dev, struct blkpg_partition* part, int op)
{
	LinuxSpecific*		arch_specific = LINUX_SPECIFIC (dev);
	struct blkpg_ioctl_arg	ioctl_arg;

	ioctl_arg.op = op;
	ioctl_arg.flags = 0;
	ioctl_arg.datalen = sizeof (struct blkpg_partition);
	ioctl_arg.data = (void*) part;

	return ioctl (arch_specific->fd, BLKPG, &ioctl_arg) == 0;
}

static int
_blkpg_add_partition (PedDisk* disk, PedPartition* part)
{
	struct blkpg_partition	linux_part;
	const char*		vol_name;
	char*			dev_name;

	if (ped_disk_type_check_feature (disk->type,
					 PED_DISK_TYPE_PARTITION_NAME))
		vol_name = ped_partition_get_name (part);
	else
		vol_name = NULL;

	dev_name = _device_get_part_path (disk->dev, part->num);
	if (!dev_name)
		return 0;

	memset (&linux_part, 0, sizeof (linux_part));
	linux_part.start = part->geom.start * PED_SECTOR_SIZE;
	linux_part.length = part->geom.length * PED_SECTOR_SIZE;
	linux_part.pno = part->num;
	strncpy (linux_part.devname, dev_name, BLKPG_DEVNAMELTH);
	if (vol_name)
		strncpy (linux_part.volname, vol_name, BLKPG_VOLNAMELTH);

	ped_free (dev_name);

	if (!_blkpg_part_command (disk->dev, &linux_part,
				  BLKPG_ADD_PARTITION)) {
		return ped_exception_throw (
			PED_EXCEPTION_ERROR,
			PED_EXCEPTION_IGNORE_CANCEL,
			_("Error informing the kernel about modifications to "
			  "partition %s -- %s.  This means Linux won't know "
			  "about any changes you made to %s until you reboot "
			  "-- so you shouldn't mount it or use it in any way "
			  "before rebooting."),
			linux_part.devname,
			strerror (errno),
			linux_part.devname)
				== PED_EXCEPTION_IGNORE;
	}

	return 1;
}

static int
_blkpg_remove_partition (PedDisk* disk, int n)
{
	struct blkpg_partition	linux_part;

	memset (&linux_part, 0, sizeof (linux_part));
	linux_part.pno = n;
	return _blkpg_part_command (disk->dev, &linux_part,
				    BLKPG_DEL_PARTITION);
}

static int
_disk_sync_part_table (PedDisk* disk)
{
	int	i;
	int	last = PED_MAX (ped_disk_get_last_partition_num (disk), 16);
	int	rets[last], errnums[last];
	int	ret = 1;

	for (i = 1; i <= last; i++) {
		rets[i - 1] = _blkpg_remove_partition (disk, i);
		errnums[i - 1] = errno;
	}

	for (i = 1; i <= last; i++) {
		PedPartition*		part;

		part = ped_disk_get_partition (disk, i);
		if (part) {
			/* extended partitions have no business in the kernel!
			 * blkpg doesn't like overlapping partitions.  Hmmm,
			 * LILO isn't going to like this.
			 */
			if (part->type & PED_PARTITION_EXTENDED)
				continue;

			/* busy... so we won't (can't!) disturb ;)  Prolly
			 * doesn't matter anyway, because users shouldn't be
			 * changing mounted partitions anyway...
			 */
			if (!rets[i - 1] && errnums[i - 1] == EBUSY)
					continue;

			/* add the (possibly modified or new) partition */
			if (!_blkpg_add_partition (disk, part))
				ret = 0;
		}
	}

	return ret;
}

static int
_kernel_reread_part_table (PedDevice* dev)
{
	LinuxSpecific*	arch_specific = LINUX_SPECIFIC (dev);
	int		retry_count = 5;

	sync();
	while (ioctl (arch_specific->fd, BLKRRPART)) {
		retry_count--;
		sync();
	    	if (!retry_count) {
			ped_exception_throw (
				PED_EXCEPTION_WARNING,
				PED_EXCEPTION_IGNORE,
			_("The kernel was unable to re-read the partition "
			  "table on %s (%s).  This means Linux won't know "
			  "anything about the modifications you made "
			  "until you reboot.  You should reboot your computer "
			  "before doing anything with %s."),
				dev->path, strerror (errno), dev->path);
			return 0;
		}
	}

	return 1;
}

static int
_have_blkpg ()
{
	static int have_blkpg = -1;
	int kver;

	if (have_blkpg != -1)
		return have_blkpg;

	kver = _get_linux_version();
	return have_blkpg = kver >= KERNEL_VERSION (2,4,0) ? 1 : 0;
}

static int
linux_disk_commit (PedDisk* disk)
{
	if (disk->dev->type != PED_DEVICE_FILE) {
		/* The ioctl() command BLKPG_ADD_PARTITION does not notify
		 * the devfs system; consequently, /proc/partitions will not
		 * be up to date, and the proper links in /dev are not
		 * created.  Therefore, if using DevFS, we must get the kernel
		 * to re-read and grok the partition table.
		 */
		if (_have_blkpg () && !_have_devfs ()) {
			if (_disk_sync_part_table (disk))
				return 1;
		}
		return _kernel_reread_part_table (disk->dev);
	}
	return 1;
}

static PedDeviceArchOps linux_dev_ops = {
	_new:		linux_new,
	destroy:	linux_destroy,
	is_busy:	linux_is_busy,
	open:		linux_open,
	refresh_open:	linux_refresh_open,
	close:		linux_close,
	refresh_close:	linux_refresh_close,
	read:		linux_read,
	write:		linux_write,
	check:		linux_check,
	sync:		linux_sync,
	sync_fast:	linux_sync_fast,
	probe_all:	linux_probe_all
};

PedDiskArchOps linux_disk_ops =  {
	partition_get_path:	linux_partition_get_path,
	partition_is_busy:	linux_partition_is_busy,
	disk_commit:		linux_disk_commit
};

PedArchitecture ped_linux_arch = {
	dev_ops:	&linux_dev_ops,
	disk_ops:	&linux_disk_ops
};
