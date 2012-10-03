/*******************************************************************************
* Copyright 2010 Broadcom Corporation.  All rights reserved.
*
*	@file	drivers/video/broadcom/displays/lcd_ILI9481.h
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

/****************************************************************************
*
*  lcd.c
*
*  PURPOSE:
*    This is the LCD-specific code for a ILI9481 module.
*
*****************************************************************************/

#ifndef __BCM_LCD_ILI9481__
#define __BCM_LCD_ILI9481__

#define RESET_SEQ {WR_CMND, 0x2A, 0},\
		{WR_DATA, 0x00, (dev->col_start) >> 8},\
		{WR_DATA, 0x00, dev->col_start & 0xFF},\
		{WR_DATA, 0x00, (dev->col_end) >> 8},\
		{WR_DATA, 0x00, dev->col_end & 0xFF},\
		{WR_CMND, 0x2B, 0},\
		{WR_DATA, 0x00, (dev->row_start) >> 8},\
		{WR_DATA, 0x00, dev->row_start & 0xFF},\
		{WR_DATA, 0x00, (dev->row_end) >> 8},\
		{WR_DATA, 0x00, dev->row_end & 0xFF},\
		{WR_CMND, 0x2C, 0}

#define LCD_CMD(x) (x)
#define LCD_DATA(x) (x)

#define LCD_HEIGHT              480
#define LCD_WIDTH               320

#define PANEL_HEIGHT              480
#define PANEL_WIDTH               320

#define LCD_BITS_PER_PIXEL	16


#define TEAR_SCANLINE	480

const char *LCD_panel_name = "HVGA ILI 9481 Controller";

int LCD_num_panels = 1;
LCD_Intf_t LCD_Intf = LCD_Z80;
LCD_Bus_t LCD_Bus = LCD_16BIT;

CSL_LCDC_PAR_SPEED_t timingReg = {24, 25, 0, 3, 6, 1};
CSL_LCDC_PAR_SPEED_t timingMem = {24, 25, 0, 3, 6, 1};

LCD_dev_info_t LCD_device[1] = {
	{
	 .panel		= LCD_main_panel,
	 .height	= LCD_HEIGHT,
	 .width		= LCD_WIDTH,
	 .bits_per_pixel = LCD_BITS_PER_PIXEL,
	 .te_supported	= false}
};

Lcd_init_t power_on_seq[] = {
	
	{WR_CMND, 0x11, 0}, 
	{SLEEP_MS, 0, 20},
	{WR_CMND, 0xD0, 0},
	{WR_DATA, 0x00, 0x07},
	{WR_DATA, 0x00, 0x42},
	{WR_DATA, 0x00, 0x1C},
	{WR_CMND, 0xD1, 0},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x2C},
	{WR_DATA, 0x00, 0x16},
	{WR_CMND, 0xD2, 0},
	{WR_DATA, 0x00, 0x01},
	{WR_DATA, 0x00, 0x11},

	{WR_CMND, 0xE4, 0},
	{WR_DATA, 0x00, 0xa0},

	{WR_CMND, 0xF3, 0},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x2A},			

	{WR_CMND, 0xC0, 0},
	{WR_DATA, 0x00, 0x10},
	{WR_DATA, 0x00, 0x3B},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x02},
	{WR_DATA, 0x00, 0x11},
	
	{WR_CMND, 0xC5, 0},
	{WR_DATA, 0x00, 0x03},
	
	{WR_CMND, 0xC8, 0},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x35},
	{WR_DATA, 0x00, 0x23},
	{WR_DATA, 0x00, 0x07},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x04},
	{WR_DATA, 0x00, 0x45},
	{WR_DATA, 0x00, 0x53},
	{WR_DATA, 0x00, 0x77},
	{WR_DATA, 0x00, 0x70},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x04},
	
	{WR_CMND, 0x36, 0},
	{WR_DATA, 0x00, 0x0A},	
	
	{WR_CMND, 0x3A, 0},
	{WR_DATA, 0x00, 0x55},//16bpp? should be changed to 18bpp(0x66)

	{WR_CMND, 0x2A, 0},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x01},
	{WR_DATA, 0x00, 0x3F},

	{WR_CMND, 0x2B, 0},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x00},
	{WR_DATA, 0x00, 0x01},
	{WR_DATA, 0x00, 0xE0},

	{SLEEP_MS, 0, 120},			
	{WR_CMND, 0x29, 0},	
	{CTRL_END, 0, 0},
};

Lcd_init_t power_off_seq[] = {
	{WR_CMND, 0x28, 0},
	{WR_CMND, 0x10, 0},
	{SLEEP_MS, 0, 120},
	{CTRL_END, 0, 0},
};

#endif
