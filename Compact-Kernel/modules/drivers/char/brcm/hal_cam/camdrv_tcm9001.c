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
*   @file   camdrv_tcm9001.c
*
*   @brief  This file is the lower level driver API of tcm9001 sensor.
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

#define TCM9001_ID 0x4810

#define HAL_PRIMARY_CAM_RESET 56  /*it should be low when using second sensor*/

#define HAL_Second_CAM_RESET 10 
#define HAL_Second_CAM_StandBy 9

typedef struct{
	UInt8 SubAddr;
	UInt8 Value;
}TCM9001Reg_t;


/*---------Sensor Power On */
static CamSensorIntfCntrl_st_t CamPowerOnSeq[] = 
{
	{PAUSE, 1, Nop_Cmd},	

	#if 0
	{GPIO_CNTRL, HAL_CAM_VGA_STANDBY, GPIO_SetHigh},
	{PAUSE, 10, Nop_Cmd},/*for vga camera shutdown*/
	
	{GPIO_CNTRL, HAL_CAM_VGA_RESET, GPIO_SetHigh},
	{PAUSE, 10, Nop_Cmd}, /*for vga camera shutdown*/
	#endif
	
	{GPIO_CNTRL, HAL_PRIMARY_CAM_RESET, GPIO_SetLow},  
	{PAUSE, 10, Nop_Cmd},

	/* -------Turn everything OFF   */
	{GPIO_CNTRL, HAL_Second_CAM_RESET, GPIO_SetLow},	
	{GPIO_CNTRL, HAL_Second_CAM_StandBy, GPIO_SetHigh},
	{MCLK_CNTRL, CamDrv_NO_CLK, CLK_TurnOff},

	/* -------Enable Clock to Cameras @ Main clock speed*/
	{GPIO_CNTRL, HAL_Second_CAM_StandBy, GPIO_SetLow},
	{PAUSE, 5, Nop_Cmd},
	
	{MCLK_CNTRL, CamDrv_24MHz, CLK_TurnOn},
	{PAUSE, 5, Nop_Cmd},

	/* -------Raise Reset to ISP*/
	{GPIO_CNTRL, HAL_Second_CAM_RESET, GPIO_SetHigh},	
	{PAUSE, 10, Nop_Cmd}

};

/*---------Sensor Power Off*/
static CamSensorIntfCntrl_st_t CamPowerOffSeq[] = 
{
	/* No Hardware Standby available. */
	{PAUSE, 50, Nop_Cmd},
/* -------Lower Reset to ISP*/
	{GPIO_CNTRL, HAL_Second_CAM_RESET, GPIO_SetLow},
	
	{GPIO_CNTRL, HAL_Second_CAM_StandBy, GPIO_SetHigh},
/* -------Disable Clock to Cameras*/
	{MCLK_CNTRL, CamDrv_NO_CLK, CLK_TurnOff},
/*--let primary camera enter soft standby mode , reset should be high-- */
	{GPIO_CNTRL, HAL_PRIMARY_CAM_RESET, GPIO_SetHigh},  
};

/** Secondary Sensor Configuration and Capabilities  */
static HAL_CAM_ConfigCaps_st_t CamSecondaryCfgCap_st = {
	// CAMDRV_DATA_MODE_S *video_mode
	{
        640,                           // unsigned short        max_width;                //Maximum width resolution
        480,                           // unsigned short        max_height;                //Maximum height resolution
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

        640,                           // unsigned short max_width;   Maximum width resolution
        480,                           // unsigned short max_height;  Maximum height resolution
        0,                              // UInt32                data_size;                //Minimum amount of data sent by the camera
        15,                             // UInt32                framerate_lo_absolute;  //Minimum possible framerate u24.8 format
        15,                             // UInt32                framerate_hi_absolute;  //Maximum possible framerate u24.8 format
        CAMDRV_TRANSFORM_NONE,          // CAMDRV_TRANSFORM_T    transform;            //Possible transformations in this mode / user requested transformations
        CAMDRV_IMAGE_YUV422,              // CAMDRV_IMAGE_TYPE_T    format;                //Image format of the frame.
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
	"TCM9001"
};

/*---------Sensor Secondary Configuration CCIR656*/
static CamIntfConfig_CCIR656_st_t CamSecondaryCfg_CCIR656_st = {
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

CamIntfConfig_CCP_CSI_st_t  CamSecondaryCfg_CCP_CSI_st =
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

/*---------Sensor Secondary Configuration YCbCr Input*/
static CamIntfConfig_YCbCr_st_t CamSecondaryCfg_YCbCr_st = {
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

/*---------Sensor Secondary Configuration I2C */
static CamIntfConfig_I2C_st_t CamSecondaryCfg_I2C_st = {
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

/*---------Sensor Secondary Configuration IOCR */
static CamIntfConfig_IOCR_st_t CamSecondaryCfg_IOCR_st = {
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

/*---------Sensor Secondary Configuration */
static CamIntfConfig_st_t CamSensorCfg_st = {
	&CamSecondaryCfgCap_st,	/* *sensor_config_caps; */
	&CamSecondaryCfg_CCIR656_st,	/* *sensor_config_ccir656; */
	&CamSecondaryCfg_CCP_CSI_st,
	&CamSecondaryCfg_YCbCr_st,	/* *sensor_config_ycbcr; */
	NULL,	                     /* *sensor_config_i2c; */
	&CamSecondaryCfg_IOCR_st,	/* *sensor_config_iocr; */
	NULL,	/* *sensor_config_jpeg; */
	NULL,			/* *sensor_config_interleave_video; */
	NULL,	/**sensor_config_interleave_stills; */
	NULL	/* *sensor_config_pkt_marker_info; */
};

static TCM9001Reg_t sSensor_init_st[]=
{
	{0x00,0x48},  
	{0x01,0x10},  
	{0x02,0xD8},  //alcint_sekiout[7:0];
	{0x03,0x00},  //alc_ac5060out/alc_les_modeout[2:0]/*/*/alcint_sekiout[9:;
	{0x04,0x20},  //alc_agout[7:0];
	{0x05,0x00},  //alc_agout[11:8];
	{0x06,0x2D},  //alc_dgout[7:0];
	{0x07,0x9D},  //alc_esout[7:0];
	{0x08,0x80},  //alc_okout//alc_esout[13:8];
	{0x09,0x45},  //awb_uiout[7:0];
	{0x0A,0x17},  //awb_uiout[15:8];
	{0x0B,0x05},  //awb_uiout[23:16];
	{0x0C,0x00},  //awb_uiout[29:24];
	{0x0D,0x39},  //awb_viout[7:0];
	{0x0E,0x56},  //awb_viout[15:8];
	{0x0F,0xFC},  //awb_viout[2316];
	{0x10,0x3F},  //awb_viout[29:24];
	{0x11,0xA1},  //awb_pixout[7:0];
	{0x12,0x28},  //awb_pixout[15:8];
	{0x13,0x03},  //awb_pixout[18:16];
	{0x14,0x85},  //awb_rgout[7:0];
	{0x15,0x80},  //awb_ggout[7:0];
	{0x16,0x6B},  //awb_bgout[7:0];
	{0x17,0x00},  
	{0x18,0x9C},  //LSTB
	{0x19,0x04},  //TSEL[2:0];
	{0x1A,0x90},  
	{0x1B,0x02},  //CKREF_DIV[4:0];
	{0x1C,0x00},  //CKVAR_SS0DIV[7:0];
	{0x1D,0x00},  // */SPCK_SEL/*/EXTCLK_THROUGH/*/*/*/CKVAR_SS0DIV[8];
	{0x1E,0x9E},  //CKVAR_SS1DIV[7:0];
	{0x1F,0x00},  //MRCK_DIV[3:0]/*/*/*/CKVAR_SS1DIV[8];
	{0x20,0xC0},  //VCO_DIV[1:0]/*/CLK_SEL[1:0]/AMON0SEL[1:0]/*;
	{0x21,0x0B},  
	{0x22,0x47},  //TBINV/RLINV//WIN_MODE//HV_INTERMIT[2:0];  
	{0x23,0x96},  //H_COUNT[7:0];
	{0x24,0x00},  
	{0x25,0x42},  //V_COUNT[7:0];
	{0x26,0x00},  //V_COUNT[10:8];
	{0x27,0x00},  
	{0x28,0x00},  
	{0x29,0x83},  
	{0x2A,0x84},  
	{0x2B,0xAE},  
	{0x2C,0x21},  
	{0x2D,0x00},  
	{0x2E,0x04},  
	{0x2F,0x7D},  
	{0x30,0x19},  
	{0x31,0x88},  
	{0x32,0x88},  
	{0x33,0x09},  
	{0x34,0x6C},  
	{0x35,0x00},  
	{0x36,0x90},  
	{0x37,0x22},  
	{0x38,0x0B},  
	{0x39,0xAA},  
	{0x3A,0x0A},  
	{0x3B,0x84},  
	{0x3C,0x03},  
	{0x3D,0x10},  
	{0x3E,0x4C},  
	{0x3F,0x1D},  
	{0x40,0x34},  
	{0x41,0x05},  
	{0x42,0x12},  
	{0x43,0xB0},  
	{0x44,0x3F},  
	{0x45,0xFF},  
	{0x46,0x44},  
	{0x47,0x44},  
	{0x48,0x00},  
	{0x49,0xE8},  
	{0x4A,0x00},  
	{0x4B,0x9F},  
	{0x4C,0x9B},  
	{0x4D,0x2B},  
	{0x4E,0x53},  
	{0x4F,0x50},  
	{0x50,0x0E},  
	{0x51,0x00},  
	{0x52,0x00},  
	{0x53,0x04},  //TP_MODE[4:0]/TPG_DR_SEL/TPG_CBLK_SW/TPG_LINE_SW;
	{0x54,0x08},  
	{0x55,0x14},  
	{0x56,0x84},  
	{0x57,0x30},  
	{0x58,0x80},  
	{0x59,0x80},  
	{0x5A,0x00},  
	{0x5B,0x06},  
	{0x5C,0xF0},  
	{0x5D,0x00},  
	{0x5E,0x00},  
	{0x5F,0xB0},  
	{0x60,0x00},  
	{0x61,0x1B},  
	{0x62,0x4F},  //HYUKO_START[7:0];
	{0x63,0x04},  //VYUKO_START[7:0];
	{0x64,0x10},  
	{0x65,0x20},  
	{0x66,0x30},  
	{0x67,0x28},  
	{0x68,0x66},  
	{0x69,0xC0},  
	{0x6A,0x30},  
	{0x6B,0x30},  
	{0x6C,0x3F},  
	{0x6D,0xBF},  
	{0x6E,0xAB},  
	{0x6F,0x30},  
	{0x70,0x80},  //AGMIN_BLACK_ADJ[7:0];
	{0x71,0x90},  //AGMAX_BLACK_ADJ[7:0];
	{0x72,0x00},  //IDR_SET[7:0];
	{0x73,0x28},  //PWB_RG[7:0];
	{0x74,0x00},  //PWB_GRG[7:0];
	{0x75,0x00},  //PWB_GBG[7:0];
	{0x76,0x58},  //PWB_BG[7:0];
	{0x77,0x00},  
	{0x78,0x80},  //LSSC_SW
	{0x79,0x52},  
	{0x7A,0x4F},  
	{0x7B,0x90},  //LSSC_LEFT_RG[7:0];
	{0x7C,0x4D},  //LSSC_LEFT_GG[7:0];
	{0x7D,0x44},  //LSSC_LEFT_BG[7:0];
	{0x7E,0xC3},  //LSSC_RIGHT_RG[7:0];
	{0x7F,0x77},  //LSSC_RIGHT_GG[7:0];
	{0x80,0x67},  //LSSC_RIGHT_BG[7:0];
	{0x81,0x6D},  //LSSC_TOP_RG[7:0];
	{0x82,0x50},  //LSSC_TOP_GG[7:0];
	{0x83,0x3C},  //LSSC_TOP_BG[7:0];
	{0x84,0x78},  //LSSC_BOTTOM_RG[7:0];
	{0x85,0x4B},  //LSSC_BOTTOM_GG[7:0];
	{0x86,0x31},  //LSSC_BOTTOM_BG[7:0];
	{0x87,0x01},  
	{0x88,0x00},  
	{0x89,0x00},  
	{0x8A,0x40},  
	{0x8B,0x09},  
	{0x8C,0xE0},  
	{0x8D,0x20},  
	{0x8E,0x20},  
	{0x8F,0x20},  
	{0x90,0x20},  
	{0x91,0x80},  //ANR_SW/*/*/*/TEST_ANR/*/*/*;
	{0x92,0x30},  //AGMIN_ANR_WIDTH[7:0];
	{0x93,0x40},  //AGMAX_ANR_WIDTH[7:0];
	{0x94,0x40},  //AGMIN_ANR_MP[7:0];
	{0x95,0x80},  //AGMAX_ANR_MP[7:0];
	{0x96,0x80},  //DTL_SW/*/*/*/*/*/*/*/;
	{0x97,0x20},  //AGMIN_HDTL_NC[7:0];
	{0x98,0x68},  //AGMIN_VDTL_NC[7:0];
	{0x99,0xFF},  //AGMAX_HDTL_NC[7:0];
	{0x9A,0xFF},  //AGMAX_VDTL_NC[7:0];
	{0x9B,0x5C},  //AGMIN_HDTL_MG[7:0];
	{0x9C,0x28},  //AGMIN_HDTL_PG[7:0];
	{0x9D,0x40},  //AGMIN_VDTL_MG[7:0];
	{0x9E,0x28},  //AGMIN_VDTL_PG[7:0];
	{0x9F,0x00},  //AGMAX_HDTL_MG[7:0];
	{0xA0,0x00},  //AGMAX_HDTL_PG[7:0];
	{0xA1,0x00},  //AGMAX_VDTL_MG[7:0];
	{0xA2,0x00},  //AGMAX_VDTL_PG[7:0];
	{0xA3,0x80},  
	{0xA4,0x82},  //HCBC_SW
	{0xA5,0x38},  //AGMIN_HCBC_G[7:0];
	{0xA6,0x18},  //AGMAX_HCBC_G[7:0];
	{0xA7,0x98},  
	{0xA8,0x98},  
	{0xA9,0x98},  
	{0xAA,0x10},  //LMCC_BMG_SEL/LMCC_BMR_SEL/*/LMCC_GMB_SEL/LMCC_GMR_SEL/*/;
	{0xAB,0x5B},  //LMCC_RMG_G[7:0];
	{0xAC,0x00},  //LMCC_RMB_G[7:0];
	{0xAD,0x00},  //LMCC_GMR_G[7:0];
	{0xAE,0x00},  //LMCC_GMB_G[7:0];
	{0xAF,0x00},  //LMCC_BMR_G[7:0];
	{0xB0,0x48},  //LMCC_BMG_G[7:0];
	{0xB1,0x82},  //GAM_SW[1:0]/*/CGC_DISP/TEST_AWBDISP/*/YUVM_AWBDISP_SW/YU;
	{0xB2,0x4D},  //R_MATRIX[6:0];
	{0xB3,0x10},  //B_MATRIX[6:0];
	{0xB4,0xC8},  //UVG_SEL/BRIGHT_SEL/*/TEST_YUVM_PE/NEG_YMIN_SW/PIC_EFFECT;
	{0xB5,0x5B},  //CONTRAST[7:0];
	{0xB6,0x4C},  //BRIGHT[7:0];
	{0xB7,0x00},  //Y_MIN[7:0];
	{0xB8,0xFF},  //Y_MAX[7:0];
	{0xB9,0x69},  //U_GAIN[7:0];
	{0xBA,0x72},  //V_GAIN[7:0];
	{0xBB,0x78},  //SEPIA_US[7:0];
	{0xBC,0x90},  //SEPIA_VS[7:0];
	{0xBD,0x04},  //U_CORING[7:0];
	{0xBE,0x04},  //V_CORING[7:0];
	{0xBF,0xC0},  
	{0xC0,0x00},  
	{0xC1,0x00},  
	{0xC2,0x80},  //ALC_SW/ALC_LOCK
	{0xC3,0x14},  //MES[7:0];
	{0xC4,0x03},  //MES[13:8];
	{0xC5,0x00},  //MDG[7:0];
	{0xC6,0x74},  //MAG[7:0];
	{0xC7,0x80},  //AGCONT_SEL[1:0]
	{0xC8,0x10},  //AG_MIN[7:0];
	{0xC9,0x06},  //AG_MAX[7:0];
	{0xCA,0x97},  //AUTO_LES_SW/AUTO_LES_MODE[2:0]/ALC_WEIGHT[1:0]/FLCKADJ[1;
	{0xCB,0xD2},  //ALC_LV[7:0];
	{0xCC,0x10},  //UPDN_MODE[2:0]/ALC_LV[9:8];
	{0xCD,0x0A},  //ALC_LVW[7:0];
	{0xCE,0x63},  //L64P600S[7:0];
	{0xCF,0x06},  //ALC_VWAIT[2:0]/L64P600S[11:8];
	{0xD0,0x80},  //UPDN_SPD[7:0];
	{0xD1,0x20},  
	{0xD2,0x80},  //NEAR_SPD[7:0];
	{0xD3,0x30},  
	{0xD4,0x8A},  //AC5060/*/ALC_SAFETY[5:0];
	{0xD5,0x02},  //*/*/*/*/*/ALC_SAFETY2[2:0];
	{0xD6,0x4F},  
	{0xD7,0x08},  
	{0xD8,0x00},  
	{0xD9,0xFF},  
	{0xDA,0x01},  
	{0xDB,0x00},  
	{0xDC,0x14},  
	{0xDD,0x00},  
	{0xDE,0x80},  //AWB_SW/AWB_LOCK/*/AWB_TEST/*/*/HEXG_SLOPE_SEL/COLGATE_SE;
	{0xDF,0x80},  //WB_MRG[7:0];
	{0xE0,0x80},  //WB_MGG[7:0];
	{0xE1,0x80},  //WB_MBG[7:0];
	{0xE2,0x22},  //WB_RBMIN[7:0];
	{0xE3,0xF8},  //WB_RBMAX[7:0];
	{0xE4,0x80},  //HEXA_SW/*/COLGATE_RANGE[1:0]/*/*/*/COLGATE_OPEN;
	{0xE5,0x2C},  //RYCUTM[6:0];
	{0xE6,0x54},  //RYCUTP[6:0];
	{0xE7,0x28},  //BYCUTM[6:0];
	{0xE8,0x2F},  //BYCUTP[6:0];
	{0xE9,0xE4},  //RBCUTL[7:0];
	{0xEA,0x3C},  //RBCUTH[7:0];
	{0xEB,0x81},  //SQ1_SW/SQ1_POL/*/*/*/*/*/YGATE_SW;
	{0xEC,0x37},  //RYCUT1L[7:0];
	{0xED,0x5A},  //RYCUT1H[7:0];
	{0xEE,0xDE},  //BYCUT1L[7:0];
	{0xEF,0x08},  //BYCUT1H[7:0];
	{0xF0,0x18},  //YGATE_L[7:0];
	{0xF1,0xFE},  //YGATE_H[7:0];
	{0xF2,0x00},  //IPIX_DISP_SW
	{0xF3,0x02},  
	{0xF4,0x02},  
	{0xF5,0x04},  //AWB_WAIT[7:0];
	{0xF6,0x00},  //AWB_SPDDLY[7:0];
	{0xF7,0x20},  //*/*/AWB_SPD[5:0];
	{0xF8,0x86},  
	{0xF9,0x00},  
	{0xFA,0x41},  //MR_HBLK_START[7:0];
	{0xFB,0x50},  //MR_HBLK_WIDTH[6:0]; /---
	{0xFC,0x0C},  //MR_VBLK_START[7:0];
	{0xFD,0x3C},  //MR_VBLK_WIDTH[5:0];/--------
	{0xFE,0x50},  //PIC_FORMAT[3:0]/PINTEST_SEL[3:0];
	{0xFF,0x85},  //SLEEP/*/PARALLEL_OUTSW[1:0]/DCLK_POL/DOUT_CBLK_SW/*/AL;
};

/* I2C transaction result */
static HAL_CAM_Result_en_t sCamI2cStatus = HAL_CAM_SUCCESS;

static HAL_CAM_Result_en_t
SensorSetPreviewMode(CamImageSize_t image_resolution, CamDataFmt_t image_format);

static HAL_CAM_Result_en_t Init_TCM9001(CamSensorSelect_t sensor);
static int checkCameraID(CamSensorSelect_t sensor);
static UInt8 tcm9001_read(unsigned int sub_addr);
static HAL_CAM_Result_en_t tcm9001_write(unsigned int sub_addr, UInt8 data);


/*****************************************************************************
*
* Function Name:   CAMDRV_GetIntfConfigTbl
*
* Description: Return Camera Sensor Interface Configuration
*
* Notes:
*
*****************************************************************************/
static CamIntfConfig_st_t *CAMDRV_GetIntfConfig_Sec(CamSensorSelect_t nSensor)
{

/* ---------Default to no configuration Table */
	CamIntfConfig_st_t *config_tbl = NULL;
 	pr_err("tcm9001 CAMDRV_GetIntfConfig ");

	switch (nSensor) {
	case CamSensorPrimary:	/* Primary Sensor Configuration */
	default:
		CamSensorCfg_st.sensor_config_caps = NULL;
		break;
	case CamSensorSecondary:	/* Secondary Sensor Configuration */
		CamSensorCfg_st.sensor_config_caps = &CamSecondaryCfgCap_st;
		break;
	}
	config_tbl = &CamSensorCfg_st;

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
static CamSensorIntfCntrl_st_t *CAMDRV_GetIntfSeqSel_Sec(CamSensorSelect_t nSensor,
					      CamSensorSeqSel_t nSeqSel,
					      UInt32 *pLength)
{

/* ---------Default to no Sequence  */
	CamSensorIntfCntrl_st_t *power_seq = NULL;
	*pLength = 0;
 	pr_err("tcm9001 CAMDRV_WakeupCAMDRV_Wakeup nSeqSel:%d",nSeqSel);

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
static HAL_CAM_Result_en_t CAMDRV_Wakeup_Sec(CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
 	pr_err("tcm9001 CAMDRV_Wakeup  ");

	result = Init_TCM9001(sensor);
	printk("Init_TCM9001 result =%d\r\n", result);
	return result;
}

static UInt16 CAMDRV_GetDeviceID_Sec(CamSensorSelect_t sensor)
{
	return ( (tcm9001_read(0x00)<<8 )|(tcm9001_read(0x01)) );
}

static int checkCameraID(CamSensorSelect_t sensor)
{
	UInt16 devId = CAMDRV_GetDeviceID_Sec(sensor);

	if (devId == TCM9001_ID) {
		printk("Camera identified as TCM9001\r\n");
		return 0;
	} else {
		printk("Camera Id wrong. Expected 0x%x but got 0x%x\r\n",
			 TCM9001_ID, devId);
		return -1;
	}
}

/*this one we should modufi it*/
static HAL_CAM_Result_en_t tcm9001_write(unsigned int sub_addr, UInt8 data)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	sCamI2cStatus = HAL_CAM_SUCCESS;
	UInt8 write_data=data;
	

	result |= CAM_WriteI2c_Byte((UInt8)sub_addr, 1, &write_data);
	if (result != HAL_CAM_SUCCESS) {
		sCamI2cStatus = result;
		pr_info("mt9t111_write(): ERROR: at addr:0x%x with value: 0x%x\n", sub_addr, data);
	}
	return result;
}

static UInt8 tcm9001_read(unsigned int sub_addr)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	sCamI2cStatus = HAL_CAM_SUCCESS;
	UInt8 data=0xFF;
	
	result |= CAM_ReadI2c_Byte((UInt8)sub_addr, 1,  &data);
	if (result != HAL_CAM_SUCCESS) {
		sCamI2cStatus = result;
		pr_info("mt9t111_read(): ERROR: %d\r\n", result);
	}

	return data;
}

static HAL_CAM_Result_en_t
SensorSetPreviewMode(CamImageSize_t image_resolution, CamDataFmt_t image_format)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
 	pr_err("tcm9001 SensorSetPreviewMode image_resolution 0x%x image_format %d  ",image_resolution,image_format  );

	if(image_resolution == CamImageSize_QVGA)
		tcm9001_write(0x22, 0x43);
	else 	if(image_resolution == CamImageSize_VGA)
		tcm9001_write(0x22, 0x47);

	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk
		    ("SensorSetPreviewMode(): Error[%d] sending preview mode  r\n",
		     sCamI2cStatus);
		result = sCamI2cStatus;
	}
	
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
static HAL_CAM_Result_en_t CAMDRV_SetVideoCaptureMode_Sec(CamImageSize_t image_resolution,
					       CamDataFmt_t image_format,
					       CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_SetVideoCaptureMode \r\n");

	/* --------Set up Camera Isp for Output Resolution & Format */
	result = SensorSetPreviewMode(image_resolution, image_format);
	return result;
}

/****************************************************************************
*
* Function Name:   HAL_CAM_Result_en_t CAMDRV_SetExposure_Sec(CamRates_t fps)
*
* Description: This function sets the Exposure compensation
*
* Notes:   
*
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetExposure_Sec(int value)
{ 
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	
	printk("tcm9001  CAMDRV_SetExposure_Sec%d \r\n",value );
        if(value == 0)
		tcm9001_write(0xB6, 0x4C);
	else if(value > 0)
		tcm9001_write(0xB6, 0x5C);
	else
		tcm9001_write(0xB6, 0x3C);
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
static HAL_CAM_Result_en_t CAMDRV_SetFrameRate_Sec(CamRates_t fps,
					CamSensorSelect_t sensor)
{ 
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;

	pr_err("tcm9001 CAMDRV_SetFrameRate %d \r\n",fps );
 
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
			switch(fps)
			{
				case CamRate_5:
				 tcm9001_write(0xCA,0x97);
				 tcm9001_write(0xCE,0x63);
				 tcm9001_write(0xCF,0x06);
				 tcm9001_write(0x1E,0x18);				 
				
				break;
				
				case CamRate_10:             
				 tcm9001_write(0xCA,0x97);
				 tcm9001_write(0xCE,0x63);
				 tcm9001_write(0xCF,0x06);
				 tcm9001_write(0x1E,0x30);				 

				break;
				
				case CamRate_15:            // 15 fps
				 tcm9001_write(0xCA,0x97);
				 tcm9001_write(0xCE,0x63);
				 tcm9001_write(0xCF,0x06);
				 tcm9001_write(0x1E,0x48);				 
				
				break;
				
				case CamRate_20:             
				 tcm9001_write(0xCA,0x97);
				 tcm9001_write(0xCE,0x63);
				 tcm9001_write(0xCF,0x06);
				 tcm9001_write(0x1E,0x60);				 
				
				break;
				
				case CamRate_25:           // 25 fps
				 tcm9001_write(0xCA,0x97);
				 tcm9001_write(0xCE,0x63);
				 tcm9001_write(0xCF,0x06);
				 tcm9001_write(0x1E,0x76);				 
			
				break;
				
				case CamRate_30:           // 30 fps
				 tcm9001_write(0xCA,0x97);
				 tcm9001_write(0xCE,0x63);
				 tcm9001_write(0xCF,0x06);
				 tcm9001_write(0x1E,0x8E);
				
				break;
				
				default:                                        // Maximum Clock Rate = 26Mhz
				
				result = HAL_CAM_ERROR_ACTION_NOT_SUPPORTED;
				 printk("CAMDRV_SetFrameRate(): Error HAL_CAM_ERROR_ACTION_NOT_SUPPORTED \r\n");
				break;
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
/ Notes:
/
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_EnableVideoCapture_Sec(CamSensorSelect_t sensor)
{
	/*[Enable stream] */
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_EnableVideoCapture \r\n");
	tcm9001_write(0xC2, 0x80);
	tcm9001_write(0xDE, 0x80);
	
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		 printk("CAMDRV_SetFrameRate(): Error[%d] \r\n",
			  sCamI2cStatus);
		 result = sCamI2cStatus;
        }
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
static HAL_CAM_Result_en_t CAMDRV_SetCamSleep_Sec(CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;

	pr_err("tcm9001 CAMDRV_SetCamSleep \r\n");

	/* To be implemented. */
	return result;
}

static HAL_CAM_Result_en_t CAMDRV_DisableCapture_Sec(CamSensorSelect_t sensor)
{
	/*[Disable stream] */
	pr_err("tcm9001 CAMDRV_DisableCapture \r\n");

	return sCamI2cStatus;
}

/****************************************************************************
/
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_DisablePreview(void)
/
/ Description: This function halts MT9M111 camera video
/
/ Notes:
/
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_DisablePreview_Sec(CamSensorSelect_t sensor)
{
	pr_err("tcm9001 CAMDRV_DisablePreview \r\n");
	
	return sCamI2cStatus;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetSceneMode(
/					CamSceneMode_t scene_mode)
/
/ Description: This function will set the scene mode of camera
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetSceneMode_Sec(CamSceneMode_t scene_mode,
					CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;

	pr_err("tcm9001 CAMDRV_SetSceneMode \r\n");

	switch(scene_mode) {
		case CamSceneMode_Auto:
			pr_info("CAMDRV_SetSceneMode() called for AUTO\n");
			
			break;
		case CamSceneMode_Night:
			pr_info("CAMDRV_SetSceneMode() called for Night\n");
			
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

static HAL_CAM_Result_en_t CAMDRV_SetWBMode_Sec(CamWB_WBMode_t wb_mode,
				     CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_SetWBMode wb_mode:%d \r\n", wb_mode);
	if (wb_mode == CamWB_Auto) {
		tcm9001_write(0xDE,0x80);
		tcm9001_write(0x73,0x28);
		tcm9001_write(0x74,0x00);
		tcm9001_write(0x75,0x00);
		tcm9001_write(0x76,0x58);
	} else if (wb_mode == CamWB_Incandescent) {
	
	} else if (wb_mode == CamWB_DaylightFluorescent)
	{
		tcm9001_write(0xDE,0x80);
		tcm9001_write(0x73,0x43);
		tcm9001_write(0x74,0x00);
		tcm9001_write(0x75,0x00);
		tcm9001_write(0x76,0x4B);
	} else if (wb_mode == CamWB_WarmFluorescent)
	{
		tcm9001_write(0xDE,0x80);
		tcm9001_write(0x73,0x50);
		tcm9001_write(0x74,0x00);
		tcm9001_write(0x75,0x00);
		tcm9001_write(0x76,0x30);
	} else if (wb_mode == CamWB_Daylight) {
		tcm9001_write(0xDE,0x00);
		tcm9001_write(0x73,0x60);
		tcm9001_write(0x74,0x00);
		tcm9001_write(0x75,0x00);
		tcm9001_write(0x76,0x14);
	} else if (wb_mode == CamWB_Cloudy) {
		tcm9001_write(0xDE,0x00);
		tcm9001_write(0x73,0x80);
		tcm9001_write(0x74,0x00);
		tcm9001_write(0x75,0x00);
		tcm9001_write(0x76,0x0D);
	} else if (wb_mode == CamWB_Twilight) {

		
	} else {
		printk("Am here in wb:%d\n", wb_mode);
		;
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

static HAL_CAM_Result_en_t CAMDRV_SetAntiBanding_Sec(CamAntiBanding_t effect,
					  CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_SetAntiBanding effect:%d \r\n", effect);
	if ((effect == CamAntiBandingAuto) || (effect == CamAntiBandingOff)) 
	{
		tcm9001_write(0xD7,0x08);

	} else if (effect == CamAntiBanding50Hz)
	{
		tcm9001_write(0xD7,0x00);
		tcm9001_write(0xD4,0x0A);		
	} else if (effect == CamAntiBanding60Hz) 
	{
		tcm9001_write(0xD7,0x00);
		tcm9001_write(0xD7,0x8A);
	} else 
	{
		
	}

	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetAntiBanding(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}

	return HAL_CAM_SUCCESS;

	//return result;
}

/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetFlashMode(
					FlashLedState_t effect)
/
/ Description: This function will set the flash mode of camera
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetFlashMode_Sec(FlashLedState_t effect,
					CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_SetFlashMode \r\n");
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
/ Description: This function will set the focus mode of camera, we not support it
/ Notes:
****************************************************************************/

static HAL_CAM_Result_en_t CAMDRV_SetFocusMode_Sec(CamFocusControlMode_t effect,
					CamSensorSelect_t sensor)
{//return HAL_CAM_SUCCESS;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_SetFocusMode \r\n");
	if (effect == CamFocusControlAuto) {


	} else if (effect == CamFocusControlMacro) {


	} else if (effect == CamFocusControlInfinity) {

		
	} else {


	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetFocusMode(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}
	return result;
}


static HAL_CAM_Result_en_t CAMDRV_TurnOffAF_Sec()
{
	//return HAL_CAM_SUCCESS;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_TurnOffAF \r\n");
	
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_TurnOffAF(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}
	return result;
}

static HAL_CAM_Result_en_t CAMDRV_TurnOnAF_Sec()
{//return HAL_CAM_SUCCESS;
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_TurnOnAF \r\n");
	/* AF DriverIC power enable */
	pr_info("CAMDRV_TurnOnAF() called\n");
	
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_TurnOnAF(): Error[%d] \r\n",
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
static HAL_CAM_Result_en_t CAMDRV_SetZoom_Sec(CamZoom_t step,
					  CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_SetZoom \r\n");
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


static HAL_CAM_Result_en_t CAMDRV_CfgStillnThumbCapture_Sec(CamImageSize_t
						 stills_resolution,
						 CamDataFmt_t stills_format,
						 CamImageSize_t
						 thumb_resolution,
						 CamDataFmt_t thumb_format,
						 CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
 	pr_err("tcm9001 CAMDRV_CfgStillnThumbCapture stills_resolution 0x%x stills_format %d  ",stills_resolution,stills_format);
        UInt16 aec_ag = 0;
        UInt16 aec_dg = 0;
        UInt16 aec_es = 0;

	if(stills_resolution == CamImageSize_QVGA)
		tcm9001_write(0x22, 0x43);
	else 	if(stills_resolution == CamImageSize_VGA)
		tcm9001_write(0x22, 0x47);
       
	tcm9001_write(0xDE, 0xC0);
	tcm9001_write(0xC2, 0xC0);
	aec_ag = tcm9001_read(0x05);
	aec_ag = (aec_ag<<8)|tcm9001_read(0x04);
              
	aec_dg = tcm9001_read(0x06);
                
	aec_es = tcm9001_read(0x08);
	aec_es = (aec_es<<8)|tcm9001_read(0x07);

	tcm9001_write(0xC2, 0x40);
	tcm9001_write(0xC6, aec_ag&0xFF);
	tcm9001_write(0xC7, (aec_ag>>8)&0xF);
	tcm9001_write(0xC5, aec_dg&0xFF);
	tcm9001_write(0xC3, aec_es&0xFF);
	tcm9001_write(0xC4, (aec_es>>8)&0x3F);
      
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk
		    ("CAMDRV_CfgStillnThumbCapture(): Error[%d] sending preview mode  r\n",
		     sCamI2cStatus);
		result = sCamI2cStatus;
	}
	
	return result;
}


static HAL_CAM_Result_en_t CAMDRV_SetJpegQuality_Sec(CamJpegQuality_t effect,
					  CamSensorSelect_t sensor)
{
	pr_err("tcm9001 CAMDRV_StoreBaseAddress \r\n");
	return HAL_CAM_SUCCESS;
}

/////////////////////////////////////////////////////////////////////////////
/****************************************************************************
/ Function Name:   HAL_CAM_Result_en_t CAMDRV_SetDigitalEffect(
/					CamDigEffect_t effect)
/
/ Description: This function will set the digital effect of camera
/ Notes:
****************************************************************************/
static HAL_CAM_Result_en_t CAMDRV_SetDigitalEffect_Sec(CamDigEffect_t effect,
					    CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	pr_err("tcm9001 CAMDRV_SetDigitalEffect effect:%d \r\n", effect);
	if (effect == CamDigEffect_NoEffect) {
		tcm9001_write(0xB4,0xC8);
	} else if (effect == CamDigEffect_MonoChrome) {
		tcm9001_write(0xB4,0x11);
	} else if ((effect == CamDigEffect_NegColor)
		   || (effect == CamDigEffect_NegMono)) {
		tcm9001_write(0xB4,0x14);
	} else if ((effect == CamDigEffect_SolarizeColor)
		   || (effect == CamDigEffect_SolarizeMono)) {
		tcm9001_write(0xB4,0xC8);
	} else if (effect == CamDigEffect_SepiaGreen) {
		tcm9001_write(0xB4,0x13);
	} else if (effect == CamDigEffect_Auqa) {

	} else {
	
	}
	if (sCamI2cStatus != HAL_CAM_SUCCESS) {
		printk("CAMDRV_SetDigitalEffect(): Error[%d] \r\n",
			 sCamI2cStatus);
		result = sCamI2cStatus;
	}

	return result;
}

ktime_t tm1;
static HAL_CAM_Result_en_t Init_TCM9001(CamSensorSelect_t sensor)
{
	HAL_CAM_Result_en_t result = HAL_CAM_SUCCESS;
	TCM9001Reg_t *reg;
	int timeout;
	tm1 = ktime_get();
	int num,i ;
	pr_err("tcm9001 Init_TCM9001 \r\n");

	num=ARRAY_SIZE(sSensor_init_st);
	
	pr_info("Entry Init Sec %d nsec %d\n", tm1.tv.sec, tm1.tv.nsec);

	CamSensorCfg_st.sensor_config_caps = &CamSecondaryCfgCap_st ;
	printk("Init Secondary Sensor TCM9001: \r\n");
	
	printk(KERN_ERR"-- Mt9t111 ID 0x%x 0x%x \r\n",tcm9001_read(0x00),tcm9001_read(0x01));

	for(i=0;i<num;i++)
	{
		reg=&(sSensor_init_st[i]);
		tcm9001_write(reg->SubAddr, reg->Value);
	}

	if (checkCameraID(sensor)) {
		;//result = HAL_CAM_ERROR_INTERNAL_ERROR;
	}
	
	tm1 = ktime_get();
	printk(KERN_ERR"Exit Init Sec %d nsec %d\n", tm1.tv.sec, tm1.tv.nsec);
}

struct sens_methods sens_meth_sec = {
    DRV_GetIntfConfig: CAMDRV_GetIntfConfig_Sec,
    DRV_GetIntfSeqSel : CAMDRV_GetIntfSeqSel_Sec,
    DRV_Wakeup : CAMDRV_Wakeup_Sec,
    DRV_SetVideoCaptureMode : CAMDRV_SetVideoCaptureMode_Sec,
    DRV_SetFrameRate : CAMDRV_SetFrameRate_Sec,
    DRV_EnableVideoCapture : CAMDRV_EnableVideoCapture_Sec,
    DRV_SetCamSleep : CAMDRV_SetCamSleep_Sec,
    DRV_DisableCapture : CAMDRV_DisableCapture_Sec,
    DRV_DisablePreview : CAMDRV_DisablePreview_Sec,
    DRV_CfgStillnThumbCapture : CAMDRV_CfgStillnThumbCapture_Sec,
    DRV_SetSceneMode : CAMDRV_SetSceneMode_Sec,
    DRV_SetWBMode : CAMDRV_SetWBMode_Sec,
    DRV_SetAntiBanding : CAMDRV_SetAntiBanding_Sec,
    DRV_SetFocusMode : CAMDRV_SetFocusMode_Sec,
    DRV_SetDigitalEffect : CAMDRV_SetDigitalEffect_Sec,
    DRV_SetFlashMode : CAMDRV_SetFlashMode_Sec,
    DRV_SetJpegQuality : CAMDRV_SetJpegQuality_Sec,//dummy
    DRV_TurnOnAF : CAMDRV_TurnOnAF_Sec,
    DRV_TurnOffAF : CAMDRV_TurnOffAF_Sec,
    DRV_SetExposure : CAMDRV_SetExposure_Sec,
};

struct sens_methods *CAMDRV_secondary_get(void)
{
	return &sens_meth_sec;
}

