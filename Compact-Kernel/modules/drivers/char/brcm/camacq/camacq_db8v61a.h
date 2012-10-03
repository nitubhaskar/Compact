/*.......................................................................................................
. COPYRIGHT (C)  SAMSUNG Electronics CO., LTD (Suwon, Korea). 2009           
. All rights are reserved. Reproduction and redistiribution in whole or 
. in part is prohibited without the written consent of the copyright owner.
. 
.   Developer:
.   Date:
.   Description:  
..........................................................................................................
*/

#if !defined(_CAMACQ_DB8V61A_H_)
#define _CAMACQ_DB8V61A_H_

/* Include */
#include "camacq_api.h"

/* Global */
#undef GLOBAL

#if !defined(_CAMACQ_API_C_)
#define GLOBAL extern
#else
#define GLOBAL
#endif /* _CAMACQ_API_C_ */

/* Include */
#if defined(WIN32)
#include "cmmfile.h"
#endif /* WIN32 */

/* Definition */
#define _SR030PC30_    //sensor option

#define CAMACQ_SUB_NAME          "cami2c_sub"//swsw_dual
#define CAMACQ_SUB_I2C_ID       0x2D	                // don't use now.
#define CAMACQ_SUB_RES_TYPE   	 CAMACQ_SENSOR_SUB   // main sensor

#define CAMACQ_SUB_ISPROBED     0
#define CAMACQ_SUB_CLOCK        0               
#define CAMACQ_SUB_YUVORDER     0
#define CAMACQ_SUB_V_SYNCPOL    0
#define CAMACQ_SUB_H_SYNCPOL    0
#define CAMACQ_SUB_SAMPLE_EDGE  0
#define CAMACQ_SUB_FULL_RANGE   0

#define CAMACQ_SUB_RST 
#define CAMACQ_SUB_RST_MUX 
#define CAMACQ_SUB_EN 
#define CAMACQ_SUB_EN_MUX 

#define CAMACQ_SUB_RST_ON          1
#define CAMACQ_SUB_RST_OFF         0
#define CAMACQ_SUB_EN_ON           1
#define CAMACQ_SUB_EN_OFF          0
#define CAMACQ_SUB_STANDBY_ON      1
#define CAMACQ_SUB_STANDBY_OFF	    0

#define CAMACQ_SUB_POWER_CTRL(onoff)
 
#define CAMACQ_SUB_2BYTE_SENSOR    1
#define CAMACQ_SUB_AF              0
#define CAMACQ_SUB_INT_MODE        0   // 0xAABBCCDD is INT_MODE, 0xAA, 0xBB, 0xCC, 0xDD is ARRAY MODE
#define CAMACQ_SUB_FS_MODE         0
#define CAMACQ_SUB_PATH_SET_FILE   "/sdcard/sensor/sub/%s.dat"

#if (CAMACQ_SUB_2BYTE_SENSOR)	
#define CAMACQ_SUB_BURST_MODE 0
#else
#define CAMACQ_SUB_BURST_MODE 0
#endif /* CAMACQ_MAIN2BYTE_SENSOR */

#define CAMACQ_SUB_BURST_I2C_TRACE 0
#define CAMACQ_SUB_BURST_MAX 100

#define CAMACQ_SUB_REG_FLAG_CNTS 	0x0F12
#define CAMACQ_SUB_REG_DELAY 		0xFE                
#define CAMACQ_SUB_BTM_OF_DATA 	{0xFF, 0xFF},       
#define CAMACQ_SUB_END_MARKER 		0xFF                
#define CAMACQ_SUB_REG_SET_SZ 		2 	// {0xFFFFFFFF} is 1, {0xFFFF,0xFFFF} is 2, {0xFF,0XFF} is 2, {0xFF,0xFF,0xFF,0xFF} is 4, {0xFFFF} is 1 
#define CAMACQ_SUB_REG_DAT_SZ 		1   // {0xFFFFFFFF} is 4, {0xFFFF,0xFFFF} is 2, {0xFF,0XFF} is 1, {0xFF,0xFF,0xFF,0xFF} is 1, {0xFFFF} is 2 
#define CAMACQ_SUB_FRATE_MIN  5
#define CAMACQ_SUB_FRATE_MAX  30

// MACRO FUNCTIONS BEGIN //////////////////////////////////////////////////////////// 
#if (CAMACQ_SUB_2BYTE_SENSOR)
#define CAMACQ_SUB_EXT_RD_SZ 1
#else
#define CAMACQ_SUB_EXT_RD_SZ 2
#endif /* CAMACQ_SUB_2BYTE_SENSOR */

#if CAMACQ_SUB_2BYTE_SENSOR
#define CAMACQ_SUB_EXT_REG_IS_BTM_OF_DATA(A)		(((A[0]==CAMACQ_SUB_END_MARKER) && (A[1]==CAMACQ_SUB_END_MARKER))? 1:0)
#define CAMACQ_SUB_EXT_REG_IS_DELAY(A)				(((A[0]==CAMACQ_SUB_REG_DELAY)&&(A[1]==CAMACQ_SUB_REG_DELAY))? 0:0)


#if (CAMACQ_SUB_FS_MODE==1)
#define CAMACQ_SUB_EXT_REG_GET_DATA(dest,srce,idx)\
memcpy(dest, &(srce[idx*CAMACQ_SUB_REG_DAT_SZ*CAMACQ_SUB_REG_SET_SZ]), CAMACQ_SUB_REG_DAT_SZ*CAMACQ_SUB_REG_SET_SZ);
#elif (CAMACQ_SUB_REG_DAT_SZ==1)
#define CAMACQ_SUB_EXT_REG_GET_DATA(dest,srce,idx)	dest[0] = (srce[idx][0] & 0xFF); dest[1] = (srce[idx][1] & 0xFF);
#elif (CAMACQ_SUB_REG_DAT_SZ==2)
#define CAMACQ_SUB_EXT_REG_GET_DATA(dest,srce,idx)	dest[0] = ((U8)(srce[idx] >> 8) & 0xFF); dest[1] = ((U8)(srce[idx]) & 0xFF);
#endif

#else // CAMACQ_SUB_2BYTE_SENSOR

#define CAMACQ_SUB_EXT_REG_IS_BTM_OF_DATA(A)		(((A[0]==CAMACQ_SUB_END_MARKER) && (A[1]==CAMACQ_SUB_END_MARKER) && \
(A[2]==CAMACQ_SUB_END_MARKER) && (A[3]==CAMACQ_SUB_END_MARKER))? 1:0)
#define CAMACQ_SUB_EXT_REG_IS_DELAY(A)				(((A[0]==((CAMACQ_SUB_REG_DELAY>>8) & 0xFF)) && (A[1]==(CAMACQ_SUB_REG_DELAY & 0xFF)))? 1:0)
#define CAMACQ_SUB_EXT_REG_IS_CNTS(A)				(((A[0]==((CAMACQ_SUB_REG_FLAG_CNTS>>8) & 0xFF)) && (A[1]==(CAMACQ_SUB_REG_FLAG_CNTS & 0xFF)))? 1:0)

#if (CAMACQ_SUB_FS_MODE==1)
#define CAMACQ_SUB_EXT_REG_GET_DATA(dest,srce,idx)\
memcpy(dest, &(srce[idx*CAMACQ_SUB_REG_DAT_SZ*CAMACQ_SUB_REG_SET_SZ]), CAMACQ_SUB_REG_DAT_SZ*CAMACQ_SUB_REG_SET_SZ);
#elif (CAMACQ_SUB_REG_DAT_SZ==2)
#define CAMACQ_SUB_EXT_REG_GET_DATA(dest,srce,idx)	dest[0]=((srce[idx][0] >> 8) & 0xFF); dest[1]=(srce[idx][0] & 0xFF); \
dest[2]=((srce[idx][1] >> 8) & 0xFF); dest[3]=(srce[idx][1] & 0xFF);
#elif (CAMACQ_SUB_REG_DAT_SZ==1)
#define CAMACQ_SUB_EXT_REG_GET_DATA(dest,srce,idx)	dest[0]=srce[idx][0]; dest[1]=srce[idx][1]; \
dest[2]=srce[idx][2]; dest[3]=srce[idx][3];
#elif (CAMACQ_SUB_REG_DAT_SZ==4)
#define CAMACQ_SUB_EXT_REG_GET_DATA(dest,srce,idx)	dest[0] = ((U8)(srce[idx] >> 24) & 0xFF); dest[1] = ((U8)(srce[idx] >> 16) & 0xFF); \
dest[2] = ((U8)(srce[idx] >> 8) & 0xFF); dest[3] = ((U8)(srce[idx]) & 0xFF);			
#endif
#endif /* CAMACQ_2BYTE_SENSOR */
// MACRO FUNCTIONS END /////////////////////////////////////////////////////////// 


/* DEFINE for sensor regs*/
#if( CAMACQ_SUB_FS_MODE )
#define CAMACQ_SUB_REG_INIT            "INIT"
#define CAMACQ_SUB_REG_VT_INIT         "VT_INIT"
#define CAMACQ_SUB_REG_PREVIEW         "PREVIEW"
#define CAMACQ_SUB_REG_SNAPSHOT        "SNAPSHOT"
#define CAMACQ_SUB_REG_CAMCORDER        "CAMCORDER"

#define CAMACQ_SUB_REG_WB_AUTO                 "WB_AUTO"
#define CAMACQ_SUB_REG_WB_DAYLIGHT             "WB_DAYLIGHT"
#define CAMACQ_SUB_REG_WB_CLOUDY               "WB_CLOUDY"
#define CAMACQ_SUB_REG_WB_INCANDESCENT         "WB_INCANDESCENT"
#define CAMACQ_SUB_REG_WB_FLUORESCENT         "WB_FLUORESCENT"

#define CAMACQ_SUB_REG_SCENE_AUTO              "SCENE_AUTO" 
#define CAMACQ_SUB_REG_SCENE_NIGHT             "SCENE_NIGHT"
#define CAMACQ_SUB_REG_SCENE_NIGHT_DARK        "SCENE_NIGHT_DARK"
#define CAMACQ_SUB_REG_SCENE_LANDSCAPE         "SCENE_LANDSCAPE"
#define CAMACQ_SUB_REG_SCENE_PORTRAIT          "SCENE_PORTRAIT"
#define CAMACQ_SUB_REG_SCENE_SPORTS            "SCENE_SPORTS"
#define CAMACQ_SUB_REG_SCENE_INDOOR            "SCENE_INDOOR"
#define CAMACQ_SUB_REG_SCENE_SUNSET            "SCENE_SUNSET"
#define CAMACQ_SUB_REG_SCENE_SUNRISE           "SCENE_SUNRISE"    // dawn
#define CAMACQ_SUB_REG_SCENE_BEACH             "SCENE_BEACH"
#define CAMACQ_SUB_REG_SCENE_FALLCOLOR         "SCENE_FALLCOLOR"
#define CAMACQ_SUB_REG_SCENE_FIREWORKS         "SCENE_FIREWORKS"
#define CAMACQ_SUB_REG_SCENE_CANDLELIGHT       "SCENE_CANDLELIGHT"
#define CAMACQ_SUB_REG_SCENE_AGAINSTLIGHT      "SCENE_AGAINSTLIGHT"  // backlight

#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_0      "BRIGHTNESS_LEVEL_0"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_1      "BRIGHTNESS_LEVEL_1"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_2      "BRIGHTNESS_LEVEL_2"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_3      "BRIGHTNESS_LEVEL_3"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_4      "BRIGHTNESS_LEVEL_4"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_5      "BRIGHTNESS_LEVEL_5"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_6      "BRIGHTNESS_LEVEL_6"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_7      "BRIGHTNESS_LEVEL_7"
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_8      "BRIGHTNESS_LEVEL_8"

#define CAMACQ_SUB_REG_METER_MATRIX            "METER_MATRIX"
#define CAMACQ_SUB_REG_METER_CW                "METER_CW"
#define CAMACQ_SUB_REG_METER_SPOT              "METER_SPOT"

#define CAMACQ_SUB_REG_EFFECT_NONE             "EFFECT_NONE"
#define CAMACQ_SUB_REG_EFFECT_GRAY             "EFFECT_GRAY" // mono, blackwhite
#define CAMACQ_SUB_REG_EFFECT_NEGATIVE         "EFFECT_NEGATIVE"
#define CAMACQ_SUB_REG_EFFECT_SEPIA            "EFFECT_SEPIA"

#define CAMACQ_SUB_REG_ADJUST_CONTRAST_M2              "CONTRAST_M2"
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_M1              "CONTRAST_M1"
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_DEFAULT         "CONTRAST_DEFAULT"
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_P1              "CONTRAST_P1"
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_P2              "CONTRAST_P2"

#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_M2             "SHARPNESS_M2"
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_M1             "SHARPNESS_M1"
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_DEFAULT        "SHARPNESS_DEFAULT"
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_P1             "SHARPNESS_P1"
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_P2             "SHARPNESS_P2"

#define CAMACQ_SUB_REG_ADJUST_SATURATION_M2            "SATURATION_M2"
#define CAMACQ_SUB_REG_ADJUST_SATURATION_M1            "SATURATION_M1"
#define CAMACQ_SUB_REG_ADJUST_SATURATION_DEFAULT       "SATURATION_DEFAULT"
#define CAMACQ_SUB_REG_ADJUST_SATURATION_P1            "SATURATION_P1"
#define CAMACQ_SUB_REG_ADJUST_SATURATION_P2            "SATURATION_P2"

#else
#define CAMACQ_SUB_REG_INIT            reg_sub_init
#define CAMACQ_SUB_REG_VT_INIT         reg_sub_vt_init
#define CAMACQ_SUB_REG_PREVIEW         reg_sub_preview
#define CAMACQ_SUB_REG_SNAPSHOT        reg_sub_snapshot
#define CAMACQ_SUB_REG_CAMCORDER      reg_sub_camcorder

#define CAMACQ_SUB_REG_WB_AUTO                 reg_sub_wb_auto
#define CAMACQ_SUB_REG_WB_DAYLIGHT             reg_sub_wb_daylight
#define CAMACQ_SUB_REG_WB_CLOUDY               reg_sub_wb_cloudy
#define CAMACQ_SUB_REG_WB_INCANDESCENT         reg_sub_wb_incandescent
#define CAMACQ_SUB_REG_WB_FLUORESCENT          reg_sub_wb_fluorescent


#define CAMACQ_SUB_REG_SCENE_AUTO              reg_sub_scene_auto  // auto, off
#define CAMACQ_SUB_REG_SCENE_NIGHT             reg_sub_scene_night
#define CAMACQ_SUB_REG_SCENE_NIGHT_DARK        reg_sub_scene_night_dark
#define CAMACQ_SUB_REG_SCENE_LANDSCAPE         reg_sub_scene_landscape
#define CAMACQ_SUB_REG_SCENE_PORTRAIT          reg_sub_scene_portrait
#define CAMACQ_SUB_REG_SCENE_SPORTS            reg_sub_scene_sports
#define CAMACQ_SUB_REG_SCENE_INDOOR            reg_sub_scene_indoor
#define CAMACQ_SUB_REG_SCENE_SUNSET            reg_sub_scene_sunset
#define CAMACQ_SUB_REG_SCENE_SUNRISE           reg_sub_scene_sunrise    // dawn
#define CAMACQ_SUB_REG_SCENE_BEACH             reg_sub_scene_beach
#define CAMACQ_SUB_REG_SCENE_FALLCOLOR         reg_sub_scene_fallcolor
#define CAMACQ_SUB_REG_SCENE_FIREWORKS         reg_sub_scene_fireworks
#define CAMACQ_SUB_REG_SCENE_CANDLELIGHT       reg_sub_scene_candlelight
#define CAMACQ_SUB_REG_SCENE_AGAINSTLIGHT      reg_sub_scene_againstlight  // backlight

#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_0      reg_sub_brightness_level_0
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_1      reg_sub_brightness_level_1
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_2      reg_sub_brightness_level_2
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_3      reg_sub_brightness_level_3
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_4      reg_sub_brightness_level_4
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_5      reg_sub_brightness_level_5
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_6      reg_sub_brightness_level_6
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_7      reg_sub_brightness_level_7
#define CAMACQ_SUB_REG_BRIGHTNESS_LEVEL_8      reg_sub_brightness_level_8

#define CAMACQ_SUB_REG_METER_MATRIX            reg_sub_meter_matrix
#define CAMACQ_SUB_REG_METER_CW                reg_sub_meter_cw
#define CAMACQ_SUB_REG_METER_SPOT              reg_sub_meter_spot

#define CAMACQ_SUB_REG_EFFECT_NONE             reg_sub_effect_none
#define CAMACQ_SUB_REG_EFFECT_GRAY             reg_sub_effect_gray // mono, blackwhite
#define CAMACQ_SUB_REG_EFFECT_NEGATIVE         reg_sub_effect_negative
#define CAMACQ_SUB_REG_EFFECT_SEPIA            reg_sub_effect_sepia

#define CAMACQ_SUB_REG_ADJUST_CONTRAST_M2              reg_sub_adjust_contrast_m2
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_M1              reg_sub_adjust_contrast_m1
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_DEFAULT         reg_sub_adjust_contrast_default
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_P1              reg_sub_adjust_contrast_p1
#define CAMACQ_SUB_REG_ADJUST_CONTRAST_P2              reg_sub_adjust_contrast_p2

#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_M2             reg_sub_adjust_sharpness_m2
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_M1             reg_sub_adjust_sharpness_m1
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_DEFAULT        reg_sub_adjust_sharpness_default
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_P1             reg_sub_adjust_sharpness_p1
#define CAMACQ_SUB_REG_ADJUST_SHARPNESS_P2             reg_sub_adjust_sharpness_p2

#define CAMACQ_SUB_REG_ADJUST_SATURATION_M2            reg_sub_adjust_saturation_m2
#define CAMACQ_SUB_REG_ADJUST_SATURATION_M1            reg_sub_adjust_saturation_m1
#define CAMACQ_SUB_REG_ADJUST_SATURATION_DEFAULT       reg_sub_adjust_saturation_default
#define CAMACQ_SUB_REG_ADJUST_SATURATION_P1            reg_sub_adjust_saturation_p1
#define CAMACQ_SUB_REG_ADJUST_SATURATION_P2            reg_sub_adjust_saturation_p2
#endif


#define CAMACQ_SUB_REG_MIDLIGHT                reg_sub_midlight
#define CAMACQ_SUB_REG_LOWLIGHT                reg_sub_lowlight
#define CAMACQ_SUB_REG_NIGHTSHOT_ON            reg_sub_nightshot_on
#define CAMACQ_SUB_REG_NIGHTSHOT_OFF           reg_sub_nightshot_off
#define CAMACQ_SUB_REG_NIGHTSHOT               reg_sub_nightshot

#define CAMACQ_SUB_REG_WB_TWILIGHT             reg_sub_wb_twilight
#define CAMACQ_SUB_REG_WB_TUNGSTEN             reg_sub_wb_tungsten
#define CAMACQ_SUB_REG_WB_OFF                  reg_sub_wb_off
#define CAMACQ_SUB_REG_WB_HORIZON              reg_sub_wb_horizon
#define CAMACQ_SUB_REG_WB_SHADE                reg_sub_wb_shade

#define CAMACQ_SUB_REG_EFFECT_SOLARIZE         reg_sub_effect_solarize
#define CAMACQ_SUB_REG_EFFECT_POSTERIZE        reg_sub_effect_posterize
#define CAMACQ_SUB_REG_EFFECT_WHITEBOARD       reg_sub_effect_whiteboard
#define CAMACQ_SUB_REG_EFFECT_BLACKBOARD       reg_sub_effect_blackboard
#define CAMACQ_SUB_REG_EFFECT_AQUA             reg_sub_effect_aqua
#define CAMACQ_SUB_REG_EFFECT_SHARPEN          reg_sub_effect_sharpen
#define CAMACQ_SUB_REG_EFFECT_VIVID            reg_sub_effect_vivid
#define CAMACQ_SUB_REG_EFFECT_GREEN            reg_sub_effect_green
#define CAMACQ_SUB_REG_EFFECT_SKETCH           reg_sub_effect_sketch


#define CAMACQ_SUB_REG_FLIP_NONE               reg_sub_flip_none
#define CAMACQ_SUB_REG_FLIP_WATER              reg_sub_flip_water
#define CAMACQ_SUB_REG_FLIP_MIRROR             reg_sub_flip_mirror
#define CAMACQ_SUB_REG_FLIP_WATER_MIRROR       reg_sub_flip_water_mirror


#define CAMACQ_SUB_REG_FPS_FIXED_5             reg_sub_fps_fixed_5
#define CAMACQ_SUB_REG_FPS_FIXED_7             reg_sub_fps_fixed_7
#define CAMACQ_SUB_REG_FPS_FIXED_10            reg_sub_fps_fixed_10
#define CAMACQ_SUB_REG_FPS_FIXED_15            reg_sub_fps_fixed_15
#define CAMACQ_SUB_REG_FPS_FIXED_20            reg_sub_fps_fixed_20
#define CAMACQ_SUB_REG_FPS_FIXED_25            reg_sub_fps_fixed_25
#define CAMACQ_SUB_REG_FPS_VAR_15              reg_sub_fps_var_15
#define CAMACQ_SUB_REG_FPS_FIXED_30            reg_sub_fps_fixed_30


#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_0 reg_sub_expcompensation_level_0
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_1 reg_sub_expcompensation_level_1
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_2 reg_sub_expcompensation_level_2
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_3 reg_sub_expcompensation_level_3
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_4 reg_sub_expcompensation_level_4
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_5 reg_sub_expcompensation_level_5
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_6 reg_sub_expcompensation_level_6
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_7 reg_sub_expcompensation_level_7
#define CAMACQ_SUB_REG_EXPCOMPENSATION_LEVEL_8 reg_sub_expcompensation_level_8

#define CAMACQ_SUB_REG_SET_AF                  reg_sub_set_af  // start af
#define CAMACQ_SUB_REG_OFF_AF                  reg_sub_off_af
#define CAMACQ_SUB_REG_CHECK_AF                reg_sub_check_af
#define CAMACQ_SUB_REG_RESET_AF                reg_sub_reset_af
#define CAMACQ_SUB_REG_MANUAL_AF               reg_sub_manual_af    // normal_af
#define CAMACQ_SUB_REG_MACRO_AF                reg_sub_macro_af
#define CAMACQ_SUB_REG_RETURN_MANUAL_AF        reg_sub_return_manual_af
#define CAMACQ_SUB_REG_RETURN_MACRO_AF         reg_sub_return_macro_af
#define CAMACQ_SUB_REG_SET_AF_NLUX             reg_sub_set_af_nlux
#define CAMACQ_SUB_REG_SET_AF_LLUX             reg_sub_set_af_llux
#define CAMACQ_SUB_REG_SET_AF_NORMAL_MODE_1    reg_sub_set_af_normal_mode_1
#define CAMACQ_SUB_REG_SET_AF_NORMAL_MODE_2    reg_sub_set_af_normal_mode_2
#define CAMACQ_SUB_REG_SET_AF_NORMAL_MODE_3    reg_sub_set_af_normal_mode_3
#define CAMACQ_SUB_REG_SET_AF_MACRO_MODE_1     reg_sub_set_af_macro_mode_1
#define CAMACQ_SUB_REG_SET_AF_MACRO_MODE_2     reg_sub_set_af_macro_mode_2
#define CAMACQ_SUB_REG_SET_AF_MACRO_MODE_3     reg_sub_set_af_macro_mode_3

#define CAMACQ_SUB_REG_ISO_AUTO                reg_sub_iso_auto
#define CAMACQ_SUB_REG_ISO_50                  reg_sub_iso_50
#define CAMACQ_SUB_REG_ISO_100                 reg_sub_iso_100
#define CAMACQ_SUB_REG_ISO_200                 reg_sub_iso_200
#define CAMACQ_SUB_REG_ISO_400                 reg_sub_iso_400
#define CAMACQ_SUB_REG_ISO_800                 reg_sub_iso_800
#define CAMACQ_SUB_REG_ISO_1600                reg_sub_iso_1600
#define CAMACQ_SUB_REG_ISO_3200                reg_sub_iso_3200


#define CAMACQ_SUB_REG_SCENE_PARTY             reg_sub_scene_party
#define CAMACQ_SUB_REG_SCENE_SNOW              reg_sub_scene_snow
#define CAMACQ_SUB_REG_SCENE_TEXT              reg_sub_scene_text

#define CAMACQ_SUB_REG_QQVGA                   reg_sub_qqvga
#define CAMACQ_SUB_REG_QCIF                    reg_sub_qcif
#define CAMACQ_SUB_REG_QVGA                    reg_sub_qvga
#define CAMACQ_SUB_REG_WQVGA                    reg_sub_wqvga
#define CAMACQ_SUB_REG_CIF                     reg_sub_cif
#define CAMACQ_SUB_REG_VGA                     reg_sub_vga
#define CAMACQ_SUB_REG_WVGA                     reg_sub_wvga // 800* 480
#define CAMACQ_SUB_REG_SVGA                    reg_sub_svga
#define CAMACQ_SUB_REG_SXGA                    reg_sub_sxga
#define CAMACQ_SUB_REG_QXGA                    reg_sub_qxga
#define CAMACQ_SUB_REG_UXGA                    reg_sub_uxga
#define CAMACQ_SUB_REG_FULL_YUV                reg_sub_full_yuv
#define CAMACQ_SUB_REG_CROP_YUV                reg_sub_crop_yuv
#define CAMACQ_SUB_REG_QVGA_V		            reg_sub_qvga_v
#define CAMACQ_SUB_REG_HALF_VGA_V	            reg_sub_half_vga_v
#define CAMACQ_SUB_REG_HALF_VGA		        reg_sub_half_vga	
#define CAMACQ_SUB_REG_VGA_V		            reg_sub_vga_v			
#define CAMACQ_SUB_REG_5M			            reg_sub_5M
#define CAMACQ_SUB_REG_1080P		            reg_sub_1080P
#define CAMACQ_SUB_REG_720P			        reg_sub_720P
#define CAMACQ_SUB_REG_NOT                     reg_sub_not

#define CAMACQ_SUB_REG_JPEG_5M                 reg_sub_jpeg_5m        //2560X1920
#define CAMACQ_SUB_REG_JPEG_5M_2               reg_sub_jpeg_5m_2      // 2592X1944
#define CAMACQ_SUB_REG_JPEG_W4M                 reg_sub_jpeg_w4m      // 2560x1536
#define CAMACQ_SUB_REG_JPEG_3M                 reg_sub_jpeg_3m        // QXGA 2048X1536
#define CAMACQ_SUB_REG_JPEG_2M                 reg_sub_jpeg_2m        // UXGA 1600x1200
#define CAMACQ_SUB_REG_JPEG_W1_5M               reg_sub_jpeg_w1_5m    // 1280x960
#define CAMACQ_SUB_REG_JPEG_1M                 reg_sub_jpeg_1m
#define CAMACQ_SUB_REG_JPEG_VGA                reg_sub_jpeg_vga   //640x480
#define CAMACQ_SUB_REG_JPEG_WQVGA              reg_sub_jpeg_wqvga //420x240
#define CAMACQ_SUB_REG_JPEG_QVGA               reg_sub_jpeg_qvga  //320x240

#define CAMACQ_SUB_REG_FLICKER_DISABLED        reg_sub_flicker_disabled
#define CAMACQ_SUB_REG_FLICKER_50HZ            reg_sub_flicker_50hz
#define CAMACQ_SUB_REG_FLICKER_60HZ            reg_sub_flicker_60hz
#define CAMACQ_SUB_REG_FLICKER_AUTO            reg_sub_flicker_auto

// image quality
#define CAMACQ_SUB_REG_JPEG_QUALITY_SUPERFINE reg_sub_jpeg_quality_superfine
#define CAMACQ_SUB_REG_JPEG_QUALITY_FINE      reg_sub_jpeg_quality_fine
#define CAMACQ_SUB_REG_JPEG_QUALITY_NORMAL    reg_sub_jpeg_quality_normal

// Private Control
#define CAMACQ_SUB_REG_PRIVCTRL_RETURNPREVIEW  reg_sub_priv_ctrl_returnpreview

// Format
#define CAMACQ_SUB_REG_FMT_YUV422 	            reg_sub_fmt_yuv422
#define CAMACQ_SUB_REG_FMT_JPG		            reg_sub_fmt_jpg


// NEW
#define CAMACQ_SUB_REG_SLEEP                   reg_sub_sleep
#define CAMACQ_SUB_REG_WAKEUP                  reg_sub_wakeup

/* Enumeration */

/* Global Value */

GLOBAL const U8 reg_sub_sleep[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


GLOBAL const U8 reg_sub_wakeup[][2] 
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_init[][2]
#if defined(_CAMACQ_API_C_)
={
//==================================
// Output Format
//==================================
 
{0xFF,0x81},	// Page Mode
{0x70,0x01},	// Output Format : YUV
{0x71,0x25},	// Output Ctrl : CAMIF_YCbCr_Y_Cb_Y_Cr, PCLK-Invert, HSYNC-Active High, VSYNC-Active Low


//==================================
// SensorCon
//==================================

{0xFF,0xB0},	// Page Mode
{0x02,0x00},	// Sensor Disable
{0x03,0x01},	// Y Flip //bykim temp
{0x12,0x44},	//
{0x14,0x80},	//
{0x17,0xe3},	//
{0x1e,0x01},	//
{0x4e,0x01},	      //
{0x4F,0x9a},	//
{0x67,0x0d},	//
{0x68,0x15},	//
{0x69,0x1f},	//
{0x60,0x02},	// Pad Current : 6mA
{0x02,0x01},	// Sensor Enable

{0xFF,0x61},	// Page Mode
{0xd4,0x00},	// Fgain

//==================================
// ISP Global Control #1
//==================================

{0xFF,0x80},	// Page Mode
{0x00,0xFF},	 // Bayer Function
{0x01,0xFF},	 // RgbYcFunc_Addr
{0x02,0x00},	 // Test Mode


//----------------------------------
// LSC
{0x46,0xA0},	 // RRp0Gain
{0x47,0x30},	 // RRp1Gain
{0x48,0x00},	 // RRp2Gain
{0x49,0x88},	 // GGp0Gain
{0x4A,0x30},	 // GGp1Gain
{0x4B,0x00},	 // GGp2Gain
{0x4C,0x80},	 // BBp0Gain
{0x4D,0x28},	 // BBp1Gain
{0x4E,0x00},	 // BBp2Gain
{0x4F,0xFF},	 // ShdGGain


//----------------------------------
// BCD
{0xD1,0x20},	 // False color threshold


//==================================
// ISP Global Control #2
//==================================

{0xFF,0x81},	// Page Mode

//----------------------------------
// Edge
{0x30,0x55},	 // Edge Option
{0x31,0x00},	 // Edge Luminance adaptive
{0x32,0x02},	 // Edge PreCoring point
{0x33,0x00},	 // Edge x1
{0x34,0x20},	 // Edge x1
{0x35,0x00},	 // Edge x2
{0x36,0xA0},	 // Edge x2
{0x37,0x08},	 // Edge Transfer slope 1
{0x38,0x10},	 // Edge Transfer slope 2
{0x39,0x20},	 // Edge Left Offset
{0x3a,0x20},	 // Edge Right offset



//==================================
// AE
//==================================
{0xFF,0x81},	// Page Mode
{0x91,0x00},	// Point A
{0x92,0x00},	
{0x93,0x35},	// Point B
{0x94,0x18},	
{0x95,0x6B},	// Point C
{0x96,0x60},	
{0x97,0xA0},	// Point D
{0x98,0x78},	

{0xFF,0x61},	// Page Mode
{0xE4,0x01},	 // AE Mode
{0xE5,0x1D},	 // AE Ctrl

{0xE8,0x21},	 // AE Weight (Left / Top)
{0xE9,0x2C},	 // AE Weight (Right / Center)
{0xEA,0x42},	 // AE weight (Factor / Bottom)
{0xEB,0x06},	 // AE Speed
{0xEF,0x08},	 // Analog gain max

{0xFA,0x34},	 // Outdoor AE Target
{0xFB,0x36},	 // Indoor AE Target

{0xFF,0x62},	 // Page Mode

{0x0D,0x2A},	 // Peak AE Target min
{0x0F,0x02},	 // Peak Y Threshold

{0x11,0x20},	 // LuxGainTB_0
{0x12,0x28},	 // LuxGainTB_1
{0x13,0x80},	 // LuxGainTB_2
{0x14,0x00},	 // LuxTimeTB_0
{0x15,0x90},	 // LuxTimeTB_1
{0x16,0x02},	 // LuxTimeTB_2
{0x17,0xB0},	 // LuxTimeTB_3
{0x18,0x08},	 // LuxTimeTB_4
{0x19,0x00},	 // LuxTimeTB_5

{0x1A,0x20},	 // LuxUpperIndex
{0x1B,0x08},	 // LuxLowerIndex

{0x31,0x04},	// MCLK 24MHz
{0x32,0xB0},	

{0x36,0x00},	 // AE Gain Max High
{0x37,0xA0},	 // AE Gain Max Low
{0x38,0x60},	 // Gain2Lut
{0x39,0x30},	 // Gain1Lut
{0x3A,0x20},	 // GainMin

{0x3B,0x03},	 // Min Frame// 
{0x3C,0x21},	 // Min Frame
{0x3D,0x0F},	 // 60Hz Max Time
{0x3E,0x0A},	 // Time2Lut60hz
{0x3F,0x08},	 // Time1Lut60hz
{0x40,0x0C},	 // 50Hz Max Time
{0x41,0x08},	 // Time2Lut50hz
{0x42,0x06},	 // Time1Lut50hz

{0x67,0x03},	 // 60Hz Min Time High
{0x68,0xe9},	 // 60Hz Min Time Low
{0x69,0x03},	 // 50Hz Min Time High
{0x6A,0x85},	 // 50Hz Min Time Low

{0xFF,0x61},	// Page Mode
{0xE5,0x3D},	 // AE Ctrl



//----------------------------------
// AWB STE
{0xFF,0x81},	// Page Mode
{0x99,0xC8},	// AE Histogram Threshold
			
{0x9F,0x01},	// AWBZone0LTx// Flash
{0xA0,0x01},	// AWBZone0LTy
{0xA1,0x02},	// AWBZone0RBx
{0xA2,0x02},	// AWBZone0RBy
{0xA3,0x74},	// AWBZone1LTx// Cloudy
{0xA4,0x4E},	// AWBZone1LTy
{0xA5,0x87},	// AWBZone1RBx
{0xA6,0x37},	// AWBZone1RBy
{0xA7,0x5A},	// AWBZone2LTx// DayLight
{0xA8,0x5A},	// AWBZone2LTy
{0xA9,0x7B},	// AWBZone2RBx
{0xAA,0x38},	// AWBZone2RBy
{0xAB,0x54},	// AWBZone3LTx// Fluorescent
{0xAC,0x62},	// AWBZone3LTy
{0xAD,0x66},	// AWBZone3RBx
{0xAE,0x4A},	// AWBZone3RBy
{0xAF,0x3E},	// AWBZone4LTx// Coolwhite
{0xB0,0x68},	// AWBZone4LTy
{0xB1,0x56},	// AWBZone4RBx
{0xB2,0x50},	// AWBZone4RBy
{0xB3,0x3F},	// AWBZone5LTx// TL84
{0xB4,0x70},	// AWBZone5LTy
{0xB5,0x54},	// AWBZone5RBx
{0xB6,0x63},	// AWBZone5RBy
{0xB7,0x39},	// AWBZone6LTx// A
{0xB8,0x8E},	// AWBZone6LTy
{0xB9,0x4C},	// AWBZone6RBx
{0xBA,0x7E},	// AWBZone6RBy
{0xBB,0x34},	// AWBZone7LTx// Horizon
{0xBC,0xA4},	// AWBZone7LTy
{0xBD,0x41},	// AWBZone7RBx
{0xBE,0x98},	// AWBZone7RBy



//==================================
// AWB
//==================================

{0xFF,0x62},	// Page Mode

{0x90,0x10},	// Min Gray Count High
{0x91,0x00},	// Min Gray Count Low

{0xAA,0x03},	// Min R Gain
{0xAB,0xA4},	// Min R Gain

{0xB0,0x07},	// Max B Gain
{0xB1,0xC8},	// Max B Gain

{0x9A,0x05},	
{0x9B,0x00},	
{0x9E,0x05},	
{0x9F,0x00},	

//==================================
// CCM D65
//==================================
{0xFF,0x62},	// Page Mode

{0xC9,0x00},	 // CrcMtx11_Addr1
{0xCA,0x66},	 // CrcMtx11_Addr0
{0xCB,0xFF},	 // CrcMtx12_Addr1
{0xCC,0xE1},	 // CrcMtx12_Addr0
{0xCD,0xFF},	 // CrcMtx13_Addr1
{0xCE,0xF8},	 // CrcMtx13_Addr0
{0xCF,0xFF},	 // CrcMtx21_Addr1
{0xD0,0xF3},	 // CrcMtx21_Addr0
{0xD1,0x00},	 // CrcMtx22_Addr1
{0xD2,0x59},	 // CrcMtx22_Addr0
{0xD3,0xFF},	 // CrcMtx23_Addr1
{0xD4,0xF4},	 // CrcMtx23_Addr0
{0xD5,0xFF},	 // CrcMtx31_Addr1
{0xD6,0xFF},	 // CrcMtx31_Addr0
{0xD7,0xFF},	 // CrcMtx32_Addr1
{0xD8,0xC8},	 // CrcMtx32_Addr0
{0xD9,0x00},	 // CrcMtx33_Addr1
{0xDA,0x7A},	 // CrcMtx33_Addr0


//==================================
// CCM Coolwhite
//==================================
{0xFF,0x62},	// Page Mode

{0xDB,0x00},	 // CrcMtx11_Addr1
{0xDC,0x4F},	 // CrcMtx11_Addr0
{0xDD,0xFF},	 // CrcMtx12_Addr1
{0xDE,0xF7},	 // CrcMtx12_Addr0
{0xDF,0xFF},	 // CrcMtx13_Addr1
{0xE0,0xF9},	 // CrcMtx13_Addr0
{0xE1,0xFF},	 // CrcMtx21_Addr1
{0xE2,0xF2},	 // CrcMtx21_Addr0
{0xE3,0x00},	 // CrcMtx22_Addr1
{0xE4,0x52},	 // CrcMtx22_Addr0
{0xE5,0xFF},	 // CrcMtx23_Addr1
{0xE6,0xFC},	 // CrcMtx23_Addr0
{0xE7,0xFF},	 // CrcMtx31_Addr1
{0xE8,0xFD},	 // CrcMtx31_Addr0
{0xE9,0xFF},	 // CrcMtx32_Addr1
{0xEA,0xD4},	 // CrcMtx32_Addr0
{0xEB,0x00},	 // CrcMtx33_Addr1
{0xEC,0x70},	 // CrcMtx33_Addr0


//==================================
// CCM A
//==================================
{0xFF,0x62},	// Page Mode

{0xED,0x00},	 // CrcMtx11_Addr1
{0xEE,0x46},	 // CrcMtx11_Addr0
{0xEF,0xFF},	 // CrcMtx12_Addr1
{0xF0,0xF9},	 // CrcMtx12_Addr0
{0xF1,0x00},	 // CrcMtx13_Addr1
{0xF2,0x00},	 // CrcMtx13_Addr0
{0xF3,0xFF},	 // CrcMtx21_Addr1
{0xF4,0xF0},	 // CrcMtx21_Addr0
{0xF5,0x00},	 // CrcMtx22_Addr1
{0xF6,0x50},	 // CrcMtx22_Addr0
{0xF7,0x00},	 // CrcMtx23_Addr1
{0xF8,0x00},	 // CrcMtx23_Addr0
{0xF9,0xFF},	 // CrcMtx31_Addr1
{0xFA,0xF0},	 // CrcMtx31_Addr0
{0xFB,0xFF},	 // CrcMtx32_Addr1
{0xFC,0xBD},	 // CrcMtx32_Addr0
{0xFD,0x00},	 // CrcMtx33_Addr1
{0xFE,0x93},	 // CrcMtx33_Addr0

//==================================
// ADF
//==================================

{0xFF,0x63},	// Page Mode
{0x70,0xFF},	 // ISP Adaptive Enable
{0x71,0xE3},	 // Analog Adaptive Enable

// ADF Threshold
{0x77,0xFA},	 // Th_LSC
{0x78,0x04},	 // Th_CDC
{0x79,0xA3},	 // Th_NSF
{0x7A,0x84},	 // Th_CNF
{0x7B,0x63},	 // Th_EDE
{0x7C,0x84},	 // Th_GDC
{0x7D,0x42},	 // Th_Gamma
{0x7E,0x42},	 // Th_Suppression

// ADF Min Value	// Dark
{0x7F,0x80},	 // Min_LSC
{0x80,0x20},	 // Min_NSFTh1
{0x81,0x90},	 // Min_NSFTh2
{0x82,0x40},	 // Min_CNFTh1
{0x83,0x80},	 // Min_CNFTh2
{0x84,0x05},	 // Min_EdgeCo
{0x85,0x08},	 // Min_EdgeSlp1
{0x86,0x0A},	 // Min_EdgeSlp2
{0x87,0x40},	 // Min_GDCTh1
{0x88,0x80},	 // Min_GDCTh2
{0x89,0x60},	 // Min_Suppression

// ADF Max Value	// Brightness
{0x8A,0xC0},	 // Max_LSC
{0x8B,0x0A},	 // Max_NSFTh1
{0x8C,0x50},	 // Max_NSFTh2
{0x8D,0x0A},	 // Max_CNFTh1
{0x8E,0x34},	 // Max_CNFTh2
{0x8F,0x03},	 // Max_EdgeCo
{0x90,0x0C},	 // Max_EdgeSlp1
{0x91,0x0A},	 // Max_EdgeSlp2
{0x92,0x20},	 // Max_GDCTh1
{0x93,0x40},	 // Max_GDCTh2
{0x94,0x80},	 // Max_Suppression

// Min(Dark) gamma 
{0x95,0x18},	 // ADF_Min_Gamma_0
{0x96,0x2D},	 // ADF_Min_Gamma_1
{0x97,0x47},	 // ADF_Min_Gamma_2
{0x98,0x57},	 // ADF_Min_Gamma_3
{0x99,0x63},	 // ADF_Min_Gamma_4
{0x9A,0x7A},	 // ADF_Min_Gamma_5
{0x9B,0x8C},	 // ADF_Min_Gamma_6
{0x9C,0x9A},	 // ADF_Min_Gamma_7
{0x9D,0xA8},	 // ADF_Min_Gamma_8
{0x9E,0xC0},	 // ADF_Min_Gamma_9
{0x9F,0xD4},	 // ADF_Min_Gamma_10
{0xA0,0xE4},	 // ADF_Min_Gamma_11
{0xA1,0xEC},	 // ADF_Min_Gamma_12
{0xA2,0xF4},	 // ADF_Min_Gamma_13
{0xA3,0xFB},	 // ADF_Min_Gamma_14


// Max(Bright) gamma 0.45 s-curve
{0xA4,0x04},	// ADF_Max_Gamma_0
{0xA5,0x0C},	// ADF_Max_Gamma_1
{0xA6,0x26},	// ADF_Max_Gamma_2
{0xA7,0x3B},	// ADF_Max_Gamma_3
{0xA8,0x4D},	// ADF_Max_Gamma_4
{0xA9,0x6A},	// ADF_Max_Gamma_5
{0xAA,0x7F},	// ADF_Max_Gamma_6
{0xAB,0x91},	// ADF_Max_Gamma_7
{0xAC,0xA2},	// ADF_Max_Gamma_8
{0xAD,0xBD},	// ADF_Max_Gamma_9
{0xAE,0xD2},	// ADF_Max_Gamma_10
{0xAF,0xE2},	// ADF_Max_Gamma_11
{0xB0,0xEA},	// ADF_Max_Gamma_12
{0xB1,0xF2},	// ADF_Max_Gamma_13
{0xB2,0xFA},	// ADF_Max_Gamma_14

//==================================
// ADF BGT/CON
//==================================
{0xE1,0x40},	// Th_BGT
{0xE2,0x08},	// Min_BGT
{0xE3,0x00},	// Max_BGT

{0xE5,0x64},	// Th_CON
{0xE6,0x00},	// Min_CON
{0xE7,0x00},	// Max_CON


//==================================
// ADF Analog
//==================================
{0xFF,0x63},	// Page Mode
{0xCD,0x23},	//
{0xCF,0x1F},	//

{0xD5,0x0a},	//
{0xD7,0x23},	//
{0xDE,0x60},	//


{0x74,0x20},	 // ADF_PreLux

//==================================
// ESD 
//==================================
{0xFF,0x80},	// Page Mode
{0x22,0x03},	 //
{0x24,0x03},	 //
{0x26,0x03},	 //


//==================================
// preview command
//==================================
 
{0xFF,0xA0},	// Page Mode
{0x10,0x02},	// Preview

//{0xFF,0xFF},	//delay 	100 //BYKIM_TEMP

//==================================
// 3A
//==================================
 
{0xFF,0x61},	// Page Mode
{0x8C,0x2F},	// AFC Disable

{0xFF, 0xFF},       
}
#endif /* _CAMACQ_API_C_ */
;
#if 1//swsw_vt_call

GLOBAL const U8 reg_sub_vt_init[][2]
#if defined(_CAMACQ_API_C_)
={
{0xFF,0x81},	// Page Mode
{0x70,0x01},	// Output Format : YUV
{0x71,0x25},	// Output Ctrl : CAMIF_YCbCr_Cb_Y_Cr_Y, PCLK-Invert, HSYNC-Active High, VSYNC-Active Low


//==================================
// SensorCon
//==================================

{0xFF,0xB0},	// Page Mode
{0x02,0x00},	// Sensor Disable
{0x03,0x01},	// Y Flip //bykim temp
{0x0F,0x07},	//
{0x12,0x44},	//
{0x14,0x80},	//
{0x17,0xe3},	//
{0x1e,0x01},	//
{0x4e,0x01},	      //
{0x4F,0x9a},	//
{0x67,0x0d},	//
{0x68,0x15},	//
{0x69,0x1f},	//
{0x60,0x02},	// Pad Current : 6mA
{0x02,0x01},	// Sensor Enable

{0xFF,0x61},	// Page Mode
{0xd4,0x00},	// Fgain

//==================================
// ISP Global Control #1
//==================================

{0xFF,0x80},	// Page Mode
{0x00,0xFF},	 // Bayer Function
{0x01,0xFF},	 // RgbYcFunc_Addr
{0x02,0x00},	 // Test Mode


//----------------------------------
// LSC
{0x46,0xA0},	 // RRp0Gain
{0x47,0x30},	 // RRp1Gain
{0x48,0x00},	 // RRp2Gain
{0x49,0x88},	 // GGp0Gain
{0x4A,0x30},	 // GGp1Gain
{0x4B,0x00},	 // GGp2Gain
{0x4C,0x80},	 // BBp0Gain
{0x4D,0x28},	 // BBp1Gain
{0x4E,0x00},	 // BBp2Gain
{0x4F,0xFF},	 // ShdGGain


//----------------------------------
// BCD
{0xD1,0x20},	 // False color threshold


//==================================
// ISP Global Control #2
//==================================

{0xFF,0x81},	// Page Mode

//----------------------------------
// Edge
{0x30,0x55},	 // Edge Option
{0x31,0x00},	// Edge Luminance adaptive
{0x32,0x02},	 // Edge PreCoring point
{0x33,0x01},	 // Edge x1
{0x34,0x80},	 // Edge x1
{0x35,0x03},	 // Edge x2
{0x36,0xFF},	 // Edge x2
{0x37,0x08},	 // Edge Transfer slope 1
{0x38,0x10},	 // Edge Transfer slope 2
{0x39,0x20},	 // Edge Left Offset
{0x3a,0x20},	 // Edge Right offset



//==================================
// AE
//==================================
{0xFF,0x81},	// Page Mode
{0x91,0x00},	// Point A
{0x92,0x00},	
{0x93,0x35},	// Point B
{0x94,0x18},	
{0x95,0x6B},	// Point C
{0x96,0x60},	
{0x97,0xA0},	// Point D
{0x98,0x78},	

{0xFF,0x61},	// Page Mode
{0xE4,0x02},	 // AE Mode
{0xE5,0x1D},	 // AE Ctrl

{0xE8,0x21},	 // AE Weight (Left / Top)
{0xE9,0x2C},	 // AE Weight (Right / Center)
{0xEA,0x42},	 // AE weight (Factor / Bottom)
{0xEB,0x06},	 // AE Speed
{0xEF,0x08},	// Analog gain max

{0xFA,0x34},	 // Outdoor AE Target
{0xFB,0x36},	 // Indoor AE Target

{0xFF,0x62},	// Page Mode

{0x0D,0x38},	// Peak AE Target min
{0x0F,0x02},	// Peak Y Threshold

{0x11,0x20},	 // LuxGainTB_0
{0x12,0x28},	 // LuxGainTB_1
{0x13,0x80},	 // LuxGainTB_2
{0x14,0x00},	 // LuxTimeTB_0
{0x15,0x90},	 // LuxTimeTB_1
{0x16,0x02},	 // LuxTimeTB_2
{0x17,0xB0},	 // LuxTimeTB_3
{0x18,0x04},	 // LuxTimeTB_4
{0x19,0x00},	 // LuxTimeTB_5

{0x1A,0x30},	 // LuxUpperIndex
{0x1B,0x00},	 // LuxLowerIndex

{0x31,0x04},	// MCLK 24MHz
{0x32,0xB0},	

{0x36,0x00},	 // AE Gain Max High
{0x37,0xA0},	 // AE Gain Max Low
{0x38,0x60},	 // Gain2Lut
{0x39,0x30},	 // Gain1Lut
{0x3A,0x20},	 // GainMin

{0x3B,0x03},	 // Min Frame	// 10fps 
{0x3C,0xE9},	 // Min Frame
{0x3D,0x0C},	 // 60Hz Max Time
{0x3E,0x0A},	 // Time2Lut60hz
{0x3F,0x08},	 // Time1Lut60hz
{0x40,0x0A},	 // 50Hz Max Time
{0x41,0x08},	 // Time2Lut50hz
{0x42,0x06},	 // Time1Lut50hz

{0x67,0x05},	 // 60Hz Min Time High
{0x68,0xdd},	 // 60Hz Min Time Low
{0x69,0x05},	 // 50Hz Min Time High
{0x6A,0xdd},	 // 50Hz Min Time Low

{0xFF,0x61},	// Page Mode
{0xE5,0x3D},	 // AE Ctrl



//----------------------------------
// AWB STE
{0xFF,0x81},	// Page Mode
{0x99,0xC8},	// AE Histogram Threshold
			
{0x9F,0x21},	// AWBZone0LTx// Flash
{0xA0,0x6E},	// AWBZone0LTy
{0xA1,0x35},	// AWBZone0RBx
{0xA2,0x5A},	// AWBZone0RBy
{0xA3,0x87},	// AWBZone1LTx// Cloudy
{0xA4,0x50},	// AWBZone1LTy
{0xA5,0x96},	// AWBZone1RBx
{0xA6,0x41},	// AWBZone1RBy
{0xA7,0x71},	// AWBZone2LTx// DayLight
{0xA8,0x5A},	// AWBZone2LTy
{0xA9,0x85},	// AWBZone2RBx
{0xAA,0x3C},	// AWBZone2RBy
{0xAB,0x54},	// AWBZone3LTx// Fluorescent
{0xAC,0x5F},	// AWBZone3LTy
{0xAD,0x6E},	// AWBZone3RBx
{0xAE,0x46},	// AWBZone3RBy
{0xAF,0x3B},	// AWBZone4LTx// Coolwhite
{0xB0,0x69},	// AWBZone4LTy
{0xB1,0x52},	// AWBZone4RBx
{0xB2,0x4E},	// AWBZone4RBy
{0xB3,0x46},	// AWBZone5LTx// TL84
{0xB4,0x6E},	// AWBZone5LTy
{0xB5,0x58},	// AWBZone5RBx
{0xB6,0x64},	// AWBZone5RBy
{0xB7,0x44},	// AWBZone6LTx// A
{0xB8,0x92},	// AWBZone6LTy
{0xB9,0x58},	// AWBZone6RBx
{0xBA,0x78},	// AWBZone6RBy
{0xBB,0x34},	// AWBZone7LTx// Horizon
{0xBC,0xB6},	// AWBZone7LTy
{0xBD,0x42},	// AWBZone7RBx
{0xBE,0xA4},	// AWBZone7RBy



//==================================
// AWB
//==================================

{0xFF,0x62},	// Page Mode

{0x90,0x00},	// Min Gray Count High
{0x91,0x80},	// Min Gray Count Low

{0xAA,0x03},	// Min R Gain
{0xAB,0xA4},	// Min R Gain

{0xB0,0x07},	// Max B Gain
{0xB1,0xC8},	// Max B Gain

{0x9A,0x05},	
{0x9B,0x00},	
{0x9E,0x05},	
{0x9F,0x00},	

//==================================
// CCM D65
//==================================
{0xFF,0x62},	// Page Mode

{0xC9,0x00},	 // CrcMtx11_Addr1
{0xCA,0x66},	 // CrcMtx11_Addr0
{0xCB,0xFF},	 // CrcMtx12_Addr1
{0xCC,0xE1},	 // CrcMtx12_Addr0
{0xCD,0xFF},	 // CrcMtx13_Addr1
{0xCE,0xF8},	 // CrcMtx13_Addr0
{0xCF,0xFF},	 // CrcMtx21_Addr1
{0xD0,0xF3},	 // CrcMtx21_Addr0
{0xD1,0x00},	 // CrcMtx22_Addr1
{0xD2,0x59},	// CrcMtx22_Addr0
{0xD3,0xFF},	 // CrcMtx23_Addr1
{0xD4,0xF4},	 // CrcMtx23_Addr0
{0xD5,0xFF},	 // CrcMtx31_Addr1
{0xD6,0xFF},	 // CrcMtx31_Addr0
{0xD7,0xFF},	 // CrcMtx32_Addr1
{0xD8,0xC8},	// CrcMtx32_Addr0
{0xD9,0x00},	 // CrcMtx33_Addr1
{0xDA,0x7A},	 // CrcMtx33_Addr0


//==================================
// CCM Coolwhite
//==================================
{0xFF,0x62},	// Page Mode

{0xDB,0x00},	 // CrcMtx11_Addr1
{0xDC,0x5E},	 // CrcMtx11_Addr0
{0xDD,0xFF},	 // CrcMtx12_Addr1
{0xDE,0xEC},	 // CrcMtx12_Addr0
{0xDF,0xFF},	 // CrcMtx13_Addr1
{0xE0,0xF6},	 // CrcMtx13_Addr0
{0xE1,0xFF},	 // CrcMtx21_Addr1
{0xE2,0xED},	 // CrcMtx21_Addr0
{0xE3,0x00},	 // CrcMtx22_Addr1
{0xE4,0x5A},	 // CrcMtx22_Addr0
{0xE5,0xFF},	 // CrcMtx23_Addr1
{0xE6,0xF9},	 // CrcMtx23_Addr0
{0xE7,0xFF},	 // CrcMtx31_Addr1
{0xE8,0xFF},	 // CrcMtx31_Addr0
{0xE9,0xFF},	 // CrcMtx32_Addr1
{0xEA,0xCF},	 // CrcMtx32_Addr0
{0xEB,0x00},	 // CrcMtx33_Addr1
{0xEC,0x72},	 // CrcMtx33_Addr0


//==================================
// CCM A
//==================================
{0xFF,0x62},	// Page Mode

{0xED,0x00},	 // CrcMtx11_Addr1
{0xEE,0x46},	 // CrcMtx11_Addr0
{0xEF,0xFF},	 // CrcMtx12_Addr1
{0xF0,0xF9},	 // CrcMtx12_Addr0
{0xF1,0x00},	 // CrcMtx13_Addr1
{0xF2,0x00},	 // CrcMtx13_Addr0
{0xF3,0xFF},	 // CrcMtx21_Addr1
{0xF4,0xF0},	 // CrcMtx21_Addr0
{0xF5,0x00},	 // CrcMtx22_Addr1
{0xF6,0x50},	 // CrcMtx22_Addr0
{0xF7,0x00},	 // CrcMtx23_Addr1
{0xF8,0x00},	 // CrcMtx23_Addr0
{0xF9,0xFF},	 // CrcMtx31_Addr1
{0xFA,0xF0},	 // CrcMtx31_Addr0
{0xFB,0xFF},	 // CrcMtx32_Addr1
{0xFC,0xBD},	 // CrcMtx32_Addr0
{0xFD,0x00},	 // CrcMtx33_Addr1
{0xFE,0x93},	 // CrcMtx33_Addr0

//==================================
// ADF
//==================================

{0xFF,0x63},	// Page Mode
{0x70,0xFF},	 // ISP Adaptive Enable
{0x71,0xE3},	 // Analog Adaptive Enable

// ADF Threshold
{0x77,0xF4},	 // Th_LSC
{0x78,0x04},	 // Th_CDC
{0x79,0xE5},	 // Th_NSF
{0x7A,0x84},	 // Th_CNF
{0x7B,0x41},	 // Th_EDE
{0x7C,0x84},	 // Th_GDC
{0x7D,0x42},	 // Th_Gamma
{0x7E,0x30},	 // Th_Suppression

// ADF Min Value	// Dark
{0x7F,0x80},	 // Min_LSC
{0x80,0x38},	 // Min_NSFTh1
{0x81,0x70},	 // Min_NSFTh2
{0x82,0x40},	 // Min_CNFTh1
{0x83,0x80},	 // Min_CNFTh2
{0x84,0x0A},	 // Min_EdgeCo
{0x85,0x04},	 // Min_EdgeSlp1
{0x86,0x03},	 // Min_EdgeSlp2
{0x87,0x40},	 // Min_GDCTh1
{0x88,0x80},	 // Min_GDCTh2
{0x89,0x40},	 // Min_Suppression

// ADF Max Value	// Brightness
{0x8A,0xC0},	// Max_LSC
{0x8B,0x08},	 // Max_NSFTh1
{0x8C,0x30},	 // Max_NSFTh2
{0x8D,0x0A},	 // Max_CNFTh1
{0x8E,0x34},	 // Max_CNFTh2
{0x8F,0x02},	 // Max_EdgeCo
{0x90,0x08},	// Max_EdgeSlp1
{0x91,0x06},	 // Max_EdgeSlp2
{0x92,0x20},	 // Max_GDCTh1
{0x93,0x40},	 // Max_GDCTh2
{0x94,0x80},	 // Max_Suppression

// Min(Dark) gamma 
{0x95,0x11},	 // ADF_Min_Gamma_0
{0x96,0x1E},	 // ADF_Min_Gamma_1
{0x97,0x4B},	 // ADF_Min_Gamma_2
{0x98,0x60},	 // ADF_Min_Gamma_3
{0x99,0x70},	 // ADF_Min_Gamma_4
{0x9A,0x86},	 // ADF_Min_Gamma_5
{0x9B,0x97},	 // ADF_Min_Gamma_6
{0x9C,0xA5},	 // ADF_Min_Gamma_7
{0x9D,0xB0},	 // ADF_Min_Gamma_8
{0x9E,0xC6},	 // ADF_Min_Gamma_9
{0x9F,0xD8},	 // ADF_Min_Gamma_10
{0xA0,0xE8},	 // ADF_Min_Gamma_11
{0xA1,0xF0},	 // ADF_Min_Gamma_12
{0xA2,0xF8},	 // ADF_Min_Gamma_13
{0xA3,0xFF},	 // ADF_Min_Gamma_14


// Max(Bright) gamma 0.45 s-curve
{0xA4,0x02},	// ADF_Max_Gamma_0
{0xA5,0x0C},	// ADF_Max_Gamma_1
{0xA6,0x2A},	// ADF_Max_Gamma_2
{0xA7,0x44},	// ADF_Max_Gamma_3
{0xA8,0x5A},	// ADF_Max_Gamma_4
{0xA9,0x7A},	// ADF_Max_Gamma_5
{0xAA,0x8C},	// ADF_Max_Gamma_6
{0xAB,0x9A},	// ADF_Max_Gamma_7
{0xAC,0xA8},	// ADF_Max_Gamma_8
{0xAD,0xC0},	// ADF_Max_Gamma_9
{0xAE,0xD4},	// ADF_Max_Gamma_10
{0xAF,0xE4},	// ADF_Max_Gamma_11
{0xB0,0xEC},	// ADF_Max_Gamma_12
{0xB1,0xF4},	// ADF_Max_Gamma_13
{0xB2,0xFB},	// ADF_Max_Gamma_14

//==================================
// ADF BGT/CON
//==================================
{0xE1,0x52},	// Th_BGT
{0xE2,0x36},	// Min_BGT
{0xE3,0x00},	// Max_BGT

{0xE5,0x10},	// Th_CON
{0xE6,0x04},	// Min_CON
{0xE7,0x00},	// Max_CON


//==================================
// ADF Analog
//==================================
{0xFF,0x63},	// Page Mode
{0xCD,0x23},	 //
{0xCF,0x1F},	//

{0xD5,0x0a},	//
{0xD7,0x23},	 //
{0xDE,0x60},	 //


{0x74,0x20},	 // ADF_PreLux


//==================================
// ESD 
//==================================
{0xFF,0x80},	// Page Mode
{0x22,0x03},	 //
{0x24,0x03},	 //
{0x26,0x03},	 //


//==================================
// preview command
//==================================
 
{0xFF,0xA0},	// Page Mode
{0x10,0x02},	// Preview
//{0xFF,0xFF},	//delay 	100 //BYKIM_TEMP
//==================================
// 3A
//==================================
 
{0xFF,0x61},	// Page Mode
{0x8C,0x2F},	// AFC Disable
{0xFF, 0xFF},       

}
#endif /* _CAMACQ_API_C_ */
;

#endif//swsw_vt_call

GLOBAL const U8 reg_sub_fmt_jpg[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_fmt_yuv422[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA PREVIEW(640*480 / FULL)
//==========================================================
GLOBAL const U8 reg_sub_preview[][2]
#if defined(_CAMACQ_API_C_)
={
{0xFF,0x61},	// Page Mode
{0xB9,0x02},	// 
{0xBA,0x03},	// 
{0xBB,0xFF},	// 
{0xBC,0x03},	// 
{0xBD,0xFF},	// 

{0xFF,0x61},	// Page Mode
{0xE5,0x1D},	 // AE Ctrl

{0xFF,0x62},	// Page Mode

{0x18,0x08},	 // Lux Time 4 
{0x19,0x00},	 // Lux Time 5

{0x3B,0x03},	 // Min Frame// 
{0x3C,0x21},	 // Min Frame
{0x3D,0x0F},	 // 60Hz Max Time
{0x3E,0x0A},	 // Time2Lut60hz
{0x3F,0x08},	 // Time1Lut60hz
{0x40,0x0C},	 // 50Hz Max Time
{0x41,0x08},	 // Time2Lut50hz
{0x42,0x06},	 // Time1Lut50hz

{0x67,0x03},	 // 60Hz Min Time High
{0x68,0xe9},	 // 60Hz Min Time Low
{0x69,0x03},	 // 50Hz Min Time High
{0x6A,0x85},	 // 50Hz Min Time Low

{0xFF,0x61},	// Page Mode
{0xE5,0x3D},	 // AE Ctrl
{0xFF, 0xFF},       

}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// SNAPSHOT
//==========================================================

GLOBAL const U8 reg_sub_snapshot[][2]
#if defined(_CAMACQ_API_C_)
={
 
{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// MIDLIGHT SNAPSHOT =======> Flash Low light snapshot
//==========================================================
GLOBAL const U8 reg_sub_midlight[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// LOWLIGHT SNAPSHOT
//==========================================================
GLOBAL const U8 reg_sub_lowlight[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;



/*****************************************************************/
/*********************** N I G H T  M O D E **********************/
/*****************************************************************/

//==========================================================
// CAMERA NIGHTMODE ON
//==========================================================
GLOBAL const U8 reg_sub_nightshot_on[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA NIGHTMODE OFF
//==========================================================
GLOBAL const U8 reg_sub_nightshot_off[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// NIGHT-MODE SNAPSHOT
//==========================================================
GLOBAL const U8 reg_sub_nightshot[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_AUTO (1/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_auto[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_DAYLIGHT (2/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_daylight[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_CLOUDY  (3/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_cloudy[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_INCANDESCENT (4/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_incandescent[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_FLUORESCENT (5/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_fluorescent[][2]
#if defined(_CAMACQ_API_C_)
={
   
    {0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_FLUORESCENT (6/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_twilight[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_FLUORESCENT (7/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_tungsten[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_FLUORESCENT (8/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_off[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_FLUORESCENT (9/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_horizon[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_WB_FLUORESCENT (10/10)
//==========================================================
GLOBAL const U8 reg_sub_wb_shade[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_EFFECT_OFF (1/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_none[][2]
#if defined(_CAMACQ_API_C_)
={
	
	{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_MONO (2/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_gray[][2]
#if defined(_CAMACQ_API_C_)
={
	
	{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_NEGATIVE (3/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_negative[][2]
#if defined(_CAMACQ_API_C_)
={
	
	{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_SOLARIZE (4/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_solarize[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_SEPIA (5/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_sepia[][2]
#if defined(_CAMACQ_API_C_)
={
	
	{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_POSTERIZE (6/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_posterize[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_WHITEBOARD (7/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_whiteboard[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_BLACKBOARD (8/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_blackboard[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_AQUA (9/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_aqua[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_SHARPEN (10/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_sharpen[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_AQUA (11/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_vivid[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_GREEN (12/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_green[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EFFECT_SKETCH (13/13)
//==========================================================
GLOBAL const U8 reg_sub_effect_sketch[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_AEC_MATRIX_METERING (2/2)
//==========================================================
GLOBAL const U8 reg_sub_meter_matrix[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_AEC_CENTERWEIGHTED_METERING (2/2)
//==========================================================
GLOBAL const U8 reg_sub_meter_cw[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_AEC_SPOT_METERING (1/2)
//==========================================================
GLOBAL const U8 reg_sub_meter_spot[][2]
#if defined(_CAMACQ_API_C_)
={

	{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_AEC_FRAME_AVERAGE (2/2)
//==========================================================
GLOBAL const U8 reg_sub_meter_frame[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FLIP_NONE (1/4)
//==========================================================
GLOBAL const U8 reg_sub_flip_none[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FLIP_VERTICAL (2/4)
//==========================================================
GLOBAL const U8 reg_sub_flip_water[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FLIP_HORIZ (3/4)
//==========================================================
GLOBAL const U8 reg_sub_flip_mirror[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_FLIP_SYMMETRIC (4/4)
//==========================================================
GLOBAL const U8 reg_sub_flip_water_mirror[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;



//==========================================================
// CAMERA_CAMCORDER
//==========================================================
GLOBAL const U8 reg_sub_camcorder[][2]
#if defined(_CAMACQ_API_C_)
={
{0xFF,0x61},	// Page Mode
{0xE5,0x1D},	 // AE Ctrl

{0xFF,0x62},	// Page Mode

{0x18,0x04},	 // Lux Time 4 
{0x19,0x00},	 // Lux Time 5

{0x3B,0x05},	 // Min Frame// 
{0x3C,0xDD},	 // Min Frame
{0x3D,0x08},	 // 60Hz Max Time
{0x3E,0x06},	 // Time2Lut60hz
{0x3F,0x04},	 // Time1Lut60hz
{0x40,0x06},	 // 50Hz Max Time
{0x41,0x04},	 // Time2Lut50hz
{0x42,0x04},	 // Time1Lut50hz

{0x67,0x03},	 // 60Hz Min Time High
{0x68,0xE9},	 // 60Hz Min Time Low
{0x69,0x03},	 // 50Hz Min Time High
{0x6A,0x85},	 // 50Hz Min Time Low

{0xFF,0x61},	// Page Mode
{0xE5,0x3D},	 // AE Ctrl
{0xFF, 0xFF},       

}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_FRAMERATE_5FPS
//==========================================================
GLOBAL const U8 reg_sub_fps_fixed_5[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_FRAMERATE_7FPS
//==========================================================
GLOBAL const U8 reg_sub_fps_fixed_7[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FRAMERATE_10FPS
//==========================================================
GLOBAL const U8 reg_sub_fps_fixed_10[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FRAMERATE_15FPS
//==========================================================
GLOBAL const U8 reg_sub_fps_fixed_15[][2]
#if defined(_CAMACQ_API_C_)
={
	  

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FRAMERATE_20FPS
//==========================================================
GLOBAL const U8 reg_sub_fps_fixed_20[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FRAMERATE_25FPS
//==========================================================
GLOBAL const U8 reg_sub_fps_fixed_25[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FRAMERATE_30FPS
//==========================================================
GLOBAL const U8 reg_sub_fps_fixed_30[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_FRAMERATE_AUTO_MAX15(5~15fps)
//==========================================================
GLOBAL const U8 reg_sub_fps_var_15[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_BRIGHTNESS_LEVEL1 (1/9) : 
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_0[][2]
#if defined(_CAMACQ_API_C_)
={
	
	{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL2 (2/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_1[][2]
#if defined(_CAMACQ_API_C_)
={
	
	{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL3 (3/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_2[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL4 (4/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_3[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL5 (5/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_4[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL6 (6/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_5[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL7 (7/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_6[][2]
#if defined(_CAMACQ_API_C_)
={


{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL8 (8/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_7[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_BRIGHTNESS_LEVEL9 (9/9)
//==========================================================
GLOBAL const U8 reg_sub_brightness_level_8[][2]
#if defined(_CAMACQ_API_C_)
={

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL1 (1/9) : 
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_0[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL2 (2/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_1[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL3 (3/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_2[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL4 (4/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_3[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL5 (5/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_4[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL6 (6/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_5[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL7 (7/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_6[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL8 (8/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_7[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_EXPOSURE_COMPENSATION_LEVEL9 (9/9)
//==========================================================
GLOBAL const U8 reg_sub_expcompensation_level_8[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_AF
//==========================================================
GLOBAL const U8 reg_sub_reset_af [][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_nlux [][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_llux [][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af[][2] // start_af
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_off_af[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


GLOBAL const U8 reg_sub_check_af[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_manual_af[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_macro_af[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_return_manual_af[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_return_macro_af[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_normal_mode_1[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_normal_mode_2[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_normal_mode_3[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_macro_mode_1[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_macro_mode_2[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_set_af_macro_mode_3[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_ISO_AUTO
//==========================================================
GLOBAL const U8 reg_sub_iso_auto[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_ISO_50
//==========================================================
GLOBAL const U8 reg_sub_iso_50[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_ISO_100
//==========================================================
GLOBAL const U8 reg_sub_iso_100[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_ISO_200
//==========================================================
GLOBAL const U8 reg_sub_iso_200[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ISO_400
//==========================================================
GLOBAL const U8 reg_sub_iso_400[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ISO_800
//==========================================================
GLOBAL const U8 reg_sub_iso_800[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ISO_1600
//==========================================================
GLOBAL const U8 reg_sub_iso_1600[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ISO_3200
//==========================================================
GLOBAL const U8 reg_sub_iso_3200[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_AUTO (OFF)
//==========================================================
GLOBAL const U8 reg_sub_scene_auto[][2]
#if defined(_CAMACQ_API_C_)
={
/* CAMERA_SCENEMODE_OFF */



{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_NIGHT
//==========================================================
GLOBAL const U8 reg_sub_scene_night[][2]
#if defined(_CAMACQ_API_C_)
={
/* CAMERA_SCENEMODE_NIGHT_Nomal */                
   
  

    {0xff, 0xff},  
}
#endif /* _CAMACQ_API_C_ */
;


GLOBAL const U8 reg_sub_scene_night_dark[][2]
#if defined(_CAMACQ_API_C_)
={
 /* CAMERA_SCENEMODE_NIGHT_Dark */                             
                                                                
                                               

    {0xff, 0xff},  
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_LANDSCAPE
//==========================================================
GLOBAL const U8 reg_sub_scene_landscape[][2]
#if defined(_CAMACQ_API_C_)
={
/* CAMERA_SCENEMODE_LANDSCAPE   :*/

	
        {0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_SUNSET
//==========================================================
GLOBAL const U8 reg_sub_scene_sunset[][2]
#if defined(_CAMACQ_API_C_)
={

/* CAMERA_SCENEMODE_SUNSET   : Àå\u017e?\u017eðµ?  */     
                 
       
        {0xff, 0xff},
        
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_PORTRAIT
//==========================================================
GLOBAL const U8 reg_sub_scene_portrait[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_SUNRISE
//==========================================================
GLOBAL const U8 reg_sub_scene_sunrise[][2]
#if defined(_CAMACQ_API_C_)
={

/* CAMERA_SCENEMODE_SUNRISE   : Àå\u017e?\u017eðµ?  */    
	
	
        {0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_INDOOR // == PARTY
//==========================================================
GLOBAL const U8 reg_sub_scene_indoor[][2]
#if defined(_CAMACQ_API_C_)
={
/* CAMERA_SCENEMODE_AGAINST   : Àå\u017e?\u017eðµ?*/

     
        
        {0xff, 0xff},}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_PARTY // == INDOOR
//==========================================================
GLOBAL const U8 reg_sub_scene_party[][2] 
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_SPORTS
//==========================================================
GLOBAL const U8 reg_sub_scene_sports[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_BEACH
//==========================================================
GLOBAL const U8 reg_sub_scene_beach[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_SNOW
//==========================================================
GLOBAL const U8 reg_sub_scene_snow[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_FALLCOLOR
//==========================================================
GLOBAL const U8 reg_sub_scene_fallcolor[][2]
#if defined(_CAMACQ_API_C_)
={

	/* CAMERA_SCENEMODE_FALL   : Àå\u017e?\u017eðµ?  */


{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_FIREWORKS
//==========================================================
GLOBAL const U8 reg_sub_scene_fireworks[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_CANDLELIGHT
//==========================================================
GLOBAL const U8 reg_sub_scene_candlelight[][2]
#if defined(_CAMACQ_API_C_)
={

/* CAMERA_SCENE_CANDLELIGHT    : Àå\u017e?\u017eðµ?*/
 

{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_AGAINSTLIGHT (BACKLight??)
//==========================================================
GLOBAL const U8 reg_sub_scene_againstlight[][2]
#if defined(_CAMACQ_API_C_)
={


{0xff, 0xff},
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_SCENE_TEXT
//==========================================================
GLOBAL const U8 reg_sub_scene_text[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_CONTRAST_M2
//==========================================================
GLOBAL const U8 reg_sub_adjust_contrast_m2[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_CONTRAST_M1
//==========================================================
GLOBAL const U8 reg_sub_adjust_contrast_m1[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_CONTRAST_DEFAULT
//==========================================================
GLOBAL const U8 reg_sub_adjust_contrast_default[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_CONTRAST_P1
//==========================================================
GLOBAL const U8 reg_sub_adjust_contrast_p1[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_CONTRAST_P2
//==========================================================
GLOBAL const U8 reg_sub_adjust_contrast_p2[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_SHARPNESS_M2
//==========================================================
GLOBAL const U8 reg_sub_adjust_sharpness_m2[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_SHARPNESS_M1
//==========================================================
GLOBAL const U8 reg_sub_adjust_sharpness_m1[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_ADJUST_SHARPNESS_DEFAULT
//==========================================================
GLOBAL const U8 reg_sub_adjust_sharpness_default[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_ADJUST_SHARPNESS_P1
//==========================================================
GLOBAL const U8 reg_sub_adjust_sharpness_p1[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_ADJUST_SHARPNESS_P2
//==========================================================
GLOBAL const U8 reg_sub_adjust_sharpness_p2[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_SATURATION_M2
//==========================================================
GLOBAL const U8 reg_sub_adjust_saturation_m2[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_SATURATION_M1
//==========================================================
GLOBAL const U8 reg_sub_adjust_saturation_m1[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_SATURATION_DEFAULT
//==========================================================
GLOBAL const U8 reg_sub_adjust_saturation_default[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_SATURATION_P1
//==========================================================
GLOBAL const U8 reg_sub_adjust_saturation_p1[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_ADJUST_SATURATION_P2
//==========================================================
GLOBAL const U8 reg_sub_adjust_saturation_p2[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;


//==========================================================
// CAMERA_output_qqvga
//==========================================================
GLOBAL const U8 reg_sub_qqvga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_qcif
//==========================================================
GLOBAL const U8 reg_sub_qcif[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_qvga
//==========================================================
GLOBAL const U8 reg_sub_qvga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_wqvga
//==========================================================
GLOBAL const U8 reg_sub_wqvga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_cif
//==========================================================
GLOBAL const U8 reg_sub_cif[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_vga
//==========================================================
GLOBAL const U8 reg_sub_vga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_wvga 800 * 480
//==========================================================
GLOBAL const U8 reg_sub_wvga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_svga
//==========================================================
GLOBAL const U8 reg_sub_svga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_sxga
//==========================================================
GLOBAL const U8 reg_sub_sxga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_qxga
//==========================================================
GLOBAL const U8 reg_sub_qxga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_qxga
//==========================================================
GLOBAL const U8 reg_sub_uxga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_full_yuv
//==========================================================
GLOBAL const U8 reg_sub_full_yuv[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_output_cropped_yuv
//==========================================================
GLOBAL const U8 reg_sub_crop_yuv[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

      
//==========================================================
// CAMERA_JPEG_output_5M
//==========================================================
GLOBAL const U8 reg_sub_jpeg_5m[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_5M_2
//==========================================================
GLOBAL const U8 reg_sub_jpeg_5m_2[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_4M
//==========================================================
GLOBAL const U8 reg_sub_jpeg_w4m[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_3M
//==========================================================
GLOBAL const U8 reg_sub_jpeg_3m[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_2M
//==========================================================
GLOBAL const U8 reg_sub_jpeg_2m[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_w1_5M
//==========================================================
GLOBAL const U8 reg_sub_jpeg_w1_5m[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_1M
//==========================================================
GLOBAL const U8 reg_sub_jpeg_1m[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_VGA
//==========================================================
GLOBAL const U8 reg_sub_jpeg_vga[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_WQVGA
//==========================================================
GLOBAL const U8 reg_sub_jpeg_wqvga[][2]
#if defined(_CAMACQ_API_C_)
={


CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

//==========================================================
// CAMERA_JPEG_output_QVGA
//==========================================================
GLOBAL const U8 reg_sub_jpeg_qvga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_qvga_v[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_half_vga_v[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_half_vga[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_vga_v[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_5M[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_1080P[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_720P[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_not[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_flicker_disabled[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_flicker_50hz[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_flicker_60hz[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_flicker_auto[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_jpeg_quality_superfine[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_jpeg_quality_fine[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_jpeg_quality_normal[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA
}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_priv_ctrl_returnpreview[][2]
#if defined(_CAMACQ_API_C_)
={

CAMACQ_SUB_BTM_OF_DATA

}
#endif /* _CAMACQ_API_C_ */
;

//BYKIM_DTP
GLOBAL const U8 reg_sub_dtp_on[][2]
#if defined(_CAMACQ_API_C_)
={
{0xFF,0x80},	// Page Mode
{0x00,0x00},	 // BayerFunc, BCD Disable
{0x01,0x28},	 // RgbYcFunc
{0x02,0x05},	 // TPG-Gamma
CAMACQ_SUB_BTM_OF_DATA     

}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_dtp_off[][2]
#if defined(_CAMACQ_API_C_)
={
{0xFF,0x80},	// Page Mode
{0x00,0xFF},	 // BayerFunc, BCD Disable
{0x01,0xFF},	 // RgbYcFunc
{0x02,0x00},	 // TPG-Gamma
CAMACQ_SUB_BTM_OF_DATA      

}
#endif /* _CAMACQ_API_C_ */
;


#undef GLOBAL

#endif /* _CAMACQ_SR200PC10_H_ */
