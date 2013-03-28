/*******************************************************************************
* Adapted Motorola's implementation for OMAP MMC controller
*
* Original Copyright: Copyright (C) 2010 Motorola, Inc.
*
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/mmc/host/bcmsdhc_raw.c
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

#include <linux/delay.h>
#include <linux/highmem.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/card.h>
#include <linux/clk.h>
#include <linux/slab.h>
/* #include <asm/mach/mmc.h> */
#include <linux/init.h>

#include <mach/sdio.h>

#include "bcmsdhc.h"
#include "bcmsdhc_raw.h"

static const unsigned int tran_exp[] = {
	10000,		100000,		1000000,	10000000,
	0,		0,		0,		0
};

static const unsigned char tran_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

static const unsigned int tacc_exp[] = {
	1,	10,	100,	1000,	10000,	100000,	1000000, 10000000,
};

static const unsigned int tacc_mant[] = {
	0,	10,	12,	13,	15,	20,	25,	30,
	35,	40,	45,	50,	55,	60,	70,	80,
};

#define UNSTUFF_BITS(resp,start,size)					\
	({								\
		const int __size = size;				\
		const u32 __mask = (__size < 32 ? 1 << __size : 0) - 1;	\
		const int __off = 3 - ((start) / 32);			\
		const int __shft = (start) & 31;			\
		u32 __res;						\
									\
		__res = resp[__off] >> __shft;				\
		if (__size + __shft > 32)				\
			__res |= resp[__off-1] << ((32 - __shft) % 32);	\
		__res & __mask;						\
	})

extern void bcmsdhc_request(struct mmc_host *mmc, struct mmc_request *mrq);
extern void bcmsdhc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios);
extern int bcmsdhc_get_ro(struct mmc_host *mmc);
extern int bcmsdhc_get_cd(struct mmc_host *mmc_host);
extern void bcmsdhc_init(struct bcmsdhc_host *host, u32 mask);

/*
 * NOTE:
 * Re-using mmc_host and bcmsdhc_host structures from the parent bcmsdhc
 * driver. These structures can NOT get corrupted and we can thus safely
 * re-use the same here. We however redefine mmc_card structure and populate
 * it based on the responses from the initialization command seq.
 */
struct mmc_card emmc_card;
struct mmc_card usd_card;

struct mmc_host *kpanic_core_mmc_host;
struct mmc_host *kpanic_core_sd_host;
static struct bcmsdhc_host *kpanic_host;
static struct mmc_card *kpanic_card;

u8 buf512[512];
u8 buf1024[1024];

/* Define global flag to store current context; Set it to NORMAL by default */
int context = NORMAL;

/*
 * Control chip select pin on a host.
 */
static void raw_mmc_set_chip_select(struct mmc_host *host, int mode)
{
	host->ios.chip_select = mode;
	host->ops->set_ios(host, &host->ios);
}

/*
 * Apply power to the MMC stack.  This is a two-stage process.
 * First, we enable power to the card without the clock running.
 * We then wait a bit for the power to stabilise.  Finally,
 * enable the bus drivers and clock to the card.
 *
 * We must _NOT_ enable the clock prior to power stablising.
 *
 * If a host does all the power sequencing itself, ignore the
 * initial MMC_POWER_UP stage.
 */
static void raw_mmc_power_up(struct mmc_host *host)
{
	host->ios.vdd = ffs(MMC_VDD_165_195) - 1;
	host->ios.chip_select = MMC_CS_DONTCARE;
	host->ios.bus_mode = MMC_BUSMODE_OPENDRAIN;
	host->ios.power_mode = MMC_POWER_UP;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;

	host->ops->set_ios(host, &host->ios);

	/*
	 * This delay should be sufficient to allow the power supply
	 * to reach the minimum voltage.
	 */
	mdelay(10);

	host->ios.clock = host->f_min;

	host->ios.power_mode = MMC_POWER_ON;
	host->ops->set_ios(host, &host->ios);

	/*
	 * This delay must be at least 74 clock sizes, or 1 ms, or the
	 * time required to reach a stable voltage.
	 */
	mdelay(10);
}

/**
 *	raw_mmc_request_done - finish processing an MMC request
 *	@host: MMC host which completed request
 *	@mrq: MMC request which request
 *
 *	MMC drivers should call this function when they have completed
 *	their processing of a request in the Panic path.
 */
void raw_mmc_request_done(struct mmc_host *host, struct mmc_request *mrq)
{
	struct mmc_command *cmd = mrq->cmd;
	int err = cmd->error;

	if (err && cmd->retries && mmc_host_is_spi(host)) {
		if (cmd->resp[0] & R1_SPI_ILLEGAL_COMMAND)
			cmd->retries = 0;
	}

	if (err && cmd->retries) {
		pr_err("KPANIC-MMC: host req failed (CMD%u): %d, retrying...\n",
			cmd->opcode, err);

		cmd->retries--;
		cmd->error = 0;
		host->ops->request(host, mrq);
	} else {
		led_trigger_event(host->led, LED_OFF);

		pr_debug("KPANIC-MMC: host req done (CMD%u): %d: %08x %08x %08x %08x\n",
			cmd->opcode, err,
			cmd->resp[0], cmd->resp[1],
			cmd->resp[2], cmd->resp[3]);

		if (mrq->data) {
			pr_debug("KPANIC-MMC: host %d bytes transferred: %d\n",
				mrq->data->bytes_xfered, mrq->data->error);
		}

		if (mrq->stop) {
			pr_debug("KPANIC-MMC: host (CMD%u): %d: %08x %08x %08x %08x\n",
				mrq->stop->opcode,
				mrq->stop->error,
				mrq->stop->resp[0], mrq->stop->resp[1],
				mrq->stop->resp[2], mrq->stop->resp[3]);
		}

		if (mrq->done) {
			return;
		}
	}
}

static void
raw_mmc_start_request(struct mmc_host *host, struct mmc_request *mrq)
{
	mrq->cmd->error = 0;
	mrq->cmd->mrq = mrq;
	if (mrq->data) {
		BUG_ON(mrq->data->blksz > host->max_blk_size);
		BUG_ON(mrq->data->blocks > host->max_blk_count);
		BUG_ON(mrq->data->blocks * mrq->data->blksz >
			host->max_req_size);

		mrq->cmd->data = mrq->data;
		mrq->data->error = 0;
		mrq->data->mrq = mrq;
		if (mrq->stop) {
			mrq->data->stop = mrq->stop;
			mrq->stop->error = 0;
			mrq->stop->mrq = mrq;
		}
	}
	host->ops->request(host, mrq);
}

void raw_mmc_wait_for_req(struct mmc_host *host, struct mmc_request *mrq)
{
	raw_mmc_start_request(host, mrq);
}

static int raw_mmc_wait_for_cmd(struct mmc_host *host,
		 struct mmc_command *cmd, int retries)
{
	struct mmc_request mrq;

	memset(&mrq, 0, sizeof(struct mmc_request));

	memset(cmd->resp, 0, sizeof(cmd->resp));
	cmd->retries = retries;

	mrq.cmd = cmd;
	cmd->data = NULL;

	raw_mmc_start_request(host, &mrq);

	return cmd->error;
}

static int raw_mmc_go_idle(struct mmc_host *host)
{
	int err;
	struct mmc_command cmd;

	/*
	 * Non-SPI hosts need to prevent chipselect going active during
	 * GO_IDLE; that would put chips into SPI mode.  Remind them of
	 * that in case of hardware that won't pull up DAT3/nCS otherwise.
	 *
	 * SPI hosts ignore ios.chip_select; it's managed according to
	 * rules that must accomodate non-MMC slaves which this layer
	 * won't even know about.
	 */
	if (!mmc_host_is_spi(host)) {
		raw_mmc_set_chip_select(host, MMC_CS_HIGH);
		mdelay(1);
	}

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_GO_IDLE_STATE;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_NONE | MMC_CMD_BC;

	err = raw_mmc_wait_for_cmd(host, &cmd, 0);

	mdelay(1);

	if (!mmc_host_is_spi(host)) {
		raw_mmc_set_chip_select(host, MMC_CS_DONTCARE);
		mdelay(1);
	}

	return err;
}

int raw_mmc_send_op_cond(struct mmc_host *host, u32 ocr, u32 *rocr)
{
	struct mmc_command cmd;
	int i, err = 0;

	BUG_ON(!host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SEND_OP_COND;
	cmd.arg = ocr;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R3 | MMC_CMD_BCR;

	for (i = 100; i; i--) {
		err = raw_mmc_wait_for_cmd(host, &cmd, 0);
		if (err)
			break;

		/* if we're just probing, do a single pass */
		if (ocr == 0)
			break;

		if (cmd.resp[0] & MMC_CARD_BUSY)
			break;

		err = -ETIMEDOUT;

		mdelay(10);
	}

	if (rocr)
		*rocr = cmd.resp[0];

	return err;
}

static void raw_mmc_power_off(struct mmc_host *host)
{
	host->ios.clock = 0;
	host->ios.vdd = 0;
	if (!mmc_host_is_spi(host)) {
		host->ios.bus_mode = MMC_BUSMODE_OPENDRAIN;
		host->ios.chip_select = MMC_CS_DONTCARE;
	}
	host->ios.power_mode = MMC_POWER_OFF;
	host->ios.bus_width = MMC_BUS_WIDTH_1;
	host->ios.timing = MMC_TIMING_LEGACY;
	host->ops->set_ios(host, &host->ios);
}

/*
 * Mask off any voltages we don't support and select
 * the lowest voltage
 */
u32 raw_mmc_select_voltage(struct mmc_host *host, u32 ocr)
{
	int bit;

	ocr &= host->ocr_avail;

	bit = ffs(ocr);
	if (bit) {
		bit -= 1;

		ocr &= 3 << bit;

		host->ios.vdd = bit;
		host->ops->set_ios(host, &host->ios);
	} else {
		pr_warning("KPANIC-MMC: host doesn't support card's voltages\n");
		ocr = 0;
	}

	return ocr;
}

int raw_mmc_all_send_cid(struct mmc_host *host, u32 *cid)
{
	int err;
	struct mmc_command cmd;

	BUG_ON(!host);
	BUG_ON(!cid);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_ALL_SEND_CID;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_BCR;

	err = raw_mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	memcpy(cid, cmd.resp, sizeof(u32) * 4);

	return 0;
}
static int raw_mmc_send_csd(struct mmc_card *card, u32 *csd)
{
	int err;
	struct mmc_host *host = card->host;
	struct mmc_command cmd;

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SEND_CSD;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_R2 | MMC_CMD_AC;

	err = raw_mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	memcpy(csd, cmd.resp, sizeof(u32) * 4);

	return 0;
}

/*
 * Given a 128-bit response, decode to our card CSD structure.
 */
static int raw_mmc_decode_csd(struct mmc_card *card)
{
	struct mmc_csd *csd = &card->csd;
	unsigned int e, m;
	u32 *resp = card->raw_csd;

	/*
	 * We only understand CSD structure v1.1 and v1.2.
	 * v1.2 has extra information in bits 15, 11 and 10.
	 * We also support eMMC v4.4 & v4.41.
	 */
	csd->structure = UNSTUFF_BITS(resp, 126, 2);
	if (csd->structure == 0) {
		pr_err("KPANIC-MMC: unrecognised CSD structure version %d\n",
			csd->structure);
		return -EINVAL;
	}

	csd->mmca_vsn	 = UNSTUFF_BITS(resp, 122, 4);
	m = UNSTUFF_BITS(resp, 115, 4);
	e = UNSTUFF_BITS(resp, 112, 3);
	csd->tacc_ns	 = (tacc_exp[e] * tacc_mant[m] + 9) / 10;
	csd->tacc_clks	 = UNSTUFF_BITS(resp, 104, 8) * 100;

	m = UNSTUFF_BITS(resp, 99, 4);
	e = UNSTUFF_BITS(resp, 96, 3);
	csd->max_dtr	  = tran_exp[e] * tran_mant[m];
	csd->cmdclass	  = UNSTUFF_BITS(resp, 84, 12);

	e = UNSTUFF_BITS(resp, 47, 3);
	m = UNSTUFF_BITS(resp, 62, 12);
	csd->capacity	  = (1 + m) << (e + 2);

	csd->read_blkbits = UNSTUFF_BITS(resp, 80, 4);
	csd->read_partial = UNSTUFF_BITS(resp, 79, 1);
	csd->write_misalign = UNSTUFF_BITS(resp, 78, 1);
	csd->read_misalign = UNSTUFF_BITS(resp, 77, 1);
	csd->r2w_factor = UNSTUFF_BITS(resp, 26, 3);
	csd->write_blkbits = UNSTUFF_BITS(resp, 22, 4);
	csd->write_partial = UNSTUFF_BITS(resp, 21, 1);

	return 0;
}

/*
 * Given the decoded CSD structure, decode the raw CID to our CID structure.
 */
static int raw_mmc_decode_cid(struct mmc_card *card)
{
	u32 *resp = card->raw_cid;

	/*
	 * The selection of the format here is based upon published
	 * specs from sandisk and from what people have reported.
	 */
	switch (card->csd.mmca_vsn) {
	case 0: /* MMC v1.0 - v1.2 */
	case 1: /* MMC v1.4 */
		card->cid.manfid	= UNSTUFF_BITS(resp, 104, 24);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.prod_name[6]	= UNSTUFF_BITS(resp, 48, 8);
		card->cid.hwrev		= UNSTUFF_BITS(resp, 44, 4);
		card->cid.fwrev		= UNSTUFF_BITS(resp, 40, 4);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 24);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;
		break;

	case 2: /* MMC v2.0 - v2.2 */
	case 3: /* MMC v3.1 - v3.3 */
	case 4: /* MMC v4 */
		card->cid.manfid	= UNSTUFF_BITS(resp, 120, 8);
		card->cid.oemid		= UNSTUFF_BITS(resp, 104, 16);
		card->cid.prod_name[0]	= UNSTUFF_BITS(resp, 96, 8);
		card->cid.prod_name[1]	= UNSTUFF_BITS(resp, 88, 8);
		card->cid.prod_name[2]	= UNSTUFF_BITS(resp, 80, 8);
		card->cid.prod_name[3]	= UNSTUFF_BITS(resp, 72, 8);
		card->cid.prod_name[4]	= UNSTUFF_BITS(resp, 64, 8);
		card->cid.prod_name[5]	= UNSTUFF_BITS(resp, 56, 8);
		card->cid.serial	= UNSTUFF_BITS(resp, 16, 32);
		card->cid.month		= UNSTUFF_BITS(resp, 12, 4);
		card->cid.year		= UNSTUFF_BITS(resp, 8, 4) + 1997;
		break;

	default:
		pr_err("card has unknown MMCA version %d\n",
			card->csd.mmca_vsn);
		return -EINVAL;
	}

	return 0;
}

int raw_mmc_set_relative_addr(struct mmc_card *card)
{
	int err;
	struct mmc_command cmd;

	BUG_ON(!card);
	BUG_ON(!card->host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SET_RELATIVE_ADDR;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

	err = raw_mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

/*
 * Change the bus mode (open drain/push-pull) of a host.
 */
void raw_mmc_set_bus_mode(struct mmc_host *host, unsigned int mode)
{
	host->ios.bus_mode = mode;
	host->ops->set_ios(host, &host->ios);
}

static int _raw_mmc_select_card(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_command cmd;

	BUG_ON(!host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SELECT_CARD;

	if (card) {
		cmd.arg = card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;
	} else {
		cmd.arg = 0;
		cmd.flags = MMC_RSP_NONE | MMC_CMD_AC;
	}

	err = raw_mmc_wait_for_cmd(host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	return 0;
}

static int raw_mmc_select_card(struct mmc_card *card)
{
	BUG_ON(!card);

	return _raw_mmc_select_card(card->host, card);
}

static int raw_mmc_deselect_cards(struct mmc_host *host)
{
	return _raw_mmc_select_card(host, NULL);
}

static int raw_mmc_send_ext_csd(struct mmc_card *card, void *buf)
{
	int err = 0;
	struct mmc_host *host = card->host;
	struct mmc_command  cmd;
	struct mmc_data data;
	struct mmc_request mrq;
	struct scatterlist sg;
	/* dma onto stack is unsafe/nonportable, but callers to this
	 * routine normally provide temporary on-stack buffers ...
	 */
	void *data_buf = &buf512[0];
	memset(data_buf, 0, 512);

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	cmd.opcode = MMC_SEND_EXT_CSD;
	cmd.arg = 0;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	data.blksz = 512;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;
	data.sg = &sg;
	data.sg_len = 1;
	data.timeout_ns = 300000000;
	data.timeout_clks = 0;

	sg_init_one(&sg, data_buf, 512);

	mrq.cmd = &cmd;
	mrq.data = &data;

	raw_mmc_wait_for_req(host, &mrq);
	if (cmd.error || data.error) {
		pr_err("KPANIC-MMC: done req with cmd.error=%#x,"
			"data.error=%#x\n", cmd.error, data.error);
		err = -1;
	}
	memcpy(buf, data_buf, 512);

	return err;
}

/*
 * Read and decode extended CSD.
 */
static int raw_mmc_read_ext_csd(struct mmc_card *card)
{
	int err;
	u8 ext_csd[512];

	BUG_ON(!card);

	if (card->csd.mmca_vsn < CSD_SPEC_VER_4)
		return 0;

	memset(ext_csd, 0, sizeof(ext_csd));

	err = raw_mmc_send_ext_csd(card, ext_csd);
	if (err) {
		/* If the host or the card can't do the switch,
		 * fail more gracefully. */
		if ((err != -EINVAL)
		 && (err != -ENOSYS)
		 && (err != -EFAULT))
			goto out;

		/*
		 * High capacity cards should have this "magic" size
		 * stored in their CSD.
		 */
		if (card->csd.capacity == (4096 * 512)) {
			pr_err("KPANIC-MMC: unable to read EXT_CSD "
				"on a possible high capacity card. "
				"Card will be ignored.\n");
		} else {
			pr_warning("KPANIC-MMC: unable to read "
				"EXT_CSD, performance might "
				"suffer.\n");
			err = 0;
		}

		goto out;
	}

	/* Version is coded in the CSD_STRUCTURE byte in the EXT_CSD register */
	if (card->csd.structure == 3) {
		int ext_csd_struct = ext_csd[EXT_CSD_STRUCTURE];
		if (ext_csd_struct > 2) {
			pr_err("KPANIC-MMC: unrecognised EXT_CSD structure "
				"version %d\n", ext_csd_struct);
			err = -EINVAL;
			goto out;
		}
	}

	card->ext_csd.rev = ext_csd[EXT_CSD_REV];
	if (card->ext_csd.rev > 5) {
		pr_err("KPANIC-MMC: unrecognised EXT_CSD revision %d\n",
			card->ext_csd.rev);
		err = -EINVAL;
		goto out;
	}

	if (card->ext_csd.rev >= 2) {
		card->ext_csd.sectors =
			ext_csd[EXT_CSD_SEC_CNT + 0] << 0 |
			ext_csd[EXT_CSD_SEC_CNT + 1] << 8 |
			ext_csd[EXT_CSD_SEC_CNT + 2] << 16 |
			ext_csd[EXT_CSD_SEC_CNT + 3] << 24;
		if (card->ext_csd.sectors) {
			unsigned boot_sectors;
			/* size is in 256K chunks, i.e. 512 sectors each */
			boot_sectors = ext_csd[EXT_CSD_BOOT_SIZE_MULTI] * 512;
			card->ext_csd.sectors -= boot_sectors;
			mmc_card_set_blockaddr(card);
		}
	}

	switch (ext_csd[EXT_CSD_CARD_TYPE] & EXT_CSD_CARD_TYPE_MASK) {
	case EXT_CSD_CARD_TYPE_52 | EXT_CSD_CARD_TYPE_26:
		card->ext_csd.hs_max_dtr = 52000000;
		break;
	case EXT_CSD_CARD_TYPE_26:
		card->ext_csd.hs_max_dtr = 26000000;
		break;
	default:
		/* MMC v4 spec says this cannot happen */
		pr_warning("KPANIC-MMC: card is mmc v4 but doesn't "
			"support any high-speed modes.\n");
	}

	if (card->ext_csd.rev >= 3) {
		u8 sa_shift = ext_csd[EXT_CSD_S_A_TIMEOUT];

		/* Sleep / awake timeout in 100ns units */
		if (sa_shift > 0 && sa_shift <= 0x17)
			card->ext_csd.sa_timeout =
					1 << ext_csd[EXT_CSD_S_A_TIMEOUT];
	}

out:

	return err;
}

int raw_mmc_send_status(struct mmc_card *card, u32 *status)
{
	int err;
	struct mmc_command cmd;

	BUG_ON(!card);
	BUG_ON(!card->host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SEND_STATUS;
	cmd.arg = card->rca << 16;
	cmd.flags = MMC_RSP_SPI_R2 | MMC_RSP_R1 | MMC_CMD_AC;

	err = raw_mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	if (status)
		*status = cmd.resp[0];

	return 0;
}

int raw_mmc_switch(struct mmc_card *card, u8 set, u8 index, u8 value)
{
	int err;
	struct mmc_command cmd;
	u32 status;

	BUG_ON(!card);
	BUG_ON(!card->host);

	memset(&cmd, 0, sizeof(struct mmc_command));

	cmd.opcode = MMC_SWITCH;
	cmd.arg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
		  (index << 16) |
		  (value << 8) |
		  set;
	cmd.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	err = raw_mmc_wait_for_cmd(card->host, &cmd, MMC_CMD_RETRIES);
	if (err)
		return err;

	/* Must check status to be sure of no errors */
	do {
		err = raw_mmc_send_status(card, &status);
		if (err)
			return err;
		if (card->host->caps & MMC_CAP_WAIT_WHILE_BUSY)
			break;
	} while (R1_CURRENT_STATE(status) == 7);

	if (status & 0xFDFFA000)
		pr_warning("unexpected status %#x after "
			   "switch", status);
	if (status & R1_SWITCH_ERROR)
		return -EBADMSG;

	return 0;
}

/*
 * Select timing parameters for host.
 */
void raw_mmc_set_timing(struct mmc_host *host, unsigned int timing)
{
	host->ios.timing = timing;
	host->ops->set_ios(host, &host->ios);
}

/*
 * Sets the host clock to the highest possible frequency that
 * is below "hz".
 */
void raw_mmc_set_clock(struct mmc_host *host, unsigned int hz)
{
	WARN_ON(hz < host->f_min);

	if (hz > host->f_max)
		hz = host->f_max;

	host->ios.clock = hz;
	host->ops->set_ios(host, &host->ios);
}

/*
 * Change data bus width of a host.
 */
void raw_mmc_set_bus_width(struct mmc_host *host, unsigned int width)
{
	host->ios.bus_width = width;
	host->ops->set_ios(host, &host->ios);
}

static int raw_mmc_init(struct mmc_host *host, struct mmc_card *card)
{
	int err = 0;
	u32 ocr;
	u32 cid[4];
	unsigned int max_dtr;

	/* Clear previous host->ios values */
	memset(&host->ios, 0, sizeof(struct mmc_ios));

	/* First Power off the host */
	raw_mmc_power_off(host);

	/* Power up the host */
	raw_mmc_power_up(host);

	/* Send CMD0 */
	err = raw_mmc_go_idle(host);
	if (err) {
		pr_err("KPANIC-MMC: host command go idle failed"
			" (%d)\n", err);
		goto out;
	}

	mdelay(20);

	err = raw_mmc_send_op_cond(host, 0, &ocr);
	if (err) {
		pr_err("KPANIC-MMC: host command reading Card OCR failed"
			" (%d)\n", err);
		goto out;
	}

	/*
	 * Sanity check the voltages that the card claims to
	 * support.
	 */
	if (ocr & 0x7F) {
		pr_warning("KPANIC-MMC: host card claims to support voltages "
				"below the defined range. These will be ignored.\n");
		ocr &= ~0x7F;
	}

	host->ocr = 0;
	host->ocr = raw_mmc_select_voltage(host, ocr);

	/*
	 * Can we support the voltage of the card?
	 */
	if (!host->ocr) {
		err = -EINVAL;
		goto out;
	}

	/*
	 * Since we're changing the OCR value, we seem to
	 * need to tell some cards to go back to the idle
	 * state.  We wait 1ms to give cards time to
	 * respond.
	 */
	raw_mmc_go_idle(host);

	/* The extra bit indicates that we support high capacity */
	err = raw_mmc_send_op_cond(host, host->ocr | (1 << 30), NULL);
	if (err) {
		pr_err("KPANIC-MMC: host command setting OCR failed"
			" (%d)\n", err);
		goto out;
	}

	/*
	 * Fetch CID from card.
	 */
	err = raw_mmc_all_send_cid(host, cid);
	if (err) {
		pr_err("KPANIC-MMC: host command fetch CID failed"
			" (%d)\n", err);
		goto out;
	}

	/*
	 * Set card RCA and quit open drain mode.
	 */
	card->host = host;
	card->type = MMC_TYPE_MMC;
	card->rca = 1;
	memcpy(card->raw_cid, cid, sizeof(card->raw_cid));

	err = raw_mmc_set_relative_addr(card);
	if (err) {
		pr_err("KPANIC-MMC: host command set RCA failed"
			" (%d)\n", err);
		goto out;
	}

	raw_mmc_set_bus_mode(host, MMC_BUSMODE_PUSHPULL);

	/*
	 * Fetch CSD from card.
	 */
	err = raw_mmc_send_csd(card, card->raw_csd);
	if (err) {
		pr_err("KPANIC-MMC: host command send CSD failed"
			" (%d)\n", err);
		goto out;
	}

	err = raw_mmc_decode_csd(card);
	if (err) {
		pr_err("KPANIC-MMC: host Decode CSD failed"
			" (%d)\n", err);
		goto out;
	}

	err = raw_mmc_decode_cid(card);
	if (err) {
		pr_err("KPANIC-MMC: host Decode CID failed"
			" (%d)\n", err);
		goto out;
	}

	err = raw_mmc_select_card(card);
	if (err) {
		pr_err("KPANIC-MMC: host command Select card failed"
			" (%d)\n", err);
		goto out;
	}

	/*
	 * Fetch and process extended CSD.
	 */
	err = raw_mmc_read_ext_csd(card);
	if (err) {
		pr_err("KPANIC-MMC: host command read EXT_CSD failed"
			" (%d)\n", err);
		goto out;
	}

	/*
	 * Activate high speed (if supported)
	 */
	if ((card->ext_csd.hs_max_dtr != 0) &&
		(host->caps & MMC_CAP_MMC_HIGHSPEED)) {
		err = raw_mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
			EXT_CSD_HS_TIMING, 1);
		if (err && err != -EBADMSG)
			goto out;

		if (err) {
			pr_warning("KPANIC-MMC: host switch to highspeed failed\n");
			err = 0;
		} else {
			mmc_card_set_highspeed(card);
			raw_mmc_set_timing(card->host, MMC_TIMING_MMC_HS);
		}
	}

	/*
	 * Compute bus speed.
	 */
	max_dtr = (unsigned int)-1;

	if (mmc_card_highspeed(card)) {
		if (max_dtr > card->ext_csd.hs_max_dtr)
			max_dtr = card->ext_csd.hs_max_dtr;
	} else if (max_dtr > card->csd.max_dtr) {
		max_dtr = card->csd.max_dtr;
	}

	raw_mmc_set_clock(host, max_dtr);

	/*
	 * Activate wide bus (if supported).
	 */
	if ((card->csd.mmca_vsn >= CSD_SPEC_VER_4) &&
	    (host->caps & (MMC_CAP_4_BIT_DATA | MMC_CAP_8_BIT_DATA))) {
		unsigned ext_csd_bit, bus_width;

		if (host->caps & MMC_CAP_8_BIT_DATA) {
			ext_csd_bit = EXT_CSD_BUS_WIDTH_8;
			bus_width = MMC_BUS_WIDTH_8;
		} else {
			ext_csd_bit = EXT_CSD_BUS_WIDTH_4;
			bus_width = MMC_BUS_WIDTH_4;
		}

		err = raw_mmc_switch(card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_BUS_WIDTH, ext_csd_bit);

		if (err && err != -EBADMSG)
			goto out;

		if (err) {
			pr_warning("KPANIC-MMC: host switch to bus width %d "
			       "failed\n",
			       1 << bus_width);
			err = 0;
		} else {
			raw_mmc_set_bus_width(card->host, bus_width);
		}
	}

	host->card = card;

	mmc_card_set_present(card);

	err = 0;

out:
	return err;
}

static int raw_sd_init(struct mmc_host *host, struct mmc_card *card)
{
	int err = 0;
	/* Stub. Will be implemented later for SD */

	return err;
}

/*
 * You never know current state of eMMC/SD card when panic happens
 * So need a clear startup
 */
static int raw_mmc_probe(struct raw_hd_struct *rhd,
			struct mmc_host *mmc)
{
	int ret = 0;
	struct bcmsdhc_host *host;

	host = mmc_priv(mmc);

	/* Clean the below structures by setting them to NULL */
	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;

	pr_err("KPANIC-MMC: probe eMMC chip\n");
	pr_err("KPANIC-MMC: host - %p, mmc_host - %p\n", host, mmc);

	pr_err("KPANIC-MMC: host(%d), ioaddr: %p\n",
		host->sdhc_slot, host->ioaddr);

	/* Reset the entire host controller core */
	if (host->bcm_plat->external_reset &&
	    host->bcm_plat->external_reset(host->ioaddr, host->sdhc_slot)) {
		goto out;
	}

	/* Configure system IOCR2 SD registry */
	ret = host->bcm_plat->syscfg_interface(SYSCFG_SDHC1 +
					       host->sdhc_slot - 1,
					       SYSCFG_INIT);
	if (ret)
		goto out;

	pr_info("KPANIC-MMC: host(%d) supports block size=%d\n",
				host->sdhc_slot, host->max_block);

	/* Disable DMA/ADMA flags to use PIO mode */
	host->flags &= ~(SDHCI_USE_DMA | SDHCI_USE_ADMA);

	bcmsdhc_init(host, SOFT_RESET_ALL);

	ret = host->bcm_plat->syscfg_interface(SYSCFG_SDHC1 +
					     host->sdhc_slot - 1,
					     SYSCFG_ENABLE);
	if (ret)
		goto out;

	return 0;

out:
	return -1;
}

int raw_mmc_panic_probe(struct raw_hd_struct *rhd, int type)
{
	int ret = 0;

	/* Initialize all the pointers to NULL to start with */
	kpanic_host = NULL;
	kpanic_card = NULL;

	if (type == MMC_TYPE_MMC) {
		/* Set context to IN_PANIC */
		context = IN_PANIC;

		/* Probe for eMMC */
		ret = raw_mmc_probe(rhd, kpanic_core_mmc_host);
		if (ret)
			return -1;

		kpanic_host = mmc_priv(kpanic_core_mmc_host);

		/* Send the initialization commands for eMMC */
		ret = raw_mmc_init(kpanic_core_mmc_host, &emmc_card);
		if (ret)
			return -1;

		kpanic_card = &emmc_card;
	} else if (type == MMC_TYPE_SD) {
		/* Set context to IN_PANIC */
		context = IN_PANIC;

		/* Probe for SD */
		ret = raw_mmc_probe(rhd, kpanic_core_sd_host);
		if (ret)
			return -1;

		kpanic_host = mmc_priv(kpanic_core_sd_host);

		/* Send the initialization commands for SD */
		ret = raw_sd_init(kpanic_core_sd_host, &usd_card);
		if (ret)
			return -1;

		kpanic_card = &usd_card;
	} else {
		return -1;
	}

	return 0;
}

static int raw_mmc_write_mmc(char *buf, sector_t start_sect, sector_t nr_sects,
			unsigned int offset, unsigned int len)
{
	int err;
	struct mmc_command	cmd;
	struct mmc_command	cmd1;
	struct mmc_data	data;
	struct mmc_command	stop;
	struct mmc_request	mrq;
	struct scatterlist sg;

	pr_err("KPANIC-MMC: %s : start_sect=%u, nr_sects=%u, "
		"offset=%u, len=%u\n", __func__, (unsigned int)start_sect,
		(unsigned int)nr_sects, offset, len);

	if (!len)
		return 0;
	if (offset + len > nr_sects * 512) {
		pr_err("KPANIC-MMC: writing buf too long for "
			"the partition\n");
		return 0;
	}
	if (offset % 512 != 0) {
		pr_err("KPANIC-MMC: writing offset not aligned to "
			"sector size\n");
		return 0;
	}
	/* truncate those bytes that are not aligned to word */
	/* buffer not aligned to sector size is taken care of */

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&stop, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));

	cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
	cmd.arg = start_sect + offset / 512;
	if (kpanic_host && !mmc_card_blockaddr(kpanic_host->mmc->card))
		cmd.arg <<= 9;
	cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	stop.opcode = MMC_STOP_TRANSMISSION;
	stop.arg = 0;
	stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	data.blksz = 512;
	data.blocks = (len + 511) / 512;
	data.timeout_ns = 300000000;
	data.timeout_clks = 0;
	data.sg = &sg;
	data.sg_len = 1;
	data.flags |= MMC_DATA_WRITE;

	sg_init_one(&sg, buf, (data.blocks * data.blksz));

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;

	if (kpanic_host != NULL)
		raw_mmc_wait_for_req(kpanic_host->mmc, &mrq);
	if (mrq.cmd->error || mrq.data->error || mrq.stop->error) {
		pr_err("KPANIC-MMC: done req with cmd->error=%#x,"
			"data->error=%#x, stop->error=%#x\n",
			mrq.cmd->error, mrq.data->error, mrq.stop->error);
		return 0;
	}

	memset(&cmd1, 0, sizeof(struct mmc_command));
	do {
		cmd1.opcode = MMC_SEND_STATUS;
		cmd1.arg = kpanic_card->rca << 16;
		cmd1.flags = MMC_RSP_R1 | MMC_CMD_AC;
		err = raw_mmc_wait_for_cmd(kpanic_host->mmc, &cmd1, 5);
		if (err) {
			printk(KERN_ERR "KPANIC-MMC: error %d requesting status\n", err);
			return 0;
		}
		/*
		 * Some cards mishandle the status bits,
		 * so make sure to check both the busy
		 * indication and the card state.
		 */
	} while (!(cmd1.resp[0] & R1_READY_FOR_DATA) ||
		(R1_CURRENT_STATE(cmd1.resp[0]) == 7));

	return len;
}

int raw_mmc_panic_write(struct raw_hd_struct *rhd, char *buf,
			unsigned int offset, unsigned int len)
{
	return raw_mmc_write_mmc(buf, rhd->start_sect, rhd->nr_sects,
			offset, len);
}

/*
 * offset and len should be aligned to 512
 */
int raw_mmc_panic_erase(struct raw_hd_struct *rhd, unsigned int offset,
			unsigned int len)
{
	return 0;
#if 0
	int ret = -1;

	if (!kpanic_host) {
		pr_err("KPANIC_MMC: no card probed\n");
		return -1;
	}
	if ((offset % 512) || (len % 512)) {
		pr_err("KPANIC_MMC: non-aligned erase\n");
		return -1;
	}
	if ((offset + len) / 512 > rhd->nr_sects) {
		pr_err("KPANIC_MMC: out of range erase\n");
		return -1;
	}
	if (!len)
		return 0;

	if (kpanic_host->card.type == MMC_TYPE_MMC)
		ret = -1;
	else if (kpanic_host->card.type == MMC_TYPE_SD)
		ret = raw_sd_erase(kpanic_host, rhd->start_sect +
			offset / 512, len / 512);
	else
		pr_err("KPANIC_MMC: card.type not recognized %d\n",
			kpanic_host->card.type);

	if (ret)
		pr_err("KPANIC_MMC: erase mmc/SD failed\n");
	return ret;
#endif
}

