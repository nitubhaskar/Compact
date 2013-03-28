/*******************************************************************************
* Adapted Motorola's implementation for OMAP MMC controller
*
* Original Copyright: Copyright (C) 2010 Motorola, Inc.
*
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	drivers/mmc/host/bcmsdhc_raw.h
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

#define MMC_CMD_RETRIES        3

extern int raw_mmc_panic_probe(struct raw_hd_struct *rhd, int type);
extern int raw_mmc_panic_write(struct raw_hd_struct *rhd, char *buf,
		unsigned int offset, unsigned int len);
extern int raw_mmc_panic_erase(struct raw_hd_struct *rhd, unsigned int offset,
		unsigned int len);

void raw_mmc_request_done(struct mmc_host *host, struct mmc_request *mrq);
