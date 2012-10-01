/*******************************************************************************
* Copyright 2011 Broadcom Corporation.  All rights reserved.
*
*	@file	include/linux/i2c/BRCMMSI_ATMEGA168_TS.h
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


#ifndef __BRCMMSI_ATMEGA168_TS_H_
#define __BRCMMSI_ATMEGA168_TS_H_

#include <plat/bcm_i2c.h>
//#define VKEY_SUPPORT
#include <linux/earlysuspend.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>


#define	ATTB_PIN_LOW	0
/* Touch status */
#define ATMEGA168_MSI_MOVE			(1 << 1)
#define ATMEGA168_MSI_RELEASE		(1 << 2)
#define ATMEGA168_MSI_PRESS			(1 << 3)
#define ATMEGA168_MSI_DETECT		(1 << 4)

/* Touchscreen absolute values */
#define ATMEGA168_MSI_MAX_XC			0x3ff
#define ATMEGA168_MSI_MAX_YC			0x3ff
#define ATMEGA168_MSI_MAX_AREA		0xff

#define ATMEGA168_MSI_MAX_FINGER		10

//msi registers
#define ATMEGA168_MSI_BASE (0x0)
#define ATMEGA168_TOUCING 0x0
#define ATMEGA168_OLD_TOUCHING 0x01

#define ATMEGA168_POSX_LOW 0x02
#define ATMEGA168_POSX_HIGH 0x03

#define ATMEGA168_POSY_LOW 0x04
#define ATMEGA168_POSY_HIGN 0x05


#define ATMEGA168_POSX2_LOW 0x06
#define ATMEGA168_POSX2_HIGH 0x07

#define ATMEGA168_POSY2_LOW 0x08
#define ATMEGA168_POSY2_HIGN 0x09

#define ATMEGA168_X1_W  0x0a
#define ATMEGA168_Y1_W  0x0b
#define ATMEGA168_X2_W  0x0c
#define ATMEGA168_Y2_W  0x0d
#define ATMEGA168_X1_Z  0x0e
#define ATMEGA168_Y1_Z  0x0f
#define ATMEGA168_X2_Z  0x10
#define ATMEGA168_Y2_Z  0x11
#define ATMEGA168_STRENGTH_LOW 0x12
#define ATMEGA168_STRENGTH_HIGH 0x13
#define ATMEGA168_STRENGTH_POWER_MODE 0x14
#define ATMEGA168_INT_MODE 0x15
#define ATMEGA168_INT_WIDTH 0x16
#define ATMEGA168_NOISE_MODE 0x17
#define ATMEGA168_NOISE_THRESHOLD 0x18
#define ATMEGA168_NOISE_LEVEL 0x19
/*26~47 RESERVED*/
#define ATMEGA168_VERSION_1ST_BYTE 0x30
#define ATMEGA168_VERSION_2ND_BYTE 0x31
#define ATMEGA168_VERSION_3RD_BYTE 0x32
#define ATMEGA168_VERSION_4TH_BYTE 0x33
#define ATMEGA168_SUBVERSION 0x34
#define ATMEGA168_CRC_1ST_BYTE 0x35
#define ATMEGA168_CRC_2ND_BYTE 0x36
#define ATMEGA168_SPECOP 0x37
#define ATMEGA168_EEROM_READ_ADDR_1ST_BYTE 0x38
#define ATMEGA168_EEROM_READ_ADDR_2ND_BYTE 0x39

#define ATMEGA168_XTHR 0x3A
#define ATMEGA168_YTHR 0x3B
#define ATMEGA168_SYNCH 0x3C
/*raw data X: 61~124*/
#define ATMEGA168_RAW_XDATA_BASE 0x3D
/*raw data X: 125~188*/
#define ATMEGA168_RAW_YDATA_BASE 0x7D


#define ATMEGA168_CROSS_ENABLE 0xBD
#define ATMEGA168_CROSS_X 0xBE
#define ATMEGA168_CROSS_Y 0xBF
#define ATMEGA168_CROSSING_1ST_BYTE 0xC0
#define ATMEGA168_CROSSING_2ND_BYTE 0xC1
#define ATMEGA168_INTERNAL_ENABLE 0xC2
#define ATMEGA168_X_SIGNAL_1ST_BYTE 0xC3
#define ATMEGA168_X_SIGNAL_2ND_BYTE 0xC4
#define ATMEGA168_X_SIGNAL_3RD_BYTE 0xC5
#define ATMEGA168_Y_SIGNAL_1ST_BYTE 0xC6
#define ATMEGA168_Y_SIGNAL_2ND_BYTE 0xC7
#define ATMEGA168_Y_SIGNAL_3RD_BYTE 0xC8



struct atmega168_msi_platform_data {
    struct i2c_slave_platform_data i2c_pdata;
	unsigned int x_size;
	unsigned int y_size;
	unsigned int x_min;
	unsigned int y_min;
	unsigned int x_max;
	unsigned int y_max;
	unsigned int max_area;
	unsigned int blen;
	unsigned int threshold;
	unsigned int voltage;
	unsigned char orient;
	int (*platform_init)(void);
	int (*pen_pullup)(void);
	int (*pen_pulldown)(void);
    int (*platform_reset)(void);
	int (*attb_read_val) (void);
};
struct atmega168_msi_finger {
	int status;
	int x;
	int y;
	int area;
};

#ifdef VKEY_SUPPORT


struct ts_printkey_data {
	unsigned int code;
	u16 left;
	u16 right;
	u16 up;
	u16 down;
};

#endif

/* Each client has this additional data */
struct atmega168_msi_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	const struct atmega168_msi_platform_data *pdata;
	struct atmega168_msi_finger finger[ATMEGA168_MSI_MAX_FINGER];
	unsigned int irq;
	int pen_down;
	unsigned int attb_pin;
	wait_queue_head_t wait;
	struct mutex lock;
	struct delayed_work work;
#ifdef VKEY_SUPPORT
	struct ts_printkey_data *pk_data;
	u16 pk_num;
#endif
    struct early_suspend early_suspend;
    u8 fw_version;

};




#endif
