/*****************************************************************************
*  Copyright 2010 Broadcom Corporation.  All rights reserved.
*
*  Unless you and Broadcom execute a separate written software license
*  agreement governing use of this software, this software is licensed to you
*  under the terms of the GNU General Public License version 2, available at
*  http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
*  Notwithstanding the above, under no circumstances may you combine this
*  software in any way with any other Broadcom software provided under a
*  license other than the GPL, without Broadcom's express prior written
*  consent.
*
*****************************************************************************/


/*
*
*****************************************************************************
*
*  bcm_reset_mmc_utils.c
*
*  PURPOSE:
*
*     This implements the driver for the Factory Reset feature on eMMC based
*	  devices and other handlers for custom reset strings
*
*
*****************************************************************************/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/reboot.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/notifier.h>
#include <linux/preempt.h>
#include <linux/completion.h>
#include <linux/bio.h>
#include <linux/mmc/core.h>
#include <linux/broadcom/bcm_reset_utils.h>

#include <plat/syscfg.h>
#include <mach/reg_syscfg.h>

static struct raw_hd_struct *mmc_misc_hd;
static void	*bounce;

/*
	Structure definition for the Bootloader Control Block (BCB)
*/
struct bootloader_message {
	char command[32];
	char status[32];
	char recovery[1024];
} bootloader_message ;

static void mmc_bio_complete(struct bio *bio, int err)
{
	complete((struct completion *)bio->bi_private);
}

/*
	Callback called before the device reboots.
	If the reboot cause is a recovery, this function is going
	to write the strings "boot-recovery" and "recovery" into the
	Bootloader Control Block (BCB) so the Android Bootloader can
	launch the recovery image instead of the boot image.
*/
static int
reboot_notifier_callback(
struct notifier_block *nb, unsigned long val, void *v)
{
	struct raw_hd_struct *mmc_hd = NULL;
	struct bootloader_message *bcb;
	char *flashblock = NULL;

	if (mmc_misc_hd == NULL)
		goto clean_up;
	else
		mmc_hd = mmc_misc_hd;

	if (v == NULL) {
		board_sysconfig(SYSCFG_RESETREASON_SOFT_RESET, SYSCFG_DISABLE);
		goto clean_up;
	}

	if (!strncmp(v, "recovery", 8))	{
		int i = 0;
		dev_t devid;
		struct block_device *bdev;
		struct bio bio;
		struct bio_vec bio_vec;
		struct completion complete;
		struct page *page;

		/* Allocate a buffer to hold a block from 'misc' */
		flashblock = kmalloc(mmc_hd->nr_sects * 512, GFP_KERNEL);

		memset(flashblock, 0, mmc_hd->nr_sects * 512);

		/* If the allocation fails, return */
		if (flashblock == NULL)
			goto clean_up;

		/* read the BCB from the misc partition */
		/* read the entire block as we'll have to
		   rewrite it hence we need to erase */
		devid = MKDEV(mmc_hd->major, mmc_hd->first_minor + mmc_hd->partno);
		bdev = open_by_devnum(devid, FMODE_READ);
		if (IS_ERR(bdev)) {
			printk(KERN_ERR "misc: open device failed with %ld\n",
				PTR_ERR(bdev));
			goto clean_up;
		}
		page = virt_to_page(bounce);

		while (i < mmc_hd->nr_sects) {
			bio_init(&bio);
			bio.bi_io_vec = &bio_vec;
			bio_vec.bv_page = page;
			bio_vec.bv_offset = 0;
			bio.bi_vcnt = 1;
			bio.bi_idx = 0;
			bio.bi_bdev = bdev;
			bio.bi_sector = i;
			if (mmc_hd->nr_sects - i >= 8) {
				bio_vec.bv_len = PAGE_SIZE;
				bio.bi_size = PAGE_SIZE;
				i += 8;
			} else {
				bio_vec.bv_len = (mmc_hd->nr_sects - i) * 512;
				bio.bi_size = (mmc_hd->nr_sects - i) * 512;
				i = mmc_hd->nr_sects;
			}
			init_completion(&complete);
			bio.bi_private = &complete;
			bio.bi_end_io = mmc_bio_complete;
			submit_bio(READ, &bio);
			wait_for_completion(&complete);

			/* Copy the read buffer */
			memcpy(flashblock + (i * 512), page, bio.bi_size);
		}

		blkdev_put(bdev, FMODE_READ);
		printk(KERN_ERR "misc: Bound to mmc block device '(%d:%d)'\n",
			mmc_hd->major, mmc_hd->first_minor + mmc_hd->partno);

		/* BCB is stored at 0-bytes */
		bcb = (struct bootloader_message *)&flashblock[0];

		/* set bcb.command to "boot-recovery" */
		strcpy(bcb->command, "boot-recovery");

		/* clean bcb.status */
		memset(bcb->status, 0, sizeof(bcb->status));

		/* set bcb.recovery to "recovery" */
		strcpy(bcb->recovery, "recovery");

		/* Write the block back to 'misc'
		   First, erase it */
		devid = MKDEV(mmc_hd->major, mmc_hd->first_minor +
			mmc_hd->partno);
		bdev = open_by_devnum(devid, FMODE_WRITE);
		if (IS_ERR(bdev)) {
			printk(KERN_ERR "misc: open device failed with %ld\n",
				PTR_ERR(bdev));
			goto clean_up;
		}
		page = virt_to_page(bounce);
		i = 0;

		while (i < mmc_hd->nr_sects) {
			bio_init(&bio);
			bio.bi_io_vec = &bio_vec;
			bio_vec.bv_page = page;
			bio_vec.bv_offset = 0;
			bio.bi_vcnt = 1;
			bio.bi_idx = 0;
			bio.bi_bdev = bdev;
			bio.bi_sector = i;
			if (mmc_hd->nr_sects - i >= 8) {
				bio_vec.bv_len = PAGE_SIZE;
				bio.bi_size = PAGE_SIZE;
				i += 8;
			} else {
				bio_vec.bv_len = (mmc_hd->nr_sects - i) * 512;
				bio.bi_size = (mmc_hd->nr_sects - i) * 512;
				i = mmc_hd->nr_sects;
			}
			init_completion(&complete);
			bio.bi_private = &complete;
			bio.bi_end_io = mmc_bio_complete;
			submit_bio(WRITE, &bio);
			wait_for_completion(&complete);
		}
		blkdev_put(bdev, FMODE_WRITE);

		/* Then write the block back */
		devid = MKDEV(mmc_hd->major, mmc_hd->first_minor +
			mmc_hd->partno);
		bdev = open_by_devnum(devid, FMODE_WRITE);
		if (IS_ERR(bdev)) {
			printk(KERN_ERR "misc: open device failed with %ld\n",
				PTR_ERR(bdev));
			goto clean_up;
		}
		page = virt_to_page(bounce);
		i = 0;

		while (i < mmc_hd->nr_sects) {
			bio_init(&bio);
			bio.bi_io_vec = &bio_vec;
			bio_vec.bv_page = page;
			bio_vec.bv_offset = 0;
			bio.bi_vcnt = 1;
			bio.bi_idx = 0;
			bio.bi_bdev = bdev;
			bio.bi_sector = i;
			if (mmc_hd->nr_sects - i >= 8) {
				/* Copy the BCB block to buffer */
				memcpy(bounce, flashblock + (i * 512), PAGE_SIZE);

				bio_vec.bv_len = PAGE_SIZE;
				bio.bi_size = PAGE_SIZE;
				i += 8;
			} else {
				/* Copy the BCB block to buffer */
				memcpy(bounce, flashblock + (i * 512),
					(mmc_hd->nr_sects - i) * 512);

				bio_vec.bv_len = (mmc_hd->nr_sects - i) * 512;
				bio.bi_size = (mmc_hd->nr_sects - i) * 512;
				i = mmc_hd->nr_sects;
			}
			init_completion(&complete);
			bio.bi_private = &complete;
			bio.bi_end_io = mmc_bio_complete;
			submit_bio(WRITE, &bio);
			wait_for_completion(&complete);
		}
		blkdev_put(bdev, FMODE_WRITE);
	}

	if (!strncmp(v, "ap_only", 7)) {
		board_sysconfig(SYSCFG_RESETREASON_AP_ONLY_BOOT, SYSCFG_DISABLE);
	}

clean_up:

	if (flashblock != NULL)
		kfree(flashblock);

	return NOTIFY_DONE;
}

/* Structure for register_reboot_notifier() */
static struct notifier_block reboot_notifier = {
	.notifier_call = reboot_notifier_callback,
};

static void mmc_freset_notify_add(struct raw_hd_struct *hd,
			struct raw_mmc_panic_ops *panic_ops)
{
	/* Is it the partition we're looking for ? */
	/* If we've found the misc partition, assign the pointer */
	mmc_misc_hd = hd;
}

static void mmc_freset_notify_remove(struct raw_hd_struct *hd)
{
	/* Is it the partition we're looking for ? */
	/* If we've found the misc partition, clear mmc_misc_hd */
	mmc_misc_hd = NULL;
}

syscfg_reset_reason_t get_ap_boot_mode(void)
{
	return board_sysconfig(SYSCFG_RESETREASON_AP_ONLY_BOOT,
			SYSCFG_INIT);
}

EXPORT_SYMBOL(get_ap_boot_mode);

/* Structure for register_mmcpart_user() */
static struct mmcpart_notifier mmc_freset_notifier = {
	.add	= mmc_freset_notify_add,
	.remove	= mmc_freset_notify_remove,
};

/* Init routine */
int __init bcm_reset_utils_init(void)
{
	strncpy(mmc_freset_notifier.partname, "misc", BDEVNAME_SIZE);
	mmc_freset_notifier.partname[BDEVNAME_SIZE - 1] = '\0';
	/* register with the mmc driver so our callback gets called */
	register_mmcpart_user(&mmc_freset_notifier);
	register_reboot_notifier(&reboot_notifier);

	bounce = (void *) __get_free_page(GFP_KERNEL);
	printk(KERN_INFO "Android kernel bcm_reset mmc handler initialized\n");

	return 0;
}

module_init(bcm_reset_utils_init);
