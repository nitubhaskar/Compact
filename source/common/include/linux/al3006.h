/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
* 	@file	include/linux/bma222.h
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

#ifndef LINUX_AL3006_MODULE_H
#define LINUX_AL3006_MODULE_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#else
#include <sys/ioctl.h>
#endif

#include <plat/bcm_i2c.h>

#ifdef __KERNEL__

struct al3006_platform_data {
	struct i2c_slave_platform_data i2c_pdata;
	int (*init) (void);
	//int (*init) (struct device *);
	void (*exit) (struct device *);
};

#define AL3006_CHIP_ID          3

#define AL3006_CHIP_ID_REG                      0x00
#define AL3006_MODE_CTRL_REG                    0x00
#define AL3006_ALS_DATA_REG                     0x05

#endif /* __KERNEL__ */

/* IOCTL MACROS */

#define DYNA_AL3006_IOCTL_MAGIC 'l'
#define DYNA_AL3006_AMBIENT_IOCTL_GET_ENABLED \
		_IOR(DYNA_AL3006_IOCTL_MAGIC, 1, int *)
#define DYNA_AL3006_AMBIENT_IOCTL_ENABLE \
		_IOW(DYNA_AL3006_IOCTL_MAGIC, 2, int *)
#define DYNA_AL3006_PSENSOR_IOCTL_GET_ENABLED \
		_IOR(DYNA_AL3006_IOCTL_MAGIC, 3, int *)
#define DYNA_AL3006_PSENSOR_IOCTL_ENABLE \
		_IOW(DYNA_AL3006_IOCTL_MAGIC, 4, int *)


#endif /* LINUX_AL3006_MODULE_H */
