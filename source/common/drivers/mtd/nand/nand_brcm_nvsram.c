/*
 * Broadcom ARM PrimeCell PL353 controller (NAND controller) driver
 *
 * Copyright (c) 2011-2012 Broadcom Corporation
 *
 * Authors: Sundar Jayakumar Dev <sundarjdev@broadcom.com>
 */

/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/mtd/nand/nand_brcm_nvsram.c
*
* Unless you and Broadcom execute a separate written software license agreement
* governing use of this software, this software is licensed to you under the
* terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a license
* other than the GPL, without Broadcom's express prior written consent.
*******************************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand_ecc512.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include <asm/io.h>
#include <asm/mach-types.h>

#include <mach/reg_nvsram.h>

#include <plat/nand_otp.h>

#include <plat/dma.h>

#define NAND_ECC_NUM_BYTES	3
#define NAND_ECC_PAGE_SIZE 512

static struct mtd_info *board_mtd;
static void __iomem *bcm_nand_io_base;
static nvsram_cmd addr;
static uint8_t chip_sel;

/* Define global flag for controller ecc; Set it to enabled by default */
static int host_controller_ecc = enabled;

static struct nand_special_dev brcm_nand_ids[] = {
	{NAND_MFR_MICRON, MICRON_ON_DIE_ECC_ENABLED, 4},
	{0x0, -1, -1}
};

/* Define global flag for OTP config; set it to NAND_OTP_NONE by default */
int config_nand_otp = NAND_OTP_NONE;
uint32_t otp_start_addr;
uint32_t otp_end_addr;

static struct nand_otp brcm_nand_otp[] = {
	{NAND_MFR_MICRON, 0x1000, 0xf000, NAND_MICRON_OTP},
	{0x0, -1, -1, NAND_OTP_NONE}
};

#ifdef CONFIG_MTD_PARTITIONS
const char *part_probes[] = { "cmdlinepart", NULL };
#endif

static uint8_t scan_ff_pattern[] = { 0xff, 0xff };

static struct nand_bbt_descr smallpage_bbt = {
	.options = NAND_BBT_SCAN2NDPAGE,
	.offs = 5,
	.len = 1,
	.pattern = scan_ff_pattern
};

static struct nand_bbt_descr largepage_bbt = {
	.options = 0,
	.offs = 0,
	.len = 1,
	.pattern = scan_ff_pattern
};

/*
 * The generic flash bbt decriptors overlap with our ecc
 * hardware, so define some Broadcom specific ones.
 */
static uint8_t bbt_pattern[] = { 'B', 'b', 't', '0' };
static uint8_t mirror_pattern[] = { '1', 't', 'b', 'B' };

static struct nand_bbt_descr bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
	    | NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 10,
	.len = 4,
	.veroffs = 14,
	.maxblocks = 4,
	.pattern = bbt_pattern,
};

static struct nand_bbt_descr bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
	    | NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 10,
	.len = 4,
	.veroffs = 14,
	.maxblocks = 4,
	.pattern = mirror_pattern,
};

/*
 * nand_hw_eccoob
 * New oob placement block for use with hardware ecc generation.
 */
static struct nand_ecclayout nand_hw_eccoob_512 = {
	.eccbytes = 3,
	.eccpos = {6, 7, 8},
	/* Reserve 5 for BI indicator */
	.oobavail = 10,
	.oobfree = {
		    {.offset = 0, .length = 4},
		    {.offset = 10, .length = 6}
		    }
};

/*
** We treat the OOB for a 2K page as if it were 4 512 byte oobs,
** except the BI is at byte 0.
*/
static struct nand_ecclayout nand_hw_eccoob_2048 = {
	.eccbytes = 12,
	.eccpos = {8, 9, 10, 24, 25, 26, 40, 41, 42, 56, 57, 58},
	/* Reserve 0 as BI indicator */
	.oobavail = 44,
	.oobfree = {
		    {.offset = 2, .length = 4},
		    {.offset = 12, .length = 12},
		    {.offset = 28, .length = 12},
		    {.offset = 44, .length = 12},
		    {.offset = 60, .length = 4},
		    }
};

/*
 * OOB layout for Micron on-die ECC
 */
static struct nand_ecclayout micron_nand_on_die_eccoob_2048 = {
	.eccbytes = 32,
	.eccpos = {
			8,  9,  10, 11, 12, 13, 14, 15,
			24, 25, 26, 27, 28, 29, 30, 31,
			40, 41, 42, 43, 44, 45, 46, 47,
			56, 57, 58, 59, 60, 61, 62, 63
		},
	/* Reserve 0 as BI indicator */
	.oobavail = 16,
	.oobfree = {
		    {.offset = 4, .length = 4},
		    {.offset = 20, .length = 4},
		    {.offset = 36, .length = 4},
		    {.offset = 52, .length = 4},
	   }
};

/* We treat the OOB for a 4K page as if it were 8 512 byte oobs,
 * except the BI is at byte 0. */
static struct nand_ecclayout nand_hw_eccoob_4096 = {
	.eccbytes = 24,
	.eccpos = {8, 9, 10, 24, 25, 26, 40, 41, 42, 56, 57, 58,
		   72, 73, 74, 88, 89, 90, 104, 105, 106, 120, 121, 122},
	/* Reserve 0 as BI indicator */
	.oobavail = 88,
	.oobfree = {
		    {.offset = 2, .length = 4},
		    {.offset = 12, .length = 12},
		    {.offset = 28, .length = 12},
		    {.offset = 44, .length = 12},
		    {.offset = 60, .length = 12},
		    {.offset = 76, .length = 12},
		    {.offset = 92, .length = 12},
		    {.offset = 108, .length = 12},
		    }
};

static uint8_t brcm_nand_read_byte(struct mtd_info *mtd)
{
	return (uint8_t) cpu_to_le16(readw(addr.data_phase_addr));
}

static uint16_t brcm_nand_read_word(struct mtd_info *mtd)
{
	return readw(addr.data_phase_addr);
}

static void brcm_nand_write_byte(const unsigned char byte)
{
	writew((unsigned short)byte, addr.data_phase_addr);
}

#define NVSRAM_READ_FIFO_SIZE	32	/* 32-bytes */
#define NVSRAM_WRITE_FIFO_SIZE	32	/* 32-bytes */

static void brcm_nand_write_buf(struct mtd_info *mtd,
				const u_char *buf, int len)
{
	unsigned int i;

	/* memcpy-ing to WRITE FIFO */
	for (i = 0; i < len; i += NVSRAM_WRITE_FIFO_SIZE) {
		memcpy((void *)addr.data_phase_addr, buf, NVSRAM_WRITE_FIFO_SIZE);
		buf += NVSRAM_WRITE_FIFO_SIZE;
	}
}

static void brcm_nand_read_buf(struct mtd_info *mtd,
				u_char *buf, int len)
{
	unsigned int i;

	/* memcpy-ing from READ FIFO */
	for (i = 0; i < len; i += NVSRAM_READ_FIFO_SIZE) {
		memcpy(buf, (void *)addr.data_phase_addr, NVSRAM_READ_FIFO_SIZE);
		buf += NVSRAM_READ_FIFO_SIZE;
	}
}

static int brcm_nand_verify_buf(struct mtd_info *mtd,
				const u_char *buf, int len)
{
	struct nand_chip *chip = mtd->priv;

	/* Allocate buffer for Read data */
	u_char *ver_buf = kmalloc(len, GFP_KERNEL);
	u_char *ver_oob_buf = kmalloc(mtd->oobsize, GFP_KERNEL);

	/* Read data from FIFO to buffer */
	chip->read_buf(mtd, ver_buf, len);

	/* Read oob data from FIFO to buffer */
	chip->read_buf(mtd, ver_oob_buf, mtd->oobsize);

	/* Compare main data with read data */
	if (memcmp(buf, ver_buf, len)) {
		kfree(ver_buf);
		kfree(ver_oob_buf);
		return -EFAULT;
	}

	/* Compare oob data with read data */
	if (memcmp(chip->oob_poi, ver_oob_buf, mtd->oobsize)) {
		kfree(ver_buf);
		kfree(ver_oob_buf);
		return -EFAULT;
	}

	/* Return happy */
	kfree(ver_buf);
	kfree(ver_oob_buf);
	return 0;
}

static void brcm_nand_wait_until_ready(void)
{
	uint32_t status = 0;
	unsigned long timeo = jiffies;
	uint32_t timeout;

	timeo += 1;	/* 1 jiffies */

	/*
	 * First wait for the NAND device to go busy because R/#B
	 * signal may be slower than fast CPU and still be ready.
	 */
	while (time_before(jiffies, timeo)) {
		status = readl(NVSRAM_REG_BASE + NVSRAM_MEMC_STATUS_OFFSET);
		/* NAND R/#B monitored by NVSRAM interface-1 raw interrupt status */
		if (!(status & NVSRAM_MEMC_STATUS_RAW_INT_STATUS_MASK))
			break;
		cond_resched();
	}

	timeout = 200;	/* 1msec max timeout */
	/* Then wait for the NAND device to go ready */
	status = readl(NVSRAM_REG_BASE + NVSRAM_MEMC_STATUS_OFFSET);
	while (!(status & NVSRAM_MEMC_STATUS_RAW_INT_STATUS_MASK) && timeout--) {
		status = readl(NVSRAM_REG_BASE + NVSRAM_MEMC_STATUS_OFFSET);
		udelay(5);
	}

	if (!timeout) {
		pr_err("%s: RY/#BY interrupt NOT received even after 50msec\n",
			__FUNCTION__);
		return;
	}

	/* Clear raw interrupt */
	writel(NVSRAM_MEMC_CFG_CLR_NAND_INT_CLR_MASK,
			NVSRAM_REG_BASE + NVSRAM_MEMC_CFG_CLR_OFFSET);
}

/* Special NAND commands */
#define NAND_CMD_GET_FEATURES	0xEE
#define NAND_CMD_SET_FEATURES	0xEF

/*
 * brcm_nand_cmdfunc:
 * Our implementation of cmdfunc for NVSRAM NAND controller.
 *
 * Used by the upper layer to write command to NAND Flash for
 * different operations to be carried out on NAND Flash.
 */
static void brcm_nand_cmdfunc(struct mtd_info *mtd, unsigned int command,
				int column, int page_addr)
{
	struct nand_chip *chip = mtd->priv;
	int state = chip->state;
	int status, count;

	/* Initialize cmd and data phase addresses */
	addr.cmd_phase_addr = (uint32_t)bcm_nand_io_base;
	addr.data_phase_addr = (uint32_t)bcm_nand_io_base;

	pr_debug("brcm_nvsram_cmdfunc (cmd = 0x%x, col = 0x%x, page = 0x%x)\n",
	      command, column, page_addr);

	/* Command pre-processing step */
	switch (command) {
	case NAND_CMD_RESET:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= NAND_CMD_RESET << NVSRAM_SMC_NAND_START_CMD;

		writel(0x0, addr.cmd_phase_addr);

		break;

	case NAND_CMD_STATUS:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= NAND_CMD_STATUS << NVSRAM_SMC_NAND_START_CMD;

		writel(0x0, addr.cmd_phase_addr);

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_CLEAR_CS 	|
								NVSRAM_SMC_RESERVED;

		break;

	case NAND_CMD_READOOB:
		/* Emulate NAND_CMD_READOOB as NAND_CMD_READ0 */
		column += mtd->writesize;
		command = NAND_CMD_READ0;

		/* Fall through */
	case NAND_CMD_READ0:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= 5 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE			|
								NVSRAM_SMC_NAND_END_CMD_REQ					|
								NAND_CMD_READSTART << NVSRAM_SMC_NAND_END_CMD|
								NAND_CMD_READ0 << NVSRAM_SMC_NAND_START_CMD;

		/*
		 * Input the address to the controller. 4 address cycles fit in
		 * 32-bit a double word and the last address cycle is sent as a
		 * seperate 32-bit double word. The first 2 cycles are column
		 * addresses and the next 3 cycles are page addresses.
		 *
		 * Read PL353 controller spec for more info.
		 *
		 * NOTE: the fifth address cycle is only for devices > 128MiB.
		 */
		if (column != -1) {
			/* Adjust columns for 16 bit buswidth */
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;
		}
		if (page_addr != -1) {
			writel((page_addr << 16) | column, addr.cmd_phase_addr);
			/* One more address cycle for devices > 128MiB */
			if (chip->chipsize > (128 << 20))
				writel(page_addr >> 16, addr.cmd_phase_addr);
		}

		if (host_controller_ecc == enabled) {
			/* Using controller ECC */

			/* Poll memc_status register for R/#B interrupt */
			brcm_nand_wait_until_ready();
		} else if (host_controller_ecc == disabled) {
			/* NOT using controller ECC; Micron NAND with on-die ECC */

			/* Check ECC in non-OTP cases ONLY */
			if (state != FL_OTPING) {
				/* Micron NAND: Wait for STATUS_READY; tR_ECC max */
				for (count = 0; count < 70; count++) {
					ndelay(1000);	/* 1 micro sec delay */

					chip->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
					status = chip->read_byte(mtd);
					if (status & NAND_STATUS_READY)
						break;
				}

				/* Check for ECC errors */
				if (status & NAND_STATUS_FAIL) {
					pr_warn("ECC error on READ operation: 0x%02x\n", status);
					/* Increment error stats */
					mtd->ecc_stats.failed++;
				}

				/* Return the device to READ mode following a STATUS command */
				addr.cmd_phase_addr = (uint32_t)bcm_nand_io_base 					|
										NAND_CMD_READ0 << NVSRAM_SMC_NAND_START_CMD;

				writel(0x0, addr.cmd_phase_addr);
			}
			else{
				/* read OTP area*/
				/* Poll memc_status register for R/#B interrupt */
				brcm_nand_wait_until_ready();
			}
		}

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_CLEAR_CS	| NVSRAM_SMC_RESERVED;

		break;

	case NAND_CMD_SEQIN:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= 5 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE			|
								NAND_CMD_SEQIN << NVSRAM_SMC_NAND_START_CMD;

		/* Read comment above (READ0 command) */
		if (column != -1) {
			/* Adjust columns for 16 bit buswidth */
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;
		}
		if (page_addr != -1) {
			writel((page_addr << 16) | column, addr.cmd_phase_addr);
			/* One more address cycle for devices > 128MiB */
			if (chip->chipsize > (128 << 20))
				writel(page_addr >> 16, addr.cmd_phase_addr);
		}

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_RESERVED;
		break;

	case NAND_CMD_PAGEPROG:
		/* Send NAND_CMD_PAGEPROG to end write transaction */
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= NVSRAM_SMC_CLEAR_CS							|
								NAND_CMD_PAGEPROG << NVSRAM_SMC_NAND_START_CMD;

		writel(0x0, addr.cmd_phase_addr);

		/* Poll memc_status register for R/#B interrupt */
		brcm_nand_wait_until_ready();
		break;

	case NAND_CMD_READID:
		/* Give small delay of 200us before issuing READID */
		udelay(200);

		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= 1 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE			|
								NVSRAM_SMC_CLEAR_CS							|
								NAND_CMD_READID << NVSRAM_SMC_NAND_START_CMD;

		writel(0x0, addr.cmd_phase_addr);

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_RESERVED;

		break;

	case NAND_CMD_ERASE1:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= 3 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE			|
								NAND_CMD_ERASE1 << NVSRAM_SMC_NAND_START_CMD;

		/* Send page_addr(PA0 - PA17) of the block to be erased to SMC */
		writel(page_addr & 0x3ffff, addr.cmd_phase_addr);

		break;

	case NAND_CMD_ERASE2:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= NAND_CMD_ERASE2 << NVSRAM_SMC_NAND_START_CMD;

		writel(0x0, addr.cmd_phase_addr);
		break;

	case NAND_CMD_RNDOUT:
		/* Construct cmd_phase_addr */
		/* RNDOUT is sub-page read; so column addr (2 cycles) is sufficient */
		addr.cmd_phase_addr |= 2 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE			|
								NVSRAM_SMC_NAND_END_CMD_REQ					|
								NAND_CMD_RNDOUTSTART << NVSRAM_SMC_NAND_END_CMD|
								NAND_CMD_RNDOUT << NVSRAM_SMC_NAND_START_CMD;

		/* Sending only column address cycle */
		if (column != -1) {
			/* Adjust columns for 16 bit buswidth */
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;

			writel(column && 0xffff, addr.cmd_phase_addr);
		}

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_CLEAR_CS	| NVSRAM_SMC_RESERVED;

		break;

	case NAND_CMD_RNDIN:
		/* Construct cmd_phase_addr */
		/* RNDIN is sub-page write; so column addr (2 cycles) is sufficient */
		addr.cmd_phase_addr |= 2 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE			|
								NAND_CMD_RNDIN << NVSRAM_SMC_NAND_START_CMD;

		/* Sending only column address cycle */
		if (column != -1) {
			/* Adjust columns for 16 bit buswidth */
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;

			writel(column && 0xffff, addr.cmd_phase_addr);
		}

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_RESERVED;

		break;

	case NAND_CMD_GET_FEATURES:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= 1 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE				|
								NAND_CMD_GET_FEATURES << NVSRAM_SMC_NAND_START_CMD;

		/* Sending only column address cycle */
		writel(column, addr.cmd_phase_addr);

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_RESERVED;

		break;

	case NAND_CMD_SET_FEATURES:
		/* Construct cmd_phase_addr */
		addr.cmd_phase_addr |= 1 << NVSRAM_SMC_CMD_NUM_ADDR_CYCLE				|
								NAND_CMD_SET_FEATURES << NVSRAM_SMC_NAND_START_CMD;

		/* Sending only column address cycle */
		writel(column, addr.cmd_phase_addr);

		/* Construct data_phase_addr */
		addr.data_phase_addr |= NVSRAM_SMC_RESERVED;

		break;

	default:
		pr_err("NAND_ERROR: Un-supported command\n");
		return;
	}
}

/**
 * brcm_nand_select_chip - Dummy place holder 
 * @mtd:	MTD device structure
 * @chipnr:	chipnumber to select, -1 for deselect
 *
 * Default select function for 1 chip devices.
 */
static void brcm_nand_select_chip(struct mtd_info *mtd, int chipnr)
{
	switch (chipnr) {
	case -1:
		break;
	case 0:
		break;

	default:
		BUG();
	}
}

/**
 * brcm_nand_block_markbad - mark a block bad
 * @mtd:	MTD device structure
 * @ofs:	offset from device start
 *
 * This is BRCM implementation of mark bad block implementation.
 * In addition to updating the flash based BBT, we also write the
 * Bad Block markers to the OOB area of the first page in the bad
 * block.
*/
static int brcm_nand_block_markbad(struct mtd_info *mtd, loff_t ofs)
{
	struct nand_chip *chip = mtd->priv;
	struct mtd_oob_ops ops;
	uint8_t buf[2] = { 0xaa, 0x55 };
	int block, ret;

	if (chip->options & NAND_BB_LAST_PAGE)
		ofs += mtd->erasesize - mtd->writesize;

	/* Get block number */
	block = (int)(ofs >> chip->bbt_erase_shift);
	if (chip->bbt)
		chip->bbt[block >> 2] |= 0x01 << ((block & 0x03) << 1);

	/* Update flash BBT */
	if (chip->options & NAND_USE_FLASH_BBT) {
		ret = nand_update_bbt(mtd, ofs);
	}

	/*
	 * Also write BAD BLOCK marker in OOB of 1st page in block for
	 * compatibility with RTOS BBM
	 */
	ofs += mtd->oobsize;
	ops.mode = MTD_OOB_PLACE;
	ops.len = ops.ooblen = 2;
	ops.datbuf = NULL;
	ops.oobbuf = buf;
	ops.ooboffs = chip->badblockpos & ~0x01;

	ret = mtd->write_oob(mtd, ofs, &ops);

	if (!ret) {
		/* Increase badblocks stat count on finding bad block */
		mtd->ecc_stats.badblocks++;
	}

	return ret;
}

static uint32_t read_hw_ecc(uint32_t offset, uint32_t mask)
{
	uint32_t dwEcc = 0;
	uint32_t timeout = 10; /* 10usec max timeout */

	dwEcc = readl(NVSRAM_REG_BASE + offset);
	/* Wait for ecc_valid bit */
	while (!(dwEcc & mask) && timeout--) {
		dwEcc = readl(NVSRAM_REG_BASE + offset);
		udelay(1);
	}

	if (!timeout) {
		pr_err("%s:  ECC generation timed out!", __FUNCTION__);
		/*
		 * Returning 0 here even though 0x000000 is a valid ECC
		 * for a 512Byte sub-page with all 0xff's. This is because,
		 * on time out, the ECC_VALUE register will also be 0x0.
		 */
		return 0;
	}

	/* Return happy */
	return dwEcc;
}

static uint32_t brcm_nand_get_hw_ecc(int page)
{
	uint32_t dwEcc = 0;

	switch (page)
	{
		case 0:
			dwEcc = read_hw_ecc(NVSRAM_ECC_VALUE0_OFFSET,
							NVSRAM_ECC_VALUE0_ECC_VALID_MASK);
			break;

		case 1:
			dwEcc = read_hw_ecc(NVSRAM_ECC_VALUE1_OFFSET,
							NVSRAM_ECC_VALUE1_ECC_VALID_MASK);
			break;

		case 2:
			dwEcc = read_hw_ecc(NVSRAM_ECC_VALUE2_OFFSET,
							NVSRAM_ECC_VALUE2_ECC_VALID_MASK);
			break;

		case 3:
			dwEcc = read_hw_ecc(NVSRAM_ECC_VALUE3_OFFSET,
							NVSRAM_ECC_VALUE3_ECC_VALID_MASK);
			break;

		default:
			pr_err("%s: Invalid page number for ECC read\n", __FUNCTION__);
			break;
	}

	if (!dwEcc) {
		pr_err("%s: ECC generation error!\n", __FUNCTION__);
		return 0;
	}

	/* Return the 3-byte ECC */
	return (dwEcc & 0x00ffffff);
}

#define TRUE 1
#define FALSE 0

static int brcm_nand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				uint8_t *buf, int page)
{
	int i, j, steps, eccsize = chip->ecc.size;
	bool skip_ecc_check = TRUE;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint8_t *p = buf;
	uint32_t ecc_calc[4];	// TODO: change to dynamic allocation for fwd compatibility for > 2048 byte pages
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	for (i = 0, steps = 0; steps < eccsteps; steps++, i++, p += eccsize) {
		/* Read main area */
		chip->read_buf(mtd, p, eccsize);

		/* Get h/w ecc */
		ecc_calc[i] = brcm_nand_get_hw_ecc(steps);
	}
	chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);

	/*
	 * For blank/erased good pages, the HW ECC is 0x0 and the spare area is 0xff
	 * therefore, ignore the ECC check for blank/erased good pages.
	 */
	for (j = 0; j < mtd->oobsize ; j++) {
		if (chip->oob_poi[j] != 0xff)
		{
			/* not a blank page, check the ecc */
			skip_ecc_check = FALSE;
			break;
		}
	}

	for (i = 0; i < chip->ecc.total; i++)
		ecc_code[i] = chip->oob_poi[eccpos[i]];

	eccsteps = chip->ecc.steps;
	p = buf;

    if (!skip_ecc_check) {
		for (i = 0, j = 0; eccsteps; eccsteps--, i += eccbytes, j++, p += eccsize) {
			int stat;

			stat = nand_correct_data512(mtd, p, &ecc_code[i], (uint8_t *)&ecc_calc[j]);
			if (stat < 0)
				mtd->ecc_stats.failed++;
			else
				mtd->ecc_stats.corrected += stat;
		}
	}
	return 0;
}

static void brcm_nand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip,
				const uint8_t *buf)
{
	int i, j, steps;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	uint32_t ecc_calc[4];	// TODO: dynamic allocation
	const uint8_t *p = buf;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	uint8_t ecc_ptr[12];

	/* Write main area */
	chip->write_buf(mtd, p, mtd->writesize);

	for (i = 0, steps = 0; steps < eccsteps; steps++, i++) {
		/* Get h/w ecc */
		ecc_calc[i] = brcm_nand_get_hw_ecc(steps);
	}

	/* Copy the 12-byte ECC values */
	for (i = 0, j = 0; i < steps; i++, j += eccbytes)
		memcpy(&ecc_ptr[j], &ecc_calc[i], eccbytes);

	/* Rewrite new h/w ecc into oob on the fly */
	for (i = 0; i < chip->ecc.total; i++)
		chip->oob_poi[eccpos[i]] = ecc_ptr[i];

	/* Write the oob last */
	chip->write_buf(mtd, chip->oob_poi, mtd->oobsize);
}

void enter_otp_mode(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	int addr;
	unsigned char p1_data;

	if (config_nand_otp == NAND_MICRON_OTP) {
		addr = 0x90;
		p1_data = 0x01;

		/* Micron send CMD #EFh ADDR P1 P2 P3 P4 */
		chip->cmdfunc(mtd, NAND_CMD_SET_FEATURES, addr, -1);
		udelay(5);
		brcm_nand_write_byte(p1_data);
		brcm_nand_write_byte(0x0);
		brcm_nand_write_byte(0x0);
		brcm_nand_write_byte(0x0);
		udelay(5);
	} else {
		/* Stubs for other flash chips */
	}
}

void exit_otp_mode(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	int addr;
	unsigned char p1_data;

	if (config_nand_otp == NAND_MICRON_OTP) {
		addr = 0x90;
		p1_data = 0x00;

		/* Micron send CMD #EFh ADDR P1 P2 P3 P4 */
		chip->cmdfunc(mtd, NAND_CMD_SET_FEATURES, addr, -1);
		udelay(5);
		brcm_nand_write_byte(p1_data);
		brcm_nand_write_byte(0x0);
		brcm_nand_write_byte(0x0);
		brcm_nand_write_byte(0x0);
		udelay(5);
	} else {
		/* Stubs for other flash chips */
	}
}

static int __devinit bcm_nand_probe(struct platform_device *pdev)
{
	struct nand_chip *chip;
	struct resource *r;
	uint32_t base_addr;
	int err = 0;
	int i, maf_idx;
	u8 id_data[8];

	/* Allocate memory for MTD device structure and private data */
	board_mtd = kmalloc(sizeof(struct mtd_info)
			    + sizeof(struct nand_chip), GFP_KERNEL);
	if (!board_mtd) {
		pr_warning(
		       "Unable to allocate NAND MTD device structure.\n");
		return -ENOMEM;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -ENXIO;

	/* map physical address */
	bcm_nand_io_base = ioremap(r->start, r->end - r->start + 1);

	if (!bcm_nand_io_base) {
		pr_err("ioremap to access BCM NVSRAM NAND chip failed\n");
		kfree(board_mtd);
		return -EIO;
	}

	/* Get pointer to private data */
	chip = (struct nand_chip *)(&board_mtd[1]);

	/* Initialize structures */
	memset((char *)board_mtd, 0, sizeof(struct mtd_info));
	memset((char *)chip, 0, sizeof(struct nand_chip));

	/* Link the private data with the MTD structure */
	board_mtd->priv = chip;

	/* Get chip_sel and NAND bus width for appropriate chip_sel */
	base_addr = r->start;
	switch(base_addr)
	{
		case CS0_BASE_ADDR:
			chip_sel = 0;
			chip->options |=
				(readl(NVSRAM_REG_BASE + NVSRAM_NAND_OPMODE_CS0_OFFSET) &
					NVSRAM_NAND_OPMODE_CS0_MW_MASK) ? NAND_BUSWIDTH_16 : 0;
			break;
		case CS1_BASE_ADDR:
			chip_sel = 1;
			chip->options |=
				(readl(NVSRAM_REG_BASE + NVSRAM_NAND_OPMODE_CS1_OFFSET) &
					NVSRAM_NAND_OPMODE_CS1_MW_MASK) ? NAND_BUSWIDTH_16 : 0;
			break;
		case CS2_BASE_ADDR:
			chip_sel = 2;
			chip->options |=
				(readl(NVSRAM_REG_BASE + NVSRAM_NAND_OPMODE_CS2_OFFSET) &
					NVSRAM_NAND_OPMODE_CS2_MW_MASK) ? NAND_BUSWIDTH_16 : 0;
			break;
		case CS3_BASE_ADDR:
			chip_sel = 3;
			chip->options |=
				(readl(NVSRAM_REG_BASE + NVSRAM_NAND_OPMODE_CS3_OFFSET) &
					NVSRAM_NAND_OPMODE_CS3_MW_MASK) ? NAND_BUSWIDTH_16 : 0;
			break;
		default:
			pr_err("NAND_ERROR: Wrong base address\n");
			iounmap(bcm_nand_io_base);
			kfree(board_mtd);
			return -EINVAL;
	}

	/* Fill nand_chip structure */
	chip->read_byte = brcm_nand_read_byte;
	chip->read_word = brcm_nand_read_word;
	chip->write_buf = brcm_nand_write_buf;
	chip->read_buf = brcm_nand_read_buf;
	chip->verify_buf = brcm_nand_verify_buf;
	chip->cmdfunc = brcm_nand_cmdfunc;
	chip->select_chip = brcm_nand_select_chip;
	chip->block_markbad = brcm_nand_block_markbad;

	/* Clear Raw interrupts */
	writel((unsigned long)(1 << NVSRAM_MEMC_CFG_CLR_NAND_INT_CLR_SHIFT),
			NVSRAM_REG_BASE + NVSRAM_MEMC_CFG_CLR_OFFSET);

	err = nand_scan_ident(board_mtd, 1, NULL);
	if (err) {
		pr_err("nand_scan failed: %d\n", err);
		iounmap(bcm_nand_io_base);
		kfree(board_mtd);
		return err;
	}

	/*
	 * Re-issue NAND_CMD_READID.
	 *
	 * This is done to check for special features like on-die ecc
	 */
	chip->cmdfunc(board_mtd, NAND_CMD_READID, 0x00, -1);

	/* Read entire ID string */
	for (i = 0; i < 8; i++) {
		id_data[i] = chip->read_byte(board_mtd);
	}

	/* Loop through all manufacture ids defined in brcm_nand_ids[] */
	for (maf_idx = 0; brcm_nand_ids[maf_idx].maf_id != 0x0; maf_idx++) {
		/* Check NAND manuf ID against ones defined with special features */
		if (id_data[0] == brcm_nand_ids[maf_idx].maf_id) {
			/*
			 * Check if the feature bits for the above MAF ID is set in the
			 * defined id string byte
			 */
			if (id_data[brcm_nand_ids[maf_idx].id_byte] &&
					brcm_nand_ids[maf_idx].features) {
				/* Set the appropriate flag */
				host_controller_ecc = disabled;
			}
		}
	}

	/* Check for OTP */
	for (maf_idx = 0; brcm_nand_otp[maf_idx].maf_id != 0x0; maf_idx++) {
		/* Check NAND manuf ID against ones defined with OTP */
		if (id_data[0] == brcm_nand_otp[maf_idx].maf_id) {
			otp_start_addr = brcm_nand_otp[maf_idx].otp_start;
			otp_end_addr = brcm_nand_otp[maf_idx].otp_end;
			config_nand_otp = brcm_nand_otp[maf_idx].flags;
		}
	}

	/* Now that we know the nand size, we can setup the ECC layout */
	if (host_controller_ecc == enabled) {
		/* Using controller ECC */
		chip->ecc.mode = NAND_ECC_HW;
		chip->ecc.size = NAND_ECC_PAGE_SIZE;
		chip->ecc.bytes = NAND_ECC_NUM_BYTES;
		chip->ecc.read_page = brcm_nand_read_page_hwecc;
		chip->ecc.write_page = brcm_nand_write_page_hwecc;
	} else if (host_controller_ecc == disabled) {
		/* NOT using controller ECC */
		chip->ecc.mode = NAND_ECC_NONE;
	}

	switch (board_mtd->writesize)
	{
		case 4096:
			chip->ecc.layout = &nand_hw_eccoob_4096;
			break;
		case 2048:
			if (host_controller_ecc == enabled) {
				/* Using controller ECC */
				chip->ecc.layout = &nand_hw_eccoob_2048;
			} else if (host_controller_ecc == disabled) {
				/* NOT using controller ECC */
				chip->ecc.layout = &micron_nand_on_die_eccoob_2048;
			}
			break;
		case 512:
			chip->ecc.layout = &nand_hw_eccoob_512;
			break;
		default:
			pr_err("NAND - Unrecognized pagesize: %d\n",
			       board_mtd->writesize);
			return -EINVAL;
	}

	if (board_mtd->writesize > 512) {
		bbt_main_descr.offs = 2;
		bbt_main_descr.veroffs = 6;
		bbt_mirror_descr.offs = 2;
		bbt_mirror_descr.veroffs = 6;
		chip->badblock_pattern = &largepage_bbt;
	} else {
		chip->badblock_pattern = &smallpage_bbt;
	}

	chip->bbt_td = &bbt_main_descr;
	chip->bbt_md = &bbt_mirror_descr;

	chip->options |= NAND_USE_FLASH_BBT;

	/* Now finish off the scan, now that ecc has been initialized. */
	err = nand_scan_tail(board_mtd);
	if (err) {
		pr_err("nand_scan failed: %d\n", err);
		iounmap(bcm_nand_io_base);
		kfree(board_mtd);
		return err;
	}

	/* Register the partitions */
	{
		int nr_partitions;
		struct mtd_partition *partition_info;

		board_mtd->name = "bcm_umi-nand";
		nr_partitions = parse_mtd_partitions(board_mtd, part_probes,
						     &partition_info, 0);

		if (nr_partitions <= 0) {
			pr_err("BCM NAND: Too few partitions-%d\n",
			       nr_partitions);
			iounmap(bcm_nand_io_base);
			kfree(board_mtd);
			return -EIO;
		}

		add_mtd_partitions(board_mtd, partition_info, nr_partitions);
	}

	/* Return happy */
	return 0;
}

static int bcm_nand_remove(struct platform_device *pdev)
{
	/* Release resources, unregister device */
	nand_release(board_mtd);

	/* unmap physical address */
	iounmap(bcm_nand_io_base);

	/* Free the MTD device structure */
	kfree(board_mtd);

	return 0;
}

#ifdef CONFIG_PM
static int bcm_nand_suspend(struct platform_device *pdev,
				pm_message_t state)
{
	return 0;
}

static int bcm_nand_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define bcm_nand_suspend   NULL
#define bcm_nand_resume    NULL
#endif

static struct platform_driver nand_driver = {
	.driver = {
		   .name = "bcm-nand",
		   .owner = THIS_MODULE,
		   },
	.probe = bcm_nand_probe,
	.remove = bcm_nand_remove,
	.suspend = bcm_nand_suspend,
	.resume = bcm_nand_resume,
};

static int __init nand_init(void)
{
	return platform_driver_register(&nand_driver);
}

static void __exit nand_exit(void)
{
	platform_driver_unregister(&nand_driver);
}

module_init(nand_init);
module_exit(nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("BCM UMI MTD NAND driver");
