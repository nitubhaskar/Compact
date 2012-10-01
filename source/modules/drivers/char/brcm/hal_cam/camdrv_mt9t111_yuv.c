/******************************************************************************
Copyright 2010 Broadcom Corporation.  All rights reserved.

Unless you and Broadcom execute a separate written software license agreement
governing use of this software, this software is licensed to you under the
terms of the GNU General Public License version 2, available at
http://www.gnu.org/copyleft/gpl.html (the "GPL").

Notwithstanding the above, under no circumstances may you combine this software
in any way with any other Broadcom software provided under a license other than
the GPL, without Broadcom's express prior written consent.
******************************************************************************/

/**
*
*   @file   camdrv_mt9t111.c
*
*   @brief  This file is the lower level driver API of MT9T111(3M 2048*1536
*	Pixel) ISP/sensor.
*
*/
/**
 * @addtogroup CamDrvGroup
 * @{
 */

  /****************************************************************************/
  /*                          Include block                                   */
  /****************************************************************************/
#include <stdarg.h>
#include <stddef.h>

#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/poll.h>
#include <linux/sysctl.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/gpio.h>
#if 0
#include <mach/reg_camera.h>
#include <mach/reg_lcd.h>
#endif
#include <mach/reg_clkpwr.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <linux/broadcom/types.h>
#include <linux/broadcom/bcm_major.h>
#include <linux/broadcom/hw_cfg.h>
#include <linux/broadcom/hal_camera.h>
#include <linux/broadcom/lcd.h>
#include <linux/broadcom/bcm_sysctl.h>
#include <linux/broadcom/PowerManager.h>
#include <plat/dma.h>
#include <linux/dma-mapping.h>

#include "hal_cam_drv_ath.h"
//#include "hal_cam.h"
//#include "hal_cam_drv.h"
//#include "cam_cntrl_2153.h"
#include "camdrv_dev.h"

#include <plat/csl/csl_cam.h>

#define CAMERA_IMAGE_INTERFACE  CSL_CAM_INTF_CPI
#define CAMERA_PHYS_INTF_PORT   CSL_CAM_PORT_AFE_1

#define MT9T111_ID 0x2680

#define HAL_CAM_RESET 56   /*gpio 56*/
#define FLASH_TRIGGER  30   /*gpio 30 for flash */


#define HAL_CAM_VGA_STANDBY  9

#define HAL_CAM_VGA_RESET 10

int real_write=0;
CamFocusControlMode_t FOCUS_MODE;


/*****************************************************************************/
/* start of CAM configuration */
/*****************************************************************************/

/*****************************************************************************/
/*  Sensor Resolution Tables:												 */
/*****************************************************************************/
/* Resolutions & Sizes available for MT9T111 ISP/Sensor (QXGA max) */
static CamResolutionConfigure_t sSensorResInfo_MT9T111_st[] = {
/* width    height  Preview_Capture_Index       Still_Capture_Index */
	{128, 96, CamImageSize_SQCIF, -1},	/* CamImageSize_SQCIF */
	{160, 120, CamImageSize_QQVGA, -1},	/* CamImageSize_QQVGA */
	{176, 144, CamImageSize_QCIF, CamImageSize_QCIF},	/* CamImageSize_QCIF */
	{240, 180, -1, -1},	/* CamImageSize_240x180 */
	{240, 320, -1, -1},	/* CamImageSize_R_QVGA */
	{320, 240, CamImageSize_QVGA, CamImageSize_QVGA},	/* CamImageSize_QVGA */
	{352, 288, CamImageSize_CIF, CamImageSize_CIF},	/* CamImageSize_CIF */
	{426, 320, -1, -1},	/* CamImageSize_426x320 */
	{640, 480, CamImageSize_VGA, CamImageSize_VGA},	/* CamImageSize_VGA */
	{800, 600, CamImageSize_SVGA, CamImageSize_SVGA},	/* CamImageSize_SVGA */
	{1024, 768, -1, CamImageSize_XGA},	/* CamImageSize_XGA */
	{1280, 960, -1, -1},	/* CamImageSize_4VGA */
	{1280, 1024, -1, CamImageSize_SXGA},	/* CamImageSize_SXGA */
	{1600, 1200, -1, CamImageSize_UXGA},	/* CamImageSize_UXGA */
	{2048, 1536, -1, CamImageSize_QXGA},	/* CamImageSize_QXGA */
	{2560, 2048, -1, -1},	/* CamImageSize_QSXGA */
  	{144,	  176,   CamImageSize_R_QCIF,	CamImageSize_R_QCIF  } 	  //  CamImageSize_R_QCIF	 
};

/*****************************************************************************/
/*  Power On/Off Tables for Main Sensor */
/*****************************************************************************/

/*---------Sensor Power On */
static CamSensorIntfCntrl_st_t CamPowerOnSeq[] = {
	{PAUSE, 1, Nop_Cmd},
		
	{GPIO_CNTRL, HAL_CAM_VGA_STANDBY, GPIO_SetHigh},
	{PAUSE, 10, Nop_Cmd},/*for vga camera shutdown*/
	
	{GPIO_CNTRL, HAL_CAM_VGA_RESET, GPIO_SetHigh},
	{PAUSE, 10, Nop_Cmd}, /*for vga camera shutdown*/
	
/* -------Turn everything OFF   */
	{GPIO_CNTRL, HAL_CAM_RESET, GPIO_SetLow},
	{MCLK_CNTRL, CamDrv_NO_CLK, CLK_TurnOff},

/* -------Enable Clock to Cameras @ Main clock speed*/
	{MCLK_CNTRL, CamDrv_26MHz, CLK_TurnOn},
	{PAUSE, 10, Nop_Cmd},

/* -------Raise Reset to ISP*/
	{GPIO_CNTRL, HAL_CAM_RESET, GPIO_SetHigh},
	{PAUSE, 10, Nop_Cmd}

};

/*---------Sensor Power Off*/
static CamSensorIntfCntrl_st_t CamPowerOffSeq[] = {
	/* No Hardware Standby available. */
	{PAUSE, 50, Nop_Cmd},
/*--let primary camera enter soft standby mode , reset should be high-- */
	{GPIO_CNTRL, HAL_CAM_RESET, GPIO_SetHigh},
/* -------Disable Clock to Cameras*/
	{MCLK_CNTRL, CamDrv_NO_CLK, CLK_TurnOff},
};

/** Primary Sensor Configuration and Capabilities  */
static HAL_CAM_ConfigCaps_st_t CamPrimaryCfgCap_st = {
	// CAMDRV_DATA_MODE_S *video_mode
	{
        800,                           // unsigned short        max_width;                //Maximum width resolution
        600,                           // unsigned short        max_height;                //Maximum height resolution
        0,                             // UInt32                data_size;                //Minimum amount of data sent by the camera
        15,                            // UInt32                framerate_lo_absolute;  //Minimum possible framerate u24.8 format
        30,                            // UInt32                framerate_hi_absolute;  //Maximum possible framerate u24.8 format
        CAMDRV_TRANSFORM_NONE,         // CAMDRV_TRANSFORM_T    transform;            //Possible transformations in this mode / user requested transformations
        CAMDRV_IMAGE_YUV422,           // CAMDRV_IMAGE_TYPE_T    format;                //Image format of the frame.
        CAMDRV_IMAGE_YUV422_YCbYCr,    // CAMDRV_IMAGE_ORDER_T    image_order;        //Format pixel order in the frame.
        CAMDRV_DATA_SIZE_16BIT,        // CAMDRV_DATA_SIZE_T    image_data_size;    //Packing mode of the data.
        CAMDRV_DECODE_NONE,            // CAMDRV_DECODE_T        periph_dec;         //The decoding that the VideoCore transciever (eg CCP2) should perform on the data after reception.
        CAMDRV_ENCODE_NONE,            // CAMDRV_ENCODE_T        periph_enc;            //The encoding that the camera IF transciever (eg CCP2) should perform on the data before writing to memory.
        0,                             // int                    block_length;        //Block length for DPCM encoded data - specified by caller
        CAMDRV_DATA_SIZE_NONE,         // CAMDRV_DATA_SIZE_T    embedded_data_size; //The embedded data size from the frame.
        CAMDRV_MODE_VIDEO,             // CAMDRV_CAPTURE_MODE_T    flags;            //A bitfield of flags that can be set on the mode.
        30,                            // UInt32                framerate;            //Framerate achievable in this mode / user requested framerate u24.8 format
        0,                             // UInt8                mechanical_shutter;    //It is possible to use mechanical shutter in this mode (set by CDI as it depends on lens driver) / user requests this feature */
        1                              // UInt32                pre_frame;            //Frames to throw out for ViewFinder/Video capture
    },

	// CAMDRV_DATA_MODE_S *stills_mode
	{

        2048,                           // unsigned short max_width;   Maximum width resolution
        1536,                           // unsigned short max_height;  Maximum height resolution
        0,                              // UInt32                data_size;                //Minimum amount of data sent by the camera
        15,                             // UInt32                framerate_lo_absolute;  //Minimum possible framerate u24.8 format
        15,                             // UInt32                framerate_hi_absolute;  //Maximum possible framerate u24.8 format
        CAMDRV_TRANSFORM_NONE,          // CAMDRV_TRANSFORM_T    transform;            //Possible transformations in this mode / user requested transformations
        CAMDRV_IMAGE_JPEG,              // CAMDRV_IMAGE_TYPE_T    format;                //Image format of the frame.
        CAMDRV_IMAGE_YUV422_YCbYCr,     // CAMDRV_IMAGE_ORDER_T    image_order;        //Format pixel order in the frame.
        CAMDRV_DATA_SIZE_16BIT,         // CAMDRV_DATA_SIZE_T    image_data_size;    //Packing mode of the data.
        CAMDRV_DECODE_NONE,             // CAMDRV_DECODE_T        periph_dec;         //The decoding that the VideoCore transciever (eg CCP2) should perform on the data after reception.
        CAMDRV_ENCODE_NONE,             // PERIPHERAL_ENCODE_T    periph_enc;            //The encoding that the camera IF transciever (eg CCP2) should perform on the data before writing to memory.
        0,                              // int                    block_length;        //Block length for DPCM encoded data - specified by caller
        CAMDRV_DATA_SIZE_NONE,          // CAMDRV_DATA_SIZE_T    embedded_data_size; //The embedded data size from the frame.
        CAMDRV_MODE_VIDEO,              // CAMDRV_CAPTURE_MODE_T    flags;            //A bitfield of flags that can be set on the mode.
        15,                             // UInt32                framerate;            //Framerate achievable in this mode / user requested framerate u24.8 format
        0,                              // UInt8                mechanical_shutter;    //It is possible to use mechanical shutter in this mode (set by CDI as it depends on lens driver) / user requests this feature */
        4                               // UInt32                pre_frame;            //Frames to throw out for Stills capture
    },

	 ///< Focus Settings & Capabilities:  CAMDRV_FOCUSCONTROL_S *focus_control_st;
	{
    #ifdef AUTOFOCUS_ENABLED
        CamFocusControlAuto,        	// CAMDRV_FOCUSCTRLMODE_T default_setting=CamFocusControlOff;
        CamFocusControlAuto,        	// CAMDRV_FOCUSCTRLMODE_T cur_setting;
        CamFocusControlOn |             // UInt32 settings;  Settings Allowed: CamFocusControlMode_t bit masked
        CamFocusControlOff |
        CamFocusControlAuto |
        CamFocusControlAutoLock |
        CamFocusControlCentroid |
        CamFocusControlQuickSearch |
        CamFocusControlInfinity |
        CamFocusControlMacro
    #else
        CamFocusControlOff,             // CAMDRV_FOCUSCTRLMODE_T default_setting=CamFocusControlOff;
        CamFocusControlOff,             // CAMDRV_FOCUSCTRLMODE_T cur_setting;
        CamFocusControlOff              // UInt32 settings;  Settings Allowed: CamFocusControlMode_t bit masked
    #endif
    },

	 ///< Digital Zoom Settings & Capabilities:  CAMDRV_DIGITALZOOMMODE_S *digital_zoom_st;
    {
        CamZoom_1_0,        ///< CAMDRV_ZOOM_T default_setting;  default=CamZoom_1_0:  Values allowed  CamZoom_t
        CamZoom_1_0,        ///< CAMDRV_ZOOM_T cur_setting;  CamZoom_t
        CamZoom_4_0,        ///< CAMDRV_ZOOM_T max_zoom;  Max Zoom Allowed (256/max_zoom = *zoom)
        TRUE                    ///< Boolean capable;  Sensor capable: TRUE/FALSE:
    },

	/*< Sensor ESD Settings & Capabilities:  CamESD_st_t esd_st; */
	{
	 0x01,			/*< UInt8 ESDTimer;  Periodic timer to retrieve
				   the camera status (ms) */
	 FALSE			/*< Boolean capable;  TRUE/FALSE: */
	 },
	 
	CAMERA_IMAGE_INTERFACE,                ///< UInt32 intf_mode;  Sensor Interfaces to Baseband
    CAMERA_PHYS_INTF_PORT,   
	/*< Sensor version string */
	"MT9T111"
};

/*---------Sensor Primary Configuration CCIR656*/
static CamIntfConfig_CCIR656_st_t CamPrimaryCfg_CCIR656_st = {
	// Vsync, Hsync, Clock
	CSL_CAM_SYNC_EXTERNAL,				///< UInt32 sync_mode;				(default)CAM_SYNC_EXTERNAL:  Sync External or Embedded
	CSL_CAM_SYNC_DEFINES_ACTIVE,		///< UInt32 vsync_control;			(default)CAM_SYNC_DEFINES_ACTIVE:		VSYNCS determines active data
	CSL_CAM_SYNC_ACTIVE_HIGH,			///< UInt32 vsync_polarity; 		   default)ACTIVE_LOW/ACTIVE_HIGH:		  Vsync active
	CSL_CAM_SYNC_DEFINES_ACTIVE,		///< UInt32 hsync_control;			(default)FALSE/TRUE:					HSYNCS determines active data
	CSL_CAM_SYNC_ACTIVE_HIGH,			///< UInt32 hsync_polarity; 		(default)ACTIVE_HIGH/ACTIVE_LOW:		Hsync active
	CSL_CAM_CLK_EDGE_POS,				///< UInt32 data_clock_sample;		(default)RISING_EDGE/FALLING_EDGE:		Pixel Clock Sample edge
	CSL_CAM_PIXEL_8BIT, 				///< UInt32 bus_width;				(default)CAM_BITWIDTH_8:				Camera bus width
	0,							///< UInt32 data_shift; 				   (default)0:							   data shift (+) left shift  (-) right shift
	CSL_CAM_FIELD_H_V,					///< UInt32 field_mode; 			(default)CAM_FIELD_H_V: 				field calculated
	CSL_CAM_INT_FRAME_END,				///< UInt32 data_intr_enable;		CAM_INTERRUPT_t:
	CSL_CAM_INT_FRAME_END,				///< UInt32 pkt_intr_enable;		CAM_INTERRUPT_t:

};

// ************************* REMEMBER TO CHANGE IT WHEN YOU CHANGE TO CCP ***************************
//CSI host connection
//---------Sensor Primary Configuration CCP CSI (sensor_config_ccp_csi)
CamIntfConfig_CCP_CSI_st_t  CamPrimaryCfg_CCP_CSI_st =
{
    CSL_CAM_INPUT_DUAL_LANE,                    ///< UInt32 input_mode;     CSL_CAM_INPUT_MODE_T:
    CSL_CAM_INPUT_MODE_DATA_CLOCK,              ///< UInt32 clk_mode;       CSL_CAM_CLOCK_MODE_T:
    CSL_CAM_ENC_NONE,                           ///< UInt32 encoder;        CSL_CAM_ENCODER_T
    FALSE,                                      ///< UInt32 enc_predictor;  CSL_CAM_PREDICTOR_MODE_t
    CSL_CAM_DEC_NONE,                           ///< UInt32 decoder;        CSL_CAM_DECODER_T
    FALSE,                                      ///< UInt32 dec_predictor;  CSL_CAM_PREDICTOR_MODE_t
    CSL_CAM_PORT_CHAN_0,                                 ///< UInt32 sub_channel;    CSL_CAM_CHAN_SEL_t
    TRUE,                                       ///< UInt32 term_sel;       BOOLEAN
    CSL_CAM_PIXEL_8BIT,                             ///< UInt32 bus_width;      CSL_CAM_BITWIDTH_t
    CSL_CAM_PIXEL_NONE,                         ///< UInt32 emb_data_type;  CSL_CAM_DATA_TYPE_T
    CSL_CAM_PORT_CHAN_0,                                 ///< UInt32 emb_data_channel; CSL_CAM_CHAN_SEL_t
    FALSE,                                      ///< UInt32 short_pkt;      BOOLEAN
    CSL_CAM_PIXEL_NONE,                         ///< UInt32 short_pkt_chan; CSL_CAM_CHAN_SEL_t
    CSL_CAM_INT_FRAME_END,                         ///< UInt32 data_intr_enable; CSL_CAM_INTERRUPT_t:
    CSL_CAM_INT_FRAME_END,                          ///< UInt32 pkt_intr_enable;  CSL_CAM_INTERRUPT_t:
};


/*---------Sensor Primary Configuration YCbCr Input*/
static CamIntfConfig_YCbCr_st_t CamPrimaryCfg_YCbCr_st = {
/* YCbCr(YUV422) Input format = YCbCr=YUV= Y0 U0 Y1 V0  Y2 U2 Y3 V2 ....*/
	TRUE,			/*[00] Boolean yuv_full_range;
				   (default)FALSE: CROPPED YUV=16-240
				   TRUE: FULL RANGE YUV= 1-254  */
	SensorYCSeq_CbYCrY,	/*[01] CamSensorYCbCrSeq_t sensor_yc_seq;
				   (default) SensorYCSeq_YCbYCr */

/* Currently Unused */
	FALSE,			/*[02] Boolean input_byte_swap;
				   Currently UNUSED!! (default)FALSE:  TRUE: */
	FALSE,			/*[03] Boolean input_word_swap;
				   Currently UNUSED!! (default)FALSE:  TRUE: */
	FALSE,			/*[04] Boolean output_byte_swap;
				   Currently UNUSED!! (default)FALSE:  TRUE: */
	FALSE,			/*[05] Boolean output_word_swap;
				   Currently UNUSED!! (default)FALSE:  TRUE: */

/* Sensor olor Conversion Coefficients:
	color conversion fractional coefficients are scaled by 2^8 */
/* e.g. for R0 = 1.164, round(1.164 * 256) = 298 or 0x12a */
	CAM_COLOR_R1R0,		/*[06] UInt32 cc_red R1R0;
				   YUV422 to RGB565 color conversion red */
	CAM_COLOR_G1G0,		/*[07] UInt32 cc_green G1G0;
				   YUV422 to RGB565 color conversion green */
	CAM_COLOR_B1		/*[08] UInt32 cc_blue B1;
				   YUV422 to RGB565 color conversion blue */
};

/*---------Sensor Primary Configuration I2C */
static CamIntfConfig_I2C_st_t CamPrimaryCfg_I2C_st = {
	0x00,			/*I2C_SPD_430K, [00] UInt32 i2c_clock_speed;
				   max clock speed for I2C */
	I2C_CAM_DEVICE_ID,	/*[01] UInt32 i2c_device_id; I2C device ID */
	0x00,			/*I2C_BUS2_ID, [02] I2C_BUS_ID_t i2c_access_mode;
				   I2C baseband port */
	0x00,			/*I2CSUBADDR_16BIT,[03] I2CSUBADDR_t i2c_sub_adr_op;
				   I2C sub-address size */
	0xFFFF,			/*[04] UInt32 i2c_page_reg;
				   I2C page register addr (if applicable)
				   **UNUSED by this driver** */
	I2C_CAM_MAX_PAGE	/*[05] UInt32 i2c_max_page; I2C max page
				   (if not used by camera drivers, set = 0xFFFF)
				   **UNUSED by this driver** */
};

/*---------Sensor Primary Configuration IOCR */
static CamIntfConfig_IOCR_st_t CamPrimaryCfg_IOCR_st = {
	FALSE,			/*[00] Boolean cam_pads_data_pd;
				   (default)FALSE: IOCR2 D0-D7 pull-down disabled
				   TRUE: IOCR2 D0-D7 pull-down enabled */
	FALSE,			/*[01] Boolean cam_pads_data_pu;
				   (default)FALSE: IOCR2 D0-D7 pull-up disabled
				   TRUE: IOCR2 D0-D7 pull-up enabled */
	FALSE,			/*[02] Boolean cam_pads_vshs_pd;
				   (default)FALSE: IOCR2 Vsync/Hsync pull-down disabled
				   TRUE: IOCR2 Vsync/Hsync pull-down enabled */
	FALSE,			/*[03] Boolean cam_pads_vshs_pu;
				   (default)FALSE: IOCR2 Vsync/Hsync pull-up disabled
				   TRUE: IOCR2 Vsync/Hsync pull-up enabled */
	FALSE,			/*[04] Boolean cam_pads_clk_pd;
				   (default)FALSE: IOCR2 CLK pull-down disabled
				   TRUE: IOCR2 CLK pull-down enabled */
	FALSE,			/*[05] Boolean cam_pads_clk_pu;
				   (default)FALSE: IOCR2 CLK pull-up disabled
				   TRUE: IOCR2 CLK pull-up enabled */

	7 << 12,		/*[06] UInt32 i2c_pwrup_drive_strength;
				   (default)IOCR4_CAM_DR_12mA:
				   IOCR drive strength */
	0x00,			/*[07] UInt32 i2c_pwrdn_drive_strength;
				   (default)0x00:    I2C2 disabled */
	0x00,			/*[08] UInt32 i2c_slew; (default) 0x00: 42ns */

	7 << 12,		/*[09] UInt32 cam_pads_pwrup_drive_strength;
				   (default)IOCR4_CAM_DR_12mA:  IOCR drive strength */
	1 << 12			/*[10] UInt32 cam_pads_pwrdn_drive_strength;
				   (default)IOCR4_CAM_DR_2mA:   IOCR drive strength */
};

/* XXXXXXXXXXXXXXXXXXXXXXXXXXX IMPORTANT XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* TO DO: MURALI */
/* HAVE TO PROGRAM THIS IN THE ISP. */
/*---------Sensor Primary Configuration JPEG */

static CamIntfConfig_Jpeg_st_t CamPrimaryCfg_Jpeg_st = {
	1200,			/*< UInt32 jpeg_packet_size_bytes;     Bytes/Hsync */
	1360,			/*< UInt32 jpeg_max_packets;           Max Hsyncs/Vsync = (3.2Mpixels/4) / 512 */
	CamJpeg_FixedPkt_VarLine,	/*< CamJpegPacketFormat_t pkt_format;  Jpeg Packet Format */
};

/* XXXXXXXXXXXXXXXXXXXXXXXXXXX IMPORTANT XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX */
/* TO DO: MURALI */
/* WILL NEED TO MODIFY THIS. */
/*---------Sensor Primary Configuration Stills n Thumbs */
static CamIntfConfig_PktMarkerInfo_st_t CamPrimaryCfg_PktMarkerInfo_st = {
	2,			/*< UInt8       marker_bytes; # of bytes for marker,
				   (= 0 if not used) */
	0,			/*< UInt8       pad_bytes; # of bytes for padding,
				   (= 0 if not used) */
	TRUE,			/*< Boolean     TN_marker_used; Thumbnail marker used */
	0xFFBE,			/*< UInt32      SOTN_marker; Start of Thumbnail marker,
				   (= 0 if not used) */
	0xFFBF,			/*< UInt32      EOTN_marker; End of Thumbnail marker,
				   (= 0 if not used) */
	TRUE,			/*< Boolean     SI_marker_used; Status Info marker used */
	0xFFBC,			/*< UInt32      SOSI_marker; Start of Status Info marker,
				   (= 0 if not used) */
	0xFFBD,			/*< UInt32      EOSI_marker; End of Status Info marker,
				   (= 0 if not used) */
	FALSE,			/*< Boolean     Padding_used; Status Padding bytes used */
	0x0000,			/*< UInt32      SOPAD_marker; Start of Padding marker,
				   (= 0 if not used) */
	0x0000,			/*< UInt32      EOPAD_marker; End of Padding marker,
				   (= 0 if not used) */
	0x0000			/*< UInt32      PAD_marker; Padding Marker,
				   (= 0 if not used) */
};

/*---------Sensor Primary Configuration Stills n Thumbs */
static CamIntfConfig_InterLeaveMode_st_t CamPrimaryCfg_InterLeaveStills_st = {
	CamInterLeave_SingleFrame,	/*< CamInterLeaveMode_t mode;
					   Interleave Mode */
	CamInterLeave_PreviewLast,	/*< CamInterLeaveSequence_t sequence;
					   InterLeaving Sequence */
	CamInterLeave_PktFormat	/*< CamInterLeaveOutputFormat_t format;
				   InterLeaving Output Format */
};

/*---------Sensor Primary Configuration */
static CamIntfConfig_st_t CamSensorCfg_st = {
	&CamPrimaryCfgCap_st,	/* *sensor_config_caps; */
	&CamPrimaryCfg_CCIR656_st,	/* *sensor_config_ccir656; */
	&CamPrimaryCfg_CCP_CSI_st,
	&CamPrimaryCfg_YCbCr_st,	/* *sensor_config_ycbcr; */
	NULL,	                     /* *sensor_config_i2c; */
	&CamPrimaryCfg_IOCR_st,	/* *sensor_config_iocr; */
	&CamPrimaryCfg_Jpeg_st,	/* *sensor_config_jpeg; */
	NULL,			/* *sensor_config_interleave_video; */
	&CamPrimaryCfg_InterLeaveStills_st,	/**sensor_config_interleave_stills; */
	&CamPrimaryCfg_PktMarkerInfo_st	/* *sensor_config_pkt_marker_info; */
};

FlashLedState_t flash_switch;

/* I2C transaction result */
static HAL_CAM_Result_en_t sCamI2cStatus = HAL_CAM_SUCCESS;

static HAL_CAM_Result_en_t
SensorSetPreviewMode(CamImageSize_t image_resolution,
		     CamDataFmt_t image_format);
static HAL_CAM_Result_en_t Init_Mt9t111(CamSensorSelect_t sensor);
static int checkCameraID(CamSensorSelect_t sensor);
static UInt16 mt9t111_read(unsigned int sub_addr);
static HAL_CAM_Result_en_t mt9t111_write(unsigned int sub_addr, UInt16 data);
static HAL_CAM_Result_en_t CAMDRV_SetFrameRate_Pri(CamRates_t fps,
					CamSensorSelect_t sensor);
static HAL_CAM_Result_en_t CAMDRV_SetVideoCaptureMode_Pri(CamImageSize_t image_resolution,
					       CamDataFmt_t image_format,
					       CamSensorSelect_t sensor);
static HAL_CAM_Result_en_t CAMDRV_SetZoom_Pri(CamZoom_t step,CamSensorSelect_t sensor);
static void control_flash(int flash_on);

/*****************************************************************************
*
* Function Name:   CAMDRV_GetIntfConfigTbl
*
* Description: Return Camera Sensor Interface Configuration
*
* Notes:
*
*****************************************************************************/
static CamIntfConfig_st_t *CAMDRV_GetIntfConfig_Pri(CamSensorSelect_t nSensor)
{

/* ---------Default to no configuration Table */
	CamIntfConfig_st_t *config_tbl = NULL;

	switch (nSensor) {
	case CamSensorPrimary:	/* Primary Sensor Configuration */
	default:
		CamSensorCfg_st.sensor_config_caps = &CamPrimaryCfgCap_st;
		break;
	case CamSensorSecondary:	/* Secondary Sensor Configuration */
		CamSensorCfg_st.sensor_config_caps = NULL;
		break;
	}
	config_tbl = &CamSensorCfg_st;

#ifdef ALLOC_TN_BUFFER
	if (thumbnail_data == NULL) {
		/*tn_buffer = (u8 *)dma_alloc_coherent( NULL, TN_BUFFER_SIZE,
		   &physPtr, GFP_KERNEL); */
		thumbnail_data = (u8 *) kmalloc(TN_BUFFER_SIZE, GFP_KERNEL);
		if (thumbnail_data == NULL) {
			pr_info
			    ("Error in allocating %d bytes for MT9T111 sensor\n",
			     TN_BUFFER_SIZE);
			return NULL;
		}
		memset(thumbnail_data, 0, TN_BUFFER_SIZE);
	}
#endif
	return config_tbl;
}

/*****************************************************************************
*
* Function Name:   CAMDRV_GetInitPwrUpSeq
*
* Description: Return Camera Sensor Init Power Up sequence
*
* Notes:
*
*****************************************************************************/
static CamSensorIntfCntrl_st_t *CAMDRV_GetIntfSeqSel_Pri(CamSensorSelect_t nSensor,
					      CamSensorSeqSel_t nSeqSel,
					      UInt32 *pLength)
{

/* ---------Default to no Sequence  */
	CamSensorIntfCntrl_st_t *power_seq = NULL;
	*pLength = 0;

	switch (nSeqSel) {
	case SensorInitPwrUp:	/* Camera Init Power Up (Unused) */
	case SensorPwrUp:
		if ((nSensor == CamSensorPrimary)
		    || (nSensor == CamSensorSecondary)) {
			printk("SensorPwrUp Sequence\r\n");
			*pLength = sizeof(CamPowerOnSeq);
			power_seq = CamPowerOnSeq;
		}
		break;

	case SensorInitPwrDn:	/* Camera Init Power Down (Unused) */
	case SensorPwrDn:	/* Both off */
		if ((nSensor == CamSensorPrimary)
		    || (nSensor == CamSensorSecondary)) {
			printk("SensorPwrDn Sequence\r\n");
			*pLength = sizeof(CamPowerOffSeq);
			power_seq = CamPowerOffSeq;
		}
		break;

	case SensorFlashEnable:	/* Flash Enable */
		break;

	case SensorFlashDisable:	/* Flash Disable */
		break;

	default:
		break;
	}
	return power_seq;

}

/***************************************************************************
* *
*       CAMDRV_Supp_Init performs additional device specific initialization
*
*   @return  HAL_CAM_Result_en_t
*
*       Notes:
*/
static HAL_CAM_Result_en_t CAMDRV_Supp_Init_Pri(CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t ret_val = HAL_CAM_SUCCESS;

	return ret_val;
}				/* CAMDRV_Supp_Init() */

/****************************************************************************
*
* Function Name:   HAL_CAM_Result_en_t CAMDRV_Wakeup(CamSensorSelect_t sensor)
*
* Description: This function wakesup camera via I2C command.  Assumes camera
*              is enabled.
*
* Notes:
*
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_Wakeup_Pri(CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;

	result = Init_Mt9t111(sensor);
	printk("Init_Mt9t111 result =%d\r\n", result);
	return result;
}

static UInt16 CAMDRV_GetDeviceID_Pri(CamSensorSelect_t sensor)
{
	return mt9t111_read(0x00);
}

static int checkCameraID(CamSensorSelect_t sensor)
{
	UInt16 devId = CAMDRV_GetDeviceID_Pri(sensor);

	if (devId == MT9T111_ID) {
		printk("Camera identified as MT9T111\r\n");
		return 0;
	} else {
		printk("Camera Id wrong. Expected 0x%x but got 0x%x\r\n",
			 MT9T111_ID, devId);
		return -1;
	}
}

static HAL_CAM_Result_en_t mt9t111_write(unsigned int sub_addr, UInt16 data)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	sCamI2cStatus = HAL_CAM_SUCCESS;
	UInt8 bytes[2];
	bytes[0] = (data & 0xFF00) >> 8;
	bytes[1] = data & 0xFF;

	result |= CAM_WriteI2c(sub_addr, 2, bytes);
	if (result != HAL_CAM_SUCCESS) {
		sCamI2cStatus = result;
		pr_info("mt9t111_write(): ERROR: at addr:0x%x with value: 0x%x\n", sub_addr, data);
	}
	return result;
}

static UInt16 mt9t111_read(unsigned int sub_addr)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	sCamI2cStatus = HAL_CAM_SUCCESS;
	UInt16 data;
	UInt16 temp;

	result |= CAM_ReadI2c(sub_addr, 2, (UInt8 *) &data);
	if (result != HAL_CAM_SUCCESS) {
		sCamI2cStatus = result;
		pr_info("mt9t111_read(): ERROR: %d\r\n", result);
	}

	temp = data;
	data = ((temp & 0xFF) << 8) | ((temp & 0xFF00) >> 8);

	return data;
}

static HAL_CAM_Result_en_t
SensorSetPreviewMode(CamImageSize_t image_resolution, CamDataFmt_t image_format)
{
	UInt32 x = 0, y = 0;
	UInt32 format = 0;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	printk(KERN_ERR"SensorSetPreviewMode image_resolution 0x%x image_format %d  ",image_resolution,image_format  );

#if 0
	mt9t111_write( 0x098E, 0xEC05 );	// MCU_ADDRESS [PRI_B_NUM_OF_FRAMES_RUN]
	mt9t111_write( 0x0990, 0x0005 	);// MCU_DATA_0
	mt9t111_write( 0x098E, 0x8400 	);// MCU_ADDRESS [SEQ_CMD]
	mt9t111_write( 0x0990, 0x0001 	);// MCU_DATA_0

	return HAL_CAM_SUCCESS;
#endif 
	switch (image_resolution) {
	
	case CamImageSize_R_QCIF:
		x = 144;
		y = 176;
		break;
	
	case CamImageSize_R_QVGA:
		x = 240;
		y = 320;
		break;
		
	case CamImageSize_SQCIF:
		x = 128;
		y = 96;
		break;

	case CamImageSize_QQVGA:
		x = 160;
		y = 120;
		break;

	case CamImageSize_QCIF:
		x = 176;
		y = 144;
		break;

	case CamImageSize_QVGA:
		x = 320;
		y = 240;
		break;

	case CamImageSize_CIF:
		x = 352;
		y = 288;
		break;

	case CamImageSize_VGA:
		x = 640;
		y = 480;
		break;

	case CamImageSize_SVGA:
		x = 800;
		y = 600;
		break;

	default:
		x = 320;
		y = 240;
		break;
	}

	/* Choose Format for Viewfinder mode  */
	/* Set the format for the Viewfinder mode */
	switch (image_format) {

	case CamDataFmtYCbCr:
		format = 1;
		break;

	case CamDataFmtRGB565:
		format = 4;
		break;

	default:
		format = 1;
		break;
	}

	mt9t111_write( 0x098E, 0x6800 );	// MCU_ADDRESS [PRI_A_IMAGE_WIDTH]
	mt9t111_write( 0x0990, x );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6802 );	// MCU_ADDRESS [PRI_A_IMAGE_HEIGHT]
	mt9t111_write( 0x0990, y );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS [SEQ_CMD]
	mt9t111_write( 0x0990, 0x0006 );	// MCU_DATA_0

	if (image_format == CamDataFmtYCbCr) { /*YUV order*/
		UInt32 output_order = 2;	/* Switch low, high bytes. Y and C. */

		/* [Set Output Format Order] */
		/*MCU_ADDRESS[PRI_A_CONFIG_JPEG_JP_MODE] */
		mt9t111_write(0x098E, 0x6809);
		mt9t111_write(0x0990, output_order);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0x8400);	/* MCU_ADDRESS [SEQ_CMD] */
		mt9t111_write(0x0990, 0x0006);	/* MCU_DATA_0 */

		printk
		    ("SensorSetPreviewMode(): Output Format Order = 0x%x\r\n",
		     output_order);
	}

	printk("SensorSetPreviewMode(): Resolution:0x%x, Format:0x%x r\n",
		 image_resolution, image_format);

	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk
		    ("SensorSetPreviewMode(): Error[%d] sending preview mode  r\n",
		     sCamI2cStatus);
		result = sCamI2cStatus;
	}
	/*[Enable stream] */
	//mt9t111_write(0x001A, 0x0218);

	return result;

}


/** @} */

/****************************************************************************
*
* Function Name:   HAL_CAM_Result_en_t CAMDRV_SetVideoCaptureMode
				(CamImageSize_t image_resolution, CamDataFmt_t image_format)
*
* Description: This function configures Video Capture
				(Same as ViewFinder Mode)
*
* Notes:
*
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetVideoCaptureMode_Pri(CamImageSize_t image_resolution,
					       CamDataFmt_t image_format,
					       CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;

	/* --------Set up Camera Isp for Output Resolution & Format */
	result = SensorSetPreviewMode(image_resolution, image_format);
	return result;
}

/****************************************************************************
*
* Function Name:   HAL_CAM_Result_en_t CAMDRV_SetExposure_Pri(CamRates_t fps)
*
* Description: This function sets the Exposure compensation
*
* Notes:   
*
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetExposure_Pri(int value)
{ 
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	
	printk("CAMDRV_SetFrameRate_Pri %d \r\n",value );

	if(value==0)
	{
		mt9t111_write(0x98E, 0xE81F);	
		mt9t111_write(0x990, 0x0040);
		mt9t111_write(0x98E, 0xA80E);	
		mt9t111_write(0x990, 0x0006);	
	}
	else if(value>0)
	{
		mt9t111_write(0x98E, 0xE81F);	
		mt9t111_write(0x990, 0x0083);
		mt9t111_write(0x98E, 0xA80E);	
		mt9t111_write(0x990, 0x0006);	
	}
	else
	{
		mt9t111_write(0x98E, 0xE81F);	
		mt9t111_write(0x990, 0x001E);
		mt9t111_write(0x98E, 0xA80E);	
		mt9t111_write(0x990, 0x0004);
	}

	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		 printk("CAMDRV_SetExposure_Pri(): Error[%d] \r\n",
			  sCamI2cStatus);
		 result = sCamI2cStatus;
	}

    return result;
}


/****************************************************************************
*
* Function Name:   HAL_CAM_Result_en_t CAMDRV_SetFrameRate(CamRates_t fps)
*
* Description: This function sets the frame rate of the Camera Sensor
*
* Notes:    15 or 30 fps are supported.
*
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetFrameRate_Pri(CamRates_t fps,
					CamSensorSelect_t sensor)
{ 
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	
	printk("CAMDRV_SetFrameRate %d \r\n",fps );
	
					
	if (fps > CamRate_30)
	{
		result = HAL_CAM_ERROR_ACTION_NOT_SUPPORTED;
	}
	else
	{
		if (fps == CamRate_Auto)
		{

		}
		else
		{
			if(fps>=CamRate_15)
			{
				
				mt9t111_write(0x98E, 0x68AA);	//TX FIFO Watermark (A)
				mt9t111_write(0x990, 0x0218);	//		= 536
				mt9t111_write(0x98E, 0x6815);	//Max FD Zone 50 Hz
				mt9t111_write(0x990, 0x0007);	//		= 7
				mt9t111_write(0x98E, 0x6817 );//Max FD Zone 60 Hz
				mt9t111_write(0x990, 0x0008 );//	  = 8
				mt9t111_write(0x98E, 0x682D);	//AE Target FD Zone
				mt9t111_write(0x990, 0x0007);	//		= 7
				
				mt9t111_write( 0x098E, 0x8400); 	// MCU_ADDRESS [SEQ_CMD]
				mt9t111_write( 0x0990, 0x0006); 	// MCU_DATA_0
			}
			else if(fps>=CamRate_10)
			{
				mt9t111_write(0x98E, 0x68AA);	//TX FIFO Watermark (A)
				mt9t111_write(0x990, 0x026A);	//      = 618
				mt9t111_write(0x98E, 0x6815);	//Max FD Zone 50 Hz
				mt9t111_write(0x990, 0x000A);	//      = 10
				mt9t111_write(0x98E, 0x6817);	//Max FD Zone 60 Hz
				mt9t111_write(0x990, 0x000C);	//      = 12
				mt9t111_write(0x98E, 0x682D);	//AE Target FD Zone
				mt9t111_write(0x990, 0x000A);	//      = 10

				mt9t111_write( 0x098E, 0x8400); 	// MCU_ADDRESS [SEQ_CMD]
				mt9t111_write( 0x0990, 0x0006); 	// MCU_DATA_0
			}
			else if(fps>=CamRate_5)
			{
				mt9t111_write(0x98E, 0x68AA);	//TX FIFO Watermark (A)
				mt9t111_write(0x990, 0x0218);	//       = 536
				mt9t111_write(0x98E, 0x6815);	//Max FD Zone 50 Hz
				mt9t111_write(0x990, 0x0014);	//       = 20
				mt9t111_write(0x98E, 0x6817);	//Max FD Zone 60 Hz
				mt9t111_write(0x990, 0x0018);	//       = 24
				mt9t111_write(0x98E, 0x682D);	//AE Target FD Zone
				mt9t111_write(0x990, 0x0014);	//      = 20

				mt9t111_write( 0x098E, 0x8400); 	// MCU_ADDRESS [SEQ_CMD]
				mt9t111_write( 0x0990, 0x0006); 	// MCU_DATA_0
			}
			else
			{
				printk("CAMDRV_SetFrameRate(): Error HAL_CAM_ERROR_ACTION_NOT_SUPPORTED \r\n");
			}	
		}       // else (if (ImageSettingsConfig_st.sensor_framerate->cur_setting == CamRate_Auto))
	}       // else (if (fps <= CamRate_Auto))

	 if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		 printk("CAMDRV_SetFrameRate(): Error[%d] \r\n",
			  sCamI2cStatus);
		 result = sCamI2cStatus;
	 }

    return result;

}

/****************************************************************************
/
/ Function Name:   HAL_CAM_Result_en_t
					CAMDRV_EnableVideoCapture(CamSensorSelect_t sensor)
/
/ Description: This function starts camera video capture mode
/
/ Notes: camera_enable for preview after set preview mode  ;we can ignore this function ,for it has been done in  set preview mode
/
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_EnableVideoCapture_Pri(CamSensorSelect_t sensor)
{return HAL_CAM_SUCCESS;
	/*[Enable stream] */
	printk(KERN_ERR"9t111 CAMDRV_EnableVideoCapture \r\n");
	mt9t111_write(0x001A, 0x0218);
	msleep(300);
	return sCamI2cStatus;
}

/****************************************************************************
/
/ Function Name:   void CAMDRV_SetCamSleep(CamSensorSelect_t sensor )
/
/ Description: This function puts ISP in sleep mode & shuts down power.
/
/ Notes:
/
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetCamSleep_Pri(CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	UInt16 readout;
	printk("CAMDRV_SetCamSleep(): mt9t111 enter soft standby mode \r\n");
	
	readout=mt9t111_read(0x0028);	/* MCU_ADDRESS [PRI_B_NUM_OF_FRAMES_RUN] */
	readout&=0xFFFE;
	mt9t111_write(0x0028,readout);
	
	readout=mt9t111_read(0x0018);
	readout |= 0x1;
	mt9t111_write(0x0018,readout);
	msleep(3);
	
	readout=mt9t111_read(0x0018);
	if(!(readout&0x4000)){
		printk("failed: CAMDRV_SetCamSleep_Pri(): mt9t111 enter soft standby mode   \r\n");
		result=HAL_CAM_ERROR_OTHERS;
	}
	
	return result;
}

/*camera_disable for still mode */
static HAL_CAM_Result_en_t CAMDRV_DisableCapture_Pri(CamSensorSelect_t sensor)
{
	//return HAL_CAM_SUCCESS;
	/*[Disable stream] */
	//printk(KERN_ERR"in 9t111 CAMDRV_DisableCapture \r\n");
	control_flash(0);

	/*[Preview on] */
	mt9t111_write(0x098E, 0xEC05);	/* MCU_ADDRESS [PRI_B_NUM_OF_FRAMES_RUN] */
	mt9t111_write(0x0990, 0x0005);	/* MCU_DATA_0 */
	mt9t111_write(0x098E, 0x8400);	/* MCU_ADDRESS [SEQ_CMD] */
	mt9t111_write(0x0990, 0x0001);	/* MCU_DATA_0 */

	//printk("CAMDRV_DisableCapture(): \r\n");
	return sCamI2cStatus;
}

/****************************************************************************
/
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_DisablePreview(void)
/
/ Description: This function halts MT9M111 camera video
/
/ Notes:   camera_disable for preview mode , after this ,preview will not output data
/
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_DisablePreview_Pri(CamSensorSelect_t sensor)
{return HAL_CAM_SUCCESS;
	/* [Disable stream] */
	mt9t111_write(0x001A, 0x0018);

	printk("mt9t111 CAMDRV_DisablePreview(): \r\n");
	return sCamI2cStatus;
}

/****************************************************************************
/
/ Function Name:   voidcontrol_flash(int flash_on)
/
/ Description: flash_on ==1:  turn on flash
/			flash_on==0:   turn off flash
/
/ Notes:
/
****************************************************************************/

static void control_flash(int flash_on)
{
	gpio_request(FLASH_TRIGGER, "hal_cam_gpio_cntrl");
	
	if(flash_on==1)
	{
		if (gpio_is_valid(FLASH_TRIGGER))
		{
			gpio_direction_output(FLASH_TRIGGER, 1);
			msleep(5);		
		}
	}
	else
	{
		if (gpio_is_valid(FLASH_TRIGGER))
		{
			gpio_direction_output(FLASH_TRIGGER, 0);	
		}
	}
		
}


/****************************************************************************
/
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_CfgStillnThumbCapture(
					CamImageSize_t image_resolution,
					CamDataFmt_t format,
					CamSensorSelect_t sensor)
/
/ Description: This function configures Stills Capture
/
/ Notes:
/
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_CfgStillnThumbCapture_Pri(CamImageSize_t
						 stills_resolution,
						 CamDataFmt_t stills_format,
						 CamImageSize_t
						 thumb_resolution,
						 CamDataFmt_t thumb_format,
						 CamSensorSelect_t sensor)
{//return HAL_CAM_SUCCESS;
	UInt32 x = 0, y = 0;
	UInt32 tx = 0, ty = 0;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;

	/*printk("***************************** \
		CAMDRV_CfgStillnThumbCapture():STARTS ************************* \r\n");*/

	if(flash_switch==Flash_On)
		control_flash(1);

	switch (stills_resolution) {
	case CamImageSize_QCIF:

		x = 176;
		y = 144;
		break;

	case CamImageSize_QVGA:
		x = 320;
		y = 240;
		break;

	case CamImageSize_CIF:
		x = 352;
		y = 288;
		break;

	case CamImageSize_VGA:
		x = 640;
		y = 480;
		break;

	case CamImageSize_SVGA:
		x = 800;
		y = 600;
		break;

	case CamImageSize_XGA:
		x = 1024;
		y = 768;
		break;

	case CamImageSize_SXGA:
		x = 1280;
		y = 1024;
		break;

	case CamImageSize_UXGA:
		x = 1600;
		y = 1200;
		break;

	case CamImageSize_QXGA:
		x = 2048;
		y = 1536;
		break;

	default:
		x = 640;
		y = 480;
		break;
	}
	
                             
	//mt9t111_write( 0x098E, 0x6C09 	);/*picture YUV order*/
	//mt9t111_write( 0x0990, 0x0002 	);   
	//msleep(5);

	mt9t111_write( 0x098E, 0x6C00 );	// MCU_ADDRESS [PRI_B_IMAGE_WIDTH]
	mt9t111_write( 0x0990, x );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6C02 );	// MCU_ADDRESS [PRI_B_IMAGE_HEIGHT]
	mt9t111_write( 0x0990, y );	// MCU_DATA_0

	msleep(3);											//delay = 3

	mt9t111_write( 0x098E, 0xEC05 );	// MCU_ADDRESS [PRI_B_NUM_OF_FRAMES_RUN]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0

	msleep(1);//delay = 1

	
	
	mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS [SEQ_CMD]
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0

	
	//mdelay(100);
	//mt9t111_write(0x098E, 0x8401);
	//printk(KERN_ERR"------in CAMDRV_CfgStillnThumbCapture---- we have got status %d ---it should be 7 ", mt9t111_read(0x0990));
		
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk
		    ("CAMDRV_CfgStillnThumbCapture():Error sending capture mode \r\n");
		result = sCamI2cStatus;
	}

	/*printk("CAMDRV_CfgStillnThumbCapture(): stills_resolution = 0x%x, \
		 stills_format=0x%x \r\n", stills_resolution, stills_format);*/

	return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetSceneMode(
/					CamSceneMode_t scene_mode)
/
/ Description: This function will set the scene mode of camera
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetSceneMode_Pri(CamSceneMode_t scene_mode,
					CamSensorSelect_t sensor)
{return HAL_CAM_SUCCESS;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	switch(scene_mode) {
		case CamSceneMode_Auto:
			pr_info("CAMDRV_SetSceneMode() called for AUTO\n");
			mt9t111_write(0x098E, 0x6815);	// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			mt9t111_write(0x0990, 0x006);   // MCU_DATA_0
			mt9t111_write(0x098E, 0x6817);  // MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			mt9t111_write(0x0990, 0x006);   // MCU_DATA_0
			mt9t111_write(0x098E, 0x682D); 	// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			mt9t111_write(0x0990, 0x0003);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x682F);  // MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			mt9t111_write(0x0990, 0x0100);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x6839);  // MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			mt9t111_write(0x0990, 0x012C);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x6835);  // MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			mt9t111_write(0x0990, 0x00F0);  // MCU_DATA_0
			break;
		case CamSceneMode_Night:
			pr_info("CAMDRV_SetSceneMode() called for Night\n");
			mt9t111_write(0x098E, 0x6815);	// MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_50HZ]
			mt9t111_write(0x0990, 0x0018);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x6817);  // MCU_ADDRESS [PRI_A_CONFIG_FD_MAX_FDZONE_60HZ]
			mt9t111_write(0x0990, 0x0018);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x682D); 	// MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_FDZONE]
			mt9t111_write(0x0990, 0x0006);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x682F);  // MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_TARGET_AGAIN]
			mt9t111_write(0x0990, 0x0100);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x6839);  // MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_AGAIN]
			mt9t111_write(0x0990, 0x012C);  // MCU_DATA_0
			mt9t111_write(0x098E, 0x6835);  // MCU_ADDRESS [PRI_A_CONFIG_AE_TRACK_AE_MAX_VIRT_DGAIN]
			mt9t111_write(0x0990, 0x00F0);  // MCU_DATA_0
			break;
		default:
			pr_info("CAMDRV_SetSceneMode() not supported for %d\n", scene_mode);
			break;
	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetSceneMode(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}

	return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetWBMode(CamWB_WBMode_t wb_mode)
/
/ Description: This function will set the white balance of camera
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetWBMode_Pri(CamWB_WBMode_t wb_mode,
				     CamSensorSelect_t sensor)
{return HAL_CAM_SUCCESS;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_info("CAMDRV_SetWBMode()  called\n");
	if (wb_mode == CamWB_Auto) {

		mt9t111_write(0x098E, 0x8002);	/* MCU_ADDRESS [MON_MODE] */
		mt9t111_write(0x0990, 0x0000);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xC8F2);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xC8F3);
		mt9t111_write(0x0990, 0x007F);
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x098E, 0xE84A);
		mt9t111_write(0x0990, 0x0080);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x098E, 0xE84C);
		mt9t111_write(0x0990, 0x0080);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x098E, 0xE84D);
		mt9t111_write(0x0990, 0x0078);	/* 85   MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x098E, 0xE84F);
		mt9t111_write(0x0990, 0x007E);	/* 81    MCU_DATA_0 */

	} else if (wb_mode == CamWB_Incandescent) {

		mt9t111_write(0x098E, 0xC8F2);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xC8F3);
		mt9t111_write(0x0990, 0x0003);
		mt9t111_write(0x098E, 0xAC33);	/* MCU_ADDRESS [AWB_CCMPOSITION] */
		mt9t111_write(0x0990, 0x0000);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x098E, 0xE84A);
		mt9t111_write(0x0990, 0x0079);	/* 6C   MCU_DATA_0  */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x098E, 0xE84C);
		mt9t111_write(0x0990, 0x00DB);	/* 9B   MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x098E, 0xE84D);
		mt9t111_write(0x0990, 0x0079);	/* 6C   MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x098E, 0xE84F);
		mt9t111_write(0x0990, 0x00DB);	/* 9B   MCU_DATA_0 */

	} else if ((wb_mode == CamWB_DaylightFluorescent)
		   || (wb_mode == CamWB_WarmFluorescent)) {

		mt9t111_write(0x098E, 0xC8F2);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xC8F3);
		mt9t111_write(0x0990, 0x0003);
		mt9t111_write(0x098E, 0xAC33);	/* MCU_ADDRESS [AWB_CCMPOSITION] */
		mt9t111_write(0x0990, 0x0000);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x098E, 0xE84A);
		mt9t111_write(0x0990, 0x0075);	/* 79 6C      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x098E, 0xE84C);
		mt9t111_write(0x0990, 0x0099);	/* DB 9B      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x098E, 0xE84D);
		mt9t111_write(0x0990, 0x0075);	/* 79 6C      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x098E, 0xE84F);
		mt9t111_write(0x0990, 0x0099);	/* DB 9B      MCU_DATA_0 */

	} else if (wb_mode == CamWB_Daylight) {

		mt9t111_write(0x098E, 0xC8F2);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xC8F3);
		mt9t111_write(0x0990, 0x0003);
		mt9t111_write(0x098E, 0xAC33);	/* MCU_ADDRESS [AWB_CCMPOSITION] */
		mt9t111_write(0x0990, 0x0000);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x098E, 0xE84A);
		mt9t111_write(0x0990, 0x008E);	/* 79 6C      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x098E, 0xE84C);
		mt9t111_write(0x0990, 0x005C);	/* DB 9B      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x098E, 0xE84D);
		mt9t111_write(0x0990, 0x008E);	/* 79 6C      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x098E, 0xE84F);
		mt9t111_write(0x0990, 0x005C);	/* DB 9B      MCU_DATA_0 */

	} else if (wb_mode == CamWB_Cloudy) {

		mt9t111_write(0x098E, 0xC8F2);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xC8F3);
		mt9t111_write(0x0990, 0x0003);
		mt9t111_write(0x098E, 0xAC33);	/* MCU_ADDRESS [AWB_CCMPOSITION] */
		mt9t111_write(0x0990, 0x0000);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x098E, 0xE84A);
		mt9t111_write(0x0990, 0x0098);	/* 79 6C      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x098E, 0xE84C);
		mt9t111_write(0x0990, 0x004C);	/* DB 9B      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x098E, 0xE84D);
		mt9t111_write(0x0990, 0x0098);	/* 79 6C      MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x098E, 0xE84F);
		mt9t111_write(0x0990, 0x004C);	/* DB 9B      MCU_DATA_0 */

	} else if (wb_mode == CamWB_Twilight) {

		mt9t111_write(0x098E, 0xC8F2);
		mt9t111_write(0x0990, 0x0057);	/* 6F */
		mt9t111_write(0x098E, 0xC8F3);
		mt9t111_write(0x0990, 0x0059);	/* 6F */
		mt9t111_write(0x098E, 0xAC33);	/* MCU_ADDRESS [AWB_CCMPOSITION] */
		mt9t111_write(0x0990, 0x0058);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84A);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x0990, 0x009F);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84B);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_G_L] */
		mt9t111_write(0x0990, 0x0083);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84C);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x0990, 0x0004);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84D);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x0990, 0x00A5);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84E);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_G_R] */
		mt9t111_write(0x0990, 0x005E);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84F);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x0990, 0x0083);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xEC4A);	/*MCU_ADDRESS[PRI_B_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x0990, 0x009F);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xEC4B);	/*MCU_ADDRESS[PRI_B_CONFIG_AWB_K_G_L] */
		mt9t111_write(0x0990, 0x0083);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xEC4C);	/*MCU_ADDRESS[PRI_B_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x0990, 0x0004);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xEC4D);	/*MCU_ADDRESS[PRI_B_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x0990, 0x00A5);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xEC4E);	/*MCU_ADDRESS[PRI_B_CONFIG_AWB_K_G_R] */
		mt9t111_write(0x0990, 0x005E);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xEC4F);	/*MCU_ADDRESS[PRI_B_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x0990, 0x0083);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0x8400);	/* MCU_ADDRESS */
		mt9t111_write(0x0990, 0x0006);	/* MCU_DATA_0 */
	} else {
		printk("Am here in wb:%d\n", wb_mode);
		mt9t111_write(0x098E, 0x8002);	/* MCU_ADDRESS [MON_MODE] */
		mt9t111_write(0x0990, 0x0000);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xC8F2);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xC8F3);
		mt9t111_write(0x0990, 0x007F);
		mt9t111_write(0x098E, 0xE84A);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_R_L] */
		mt9t111_write(0x0990, 0x0080);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84C);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_B_L] */
		mt9t111_write(0x0990, 0x0080);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84D);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_R_R] */
		mt9t111_write(0x0990, 0x0078);	/* 85   MCU_DATA_0 */
		mt9t111_write(0x098E, 0xE84F);	/*MCU_ADDRESS[PRI_A_CONFIG_AWB_K_B_R] */
		mt9t111_write(0x0990, 0x007E);	/* 81   MCU_DATA_0 */
	}

	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetWBMode(): Error[%d] \r\n", sCamI2cStatus);
		result = sCamI2cStatus;
	}

	return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetAntiBanding(
/					CamAntiBanding_t effect)
/
/ Description: This function will set the antibanding effect of camera
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetAntiBanding_Pri(CamAntiBanding_t effect,
					  CamSensorSelect_t sensor)
{return HAL_CAM_SUCCESS;

	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_info("CAMDRV_SetAntiBanding()  called\n");
	if ((effect == CamAntiBandingAuto) || (effect == CamAntiBandingOff)) {
		mt9t111_write(0x098E, 0xA005);	/* MCU_ADDRESS [FD_FDPERIOD_SELECT] */
		mt9t111_write(0x0990, 0x0001);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_B_CONFIG_FD_ALGO_RUN] */
		mt9t111_write(0x098E, 0x6C11);
		mt9t111_write(0x0990, 0x0003);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_FD_ALGO_RUN] */
		mt9t111_write(0x098E, 0x6811);
		mt9t111_write(0x0990, 0x0003);	/* MCU_DATA_0 */

	} else if (effect == CamAntiBanding50Hz) {

		mt9t111_write(0x098E, 0xA005);	/* MCU_ADDRESS [FD_FDPERIOD_SELECT] */
		mt9t111_write(0x0990, 0x0001);	/* MCU_DATA_0 =>'0'=60Hz, '1'=50Hz */
		mt9t111_write(0x098E, 0x6C11);	/* MCU_ADDRESS */
		mt9t111_write(0x0990, 0x0002);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_FD_ALGO_RUN] */
		mt9t111_write(0x098E, 0x6811);
		mt9t111_write(0x0990, 0x0002);	/* MCU_DATA_0 */

	} else if (effect == CamAntiBanding60Hz) {

		mt9t111_write(0x098E, 0xA005);	/* MCU_ADDRESS [FD_FDPERIOD_SELECT] */
		mt9t111_write(0x0990, 0x0000);	/* MCU_DATA_0 =>'0'=60Hz, '1'=50Hz */
		mt9t111_write(0x098E, 0x6C11);	/* MCU_ADDRESS */
		mt9t111_write(0x0990, 0x0002);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_FD_ALGO_RUN] */
		mt9t111_write(0x098E, 0x6811);
		mt9t111_write(0x0990, 0x0002);	/* MCU_DATA_0 */

	} else {
		mt9t111_write(0x098E, 0xA005);	/* MCU_ADDRESS [FD_FDPERIOD_SELECT] */
		mt9t111_write(0x0990, 0x0001);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_B_CONFIG_FD_ALGO_RUN] */
		mt9t111_write(0x098E, 0x6C11);
		mt9t111_write(0x0990, 0x0003);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_FD_ALGO_RUN] */
		mt9t111_write(0x098E, 0x6811);
		mt9t111_write(0x0990, 0x0003);	/* MCU_DATA_0 */
	}

	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetAntiBanding(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}

	return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetFlashMode(
					FlashLedState_t effect)
/
/ Description: This function will set the flash mode of camera
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetFlashMode_Pri(FlashLedState_t effect,
					CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_info("CAMDRV_SetFlashMode()  called\n");
	
	flash_switch=effect;
	if (effect == Flash_Off) {
		/* do sth */
	} else if (effect == Flash_On) {
		/* do sth */
	} else if (effect == Torch_On) {
		/* do sth */
	} else if (effect == FlashLight_Auto) {
		/* do sth */
	} else {
		/* do sth */
	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetFlashMode(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}
	return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetFocusMode(
/					CamFocusStatus_t effect)
/
/ Description: This function will set the focus mode of camera
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetFocusMode_Pri(CamFocusControlMode_t effect,
					CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_info("CAMDRV_SetFocusMode()  called effect 0x%x \n",effect);
	if (effect == CamFocusControlAuto) {

		mt9t111_write( 0x098E, 0x3003); 	// MCU_ADDRESS [AF_ALGO]
		mt9t111_write( 0x0990, 0x0002); 	// MCU_DATA_0		
		
	} else if (effect == CamFocusControlMacro) {

		mt9t111_write( 0x098E, 0x3003 );	// MCU_ADDRESS [AF_ALGO]
		mt9t111_write( 0x0990, 0x0001 );	// MCU_DATA_0
		mt9t111_write( 0x098E, 0xB024 );	// MCU_ADDRESS [AF_BEST_POSITION]
		mt9t111_write( 0x0990, 0x00AF );	// MCU_DATA_0

	} else if (effect == CamFocusControlInfinity) {

		mt9t111_write( 0x098E, 0x3003 );	// MCU_ADDRESS [AF_ALGO]
		mt9t111_write( 0x0990, 0x0001 );	// MCU_DATA_0
		mt9t111_write( 0x098E, 0xB024 );	// MCU_ADDRESS [AF_BEST_POSITION]
		mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0

	} else {

		printk(KERN_ERR" can not support effect focus 0x%x\r\n",effect);

	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetFocusMode(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}
	
	FOCUS_MODE=effect;
	
	return result;
}

static HAL_CAM_Result_en_t CAMDRV_TurnOffAF_Pri()
{
	
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	
	return result;
}

static HAL_CAM_Result_en_t CAMDRV_TurnOnAF_Pri()
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	
	switch (FOCUS_MODE)
	{
		case CamFocusControlAuto: /*AF trigger*/
			
			mt9t111_write( 0x098E, 0xB019 );	// MCU_ADDRESS [AF_PROGRESS]
			mt9t111_write( 0x0990, 0x0001 );	// MCU_DATA_0

			break;

		case CamFocusControlMacro:/*we need not doing trigger for this mode */
			
			msleep(100);
			break;

		case CamFocusControlInfinity:/*we need not doing trigger for this mode */
			
			msleep(100);
			break;

		default:
			printk(KERN_ERR"error in CAMDRV_TurnOnAF_Pri , can not support focus mode 0x%x \n",FOCUS_MODE);
				
	}
	

	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_TurnOnAF(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}
	return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetJpegQuality(
/					CamFocusStatus_t effect)
/
/ Description: This function will set the focus mode of camera
/ Notes:
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetJpegQuality_Pri(CamJpegQuality_t effect,
					  CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_info("CAMDRV_SetJpegQuality()  called\n");
	if (effect == CamJpegQuality_Min) {
		/* do sth */
	} else if (effect == CamJpegQuality_Nom) {
		/* do sth */
	} else {
		/* do sth */
	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetJpegQuality(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}
	return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetZoom()
/
/ Description: This function will set the zoom value of camera
/ Notes:
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetZoom_Pri(CamZoom_t step,
					  CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_info("CAMDRV_SetZoom()  called\n");
	if (step == CamZoom_1_0) {
		//TBD
	} else if (step == CamZoom_1_15) {
		//TBD
	} else if (step == CamZoom_1_6) {
		//TBD
	} else if (step == CamZoom_2_0) {
		//TBD
	} else {
		//TBD
	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetZoom(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}
	return result;
}



/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetDigitalEffect(
/					CamDigEffect_t effect)
/
/ Description: This function will set the digital effect of camera
/ Notes:
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetDigitalEffect_Pri(CamDigEffect_t effect,
					    CamSensorSelect_t sensor)
{//return HAL_CAM_SUCCESS;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	
	if (effect == CamDigEffect_NoEffect) {

		mt9t111_write(0x098E, 0xE883);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xEC83);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0x8400);
		mt9t111_write(0x0990, 0x0006);

	} else if (effect == CamDigEffect_MonoChrome) {

		mt9t111_write(0x098E, 0xE883);
		mt9t111_write(0x0990, 0x0001);
		mt9t111_write(0x098E, 0xEC83);
		mt9t111_write(0x0990, 0x0001);
		mt9t111_write(0x098E, 0x8400);
		mt9t111_write(0x0990, 0x0006);

	} else if ((effect == CamDigEffect_NegColor)
		   || (effect == CamDigEffect_NegMono)) {
		mt9t111_write(0x098E, 0xE883);
		mt9t111_write(0x0990, 0x0003);
		mt9t111_write(0x098E, 0xEC83);
		mt9t111_write(0x0990, 0x0003);
		mt9t111_write(0x098E, 0x8400);
		mt9t111_write(0x0990, 0x0006);

	} else if ((effect == CamDigEffect_SolarizeColor)
		   || (effect == CamDigEffect_SolarizeMono)) {
		mt9t111_write(0x098E, 0xE883);
		mt9t111_write(0x0990, 0x0004);
		mt9t111_write(0x098E, 0xEC83);
		mt9t111_write(0x0990, 0x0004);
		mt9t111_write(0x098E, 0x8400);
		mt9t111_write(0x0990, 0x0006);

	} else if (effect == CamDigEffect_SepiaGreen) {

		mt9t111_write(0x098E, 0xE883);
		mt9t111_write(0x0990, 0x0002);
		mt9t111_write(0x098E, 0xEC83);
		mt9t111_write(0x0990, 0x0002);
		mt9t111_write(0x098E, 0xE885);
		mt9t111_write(0x0990, 0x0037);
		mt9t111_write(0x098E, 0xEC85);
		mt9t111_write(0x0990, 0x0037);
		mt9t111_write(0x098E, 0xE886);
		mt9t111_write(0x0990, 0x00BE);
		mt9t111_write(0x098E, 0xEC86);
		mt9t111_write(0x0990, 0x00BE);
		mt9t111_write(0x098E, 0x8400);
		mt9t111_write(0x0990, 0x0006);

	} else if (effect == CamDigEffect_Auqa) {

		/* MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX] */
		mt9t111_write(0x098E, 0xE883);
		mt9t111_write(0x0990, 0x0002);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX] */
		mt9t111_write(0x098E, 0xEC83);
		mt9t111_write(0x0990, 0x0002);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CR] */
		mt9t111_write(0x098E, 0xE885);
		mt9t111_write(0x0990, 0x008C);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CR] */
		mt9t111_write(0x098E, 0xEC85);
		mt9t111_write(0x0990, 0x008C);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CB] */
		mt9t111_write(0x098E, 0xE886);
		mt9t111_write(0x0990, 0x0042);	/* MCU_DATA_0 */
		/* MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CB] */
		mt9t111_write(0x098E, 0xEC86);
		mt9t111_write(0x0990, 0x0042);	/* MCU_DATA_0 */
		mt9t111_write(0x098E, 0x8400);	/* MCU_ADDRESS [SEQ_CMD] */
		mt9t111_write(0x0990, 0x0006);	/* MCU_DATA_0 */

	} else {
		mt9t111_write(0x098E, 0xE883);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0xEC83);
		mt9t111_write(0x0990, 0x0000);
		mt9t111_write(0x098E, 0x8400);
		mt9t111_write(0x0990, 0x0006);

	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetDigitalEffect(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}

	return result;
}

static HAL_CAM_Result_en_t Init_Mt9t111(CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	static ktime_t tm1;
	tm1 = ktime_get();
	
	pr_info("Entry Init Sec %d nsec %d\n", tm1.tv.sec, tm1.tv.nsec);
	
	CamSensorCfg_st.sensor_config_caps = &CamPrimaryCfgCap_st;
	printk("Init Primary Sensor MT9T111: \r\n");
	
	/*  
	   * Preview YUV 640x480  Frame rate 10~ 27.5fps
          * Capture YUV 2048x1536	Frame Rate 8.45
          * XMCLK 26MHz	PCLK 64MHz
       */
       
	mt9t111_write( 0x001A, 0x0019);   // RESET_AND_MISC_CONTROL
	msleep(1);											  //DELAY=  1                   
	mt9t111_write( 0x001A, 0x0018);   // RESET_AND_MISC_CONTROL
	                             
	                             
	mt9t111_write( 0x001A, 0x0219); 	// RESET_AND_MISC_CONTROL
	mt9t111_write( 0x001A, 0x0018); 	// RESET_AND_MISC_CONTROL
	mt9t111_write( 0x0014, 0x2425); 	// PLL_CONTROL
	mt9t111_write( 0x0014, 0x2425); 	// PLL_CONTROL
	                            
	mt9t111_write( 0x0014, 0x2145);	//PLL control: BYPASS PLL = 8517
	mt9t111_write( 0x0010, 0x0C80);	//PLL Dividers = 3200
	mt9t111_write( 0x0012, 0x0070);	//PLL P Dividers = 112
	mt9t111_write( 0x002A, 0x77AA);	//PLL P Dividers 4-5-6 = 30634
	mt9t111_write( 0x001A, 0x0218); //Reset Misc. Control = 536
	mt9t111_write( 0x0014, 0x2545);	//PLL control: TEST_BYPASS on = 9541
	mt9t111_write( 0x0014, 0x2547);	//PLL control: PLL_ENABLE on = 9543
	mt9t111_write( 0x0014, 0x2447);	//PLL control: SEL_LOCK_DET on = 9287
	mt9t111_write( 0x0014, 0x2047);	//PLL control: TEST_BYPASS off = 8263

	//  POLL  PLL_CONTROL::PLL_LOCK =>  0x01

	msleep(100)	;										//Delay = 100


	mt9t111_write( 0x0014, 0x2046); 	// PLL_CONTROL
	mt9t111_write( 0x0022, 0x0208); 	// VDD_DIS_COUNTER
	mt9t111_write( 0x001E, 0x0777); 	// PAD_SLEW_PAD_CONFIG
	mt9t111_write( 0x0016, 0x0400); 	// CLOCKS_CONTROL
	mt9t111_write( 0x001E, 0x0777); 	// PAD_SLEW_PAD_CONFIG
	mt9t111_write( 0x0018, 0x402D); 	// STANDBY_CONTROL_AND_STATUS
	mt9t111_write( 0x0018, 0x402C); 	// STANDBY_CONTROL_AND_STATUS
	//  POLL  STANDBY_CONTROL_AND_STATUS::STANDBY_DONE =>  0x01, ..., 0x00 (5 reads)
	msleep(200);											//delay = 200

	//mt9t111_write( 0x098E, 0x6006 );	// MCU_ADDRESS
	//mt9t111_write( 0x0990, 0x00E9 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6800 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0280 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6802 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x01E0 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE88E );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x68A0 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x082D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4802 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4804 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4806 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x060D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4808 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x080D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x480A );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0111 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x480C );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x046C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x480F );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x00CC );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4811 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0381 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4813 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x024F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x481D );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x035D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x481F );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x05D0 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4825 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x07AC );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x482B );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0408 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x482D );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0308 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6C00 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0800 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6C02 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0600 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xEC8E );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6CA0 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x082D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x484A );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0004 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x484C );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0004 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x484E );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x060B );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4850 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x080B );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4852 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0111 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4854 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0024 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4857 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x008C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4859 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x01F1 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x485B );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x00FF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4865 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x069F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4867 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0378 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x486D );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0CB1 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4873 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0808 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4875 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0608 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC8A5 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0026 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC8A6 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0028 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC8A7 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x002E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC8A8 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0030 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC844 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x00ED );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC92F );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC845 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x00C5 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC92D );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC88C );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x008F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC930 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC88D );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0077 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC92E );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB825 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0005 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xA009 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xA00A );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0003 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xA00C );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x000A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4846 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0080 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x68AA );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x026A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6815 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x000A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6817 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x000C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x682D );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x000A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x488E );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0080 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6CAA );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0130 );	// MCU_DATA_0

	mt9t111_write( 0x3C20, 0x0000 );	// TX_SS_CONTROL
	                                  
	// LSC Setting
	                 
	//mt9t111_write( 0x3210, 0x01B0 );	// COLOR_PIPELINE_CONTROL
	//mt9t111_write( 0x3640, 0x0550 );	// P_G1_P0Q0
	//mt9t111_write( 0x3642, 0xAB8B );	// P_G1_P0Q1
	//mt9t111_write( 0x3644, 0x1B51 );	// P_G1_P0Q2
	//mt9t111_write( 0x3646, 0xE10D );	// P_G1_P0Q3
	//mt9t111_write( 0x3648, 0x2D2C );	// P_G1_P0Q4
	//mt9t111_write( 0x364A, 0x01F0 );	// P_R_P0Q0
	//mt9t111_write( 0x364C, 0x9A8B );	// P_R_P0Q1
	//mt9t111_write( 0x364E, 0x5BB1 );	// P_R_P0Q2
	//mt9t111_write( 0x3650, 0x8BEF );	// P_R_P0Q3
	//mt9t111_write( 0x3652, 0x91EA );	// P_R_P0Q4
	//mt9t111_write( 0x3654, 0x0210 );	// P_B_P0Q0
	//mt9t111_write( 0x3656, 0xDAAA );	// P_B_P0Q1
	//mt9t111_write( 0x3658, 0x18B1 );	// P_B_P0Q2
	//mt9t111_write( 0x365A, 0xB60E );	// P_B_P0Q3
	//mt9t111_write( 0x365C, 0xA3EF );	// P_B_P0Q4
	//mt9t111_write( 0x365E, 0x0270 );	// P_G2_P0Q0
	//mt9t111_write( 0x3660, 0xC30B );	// P_G2_P0Q1
	//mt9t111_write( 0x3662, 0x1471 );	// P_G2_P0Q2
	//mt9t111_write( 0x3664, 0xE96D );	// P_G2_P0Q3
	//mt9t111_write( 0x3666, 0x0C8C );	// P_G2_P0Q4
	//mt9t111_write( 0x3680, 0xC629 );	// P_G1_P1Q0
	//mt9t111_write( 0x3682, 0x8F0F );	// P_G1_P1Q1
	//mt9t111_write( 0x3684, 0x6DAF );	// P_G1_P1Q2
	//mt9t111_write( 0x3686, 0x760F );	// P_G1_P1Q3
	//mt9t111_write( 0x3688, 0x9EB1 );	// P_G1_P1Q4
	//mt9t111_write( 0x368A, 0xC86A );	// P_R_P1Q0
	//mt9t111_write( 0x368C, 0x46CE );	// P_R_P1Q1
	//mt9t111_write( 0x368E, 0x1B50 );	// P_R_P1Q2
	//mt9t111_write( 0x3690, 0xD46E );	// P_R_P1Q3
	//mt9t111_write( 0x3692, 0x8A52 );	// P_R_P1Q4
	//mt9t111_write( 0x3694, 0x936B );	// P_B_P1Q0
	//mt9t111_write( 0x3696, 0xB7CE );	// P_B_P1Q1
	//mt9t111_write( 0x3698, 0x770E );	// P_B_P1Q2
	//mt9t111_write( 0x369A, 0x726F );	// P_B_P1Q3
	//mt9t111_write( 0x369C, 0xBF90 );	// P_B_P1Q4
	//mt9t111_write( 0x369E, 0x7F4A );	// P_G2_P1Q0
	//mt9t111_write( 0x36A0, 0x65AE );	// P_G2_P1Q1
	//mt9t111_write( 0x36A2, 0x1B4F );	// P_G2_P1Q2
	//mt9t111_write( 0x36A4, 0xC72E );	// P_G2_P1Q3
	//mt9t111_write( 0x36A6, 0x80F1 );	// P_G2_P1Q4
	//mt9t111_write( 0x36C0, 0x19B2 );	// P_G1_P2Q0
	//mt9t111_write( 0x36C2, 0xBAEF );	// P_G1_P2Q1
	//mt9t111_write( 0x36C4, 0x8E73 );	// P_G1_P2Q2
	//mt9t111_write( 0x36C6, 0xA56E );	// P_G1_P2Q3
	//mt9t111_write( 0x36C8, 0x1095 );	// P_G1_P2Q4
	//mt9t111_write( 0x36CA, 0x3772 );	// P_R_P2Q0
	//mt9t111_write( 0x36CC, 0x928E );	// P_R_P2Q1
	//mt9t111_write( 0x36CE, 0xA7F2 );	// P_R_P2Q2
	//mt9t111_write( 0x36D0, 0xA250 );	// P_R_P2Q3
	//mt9t111_write( 0x36D2, 0x4972 );	// P_R_P2Q4
	//mt9t111_write( 0x36D4, 0x21D2 );	// P_B_P2Q0
	//mt9t111_write( 0x36D6, 0xCC6E );	// P_B_P2Q1
	//mt9t111_write( 0x36D8, 0xA8F3 );	// P_B_P2Q2
	//mt9t111_write( 0x36DA, 0xC170 );	// P_B_P2Q3
	//mt9t111_write( 0x36DC, 0x1ED5 );	// P_B_P2Q4
	//mt9t111_write( 0x36DE, 0x24B2 );	// P_G2_P2Q0
	//mt9t111_write( 0x36E0, 0xD28E );	// P_G2_P2Q1
	//mt9t111_write( 0x36E2, 0x98B3 );	// P_G2_P2Q2
	//mt9t111_write( 0x36E4, 0x9010 );	// P_G2_P2Q3
	//mt9t111_write( 0x36E6, 0x1955 );	// P_G2_P2Q4
	//mt9t111_write( 0x3700, 0x3590 );	// P_G1_P3Q0
	//mt9t111_write( 0x3702, 0xC690 );	// P_G1_P3Q1
	//mt9t111_write( 0x3704, 0xAB8F );	// P_G1_P3Q2
	//mt9t111_write( 0x3706, 0x6751 );	// P_G1_P3Q3
	//mt9t111_write( 0x3708, 0x7213 );	// P_G1_P3Q4
	//mt9t111_write( 0x370A, 0x59D0 );	// P_R_P3Q0
	//mt9t111_write( 0x370C, 0x3510 );	// P_R_P3Q1
	//mt9t111_write( 0x370E, 0x9F72 );	// P_R_P3Q2
	//mt9t111_write( 0x3710, 0xF2D2 );	// P_R_P3Q3
	//mt9t111_write( 0x3712, 0x7414 );	// P_R_P3Q4
	//mt9t111_write( 0x3714, 0x2BD0 );	// P_B_P3Q0
	//mt9t111_write( 0x3716, 0x8DAF );	// P_B_P3Q1
	//mt9t111_write( 0x3718, 0xFE2E );	// P_B_P3Q2
	//mt9t111_write( 0x371A, 0x87F1 );	// P_B_P3Q3
	//mt9t111_write( 0x371C, 0x1D53 );	// P_B_P3Q4
	//mt9t111_write( 0x371E, 0x7570 );	// P_G2_P3Q0
	//mt9t111_write( 0x3720, 0x2BB0 );	// P_G2_P3Q1
	//mt9t111_write( 0x3722, 0x9C32 );	// P_G2_P3Q2
	//mt9t111_write( 0x3724, 0x9713 );	// P_G2_P3Q3
	//mt9t111_write( 0x3726, 0x3494 );	// P_G2_P3Q4
	//mt9t111_write( 0x3740, 0xDA30 );	// P_G1_P4Q0
	//mt9t111_write( 0x3742, 0x7EEC );	// P_G1_P4Q1
	//mt9t111_write( 0x3744, 0x2335 );	// P_G1_P4Q2
	//mt9t111_write( 0x3746, 0xF3D4 );	// P_G1_P4Q3
	//mt9t111_write( 0x3748, 0x4BF5 );	// P_G1_P4Q4
	//mt9t111_write( 0x374A, 0x8051 );	// P_R_P4Q0
	//mt9t111_write( 0x374C, 0xB56E );	// P_R_P4Q1
	//mt9t111_write( 0x374E, 0x0C51 );	// P_R_P4Q2
	//mt9t111_write( 0x3750, 0x8F95 );	// P_R_P4Q3
	//mt9t111_write( 0x3752, 0x0FB8 );	// P_R_P4Q4
	//mt9t111_write( 0x3754, 0x92F2 );	// P_B_P4Q0
	//mt9t111_write( 0x3756, 0x9A2E );	// P_B_P4Q1
	//mt9t111_write( 0x3758, 0x4B95 );	// P_B_P4Q2
	//mt9t111_write( 0x375A, 0xC414 );	// P_B_P4Q3
	//mt9t111_write( 0x375C, 0x6FB3 );	// P_B_P4Q4
	//mt9t111_write( 0x375E, 0xA451 );	// P_G2_P4Q0
	//mt9t111_write( 0x3760, 0x7C4F );	// P_G2_P4Q1
	//mt9t111_write( 0x3762, 0x1775 );	// P_G2_P4Q2
	//mt9t111_write( 0x3764, 0x80F5 );	// P_G2_P4Q3
	//mt9t111_write( 0x3766, 0x64D5 );	// P_G2_P4Q4
	//mt9t111_write( 0x3782, 0x02F0 );	// CENTER_ROW
	//mt9t111_write( 0x3784, 0x0420 );	// CENTER_COLUMN
	//mt9t111_write( 0x3210, 0x01B8 );	// COLOR_PIPELINE_CONTROL

	// Brightness Metric
	mt9t111_write( 0x098E, 0xC913 );	// MCU_ADDRESS [CAM1_STAT_BRIGHTNESS_METRIC_PREDIVIDER]
	mt9t111_write( 0x0990, 0x000A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x686B );	// MCU_ADDRESS [PRI_A_CONFIG_LL_START_BRIGHTNESS]
	mt9t111_write( 0x0990, 0x05DC );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x686D );	// MCU_ADDRESS [PRI_A_CONFIG_LL_STOP_BRIGHTNESS]
	mt9t111_write( 0x0990, 0x0BB8 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6C6B );	// MCU_ADDRESS [PRI_B_CONFIG_LL_START_BRIGHTNESS]
	mt9t111_write( 0x0990, 0x05DC );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6C6D );	// MCU_ADDRESS [PRI_B_CONFIG_LL_STOP_BRIGHTNESS]
	mt9t111_write( 0x0990, 0x0BB8 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x3439 );	// MCU_ADDRESS [AS_ASSTART_BRIGHTNESS]
	mt9t111_write( 0x0990, 0x05DC );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x343B );	// MCU_ADDRESS [AS_ASSTOP_BRIGHTNESS]
	mt9t111_write( 0x0990, 0x0BB8 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4926 );	// MCU_ADDRESS [CAM1_LL_START_GAMMA_BM]
	mt9t111_write( 0x0990, 0x0001 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4928 );	// MCU_ADDRESS [CAM1_LL_MID_GAMMA_BM]
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x492A );	// MCU_ADDRESS [CAM1_LL_STOP_GAMMA_BM]
	mt9t111_write( 0x0990, 0x0656 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4D26 );	// MCU_ADDRESS [CAM2_LL_START_GAMMA_BM]
	mt9t111_write( 0x0990, 0x0001 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4D28 );	// MCU_ADDRESS [CAM2_LL_MID_GAMMA_BM]
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4D2A );	// MCU_ADDRESS [CAM2_LL_STOP_GAMMA_BM]
	mt9t111_write( 0x0990, 0x0656 );	// MCU_DATA_0

	// FW kernel
	mt9t111_write( 0x33F4, 0x040B );	// KERNEL_CONFIG
	mt9t111_write( 0x098E, 0xC916 );	// MCU_ADDRESS [CAM1_LL_LL_START_0]
	mt9t111_write( 0x0990, 0x0014 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC919 );	// MCU_ADDRESS [CAM1_LL_LL_STOP_0]
	mt9t111_write( 0x0990, 0x0028 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC917 );	// MCU_ADDRESS [CAM1_LL_LL_START_1]
	mt9t111_write( 0x0990, 0x0004 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC918 );	// MCU_ADDRESS [CAM1_LL_LL_START_2]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC91A );	// MCU_ADDRESS [CAM1_LL_LL_STOP_1]
	mt9t111_write( 0x0990, 0x0001 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC91B );	// MCU_ADDRESS [CAM1_LL_LL_STOP_2]
	mt9t111_write( 0x0990, 0x0009 );	// MCU_DATA_0
	mt9t111_write( 0x326C, 0x0C00 );	// APERTURE_PARAMETERS_2D
	mt9t111_write( 0x098E, 0x494B );	// MCU_ADDRESS [CAM1_LL_EXT_START_GAIN_METRIC]
	mt9t111_write( 0x0990, 0x0042 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x494D );	// MCU_ADDRESS [CAM1_LL_EXT_STOP_GAIN_METRIC]
	mt9t111_write( 0x0990, 0x012C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC91E );	// MCU_ADDRESS [CAM1_LL_NR_START_0]
	mt9t111_write( 0x0990, 0x0012 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC91F );	// MCU_ADDRESS [CAM1_LL_NR_START_1]
	mt9t111_write( 0x0990, 0x000A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC920 );	// MCU_ADDRESS [CAM1_LL_NR_START_2]
	mt9t111_write( 0x0990, 0x0012 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC921 );	// MCU_ADDRESS [CAM1_LL_NR_START_3]
	mt9t111_write( 0x0990, 0x000A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC922 );	// MCU_ADDRESS [CAM1_LL_NR_STOP_0]
	mt9t111_write( 0x0990, 0x0026 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC923 );	// MCU_ADDRESS [CAM1_LL_NR_STOP_1]
	mt9t111_write( 0x0990, 0x001E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC924 );	// MCU_ADDRESS [CAM1_LL_NR_STOP_2]
	mt9t111_write( 0x0990, 0x0026 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC925 );	// MCU_ADDRESS [CAM1_LL_NR_STOP_3]
	mt9t111_write( 0x0990, 0x0026 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC02 );	// MCU_ADDRESS [LL_MODE]
	mt9t111_write( 0x0990, 0x0003 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC05 );	// MCU_ADDRESS [LL_CLUSTER_DC_TH]
	mt9t111_write( 0x0990, 0x000E );	// MCU_DATA_0
	mt9t111_write( 0x316C, 0x350F );	// DAC_TXLO
	mt9t111_write( 0x098E, 0xC950 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_1]
	mt9t111_write( 0x0990, 0x0064 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC94F );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_0]
	mt9t111_write( 0x0990, 0x0038 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC952 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_3]
	mt9t111_write( 0x0990, 0x0064 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC951 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_2]
	mt9t111_write( 0x0990, 0x0051 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC954 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_5]
	mt9t111_write( 0x0990, 0x0010 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC953 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_4]
	mt9t111_write( 0x0990, 0x0020 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC956 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_7]
	mt9t111_write( 0x0990, 0x0010 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC955 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_START_6]
	mt9t111_write( 0x0990, 0x0020 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC958 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_1]
	mt9t111_write( 0x0990, 0x0020 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC957 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_0]
	mt9t111_write( 0x0990, 0x0014 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC95A );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_3]
	mt9t111_write( 0x0990, 0x001D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC959 );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_2]
	mt9t111_write( 0x0990, 0x0020 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC95C );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_5]
	mt9t111_write( 0x0990, 0x000C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC95B );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_4]
	mt9t111_write( 0x0990, 0x0008 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC95E );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_7]
	mt9t111_write( 0x0990, 0x000C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC95D );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_THRESHOLDS_STOP_6]
	mt9t111_write( 0x0990, 0x0008 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC95F );	// MCU_ADDRESS [CAM1_LL_EXT_GRB_WINDOW_PERCENT]
	mt9t111_write( 0x0990, 0x0064 );	// MCU_DATA_0

	// Dark Delta CCM settings
	mt9t111_write( 0x098E, 0x48DC );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_0]
	mt9t111_write( 0x0990, 0x004D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48DE );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_1]
	mt9t111_write( 0x0990, 0x0096 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48E0 );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_2]
	mt9t111_write( 0x0990, 0x001D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48E2 );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_3]
	mt9t111_write( 0x0990, 0x004D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48E4 );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_4]
	mt9t111_write( 0x0990, 0x0096 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48E6 );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_5]
	mt9t111_write( 0x0990, 0x001D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48E8 );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_6]
	mt9t111_write( 0x0990, 0x004D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48EA );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_7]
	mt9t111_write( 0x0990, 0x0096 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48EC );	// MCU_ADDRESS [CAM1_AWB_LL_CCM_8]
	mt9t111_write( 0x0990, 0x001D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xDC2A );	// MCU_ADDRESS [SYS_DELTA_GAIN]
	mt9t111_write( 0x0990, 0x000B );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xDC2B );	// MCU_ADDRESS [SYS_DELTA_THRESH]
	mt9t111_write( 0x0990, 0x0017 );	// MCU_DATA_0

	// Gamma Correction sRGB
	mt9t111_write( 0x098E, 0xBC0B );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_0]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC0C );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_1]
	mt9t111_write( 0x0990, 0x001B );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC0D );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_2]
	mt9t111_write( 0x0990, 0x002A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC0E );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_3]
	mt9t111_write( 0x0990, 0x003E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC0F );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_4]
	mt9t111_write( 0x0990, 0x005A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC10 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_5]
	mt9t111_write( 0x0990, 0x0070 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC11 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_6]
	mt9t111_write( 0x0990, 0x0081 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC12 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_7]
	mt9t111_write( 0x0990, 0x0090 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC13 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_8]
	mt9t111_write( 0x0990, 0x009E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC14 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_9]
	mt9t111_write( 0x0990, 0x00AB );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC15 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_10]
	mt9t111_write( 0x0990, 0x00B6 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC16 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_11]
	mt9t111_write( 0x0990, 0x00C1 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC17 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_12]
	mt9t111_write( 0x0990, 0x00CB );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC18 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_13]
	mt9t111_write( 0x0990, 0x00D5 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC19 );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_14]
	mt9t111_write( 0x0990, 0x00DE );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC1A );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_15]
	mt9t111_write( 0x0990, 0x00E7 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC1B );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_16]
	mt9t111_write( 0x0990, 0x00EF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC1C );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_17]
	mt9t111_write( 0x0990, 0x00F7 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC1D );	// MCU_ADDRESS [LL_GAMMA_CONTRAST_CURVE_18]
	mt9t111_write( 0x0990, 0x00FF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC1E );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_0]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC1F );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_1]
	mt9t111_write( 0x0990, 0x001B );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC20 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_2]
	mt9t111_write( 0x0990, 0x002A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC21 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_3]
	mt9t111_write( 0x0990, 0x003E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC22 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_4]
	mt9t111_write( 0x0990, 0x005A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC23 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_5]
	mt9t111_write( 0x0990, 0x0070 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC24 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_6]
	mt9t111_write( 0x0990, 0x0081 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC25 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_7]
	mt9t111_write( 0x0990, 0x0090 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC26 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_8]
	mt9t111_write( 0x0990, 0x009E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC27 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_9]
	mt9t111_write( 0x0990, 0x00AB );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC28 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_10]
	mt9t111_write( 0x0990, 0x00B6 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC29 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_11]
	mt9t111_write( 0x0990, 0x00C1 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC2A );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_12]
	mt9t111_write( 0x0990, 0x00CB );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC2B );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_13]
	mt9t111_write( 0x0990, 0x00D5 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC2C );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_14]
	mt9t111_write( 0x0990, 0x00DE );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC2D );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_15]
	mt9t111_write( 0x0990, 0x00E7 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC2E );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_16]
	mt9t111_write( 0x0990, 0x00EF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC2F );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_17]
	mt9t111_write( 0x0990, 0x00F7 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC30 );	// MCU_ADDRESS [LL_GAMMA_NEUTRAL_CURVE_18]
	mt9t111_write( 0x0990, 0x00FF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC31 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_0]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC32 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_1]
	mt9t111_write( 0x0990, 0x000D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC33 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_2]
	mt9t111_write( 0x0990, 0x0019 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC34 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_3]
	mt9t111_write( 0x0990, 0x0030 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC35 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_4]
	mt9t111_write( 0x0990, 0x0056 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC36 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_5]
	mt9t111_write( 0x0990, 0x0070 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC37 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_6]
	mt9t111_write( 0x0990, 0x0081 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC38 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_7]
	mt9t111_write( 0x0990, 0x0090 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC39 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_8]
	mt9t111_write( 0x0990, 0x009E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC3A );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_9]
	mt9t111_write( 0x0990, 0x00AB );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC3B );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_10]
	mt9t111_write( 0x0990, 0x00B6 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC3C );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_11]
	mt9t111_write( 0x0990, 0x00C1 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC3D );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_12]
	mt9t111_write( 0x0990, 0x00CB );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC3E );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_13]
	mt9t111_write( 0x0990, 0x00D5 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC3F );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_14]
	mt9t111_write( 0x0990, 0x00DE );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC40 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_15]
	mt9t111_write( 0x0990, 0x00E7 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC41 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_16]
	mt9t111_write( 0x0990, 0x00EF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC42 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_17]
	mt9t111_write( 0x0990, 0x00F7 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC43 );	// MCU_ADDRESS [LL_GAMMA_NRCURVE_18]
	mt9t111_write( 0x0990, 0x00FF );	// MCU_DATA_0


	// TC Initialize
	mt9t111_write( 0x098E, 0x6865 );	// MCU_ADDRESS [PRI_A_CONFIG_LL_ALGO_ENTER]
	mt9t111_write( 0x0990, 0x00E0 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6867 );	// MCU_ADDRESS [PRI_A_CONFIG_LL_ALGO_RUN]
	mt9t111_write( 0x0990, 0x00F4 );	// MCU_DATA_0

	//mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS [SEQ_CMD]
	//mt9t111_write( 0x0990, 0x0006 );	// MCU_DATA_0

	mt9t111_write( 0x098E, 0xBC4A );	// MCU_ADDRESS [LL_TONAL_CURVE_HIGH]
	mt9t111_write( 0x0990, 0x007F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC4B );	// MCU_ADDRESS [LL_TONAL_CURVE_MED]
	mt9t111_write( 0x0990, 0x007F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xBC4C );	// MCU_ADDRESS [LL_TONAL_CURVE_LOW]
	mt9t111_write( 0x0990, 0x007F );	// MCU_DATA_0

	// New TC Curve
	mt9t111_write( 0x3542, 0x0010 );	// TONAL_X0
	mt9t111_write( 0x3544, 0x0030 );	// TONAL_X1
	mt9t111_write( 0x3546, 0x0040 );	// TONAL_X2
	mt9t111_write( 0x3548, 0x0080 );	// TONAL_X3
	mt9t111_write( 0x354A, 0x0100 );	// TONAL_X4
	mt9t111_write( 0x354C, 0x0200 );	// TONAL_X5
	mt9t111_write( 0x354E, 0x0300 );	// TONAL_X6
	mt9t111_write( 0x3550, 0x0010 );	// TONAL_Y0
	mt9t111_write( 0x3552, 0x0030 );	// TONAL_Y1
	mt9t111_write( 0x3554, 0x0040 );	// TONAL_Y2
	mt9t111_write( 0x3556, 0x0080 );	// TONAL_Y3
	mt9t111_write( 0x3558, 0x012C );	// TONAL_Y4
	mt9t111_write( 0x355A, 0x0320 );	// TONAL_Y5
	mt9t111_write( 0x355C, 0x03E8 );	// TONAL_Y6
	mt9t111_write( 0x3560, 0x0040 );	// RECIPROCAL_OF_X0_MINUS_ZERO
	mt9t111_write( 0x3562, 0x0020 );	// RECIPROCAL_OF_X1_MINUS_X0
	mt9t111_write( 0x3564, 0x0040 );	// RECIPROCAL_OF_X2_MINUS_X1
	mt9t111_write( 0x3566, 0x0010 );	// RECIPROCAL_OF_X3_MINUS_X2
	mt9t111_write( 0x3568, 0x0008 );	// RECIPROCAL_OF_X4_MINUS_X3
	mt9t111_write( 0x356A, 0x0004 );	// RECIPROCAL_OF_X5_MINUS_X4
	mt9t111_write( 0x356C, 0x0004 );	// RECIPROCAL_OF_X6_MINUS_X5
	mt9t111_write( 0x356E, 0x0004 );	// RECIPROCAL_OF_400_MINUS_X6

	// Fade to black
	mt9t111_write( 0x098E, 0x3C4D );	// MCU_ADDRESS [LL_START_GAMMA_FTB]
	mt9t111_write( 0x0990, 0x0DAC );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x3C4F );	// MCU_ADDRESS [LL_STOP_GAMMA_FTB]
	mt9t111_write( 0x0990, 0x148A );	// MCU_DATA_0

	// AWB Start
	mt9t111_write( 0x098E, 0xC911 );	// MCU_ADDRESS [CAM1_STAT_LUMA_THRESH_HIGH]
	mt9t111_write( 0x0990, 0x00C8 );	// MCU_DATA_0


	//SOC3130  AWB_CCM
	mt9t111_write( 0x098E, 0xC8F4 );	// MCU_ADDRESS [CAM1_AWB_AWB_XSCALE]
	mt9t111_write( 0x0990, 0x0004 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC8F5 );	// MCU_ADDRESS [CAM1_AWB_AWB_YSCALE]
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48F6 );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_0]
	mt9t111_write( 0x0990, 0x3B4D );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48F8 );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_1]
	mt9t111_write( 0x0990, 0x6380 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48FA );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_2]
	mt9t111_write( 0x0990, 0x9B18 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48FC );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_3]
	mt9t111_write( 0x0990, 0x5D51 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48FE );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_4]
	mt9t111_write( 0x0990, 0xEDE8 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4900 );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_5]
	mt9t111_write( 0x0990, 0xE515 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4902 );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_6]
	mt9t111_write( 0x0990, 0xBFF4 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4904 );	// MCU_ADDRESS [CAM1_AWB_AWB_WEIGHTS_7]
	mt9t111_write( 0x0990, 0x001E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4906 );	// MCU_ADDRESS [CAM1_AWB_AWB_XSHIFT_PRE_ADJ]
	mt9t111_write( 0x0990, 0x0026 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4908 );	// MCU_ADDRESS [CAM1_AWB_AWB_YSHIFT_PRE_ADJ]
	mt9t111_write( 0x0990, 0x0033 );	// MCU_DATA_0


	mt9t111_write( 0x098E, 0xE84A );	// MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_L]
	mt9t111_write( 0x0990, 0x0083 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE84D );	// MCU_ADDRESS [PRI_A_CONFIG_AWB_K_R_R]
	mt9t111_write( 0x0990, 0x0083 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE84C );	// MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_L]
	mt9t111_write( 0x0990, 0x0080 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE84F );	// MCU_ADDRESS [PRI_A_CONFIG_AWB_K_B_R]
	mt9t111_write( 0x0990, 0x0080 );	// MCU_DATA_0

	//mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS [SEQ_CMD]
	//mt9t111_write( 0x0990, 0x0006 );	// MCU_DATA_0



	mt9t111_write( 0x098E, 0x48B0 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_0]
	mt9t111_write( 0x0990, 0x0180 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48B2 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_1]
	mt9t111_write( 0x0990, 0xFF7A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48B4 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_2]
	mt9t111_write( 0x0990, 0x0018 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48B6 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_3]
	mt9t111_write( 0x0990, 0xFFCA );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48B8 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_4]
	mt9t111_write( 0x0990, 0x017C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48BA );	// MCU_ADDRESS [CAM1_AWB_CCM_L_5]
	mt9t111_write( 0x0990, 0xFFCC );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48BC );	// MCU_ADDRESS [CAM1_AWB_CCM_L_6]
	mt9t111_write( 0x0990, 0x000C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48BE );	// MCU_ADDRESS [CAM1_AWB_CCM_L_7]
	mt9t111_write( 0x0990, 0xFF1F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48C0 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_8]
	mt9t111_write( 0x0990, 0x01E8 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48C2 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_9]
	mt9t111_write( 0x0990, 0x0020 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48C4 );	// MCU_ADDRESS [CAM1_AWB_CCM_L_10]
	mt9t111_write( 0x0990, 0x0044 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48C6 );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_0]
	mt9t111_write( 0x0990, 0x0079 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48C8 );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_1]
	mt9t111_write( 0x0990, 0xFFAD );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48CA );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_2]
	mt9t111_write( 0x0990, 0xFFE2 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48CC );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_3]
	mt9t111_write( 0x0990, 0x0033 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48CE );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_4]
	mt9t111_write( 0x0990, 0x002A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48D0 );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_5]
	mt9t111_write( 0x0990, 0xFFAA );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48D2 );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_6]
	mt9t111_write( 0x0990, 0x0017 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48D4 );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_7]
	mt9t111_write( 0x0990, 0x004B );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48D6 );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_8]
	mt9t111_write( 0x0990, 0xFFA5 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48D8 );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_9]
	mt9t111_write( 0x0990, 0x0015 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x48DA );	// MCU_ADDRESS [CAM1_AWB_CCM_RL_10]
	mt9t111_write( 0x0990, 0xFFE2 );	// MCU_DATA_0
	mt9t111_write( 0x35A2, 0x0014 );	// DARK_COLOR_KILL_CONTROLS
	mt9t111_write( 0x098E, 0xC949 );	// MCU_ADDRESS [CAM1_SYS_DARK_COLOR_KILL]
	mt9t111_write( 0x0990, 0x0024 );	// MCU_DATA_0
	mt9t111_write( 0x35A4, 0x0596 );	// BRIGHT_COLOR_KILL_CONTROLS
	mt9t111_write( 0x098E, 0xC94A );	// MCU_ADDRESS [CAM1_SYS_BRIGHT_COLORKILL]
	mt9t111_write( 0x0990, 0x0062 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC948 );	// MCU_ADDRESS [CAM1_SYS_UV_COLOR_BOOST]
	mt9t111_write( 0x0990, 0x0006 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC914 );	// MCU_ADDRESS [CAM1_LL_START_DESATURATION]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC915 );	// MCU_ADDRESS [CAM1_LL_END_DESATURATION]
	mt9t111_write( 0x0990, 0x00FF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE86F );	// MCU_ADDRESS [PRI_A_CONFIG_LL_START_SATURATION]
	mt9t111_write( 0x0990, 0x0060 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE870 );	// MCU_ADDRESS [PRI_A_CONFIG_LL_END_SATURATION]
	mt9t111_write( 0x0990, 0x003C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xEC6F );	// MCU_ADDRESS [PRI_B_CONFIG_LL_START_SATURATION]
	mt9t111_write( 0x0990, 0x0060 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xEC70 );	// MCU_ADDRESS [PRI_B_CONFIG_LL_END_SATURATION]
	mt9t111_write( 0x0990, 0x003C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE883 );	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SELECT_FX]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xEC83 );	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SELECT_FX]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0

	//mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS [SEQ_CMD]
	//mt9t111_write( 0x0990, 0x0006 );	// MCU_DATA_0



	mt9t111_write( 0x098E, 0xE885 );	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CR]
	mt9t111_write( 0x0990, 0x001E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE886 );	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SEPIA_CB]
	mt9t111_write( 0x0990, 0x00D8 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xEC85 );	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CR]
	mt9t111_write( 0x0990, 0x001E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xEC86 );	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SEPIA_CB]
	mt9t111_write( 0x0990, 0x00D8 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xE884 );	// MCU_ADDRESS [PRI_A_CONFIG_SYSCTRL_SOLARIZATION_TH]
	mt9t111_write( 0x0990, 0x005C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xEC84 );	// MCU_ADDRESS [PRI_B_CONFIG_SYSCTRL_SOLARIZATION_TH]
	mt9t111_write( 0x0990, 0x005C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x490A );	// MCU_ADDRESS [CAM1_AS_INTEG_SCALE_FIRST_PASS]
	mt9t111_write( 0x0990, 0x0666 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x490C );	// MCU_ADDRESS [CAM1_AS_MIN_INT_TIME_FIRST_PASS]
	mt9t111_write( 0x0990, 0x0140 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6857 );	// MCU_ADDRESS [PRI_A_CONFIG_IS_FEATURE_THRESHOLD]
	mt9t111_write( 0x0990, 0x0014 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x685C );	// MCU_ADDRESS [PRI_A_CONFIG_IS_BLUR_INPUT_PARAMETER]
	mt9t111_write( 0x0990, 0x0005 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x490E );	// MCU_ADDRESS [CAM1_AS_MAX_DIGITAL_GAIN_ALLOWED]
	mt9t111_write( 0x0990, 0x00A4 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB43D );	// MCU_ADDRESS [AS_START_ASVALUES_0]
	mt9t111_write( 0x0990, 0x0031 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB43E );	// MCU_ADDRESS [AS_START_ASVALUES_1]
	mt9t111_write( 0x0990, 0x001B );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB43F );	// MCU_ADDRESS [AS_START_ASVALUES_2]
	mt9t111_write( 0x0990, 0x0028 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB440 );	// MCU_ADDRESS [AS_START_ASVALUES_3]
	mt9t111_write( 0x0990, 0x0003 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB441 );	// MCU_ADDRESS [AS_STOP_ASVALUES_0]
	mt9t111_write( 0x0990, 0x00CD );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB442 );	// MCU_ADDRESS [AS_STOP_ASVALUES_1]
	mt9t111_write( 0x0990, 0x0064 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB443 );	// MCU_ADDRESS [AS_STOP_ASVALUES_2]
	mt9t111_write( 0x0990, 0x000F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB444 );	// MCU_ADDRESS [AS_STOP_ASVALUES_3]
	mt9t111_write( 0x0990, 0x0007 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x300D );	// MCU_ADDRESS [AF_FILTERS]
	mt9t111_write( 0x0990, 0x000F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x3017 );	// MCU_ADDRESS [AF_THRESHOLDS]
	mt9t111_write( 0x0990, 0x0F0F );	// MCU_DATA_0

	//mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS [SEQ_CMD]
	//mt9t111_write( 0x0990, 0x0006 );	// MCU_DATA_0


	mt9t111_write( 0x098E, 0xE81F );	// MCU_ADDRESS [PRI_A_CONFIG_AE_RULE_BASE_TARGET]
	mt9t111_write( 0x0990, 0x0040 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x68A0 );	// MCU_ADDRESS [PRI_A_CONFIG_JPEG_OB_TX_CONTROL_VAR]
	mt9t111_write( 0x0990, 0x082E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x6CA0 );	// MCU_ADDRESS [PRI_B_CONFIG_JPEG_OB_TX_CONTROL_VAR]
	mt9t111_write( 0x0990, 0x082E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x70A0 );	// MCU_ADDRESS [SEC_A_CONFIG_JPEG_OB_TX_CONTROL_VAR]
	mt9t111_write( 0x0990, 0x082E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x74A0 );	// MCU_ADDRESS [SEC_B_CONFIG_JPEG_OB_TX_CONTROL_VAR]
	mt9t111_write( 0x0990, 0x082E );	// MCU_DATA_0
	mt9t111_write( 0x3C52, 0x082E );	// OB_TX_CONTROL
	mt9t111_write( 0x098E, 0x488E );	// MCU_ADDRESS [CAM1_CTX_B_RX_FIFO_TRIGGER_MARK]
	mt9t111_write( 0x0990, 0x0020 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xECAC );	// MCU_ADDRESS [PRI_B_CONFIG_IO_OB_MANUAL_FLAG]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0


	// For Preview YUV Order
	mt9t111_write( 0x098E, 0x6809 );	// MCU_ADDRESS [PRI_A_OUTPUT_FORMAT_ORDER]
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0

	// For Capture YUV Order
	mt9t111_write( 0x098E, 0x6C09 );	// MCU_ADDRESS [PRI_B_OUTPUT_FORMAT_ORDER] 
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0

	//AF
	mt9t111_write( 0x0604, 0x0F01 );	// GPIO_DATA
	mt9t111_write( 0x0606, 0x0F00 );	// GPIO_DIR
	
	msleep(1);
	
	mt9t111_write( 0x098E, 0x4403 );	// MCU_ADDRESS [AFM_ALGO]
	mt9t111_write( 0x0990, 0x8001 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x4411 );	// MCU_ADDRESS [AFM_TIMER_VMT]
	mt9t111_write( 0x0990, 0xF41C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xC421 );	// MCU_ADDRESS [AFM_SI_SLAVE_ADDR]
	mt9t111_write( 0x0990, 0x0018 );	// MCU_DATA_0
	
	
	mt9t111_write( 0x098E, 0xB002 );	// MCU_ADDRESS [AF_MODE]
	mt9t111_write( 0x0990, 0x0028 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x3003 );	// MCU_ADDRESS [AF_ALGO]
	mt9t111_write( 0x0990, 0x0002 );	// MCU_DATA_0	
	
	mt9t111_write( 0x098E, 0x300D );	// MCU_ADDRESS [AF_FILTERS]
	mt9t111_write( 0x0990, 0x000F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x3017 );	// MCU_ADDRESS [AF_THRESHOLDS]
	mt9t111_write( 0x0990, 0x0F0F );	// MCU_DATA_0
	
	mt9t111_write( 0x098E, 0xB01D );	// MCU_ADDRESS [AF_NUM_STEPS]
	mt9t111_write( 0x0990, 0x000C );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB01F );	// MCU_ADDRESS [AF_STEP_SIZE]
	mt9t111_write( 0x0990, 0x000F );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x440B );	// MCU_ADDRESS [AFM_POS_MIN]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x440D );	// MCU_ADDRESS [AFM_POS_MAX]
	mt9t111_write( 0x0990, 0x03C0 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB01C );	// MCU_ADDRESS [AF_INIT_POS]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB025 );	// MCU_ADDRESS [AF_POSITIONS_0]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB026 );	// MCU_ADDRESS [AF_POSITIONS_1]
	mt9t111_write( 0x0990, 0x003A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB027 );	// MCU_ADDRESS [AF_POSITIONS_2]
	mt9t111_write( 0x0990, 0x004A );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB028 );	// MCU_ADDRESS [AF_POSITIONS_3]
	mt9t111_write( 0x0990, 0x004E );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB029 );	// MCU_ADDRESS [AF_POSITIONS_4]
	mt9t111_write( 0x0990, 0x0052 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB02A );	// MCU_ADDRESS [AF_POSITIONS_5]
	mt9t111_write( 0x0990, 0x0063 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB02B );	// MCU_ADDRESS [AF_POSITIONS_6]
	mt9t111_write( 0x0990, 0x0072 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB02C );	// MCU_ADDRESS [AF_POSITIONS_7]
	mt9t111_write( 0x0990, 0x0080 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB02D );	// MCU_ADDRESS [AF_POSITIONS_8]
	mt9t111_write( 0x0990, 0x0090 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB02E );	// MCU_ADDRESS [AF_POSITIONS_9]
	mt9t111_write( 0x0990, 0x0098 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB02F );	// MCU_ADDRESS [AF_POSITIONS_10]
	mt9t111_write( 0x0990, 0x00AF );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0xB030 );	// MCU_ADDRESS [AF_POSITIONS_11]
	mt9t111_write( 0x0990, 0x0000 );	// MCU_DATA_0


	mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0006 );	// MCU_DATA_0

	msleep(300); 											//delay = 300

	mt9t111_write( 0x098E, 0x8400 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0005 );	// MCU_DATA_0

	msleep(200);											//delay = 200

	mt9t111_write( 0x3084, 0x2409 );	// DAC_LD_4_5
	mt9t111_write( 0x3092, 0x0A49 );	// DAC_LD_18_19
	mt9t111_write( 0x3094, 0x4949 );	// DAC_LD_20_21
	mt9t111_write( 0x3096, 0x4950 );	// DAC_LD_22_23

	// K26A_Rev3_patch
	mt9t111_write( 0x0982, 0x0000 );	// ACCESS_CTL_STAT
	mt9t111_write( 0x098A, 0x0CFB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x3C3C );
	mt9t111_write( 0x0992, 0x3C3C );
	mt9t111_write( 0x0994, 0x3C3C );
	mt9t111_write( 0x0996, 0x5F4F );
	mt9t111_write( 0x0998, 0x30ED );
	mt9t111_write( 0x099A, 0x0AED );
	mt9t111_write( 0x099C, 0x08BD );
	mt9t111_write( 0x099E, 0x61D5 );
	mt9t111_write( 0x098A, 0x0D0B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xCE04 );
	mt9t111_write( 0x0992, 0xCD1F );
	mt9t111_write( 0x0994, 0x1702 );
	mt9t111_write( 0x0996, 0x11CC );
	mt9t111_write( 0x0998, 0x332E );
	mt9t111_write( 0x099A, 0x30ED );
	mt9t111_write( 0x099C, 0x02CC );
	mt9t111_write( 0x099E, 0xFFFD );
	mt9t111_write( 0x098A, 0x0D1B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xED00 );
	mt9t111_write( 0x0992, 0xCC00 );
	mt9t111_write( 0x0994, 0x02BD );
	mt9t111_write( 0x0996, 0x706D );
	mt9t111_write( 0x0998, 0x18DE );
	mt9t111_write( 0x099A, 0x1F18 );
	mt9t111_write( 0x099C, 0x1F8E );
	mt9t111_write( 0x099E, 0x0110 );
	mt9t111_write( 0x098A, 0x0D2B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xCC3C );
	mt9t111_write( 0x0992, 0x5230 );
	mt9t111_write( 0x0994, 0xED00 );
	mt9t111_write( 0x0996, 0x18EC );
	mt9t111_write( 0x0998, 0xA0C4 );
	mt9t111_write( 0x099A, 0xFDBD );
	mt9t111_write( 0x099C, 0x7021 );
	mt9t111_write( 0x099E, 0x201E );
	mt9t111_write( 0x098A, 0x0D3B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xCC3C );
	mt9t111_write( 0x0992, 0x5230 );
	mt9t111_write( 0x0994, 0xED00 );
	mt9t111_write( 0x0996, 0xDE1F );
	mt9t111_write( 0x0998, 0xECA0 );
	mt9t111_write( 0x099A, 0xBD70 );
	mt9t111_write( 0x099C, 0x21CC );
	mt9t111_write( 0x099E, 0x3C52 );
	mt9t111_write( 0x098A, 0x0D4B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x30ED );
	mt9t111_write( 0x0992, 0x02CC );
	mt9t111_write( 0x0994, 0xFFFC );
	mt9t111_write( 0x0996, 0xED00 );
	mt9t111_write( 0x0998, 0xCC00 );
	mt9t111_write( 0x099A, 0x02BD );
	mt9t111_write( 0x099C, 0x706D );
	mt9t111_write( 0x099E, 0xFC04 );
	mt9t111_write( 0x098A, 0x0D5B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xE11A );
	mt9t111_write( 0x0992, 0x8300 );
	mt9t111_write( 0x0994, 0x0127 );
	mt9t111_write( 0x0996, 0x201A );
	mt9t111_write( 0x0998, 0x8300 );
	mt9t111_write( 0x099A, 0x0427 );
	mt9t111_write( 0x099C, 0x221A );
	mt9t111_write( 0x099E, 0x8300 );
	mt9t111_write( 0x098A, 0x0D6B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0827 );
	mt9t111_write( 0x0992, 0x241A );
	mt9t111_write( 0x0994, 0x8300 );
	mt9t111_write( 0x0996, 0x1027 );
	mt9t111_write( 0x0998, 0x261A );
	mt9t111_write( 0x099A, 0x8300 );
	mt9t111_write( 0x099C, 0x2027 );
	mt9t111_write( 0x099E, 0x281A );
	mt9t111_write( 0x098A, 0x0D7B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x8300 );
	mt9t111_write( 0x0992, 0x4027 );
	mt9t111_write( 0x0994, 0x2A20 );
	mt9t111_write( 0x0996, 0x2ECC );
	mt9t111_write( 0x0998, 0x001E );
	mt9t111_write( 0x099A, 0x30ED );
	mt9t111_write( 0x099C, 0x0A20 );
	mt9t111_write( 0x099E, 0x26CC );
	mt9t111_write( 0x098A, 0x0D8B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0022 );
	mt9t111_write( 0x0992, 0x30ED );
	mt9t111_write( 0x0994, 0x0A20 );
	mt9t111_write( 0x0996, 0x1ECC );
	mt9t111_write( 0x0998, 0x0021 );
	mt9t111_write( 0x099A, 0x30ED );
	mt9t111_write( 0x099C, 0x0A20 );
	mt9t111_write( 0x099E, 0x16CC );
	mt9t111_write( 0x098A, 0x0D9B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0020 );
	mt9t111_write( 0x0992, 0x30ED );
	mt9t111_write( 0x0994, 0x0A20 );
	mt9t111_write( 0x0996, 0x0ECC );
	mt9t111_write( 0x0998, 0x002A );
	mt9t111_write( 0x099A, 0x30ED );
	mt9t111_write( 0x099C, 0x0A20 );
	mt9t111_write( 0x099E, 0x06CC );
	mt9t111_write( 0x098A, 0x0DAB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x002B );
	mt9t111_write( 0x0992, 0x30ED );
	mt9t111_write( 0x0994, 0x0ACC );
	mt9t111_write( 0x0996, 0x3400 );
	mt9t111_write( 0x0998, 0x30ED );
	mt9t111_write( 0x099A, 0x0034 );
	mt9t111_write( 0x099C, 0xBD6F );
	mt9t111_write( 0x099E, 0xD184 );
	mt9t111_write( 0x098A, 0x0DBB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0330 );
	mt9t111_write( 0x0992, 0xED07 );
	mt9t111_write( 0x0994, 0xA60C );
	mt9t111_write( 0x0996, 0x4848 );
	mt9t111_write( 0x0998, 0x5FED );
	mt9t111_write( 0x099A, 0x05EC );
	mt9t111_write( 0x099C, 0x07EA );
	mt9t111_write( 0x099E, 0x06AA );
	mt9t111_write( 0x098A, 0x0DCB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0531 );
	mt9t111_write( 0x0992, 0xBD70 );
	mt9t111_write( 0x0994, 0x21DE );
	mt9t111_write( 0x0996, 0x1F1F );
	mt9t111_write( 0x0998, 0x8E01 );
	mt9t111_write( 0x099A, 0x08EC );
	mt9t111_write( 0x099C, 0x9B05 );
	mt9t111_write( 0x099E, 0x30ED );
	mt9t111_write( 0x098A, 0x0DDB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0820 );
	mt9t111_write( 0x0992, 0x3BDE );
	mt9t111_write( 0x0994, 0x1FEC );
	mt9t111_write( 0x0996, 0x0783 );
	mt9t111_write( 0x0998, 0x0040 );
	mt9t111_write( 0x099A, 0x2628 );
	mt9t111_write( 0x099C, 0x7F30 );
	mt9t111_write( 0x099E, 0xC4CC );
	mt9t111_write( 0x098A, 0x0DEB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x3C68 );
	mt9t111_write( 0x0992, 0xBD6F );
	mt9t111_write( 0x0994, 0xD1FD );
	mt9t111_write( 0x0996, 0x30C5 );
	mt9t111_write( 0x0998, 0xCC01 );
	mt9t111_write( 0x099A, 0xF4FD );
	mt9t111_write( 0x099C, 0x30C7 );
	mt9t111_write( 0x099E, 0xC640 );
	mt9t111_write( 0x098A, 0x0DFB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xF730 );
	mt9t111_write( 0x0992, 0xC4CC );
	mt9t111_write( 0x0994, 0x0190 );
	mt9t111_write( 0x0996, 0xFD30 );
	mt9t111_write( 0x0998, 0xC501 );
	mt9t111_write( 0x099A, 0x0101 );
	mt9t111_write( 0x099C, 0xFC30 );
	mt9t111_write( 0x099E, 0xC230 );
	mt9t111_write( 0x098A, 0x0E0B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xED08 );
	mt9t111_write( 0x0992, 0x200A );
	mt9t111_write( 0x0994, 0xCC3C );
	mt9t111_write( 0x0996, 0x68BD );
	mt9t111_write( 0x0998, 0x6FD1 );
	mt9t111_write( 0x099A, 0x0530 );
	mt9t111_write( 0x099C, 0xED08 );
	mt9t111_write( 0x099E, 0xCC34 );
	mt9t111_write( 0x098A, 0x0E1B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x08ED );
	mt9t111_write( 0x0992, 0x00EC );
	mt9t111_write( 0x0994, 0x08BD );
	mt9t111_write( 0x0996, 0x7021 );
	mt9t111_write( 0x0998, 0x30C6 );
	mt9t111_write( 0x099A, 0x0C3A );
	mt9t111_write( 0x099C, 0x3539 );
	mt9t111_write( 0x099E, 0x373C );
	mt9t111_write( 0x098A, 0x0E2B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x3C3C );
	mt9t111_write( 0x0992, 0x34DE );
	mt9t111_write( 0x0994, 0x2FEE );
	mt9t111_write( 0x0996, 0x0EAD );
	mt9t111_write( 0x0998, 0x007D );
	mt9t111_write( 0x099A, 0x13EF );
	mt9t111_write( 0x099C, 0x277C );
	mt9t111_write( 0x099E, 0xCE13 );
	mt9t111_write( 0x098A, 0x0E3B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xE01E );
	mt9t111_write( 0x0992, 0x0510 );
	mt9t111_write( 0x0994, 0x60E6 );
	mt9t111_write( 0x0996, 0x0E4F );
	mt9t111_write( 0x0998, 0xC313 );
	mt9t111_write( 0x099A, 0xF08F );
	mt9t111_write( 0x099C, 0xE600 );
	mt9t111_write( 0x099E, 0x30E1 );
	mt9t111_write( 0x098A, 0x0E4B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0722 );
	mt9t111_write( 0x0992, 0x16F6 );
	mt9t111_write( 0x0994, 0x13EE );
	mt9t111_write( 0x0996, 0x4FC3 );
	mt9t111_write( 0x0998, 0x13F3 );
	mt9t111_write( 0x099A, 0x8FE6 );
	mt9t111_write( 0x099C, 0x0030 );
	mt9t111_write( 0x099E, 0xE107 );
	mt9t111_write( 0x098A, 0x0E5B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x2507 );
	mt9t111_write( 0x0992, 0xF613 );
	mt9t111_write( 0x0994, 0xEEC1 );
	mt9t111_write( 0x0996, 0x0325 );
	mt9t111_write( 0x0998, 0x3C7F );
	mt9t111_write( 0x099A, 0x13EE );
	mt9t111_write( 0x099C, 0xF613 );
	mt9t111_write( 0x099E, 0xEFE7 );
	mt9t111_write( 0x098A, 0x0E6B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x06CC );
	mt9t111_write( 0x0992, 0x13F0 );
	mt9t111_write( 0x0994, 0xED04 );
	mt9t111_write( 0x0996, 0xCC13 );
	mt9t111_write( 0x0998, 0xF320 );
	mt9t111_write( 0x099A, 0x0F7C );
	mt9t111_write( 0x099C, 0x13EE );
	mt9t111_write( 0x099E, 0xEC04 );
	mt9t111_write( 0x098A, 0x0E7B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xC300 );
	mt9t111_write( 0x0992, 0x01ED );
	mt9t111_write( 0x0994, 0x04EC );
	mt9t111_write( 0x0996, 0x02C3 );
	mt9t111_write( 0x0998, 0x0001 );
	mt9t111_write( 0x099A, 0xED02 );
	mt9t111_write( 0x099C, 0xF613 );
	mt9t111_write( 0x099E, 0xEEE1 );
	mt9t111_write( 0x098A, 0x0E8B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0624 );
	mt9t111_write( 0x0992, 0x12EE );
	mt9t111_write( 0x0994, 0x04E6 );
	mt9t111_write( 0x0996, 0x0030 );
	mt9t111_write( 0x0998, 0xE107 );
	mt9t111_write( 0x099A, 0x22DF );
	mt9t111_write( 0x099C, 0xEE02 );
	mt9t111_write( 0x099E, 0xE600 );
	mt9t111_write( 0x098A, 0x0E9B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x30E1 );
	mt9t111_write( 0x0992, 0x0725 );
	mt9t111_write( 0x0994, 0xD6DE );
	mt9t111_write( 0x0996, 0x49EE );
	mt9t111_write( 0x0998, 0x08AD );
	mt9t111_write( 0x099A, 0x00CC );
	mt9t111_write( 0x099C, 0x13F6 );
	mt9t111_write( 0x099E, 0x30ED );
	mt9t111_write( 0x098A, 0x0EAB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x00DE );
	mt9t111_write( 0x0992, 0x2FEE );
	mt9t111_write( 0x0994, 0x10CC );
	mt9t111_write( 0x0996, 0x13FA );
	mt9t111_write( 0x0998, 0xAD00 );
	mt9t111_write( 0x099A, 0x3838 );
	mt9t111_write( 0x099C, 0x3838 );
	mt9t111_write( 0x099E, 0x3937 );
	mt9t111_write( 0x098A, 0x0EBB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x363C );
	mt9t111_write( 0x0992, 0x3C3C );
	mt9t111_write( 0x0994, 0x5F4F );
	mt9t111_write( 0x0996, 0x30ED );
	mt9t111_write( 0x0998, 0x04EC );
	mt9t111_write( 0x099A, 0x06ED );
	mt9t111_write( 0x099C, 0x008F );
	mt9t111_write( 0x099E, 0xC300 );
	mt9t111_write( 0x098A, 0x0ECB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x04BD );
	mt9t111_write( 0x0992, 0x0F43 );
	mt9t111_write( 0x0994, 0x30EC );
	mt9t111_write( 0x0996, 0x04BD );
	mt9t111_write( 0x0998, 0x0F76 );
	mt9t111_write( 0x099A, 0x30ED );
	mt9t111_write( 0x099C, 0x0238 );
	mt9t111_write( 0x099E, 0x3838 );
	mt9t111_write( 0x098A, 0x0EDB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x3839 );
	mt9t111_write( 0x0992, 0x373C );
	mt9t111_write( 0x0994, 0x3C3C );
	mt9t111_write( 0x0996, 0x3C30 );
	mt9t111_write( 0x0998, 0xE608 );
	mt9t111_write( 0x099A, 0x2712 );
	mt9t111_write( 0x099C, 0xC101 );
	mt9t111_write( 0x099E, 0x2713 );
	mt9t111_write( 0x098A, 0x0EEB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xC102 );
	mt9t111_write( 0x0992, 0x2714 );
	mt9t111_write( 0x0994, 0xC103 );
	mt9t111_write( 0x0996, 0x2715 );
	mt9t111_write( 0x0998, 0xC104 );
	mt9t111_write( 0x099A, 0x2716 );
	mt9t111_write( 0x099C, 0x2019 );
	mt9t111_write( 0x099E, 0xCC30 );
	mt9t111_write( 0x098A, 0x0EFB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x5E20 );
	mt9t111_write( 0x0992, 0x12CC );
	mt9t111_write( 0x0994, 0x305A );
	mt9t111_write( 0x0996, 0x200D );
	mt9t111_write( 0x0998, 0xCC30 );
	mt9t111_write( 0x099A, 0x5620 );
	mt9t111_write( 0x099C, 0x08CC );
	mt9t111_write( 0x099E, 0x305C );
	mt9t111_write( 0x098A, 0x0F0B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x2003 );
	mt9t111_write( 0x0992, 0xCC30 );
	mt9t111_write( 0x0994, 0x58ED );
	mt9t111_write( 0x0996, 0x065F );
	mt9t111_write( 0x0998, 0x4FED );
	mt9t111_write( 0x099A, 0x04EC );
	mt9t111_write( 0x099C, 0x0BED );
	mt9t111_write( 0x099E, 0x008F );
	mt9t111_write( 0x098A, 0x0F1B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xC300 );
	mt9t111_write( 0x0992, 0x04BD );
	mt9t111_write( 0x0994, 0x0F43 );
	mt9t111_write( 0x0996, 0x30EC );
	mt9t111_write( 0x0998, 0x048A );
	mt9t111_write( 0x099A, 0x02ED );
	mt9t111_write( 0x099C, 0x02EC );
	mt9t111_write( 0x099E, 0x06ED );
	mt9t111_write( 0x098A, 0x0F2B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x008F );
	mt9t111_write( 0x0992, 0xC300 );
	mt9t111_write( 0x0994, 0x02DE );
	mt9t111_write( 0x0996, 0x0EAD );
	mt9t111_write( 0x0998, 0x0030 );
	mt9t111_write( 0x099A, 0xEC04 );
	mt9t111_write( 0x099C, 0xBD0F );
	mt9t111_write( 0x099E, 0x7630 );
	mt9t111_write( 0x098A, 0x0F3B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xED02 );
	mt9t111_write( 0x0992, 0x3838 );
	mt9t111_write( 0x0994, 0x3838 );
	mt9t111_write( 0x0996, 0x3139 );
	mt9t111_write( 0x0998, 0x3736 );
	mt9t111_write( 0x099A, 0x30EC );
	mt9t111_write( 0x099C, 0x041A );
	mt9t111_write( 0x099E, 0x8300 );
	mt9t111_write( 0x098A, 0x0F4B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x4025 );
	mt9t111_write( 0x0992, 0x22EC );
	mt9t111_write( 0x0994, 0x041A );
	mt9t111_write( 0x0996, 0x8300 );
	mt9t111_write( 0x0998, 0x8024 );
	mt9t111_write( 0x099A, 0x0504 );
	mt9t111_write( 0x099C, 0xCA40 );
	mt9t111_write( 0x099E, 0x2015 );
	mt9t111_write( 0x098A, 0x0F5B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xEC04 );
	mt9t111_write( 0x0992, 0x1A83 );
	mt9t111_write( 0x0994, 0x0100 );
	mt9t111_write( 0x0996, 0x2406 );
	mt9t111_write( 0x0998, 0x0404 );
	mt9t111_write( 0x099A, 0xCA80 );
	mt9t111_write( 0x099C, 0x2007 );
	mt9t111_write( 0x099E, 0xEC04 );
	mt9t111_write( 0x098A, 0x0F6B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0404 );
	mt9t111_write( 0x0992, 0x04CA );
	mt9t111_write( 0x0994, 0xC0EE );
	mt9t111_write( 0x0996, 0x00ED );
	mt9t111_write( 0x0998, 0x0038 );
	mt9t111_write( 0x099A, 0x3937 );
	mt9t111_write( 0x099C, 0x363C );
	mt9t111_write( 0x099E, 0x301F );
	mt9t111_write( 0x098A, 0x0F7B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0340 );
	mt9t111_write( 0x0992, 0x0E1F );
	mt9t111_write( 0x0994, 0x0380 );
	mt9t111_write( 0x0996, 0x0AEC );
	mt9t111_write( 0x0998, 0x02C4 );
	mt9t111_write( 0x099A, 0x3F4F );
	mt9t111_write( 0x099C, 0x0505 );
	mt9t111_write( 0x099E, 0x0520 );
	mt9t111_write( 0x098A, 0x0F8B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x1B1F );
	mt9t111_write( 0x0992, 0x0380 );
	mt9t111_write( 0x0994, 0x09EC );
	mt9t111_write( 0x0996, 0x02C4 );
	mt9t111_write( 0x0998, 0x3F4F );
	mt9t111_write( 0x099A, 0x0505 );
	mt9t111_write( 0x099C, 0x200E );
	mt9t111_write( 0x099E, 0x1F03 );
	mt9t111_write( 0x098A, 0x0F9B );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x4008 );
	mt9t111_write( 0x0992, 0xEC02 );
	mt9t111_write( 0x0994, 0xC43F );
	mt9t111_write( 0x0996, 0x4F05 );
	mt9t111_write( 0x0998, 0x2002 );
	mt9t111_write( 0x099A, 0xEC02 );
	mt9t111_write( 0x099C, 0xED00 );
	mt9t111_write( 0x099E, 0x3838 );
	mt9t111_write( 0x098A, 0x8FAB );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0039 );	// MCU_DATA_0
	mt9t111_write( 0x098A, 0x1000 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xCC10 );
	mt9t111_write( 0x0992, 0x09BD );
	mt9t111_write( 0x0994, 0x4224 );
	mt9t111_write( 0x0996, 0x7E10 );
	mt9t111_write( 0x0998, 0x09C6 );
	mt9t111_write( 0x099A, 0x01F7 );
	mt9t111_write( 0x099C, 0x018A );
	mt9t111_write( 0x099E, 0xC609 );
	mt9t111_write( 0x098A, 0x1010 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xF701 );
	mt9t111_write( 0x0992, 0x8BDE );
	mt9t111_write( 0x0994, 0x3F18 );
	mt9t111_write( 0x0996, 0xCE0B );
	mt9t111_write( 0x0998, 0xF3CC );
	mt9t111_write( 0x099A, 0x0011 );
	mt9t111_write( 0x099C, 0xBDD7 );
	mt9t111_write( 0x099E, 0x00CC );
	mt9t111_write( 0x098A, 0x1020 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0BF3 );
	mt9t111_write( 0x0992, 0xDD3F );
	mt9t111_write( 0x0994, 0xDE35 );
	mt9t111_write( 0x0996, 0x18CE );
	mt9t111_write( 0x0998, 0x0C05 );
	mt9t111_write( 0x099A, 0xCC00 );
	mt9t111_write( 0x099C, 0x3FBD );
	mt9t111_write( 0x099E, 0xD700 );
	mt9t111_write( 0x098A, 0x1030 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xCC0C );
	mt9t111_write( 0x0992, 0x05DD );
	mt9t111_write( 0x0994, 0x35DE );
	mt9t111_write( 0x0996, 0x4718 );
	mt9t111_write( 0x0998, 0xCE0C );
	mt9t111_write( 0x099A, 0x45CC );
	mt9t111_write( 0x099C, 0x0015 );
	mt9t111_write( 0x099E, 0xBDD7 );
	mt9t111_write( 0x098A, 0x1040 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x00CC );
	mt9t111_write( 0x0992, 0x0C45 );
	mt9t111_write( 0x0994, 0xDD47 );
	mt9t111_write( 0x0996, 0xFE00 );
	mt9t111_write( 0x0998, 0x3318 );
	mt9t111_write( 0x099A, 0xCE0C );
	mt9t111_write( 0x099C, 0x5BCC );
	mt9t111_write( 0x099E, 0x0009 );
	mt9t111_write( 0x098A, 0x1050 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xBDD7 );
	mt9t111_write( 0x0992, 0x00CC );
	mt9t111_write( 0x0994, 0x0C5B );
	mt9t111_write( 0x0996, 0xFD00 );
	mt9t111_write( 0x0998, 0x33DE );
	mt9t111_write( 0x099A, 0x3118 );
	mt9t111_write( 0x099C, 0xCE0C );
	mt9t111_write( 0x099E, 0x65CC );
	mt9t111_write( 0x098A, 0x1060 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0029 );
	mt9t111_write( 0x0992, 0xBDD7 );
	mt9t111_write( 0x0994, 0x00CC );
	mt9t111_write( 0x0996, 0x0C65 );
	mt9t111_write( 0x0998, 0xDD31 );
	mt9t111_write( 0x099A, 0xDE39 );
	mt9t111_write( 0x099C, 0x18CE );
	mt9t111_write( 0x099E, 0x0C8F );
	mt9t111_write( 0x098A, 0x1070 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xCC00 );
	mt9t111_write( 0x0992, 0x23BD );
	mt9t111_write( 0x0994, 0xD700 );
	mt9t111_write( 0x0996, 0xCC0C );
	mt9t111_write( 0x0998, 0x8FDD );
	mt9t111_write( 0x099A, 0x39DE );
	mt9t111_write( 0x099C, 0x4918 );
	mt9t111_write( 0x099E, 0xCE0C );
	mt9t111_write( 0x098A, 0x1080 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xB3CC );
	mt9t111_write( 0x0992, 0x000D );
	mt9t111_write( 0x0994, 0xBDD7 );
	mt9t111_write( 0x0996, 0x00CC );
	mt9t111_write( 0x0998, 0x0CB3 );
	mt9t111_write( 0x099A, 0xDD49 );
	mt9t111_write( 0x099C, 0xFC04 );
	mt9t111_write( 0x099E, 0xC2FD );
	mt9t111_write( 0x098A, 0x1090 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0BF1 );
	mt9t111_write( 0x0992, 0x18FE );
	mt9t111_write( 0x0994, 0x0BF1 );
	mt9t111_write( 0x0996, 0xCDEE );
	mt9t111_write( 0x0998, 0x1518 );
	mt9t111_write( 0x099A, 0xCE0C );
	mt9t111_write( 0x099C, 0xC1CC );
	mt9t111_write( 0x099E, 0x0029 );
	mt9t111_write( 0x098A, 0x10A0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xBDD7 );
	mt9t111_write( 0x0992, 0x00FE );
	mt9t111_write( 0x0994, 0x0BF1 );
	mt9t111_write( 0x0996, 0xCC0C );
	mt9t111_write( 0x0998, 0xC1ED );
	mt9t111_write( 0x099A, 0x15CC );
	mt9t111_write( 0x099C, 0x11A5 );
	mt9t111_write( 0x099E, 0xFD0B );
	mt9t111_write( 0x098A, 0x10B0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xFFCC );
	mt9t111_write( 0x0992, 0x0CFB );
	mt9t111_write( 0x0994, 0xFD0C );
	mt9t111_write( 0x0996, 0x21CC );
	mt9t111_write( 0x0998, 0x128F );
	mt9t111_write( 0x099A, 0xFD0C );
	mt9t111_write( 0x099C, 0x53CC );
	mt9t111_write( 0x099E, 0x114E );
	mt9t111_write( 0x098A, 0x10C0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xFD0C );
	mt9t111_write( 0x0992, 0x5DCC );
	mt9t111_write( 0x0994, 0x10E2 );
	mt9t111_write( 0x0996, 0xFD0C );
	mt9t111_write( 0x0998, 0x6FCC );
	mt9t111_write( 0x099A, 0x0EDD );
	mt9t111_write( 0x099C, 0xFD0C );
	mt9t111_write( 0x099E, 0xD7CC );
	mt9t111_write( 0x098A, 0x10D0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0EBA );
	mt9t111_write( 0x0992, 0xFD0C );
	mt9t111_write( 0x0994, 0xE9CC );
	mt9t111_write( 0x0996, 0x1350 );
	mt9t111_write( 0x0998, 0xFD0C );
	mt9t111_write( 0x099A, 0x9BCC );
	mt9t111_write( 0x099C, 0x0E29 );
	mt9t111_write( 0x099E, 0xFD0C );
	mt9t111_write( 0x098A, 0x10E0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xBF39 );
	mt9t111_write( 0x0992, 0x373C );
	mt9t111_write( 0x0994, 0x3CDE );
	mt9t111_write( 0x0996, 0x1DEC );
	mt9t111_write( 0x0998, 0x0C5F );
	mt9t111_write( 0x099A, 0x8402 );
	mt9t111_write( 0x099C, 0x4416 );
	mt9t111_write( 0x099E, 0x4FF7 );
	mt9t111_write( 0x098A, 0x10F0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0CEB );
	mt9t111_write( 0x0992, 0xE60B );
	mt9t111_write( 0x0994, 0xC407 );
	mt9t111_write( 0x0996, 0xF70C );
	mt9t111_write( 0x0998, 0xEC7F );
	mt9t111_write( 0x099A, 0x30C4 );
	mt9t111_write( 0x099C, 0xEC25 );
	mt9t111_write( 0x099E, 0xFD30 );
	mt9t111_write( 0x098A, 0x1100 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xC5FC );
	mt9t111_write( 0x0992, 0x06D6 );
	mt9t111_write( 0x0994, 0xFD30 );
	mt9t111_write( 0x0996, 0xC701 );
	mt9t111_write( 0x0998, 0xFC30 );
	mt9t111_write( 0x099A, 0xC0FD );
	mt9t111_write( 0x099C, 0x0BED );
	mt9t111_write( 0x099E, 0xFC30 );
	mt9t111_write( 0x098A, 0x1110 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xC2FD );
	mt9t111_write( 0x0992, 0x0BEF );
	mt9t111_write( 0x0994, 0xFC04 );
	mt9t111_write( 0x0996, 0xC283 );
	mt9t111_write( 0x0998, 0xFFFF );
	mt9t111_write( 0x099A, 0x2728 );
	mt9t111_write( 0x099C, 0xDE06 );
	mt9t111_write( 0x099E, 0xEC22 );
	mt9t111_write( 0x098A, 0x1120 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x8322 );
	mt9t111_write( 0x0992, 0x0026 );
	mt9t111_write( 0x0994, 0x1FCC );
	mt9t111_write( 0x0996, 0x3064 );
	mt9t111_write( 0x0998, 0x30ED );
	mt9t111_write( 0x099A, 0x008F );
	mt9t111_write( 0x099C, 0xC300 );
	mt9t111_write( 0x099E, 0x02DE );
	mt9t111_write( 0x098A, 0x1130 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0CAD );
	mt9t111_write( 0x0992, 0x0030 );
	mt9t111_write( 0x0994, 0x1D02 );
	mt9t111_write( 0x0996, 0x01CC );
	mt9t111_write( 0x0998, 0x3064 );
	mt9t111_write( 0x099A, 0xED00 );
	mt9t111_write( 0x099C, 0x8FC3 );
	mt9t111_write( 0x099E, 0x0002 );
	mt9t111_write( 0x098A, 0x1140 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xDE0E );
	mt9t111_write( 0x0992, 0xAD00 );
	mt9t111_write( 0x0994, 0x30E6 );
	mt9t111_write( 0x0996, 0x04BD );
	mt9t111_write( 0x0998, 0x5203 );
	mt9t111_write( 0x099A, 0x3838 );
	mt9t111_write( 0x099C, 0x3139 );
	mt9t111_write( 0x099E, 0x3C3C );
	mt9t111_write( 0x098A, 0x1150 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x3C21 );
	mt9t111_write( 0x0992, 0x01CC );
	mt9t111_write( 0x0994, 0x0018 );
	mt9t111_write( 0x0996, 0xBD6F );
	mt9t111_write( 0x0998, 0xD1C5 );
	mt9t111_write( 0x099A, 0x0426 );
	mt9t111_write( 0x099C, 0xF5DC );
	mt9t111_write( 0x099E, 0x2530 );
	mt9t111_write( 0x098A, 0x1160 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xED04 );
	mt9t111_write( 0x0992, 0x2012 );
	mt9t111_write( 0x0994, 0xEE04 );
	mt9t111_write( 0x0996, 0x3C18 );
	mt9t111_write( 0x0998, 0x38E6 );
	mt9t111_write( 0x099A, 0x2118 );
	mt9t111_write( 0x099C, 0xE7BE );
	mt9t111_write( 0x099E, 0x30EE );
	mt9t111_write( 0x098A, 0x1170 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x04EC );
	mt9t111_write( 0x0992, 0x1D30 );
	mt9t111_write( 0x0994, 0xED04 );
	mt9t111_write( 0x0996, 0xEC04 );
	mt9t111_write( 0x0998, 0x26EA );
	mt9t111_write( 0x099A, 0xCC00 );
	mt9t111_write( 0x099C, 0x1AED );
	mt9t111_write( 0x099E, 0x02CC );
	mt9t111_write( 0x098A, 0x1180 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xFBFF );
	mt9t111_write( 0x0992, 0xED00 );
	mt9t111_write( 0x0994, 0xCC04 );
	mt9t111_write( 0x0996, 0x00BD );
	mt9t111_write( 0x0998, 0x706D );
	mt9t111_write( 0x099A, 0xCC00 );
	mt9t111_write( 0x099C, 0x1A30 );
	mt9t111_write( 0x099E, 0xED02 );
	mt9t111_write( 0x098A, 0x1190 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xCCFB );
	mt9t111_write( 0x0992, 0xFFED );
	mt9t111_write( 0x0994, 0x005F );
	mt9t111_write( 0x0996, 0x4FBD );
	mt9t111_write( 0x0998, 0x706D );
	mt9t111_write( 0x099A, 0x5FBD );
	mt9t111_write( 0x099C, 0x5B17 );
	mt9t111_write( 0x099E, 0xBD55 );
	mt9t111_write( 0x098A, 0x11A0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x8B38 );
	mt9t111_write( 0x0992, 0x3838 );
	mt9t111_write( 0x0994, 0x393C );
	mt9t111_write( 0x0996, 0x3CC6 );
	mt9t111_write( 0x0998, 0x40F7 );
	mt9t111_write( 0x099A, 0x30C4 );
	mt9t111_write( 0x099C, 0xFC0B );
	mt9t111_write( 0x099E, 0xEDFD );
	mt9t111_write( 0x098A, 0x11B0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x30C0 );
	mt9t111_write( 0x0992, 0xFC0B );
	mt9t111_write( 0x0994, 0xEFFD );
	mt9t111_write( 0x0996, 0x30C2 );
	mt9t111_write( 0x0998, 0xDE1D );
	mt9t111_write( 0x099A, 0xEC25 );
	mt9t111_write( 0x099C, 0xFD30 );
	mt9t111_write( 0x099E, 0xC501 );
	mt9t111_write( 0x098A, 0x11C0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0101 );
	mt9t111_write( 0x0992, 0xFC30 );
	mt9t111_write( 0x0994, 0xC2FD );
	mt9t111_write( 0x0996, 0x06D6 );
	mt9t111_write( 0x0998, 0xEC0C );
	mt9t111_write( 0x099A, 0x5F84 );
	mt9t111_write( 0x099C, 0x0244 );
	mt9t111_write( 0x099E, 0x164F );
	mt9t111_write( 0x098A, 0x11D0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x30E7 );
	mt9t111_write( 0x0992, 0x03F1 );
	mt9t111_write( 0x0994, 0x0CEB );
	mt9t111_write( 0x0996, 0x2715 );
	mt9t111_write( 0x0998, 0xF10C );
	mt9t111_write( 0x099A, 0xEB23 );
	mt9t111_write( 0x099C, 0x09FC );
	mt9t111_write( 0x099E, 0x06D6 );
	mt9t111_write( 0x098A, 0x11E0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x04FD );
	mt9t111_write( 0x0992, 0x06D6 );
	mt9t111_write( 0x0994, 0x2007 );
	mt9t111_write( 0x0996, 0xFC06 );
	mt9t111_write( 0x0998, 0xD605 );
	mt9t111_write( 0x099A, 0xFD06 );
	mt9t111_write( 0x099C, 0xD6DE );
	mt9t111_write( 0x099E, 0x1DE6 );
	mt9t111_write( 0x098A, 0x11F0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0BC4 );
	mt9t111_write( 0x0992, 0x0730 );
	mt9t111_write( 0x0994, 0xE702 );
	mt9t111_write( 0x0996, 0xF10C );
	mt9t111_write( 0x0998, 0xEC27 );
	mt9t111_write( 0x099A, 0x2C7D );
	mt9t111_write( 0x099C, 0x0CEC );
	mt9t111_write( 0x099E, 0x2727 );
	mt9t111_write( 0x098A, 0x1200 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x5D27 );
	mt9t111_write( 0x0992, 0x247F );
	mt9t111_write( 0x0994, 0x30C4 );
	mt9t111_write( 0x0996, 0xFC06 );
	mt9t111_write( 0x0998, 0xD6FD );
	mt9t111_write( 0x099A, 0x30C5 );
	mt9t111_write( 0x099C, 0xF60C );
	mt9t111_write( 0x099E, 0xEC4F );
	mt9t111_write( 0x098A, 0x1210 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xFD30 );
	mt9t111_write( 0x0992, 0xC7C6 );
	mt9t111_write( 0x0994, 0x40F7 );
	mt9t111_write( 0x0996, 0x30C4 );
	mt9t111_write( 0x0998, 0xE602 );
	mt9t111_write( 0x099A, 0x4FFD );
	mt9t111_write( 0x099C, 0x30C5 );
	mt9t111_write( 0x099E, 0x0101 );
	mt9t111_write( 0x098A, 0x1220 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x01FC );
	mt9t111_write( 0x0992, 0x30C2 );
	mt9t111_write( 0x0994, 0xFD06 );
	mt9t111_write( 0x0996, 0xD67D );
	mt9t111_write( 0x0998, 0x06CB );
	mt9t111_write( 0x099A, 0x272E );
	mt9t111_write( 0x099C, 0xC640 );
	mt9t111_write( 0x099E, 0xF730 );
	mt9t111_write( 0x098A, 0x1230 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xC4FC );
	mt9t111_write( 0x0992, 0x06C1 );
	mt9t111_write( 0x0994, 0x04F3 );
	mt9t111_write( 0x0996, 0x06D6 );
	mt9t111_write( 0x0998, 0xED00 );
	mt9t111_write( 0x099A, 0x5F6D );
	mt9t111_write( 0x099C, 0x002A );
	mt9t111_write( 0x099E, 0x0153 );
	mt9t111_write( 0x098A, 0x1240 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x17FD );
	mt9t111_write( 0x0992, 0x30C0 );
	mt9t111_write( 0x0994, 0xEC00 );
	mt9t111_write( 0x0996, 0xFD30 );
	mt9t111_write( 0x0998, 0xC2FC );
	mt9t111_write( 0x099A, 0x06C1 );
	mt9t111_write( 0x099C, 0xFD30 );
	mt9t111_write( 0x099E, 0xC501 );
	mt9t111_write( 0x098A, 0x1250 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x0101 );
	mt9t111_write( 0x0992, 0xFC30 );
	mt9t111_write( 0x0994, 0xC2FD );
	mt9t111_write( 0x0996, 0x06C7 );
	mt9t111_write( 0x0998, 0x2022 );
	mt9t111_write( 0x099A, 0x7F30 );
	mt9t111_write( 0x099C, 0xC4DE );
	mt9t111_write( 0x099E, 0x1DEC );
	mt9t111_write( 0x098A, 0x1260 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x25FD );
	mt9t111_write( 0x0992, 0x30C5 );
	mt9t111_write( 0x0994, 0xFC06 );
	mt9t111_write( 0x0996, 0xD6FD );
	mt9t111_write( 0x0998, 0x30C7 );
	mt9t111_write( 0x099A, 0x01FC );
	mt9t111_write( 0x099C, 0x30C0 );
	mt9t111_write( 0x099E, 0xFD06 );
	mt9t111_write( 0x098A, 0x1270 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xD0FC );
	mt9t111_write( 0x0992, 0x30C2 );
	mt9t111_write( 0x0994, 0xFD06 );
	mt9t111_write( 0x0996, 0xD2EC );
	mt9t111_write( 0x0998, 0x25FD );
	mt9t111_write( 0x099A, 0x06C3 );
	mt9t111_write( 0x099C, 0xBD95 );
	mt9t111_write( 0x099E, 0x3CDE );
	mt9t111_write( 0x098A, 0x1280 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x3FEE );
	mt9t111_write( 0x0992, 0x10AD );
	mt9t111_write( 0x0994, 0x00DE );
	mt9t111_write( 0x0996, 0x1DFC );
	mt9t111_write( 0x0998, 0x06CC );
	mt9t111_write( 0x099A, 0xED3E );
	mt9t111_write( 0x099C, 0x3838 );
	mt9t111_write( 0x099E, 0x3930 );
	mt9t111_write( 0x098A, 0x1290 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x8FC3 );
	mt9t111_write( 0x0992, 0xFFEC );
	mt9t111_write( 0x0994, 0x8F35 );
	mt9t111_write( 0x0996, 0xBDAD );
	mt9t111_write( 0x0998, 0x15DE );
	mt9t111_write( 0x099A, 0x198F );
	mt9t111_write( 0x099C, 0xC301 );
	mt9t111_write( 0x099E, 0x4B8F );
	mt9t111_write( 0x098A, 0x12A0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xEC00 );
	mt9t111_write( 0x0992, 0xFD05 );
	mt9t111_write( 0x0994, 0x0EEC );
	mt9t111_write( 0x0996, 0x02FD );
	mt9t111_write( 0x0998, 0x0510 );
	mt9t111_write( 0x099A, 0x8FC3 );
	mt9t111_write( 0x099C, 0xFFCB );
	mt9t111_write( 0x099E, 0x8FE6 );
	mt9t111_write( 0x098A, 0x12B0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x00F7 );
	mt9t111_write( 0x0992, 0x0514 );
	mt9t111_write( 0x0994, 0xE603 );
	mt9t111_write( 0x0996, 0xF705 );
	mt9t111_write( 0x0998, 0x15FC );
	mt9t111_write( 0x099A, 0x055B );
	mt9t111_write( 0x099C, 0xFD05 );
	mt9t111_write( 0x099E, 0x12DE );
	mt9t111_write( 0x098A, 0x12C0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x37EE );
	mt9t111_write( 0x0992, 0x08AD );
	mt9t111_write( 0x0994, 0x00F6 );
	mt9t111_write( 0x0996, 0x0516 );
	mt9t111_write( 0x0998, 0x4F30 );
	mt9t111_write( 0x099A, 0xED04 );
	mt9t111_write( 0x099C, 0xDE1F );
	mt9t111_write( 0x099E, 0xEC6B );
	mt9t111_write( 0x098A, 0x12D0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xFD05 );
	mt9t111_write( 0x0992, 0x0EEC );
	mt9t111_write( 0x0994, 0x6DFD );
	mt9t111_write( 0x0996, 0x0510 );
	mt9t111_write( 0x0998, 0xDE19 );
	mt9t111_write( 0x099A, 0x8FC3 );
	mt9t111_write( 0x099C, 0x0117 );
	mt9t111_write( 0x099E, 0x8FE6 );
	mt9t111_write( 0x098A, 0x12E0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x00F7 );
	mt9t111_write( 0x0992, 0x0514 );
	mt9t111_write( 0x0994, 0xE603 );
	mt9t111_write( 0x0996, 0xF705 );
	mt9t111_write( 0x0998, 0x15FC );
	mt9t111_write( 0x099A, 0x0559 );
	mt9t111_write( 0x099C, 0xFD05 );
	mt9t111_write( 0x099E, 0x12DE );
	mt9t111_write( 0x098A, 0x12F0 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x37EE );
	mt9t111_write( 0x0992, 0x08AD );
	mt9t111_write( 0x0994, 0x00F6 );
	mt9t111_write( 0x0996, 0x0516 );
	mt9t111_write( 0x0998, 0x4F30 );
	mt9t111_write( 0x099A, 0xED06 );
	mt9t111_write( 0x099C, 0xDE1F );
	mt9t111_write( 0x099E, 0xEC6B );
	mt9t111_write( 0x098A, 0x1300 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xFD05 );
	mt9t111_write( 0x0992, 0x0EEC );
	mt9t111_write( 0x0994, 0x6DFD );
	mt9t111_write( 0x0996, 0x0510 );
	mt9t111_write( 0x0998, 0xDE19 );
	mt9t111_write( 0x099A, 0x8FC3 );
	mt9t111_write( 0x099C, 0x0118 );
	mt9t111_write( 0x099E, 0x8FE6 );
	mt9t111_write( 0x098A, 0x1310 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x00F7 );
	mt9t111_write( 0x0992, 0x0514 );
	mt9t111_write( 0x0994, 0xE603 );
	mt9t111_write( 0x0996, 0xF705 );
	mt9t111_write( 0x0998, 0x15FC );
	mt9t111_write( 0x099A, 0x0559 );
	mt9t111_write( 0x099C, 0xFD05 );
	mt9t111_write( 0x099E, 0x12DE );
	mt9t111_write( 0x098A, 0x1320 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x37EE );
	mt9t111_write( 0x0992, 0x08AD );
	mt9t111_write( 0x0994, 0x00F6 );
	mt9t111_write( 0x0996, 0x0516 );
	mt9t111_write( 0x0998, 0x4F30 );
	mt9t111_write( 0x099A, 0xED08 );
	mt9t111_write( 0x099C, 0xCC32 );
	mt9t111_write( 0x099E, 0x8EED );
	mt9t111_write( 0x098A, 0x1330 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x00EC );
	mt9t111_write( 0x0992, 0x04BD );
	mt9t111_write( 0x0994, 0x7021 );
	mt9t111_write( 0x0996, 0xCC32 );
	mt9t111_write( 0x0998, 0x6C30 );
	mt9t111_write( 0x099A, 0xED02 );
	mt9t111_write( 0x099C, 0xCCF8 );
	mt9t111_write( 0x099E, 0x00ED );
	mt9t111_write( 0x098A, 0x1340 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x00A6 );
	mt9t111_write( 0x0992, 0x07E3 );
	mt9t111_write( 0x0994, 0x0884 );
	mt9t111_write( 0x0996, 0x07BD );
	mt9t111_write( 0x0998, 0x706D );
	mt9t111_write( 0x099A, 0x30C6 );
	mt9t111_write( 0x099C, 0x143A );
	mt9t111_write( 0x099E, 0x3539 );
	mt9t111_write( 0x098A, 0x1350 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x3CBD );
	mt9t111_write( 0x0992, 0x776D );
	mt9t111_write( 0x0994, 0xCC32 );
	mt9t111_write( 0x0996, 0x5C30 );
	mt9t111_write( 0x0998, 0xED00 );
	mt9t111_write( 0x099A, 0xFC13 );
	mt9t111_write( 0x099C, 0x8683 );
	mt9t111_write( 0x099E, 0x0001 );
	mt9t111_write( 0x098A, 0x1360 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0xBD70 );
	mt9t111_write( 0x0992, 0x21CC );
	mt9t111_write( 0x0994, 0x325E );
	mt9t111_write( 0x0996, 0x30ED );
	mt9t111_write( 0x0998, 0x00FC );
	mt9t111_write( 0x099A, 0x1388 );
	mt9t111_write( 0x099C, 0x8300 );
	mt9t111_write( 0x099E, 0x01BD );
	mt9t111_write( 0x098A, 0x1370 );	// PHYSICAL_ADDR_ACCESS
	mt9t111_write( 0x0990, 0x7021 );
	mt9t111_write( 0x0992, 0x3839 );
	mt9t111_write( 0x098E, 0x0010 );	// MCU_ADDRESS [MON_ADDR]
	mt9t111_write( 0x0990, 0x1000 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x0003 );	// MCU_ADDRESS [MON_ALGO]
	mt9t111_write( 0x0990, 0x0004 );	// MCU_DATA_0
	//  POLL  MON_PATCH_0 =>  0x01
	msleep(200);											//delay = 200

	mt9t111_write( 0x098E, 0x4815 );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0004 );	// MCU_DATA_0
	mt9t111_write( 0x098E, 0x485D );	// MCU_ADDRESS
	mt9t111_write( 0x0990, 0x0004 );	// MCU_DATA_0
	mt9t111_write( 0x0018, 0x0028 );	// STANDBY_CONTROL_AND_STATUS
//  POLL  SEQ_STATE =>  0x03

		if (checkCameraID(sensor)) {
		result = HAL_CAM_ERROR_INTERNAL_ERROR;
	}
	tm1 = ktime_get();
	pr_info("Exit Init Sec %d nsec %d\n", tm1.tv.sec, tm1.tv.nsec);

	return result;
}


struct sens_methods sens_meth_pri = {
    DRV_GetIntfConfig: CAMDRV_GetIntfConfig_Pri,
    DRV_GetIntfSeqSel : CAMDRV_GetIntfSeqSel_Pri,
    DRV_Wakeup : CAMDRV_Wakeup_Pri,
    DRV_SetVideoCaptureMode : CAMDRV_SetVideoCaptureMode_Pri,
    DRV_SetFrameRate : CAMDRV_SetFrameRate_Pri,
    DRV_EnableVideoCapture : CAMDRV_EnableVideoCapture_Pri,
    DRV_SetCamSleep : CAMDRV_SetCamSleep_Pri,
    DRV_DisableCapture : CAMDRV_DisableCapture_Pri,
    DRV_DisablePreview : CAMDRV_DisablePreview_Pri,
    DRV_CfgStillnThumbCapture : CAMDRV_CfgStillnThumbCapture_Pri,
    DRV_SetSceneMode : CAMDRV_SetSceneMode_Pri,
    DRV_SetWBMode : CAMDRV_SetWBMode_Pri,
    DRV_SetAntiBanding : CAMDRV_SetAntiBanding_Pri,
    DRV_SetFocusMode : CAMDRV_SetFocusMode_Pri,
    DRV_SetDigitalEffect : CAMDRV_SetDigitalEffect_Pri,
    DRV_SetFlashMode : CAMDRV_SetFlashMode_Pri,
    DRV_SetJpegQuality : CAMDRV_SetJpegQuality_Pri,
    DRV_TurnOnAF : CAMDRV_TurnOnAF_Pri,
    DRV_TurnOffAF : CAMDRV_TurnOffAF_Pri,
    DRV_SetExposure : CAMDRV_SetExposure_Pri,
};

struct sens_methods *CAMDRV_primary_get(void)
{
	return &sens_meth_pri;
}
