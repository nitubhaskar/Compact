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

#if !defined(_CAMACQ_SR030PC30_H_)
#define _CAMACQ_SR030PC30_H_

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
#define CAMACQ_SUB_EXT_REG_IS_DELAY(A)				((A[0]==CAMACQ_SUB_REG_DELAY)? 1:0)


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
{0x03,0x00},
{0x01,0xf1},
{0x01,0xf3},
{0x01,0xf1},
     
{0x03,0x20},
{0x10,0x1c},
{0x03,0x22},
{0x10,0x7b},
     
//Page0
{0x03,0x00}, 
{0x0b,0xaa},
{0x0c,0xaa},
{0x0d,0xaa},
{0x10,0x00},
{0x11,0x91}, //On Var Frame - None : 90, X Flip : 91, Y Flip :
{0x12,0x04},//PCLK Inversion
{0x20,0x00},
{0x21,0x06},
{0x22,0x00},
{0x23,0x06},
{0x24,0x01},
{0x25,0xe0},
{0x26,0x02},
{0x27,0x80},
{0x40,0x01}, //Hblank 336
{0x41,0x50},
     
{0x42,0x01}, //Vblank 300
{0x43,0x2c},
     
//BLC 
{0x80,0x3e},
{0x81,0x96},
{0x82,0x90},
{0x83,0x00},
{0x84,0x2c},
   
{0x90,0x0f}, //BLC_TIME_TH_ON
{0x91,0x0f}, //BLC_TIME_TH_OFF 
{0x92,0x78}, //BLC_AG_TH_ON
{0x93,0x70}, //BLC_AG_TH_OFF
{0x94,0x88},
{0x95,0x80},
    
{0x98,0x20},
{0xa0,0x40},
{0xa2,0x41},
{0xa4,0x40},
{0xa6,0x41},
{0xa8,0x44},
{0xaa,0x43},
{0xac,0x43},
{0xae,0x43},
     
//Page2
{0x03,0x02},
{0x10,0x00},
{0x13,0x00},
{0x18,0x1C},
{0x19,0x00},
{0x1a,0x00},
{0x1b,0x08},
{0x1C,0x00},
{0x1D,0x00},
{0x20,0x33},
{0x21,0xaa},
{0x22,0xa6},
{0x23,0xb0},
{0x31,0x99},
{0x32,0x00},
{0x33,0x00},
{0x34,0x3C},
{0x50,0x21},
{0x54,0x30},
{0x56,0xfe},
{0x62,0x78},
{0x63,0x9E},
{0x64,0x78},
{0x65,0x9E},
{0x72,0x7A},
{0x73,0x9A},
{0x74,0x7A},
{0x75,0x9A},
{0x82,0x09},
{0x84,0x09},
{0x86,0x09},
{0xa0,0x03},
{0xa8,0x1D},
{0xaa,0x49},
{0xb9,0x8A},
{0xbb,0x8A},
{0xbc,0x04},
{0xbd,0x10},
{0xbe,0x04},
{0xbf,0x10},
     
//Page10
{0x03,0x10},//
{0x10,0x03},//ISPCTL1
{0x12,0x30},//Y offet, dy offseet enable
{0x40,0x80},
{0x41,0x08},//
{0x50,0x78},//
     
{0x60,0x1f},
{0x61,0x80},
{0x62,0x80},
{0x63,0xf0},
{0x64,0x80},
     
//Page11
{0x03,0x11},
{0x10,0x99},
{0x11,0x0e},
{0x21,0x29},
{0x50,0x05},
{0x60,0x0f},
{0x62,0x43},
{0x63,0x63},
{0x74,0x08},
{0x75,0x08},
     
//Page12
{0x03,0x12},
{0x40,0x23},
{0x41,0x3b},
{0x50,0x05},
{0x70,0x1d},
{0x74,0x05},
{0x75,0x04},
{0x77,0x20},
{0x78,0x10},
{0x91,0x34},
{0xb0,0xc9},
{0xd0,0xb1},
     
//Page13
{0x03,0x13},
{0x10,0x3b},
{0x11,0x03},
{0x12,0x00},
{0x13,0x02},
{0x14,0x18},
    
{0x20,0x02},
{0x21,0x01},
{0x23,0x24},
{0x24,0x06},
{0x25,0x4a},
{0x28,0x00},
{0x29,0x78},
{0x30,0xff},
{0x80,0x0D},
{0x81,0x13},
     
{0x83,0x5d},
     
{0x90,0x03},
{0x91,0x02},
{0x93,0x3d},
{0x94,0x03},
{0x95,0x8f},
     
//Page14
{0x03,0x14},
{0x10,0x01},
{0x20,0x90},
{0x21,0xa0},
{0x22,0x53},
{0x23,0x40},
{0x24,0x3e},
     
//Page15 CmC
{0x03,0x15},
{0x10,0x03},
     
{0x14,0x3c},
{0x16,0x2c},
{0x17,0x2f},
    
{0x30,0xcb},
{0x31,0x61},
{0x32,0x16},
{0x33,0x23},
{0x34,0xce},
{0x35,0x2b},
{0x36,0x01},
{0x37,0x34},
{0x38,0x75},

{0x40,0x87},
{0x41,0x18},
{0x42,0x91},
{0x43,0x94},
{0x44,0x9f},
{0x45,0x33},
{0x46,0x00},
{0x47,0x94},
{0x48,0x14},
    
    
{0x03,0x16},
{0x10,0x01},
{0x30,0x00},
{0x31,0x09},
{0x32,0x1b},
{0x33,0x35},
{0x34,0x5d},
{0x35,0x7a},
{0x36,0x93},
{0x37,0xa7},
{0x38,0xb8},
{0x39,0xc6},
{0x3a,0xd2},
{0x3b,0xe4},
{0x3c,0xf1},
{0x3d,0xf9},
{0x3e,0xff},
     
//Page20 AE
{0x03,0x20},
{0x10,0x0c},
{0x11,0x04},
     
{0x20,0x01},
{0x28,0x3f},
{0x29,0xa3},
{0x2a,0xf0},
{0x2b,0xf4},
     
{0x30,0xf8},
    
{0x60,0xd5},
     
{0x68,0x34},
{0x69,0x6e},
{0x6A,0x28},
{0x6B,0xc8},
     
{0x70,0x34},//Y Target
     
{0x78,0x12},//Yth 1
{0x79,0x11},//Yth 2
{0x7A,0x23},
     
{0x83,0x00}, //EXP Normal 33.33 fps 
{0x84,0xaf}, 
{0x85,0xc8}, 
{0x86,0x00}, //EXPMin 6000.00 fps
{0x87,0xfa}, 
{0x88,0x02}, //EXP Max 10.00 fps 
{0x89,0x49}, 
{0x8a,0xf0}, 
{0x8B,0x3a}, //EXP100 
{0x8C,0x98}, 
{0x8D,0x30}, //EXP120 
{0x8E,0xd4}, 
    
{0x8f,0x04},
{0x90,0x93},
     
{0x91,0x02},
{0x92,0xd2},
{0x93,0xa8},
     
{0x98,0x8C},
{0x99,0x23},
     
{0x9c,0x06}, //EXP Limit 857.14 fps 
{0x9d,0xd6}, 
{0x9e,0x00}, //EXP Unit 
{0x9f,0xfa}, 
    
{0xb0,0x14},
{0xb1,0x14},
{0xb2,0xd0},
{0xb3,0x14},
{0xb4,0x1c},
{0xb5,0x48},
{0xb6,0x32},
{0xb7,0x2b},
{0xb8,0x27},
{0xb9,0x25},
{0xba,0x23},
{0xbb,0x22},
{0xbc,0x22},
{0xbd,0x21},
     
{0xc0,0x14},
     
{0xc8,0x70},
{0xc9,0x80},
     
//Page22 AWB
{0x03,0x22},
{0x10,0xe2},
{0x11,0x26},
{0x21,0x00},
     
{0x30,0x80},
{0x31,0x80},
{0x38,0x11},
{0x39,0x33},
{0x40,0xf0},
{0x41,0x33},
{0x42,0x33},
{0x43,0xf3},
{0x44,0x55},
{0x45,0x44},
{0x46,0x02},
     
     
{0x80,0x45},
{0x81,0x20},
{0x82,0x48},
     
{0x83,0x54},//54
{0x84,0x22},//22 29 27
{0x85,0x52},//54 53 50
{0x86,0x20},//22
     
{0x87,0x4c},
{0x88,0x34},
{0x89,0x30},
{0x8a,0x22},
     
{0x8b,0x00},
{0x8d,0x22},
{0x8e,0x71},
     
{0x8f,0x63},
{0x90,0x60},
{0x91,0x5c},
{0x92,0x59},
{0x93,0x55},
{0x94,0x50},
{0x95,0x48},
{0x96,0x3e},
{0x97,0x37},
{0x98,0x30},
{0x99,0x29},
{0x9a,0x26},
{0x9b,0x09},
     
{0x03,0x22},
{0x10,0xfb},
     
{0x03,0x20},
{0x10,0x9c},
     
{0x03,0x00},
{0x01,0xf0},
 CAMACQ_SUB_BTM_OF_DATA   
}
#endif /* _CAMACQ_API_C_ */
;
#if 1//swsw_vt_call

GLOBAL const U8 reg_sub_vt_init[][2]
#if defined(_CAMACQ_API_C_)
={
{0x03,0x00},
{0x01,0xf1},
{0x01,0xf3},
{0x01,0xf1},
     
{0x03,0x20}, //page 3
{0x10,0x1c}, //ae off
{0x03,0x22}, //page 4
{0x10,0x7b}, //awb off
     
//Page0
{0x03,0x00},
{0x10,0x00},
{0x11,0x95}, //On Fix Frame - None : 94, X Flip : 95, Y Flip : 96, XY : 97
{0x12,0x04},
{0x20,0x00},
{0x21,0x06},
{0x22,0x00},
{0x23,0x06},
{0x24,0x01},
{0x25,0xe0},
{0x26,0x02},
{0x27,0x80},
{0x40,0x01}, //Hblank 336
{0x41,0x50},
     
{0x42,0x00}, //Vblank 20
{0x43,0x14},
     
//BLC
{0x80,0x3e},
{0x81,0x96},
{0x82,0x90},
{0x83,0x00},
{0x84,0x2c},
     
{0x90,0x0e},
{0x91,0x0f},
{0x92,0x3a},
{0x93,0x30},
{0x94,0x88},
{0x95,0x80},
     
{0x98,0x20},
{0xa0,0x40},
{0xa2,0x41},
{0xa4,0x40},
{0xa6,0x41},
{0xa8,0x44},
{0xaa,0x43},
{0xac,0x44},
{0xae,0x43},
     
//Page2
     
{0x03,0x02}, //
{0x10,0x00}, //
{0x13,0x00}, //
{0x18,0x1C}, //
{0x19,0x00}, //
{0x1a,0x00}, //
{0x1b,0x08}, //
{0x1C,0x00}, //
{0x1D,0x00}, //
{0x20,0x33}, //
{0x21,0xaa}, //
{0x22,0xa6}, //
{0x23,0xb0}, //
{0x31,0x99}, //
{0x32,0x00}, //
{0x33,0x00}, //
{0x34,0x3C}, //
{0x50,0x21}, //
{0x54,0x30}, //
{0x56,0xfe}, //
{0x62,0x78}, //
{0x63,0x9E}, //
{0x64,0x78}, //
{0x65,0x9E}, //
{0x72,0x7A}, //
{0x73,0x9A}, //
{0x74,0x7A}, //
{0x75,0x9A}, //
{0x82,0x09}, // 
{0x84,0x09}, // 
{0x86,0x09}, // 
{0xa0,0x03}, //
{0xa8,0x1D}, //
{0xaa,0x49}, //
{0xb9,0x8A}, // 
{0xbb,0x8A}, // 
{0xbc,0x04}, // 
{0xbd,0x10}, // 
{0xbe,0x04}, // 
{0xbf,0x10}, //
     
//Page10
{0x03,0x10},//
{0x10,0x03},//ISPCTL1
{0x12,0x30},//Y offet, dy offseet enable
{0x40,0x80},
{0x41,0x30},//24->22
{0x50,0xa5},//
     
{0x60,0x1f},
{0x61,0x80},
{0x62,0x80},
{0x63,0xf0},
{0x64,0x80},
     
//Page11
{0x03,0x11},
{0x10,0x99},
{0x11,0x0e},
{0x21,0x29},
{0X50,0x05},
{0x60,0x0f},
{0x62,0x43},
{0x63,0x63},
{0x74,0x08},
{0x75,0x08},
     
//Page12
{0x03,0x12},
{0x40,0x23},
{0x41,0x3b},
{0x50,0x05},
{0x70,0x1d},
{0x74,0x05},
{0x75,0x04},
{0x77,0x20},
{0x78,0x10},
{0x91,0x34},
{0xb0,0xc9},
{0xd0,0xb1},
     
//Page13
{0x03,0x13},
{0x10,0x3b},
{0x11,0x03},
{0x12,0x00},
{0x13,0x02},
{0x14,0x18},
{0x20,0x02},
{0x21,0x01},
{0x23,0x24},
{0x24,0x06},
{0x25,0x4a},
{0x28,0x00},
{0x29,0x78},
{0x30,0xff},
{0x80,0x0D},
{0x81,0x13},
     
{0x83,0x5d},
     
{0x90,0x03},
{0x91,0x02},
{0x93,0x3d},
{0x94,0x03},
{0x95,0x8f},
     
//Page14
{0x03,0x14},
{0x10,0x01},
{0x20,0x90},
{0x21,0xa0},
{0x22,0x53},
{0x23,0x40},
{0x24,0x3e},
     
//Page 15 CmC
//Page15 CmC
{0x03,0x15},
{0x10,0x03},
     
{0x14,0x49},
{0x16,0x40},
{0x17,0x2f},
     
{0x30,0xcb},
{0x31,0x61},
{0x32,0x16},
{0x33,0x23},
{0x34,0xce},
{0x35,0x2b},
{0x36,0x03},
{0x37,0x37},
{0x38,0x7a},
     
{0x40,0x02},
{0x41,0x14},
{0x42,0x96},
{0x43,0xa4},
{0x44,0x00},
{0x45,0x24},
{0x46,0x04},
{0x47,0x82},
{0x48,0x82},
     
{0x03,0x16},
{0x10,0x01},
{0x30,0x00},
{0x31,0x14},
{0x32,0x23},
{0x33,0x3b},
{0x34,0x5d},
{0x35,0x79},
{0x36,0x8e},
{0x37,0x9f},
{0x38,0xaf},
{0x39,0xbd},
{0x3a,0xca},
{0x3b,0xdd},
{0x3c,0xec},
{0x3d,0xf7},
{0x3e,0xff},
     
//Page20 AE
{0x03,0x20},
{0x10,0x0c},
{0x11,0x04},
     
{0x20,0x01},
{0x28,0x3f},
{0x29,0xa3},
{0x2a,0x93},
{0x2b,0xf5},
     
{0x30,0xf8},
     
{0x60,0xc0},
     
{0x68,0x34},
{0x69,0x6e},
{0x6A,0x28},
{0x6B,0xc8},
     
{0x70,0x42}, //Y LVL
     
{0x78,0x12}, //yth1
{0x79,0x16}, //yth2
{0x7A,0x23}, //yth3
     
{0x83,0x00}, //EXP Normal 33.33 fps 
{0x84,0xaf}, 
{0x85,0xc8}, 
{0x86,0x00}, //EXPMin 6000.00 fps
{0x87,0xfa}, 
{0x88, 0x01}, //EXP Max 12.50 fps 
{0x89, 0xd4}, 
{0x8a, 0xc0},   
{0x8B,0x3a}, //EXP100 
{0x8C,0x98}, 
{0x8D,0x30}, //EXP120 
{0x8E,0xd4}, 
     
{0x8f,0x04},
{0x90,0x93},
     
{0x91,0x02}, //EXP Fix 10.00 fps
{0x92,0x40}, 
{0x93,0x2c}, 
     
{0x98,0x8C},
{0x99,0x23},
     
{0x9c,0x06}, //EXP Limit 857.14 fps 
{0x9d,0xd6}, 
{0x9e,0x00}, //EXP Unit 
{0x9f,0xfa}, 
     
{0xb0,0x14}, //
{0xb1,0x14}, // 08->14
{0xb2,0xf0}, // a0->f0->e0
{0xb3,0x14}, //
{0xb4,0x14}, //
{0xb5,0x38}, //
{0xb6,0x26}, //
{0xb7,0x20}, //
{0xb8,0x1d}, //
{0xb9,0x1b}, //
{0xba,0x1a}, //
{0xbb,0x19}, //
{0xbc,0x19}, //
{0xbd,0x18}, //
     
{0xc0,0x14},
     
{0xc8,0x70},
{0xc9,0x80},
     
//Page 22 AWB
{0x03,0x22}, //
{0x10,0xe2},
{0x11,0x2e}, // AD CMC off
{0x21,0x40}, //
     
{0x30,0x80}, //
{0x31,0x80}, //
{0x38,0x11}, //
{0x39,0x33},
{0x40,0xf0}, //
{0x41,0x33},
{0x42,0x33},
{0x43,0xf3}, //
{0x44,0x55},
{0x45,0x44},
{0x46,0x02}, //
     
{0x80,0x40}, //
{0x81,0x20}, //
{0x82,0x43}, //
     
{0x83,0x66}, // RMAX Default : 50 
{0x84,0x1f}, // RMIN Default : 20
{0x85,0x61}, // BMAX Default : 50
{0x86,0x20}, // BMIN Default : 20
     
{0x87,0x50}, // RMAXB Default : 50 
{0x88,0x45}, // RMINB Default : 3e
{0x89,0x2d}, // BMAXB Default : 2e
{0x8a,0x22}, // BMINB Default : 20
     
{0x8b,0x00},
{0x8d,0x21},
{0x8e,0x71},
     
{0x8f,0x63},
{0x90,0x60},
{0x91,0x5c},
{0x92,0x59},
{0x93,0x55},
{0x94,0x50},
{0x95,0x48},
{0x96,0x3e},
{0x97,0x37},
{0x98,0x30},
{0x99,0x29},
{0x9a,0x26},
{0x9b,0x09},
    
{0x03,0x22},
{0x10,0xfb},
     
{0x03,0x20},
{0x10,0x9c},
     
{0x03,0x00},
{0x01,0xf0},


CAMACQ_SUB_BTM_OF_DATA       
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
{0xff, 0xff},
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

/*==========================================================
//	CAMERA_CAMCORDER_FPS_14.91FIX 
//==========================================================*/


{0x03,0x00},
{0x01,0xf1},
{0x01,0xf3},
{0x01,0xf1},
     
{0x03,0x20}, //page 3
{0x10,0x1c}, //ae off
{0x03,0x22}, //page 4
{0x10,0x7b}, //awb off
     
//Page0
{0x03,0x00},
{0x10,0x00},
{0x11,0x95}, //On Fix Frame - None : 94, X Flip : 95, Y Flip : 96, XY : 97
{0x12,0x04},
{0x20,0x00},
{0x21,0x06},
{0x22,0x00},
{0x23,0x06},
{0x24,0x01},
{0x25,0xe0},
{0x26,0x02},
{0x27,0x80},
{0x40,0x01}, //Hblank 336
{0x41,0x50},
     
{0x42,0x00}, //Vblank 20
{0x43,0x14},
     
//BLC
{0x80,0x3e},
{0x81,0x96},
{0x82,0x90},
{0x83,0x00},
{0x84,0x2c},
     
{0x90,0x0e},
{0x91,0x0f},
{0x92,0x3a},
{0x93,0x30},
{0x94,0x88},
{0x95,0x80},
     
{0x98,0x20},
{0xa0,0x40},
{0xa2,0x41},
{0xa4,0x40},
{0xa6,0x41},
{0xa8,0x44},
{0xaa,0x43},
{0xac,0x44},
{0xae,0x43},
     
//Page2
     
{0x03,0x02}, //
{0x10,0x00}, //
{0x13,0x00}, //
{0x18,0x1C}, //
{0x19,0x00}, //
{0x1a,0x00}, //
{0x1b,0x08}, //
{0x1C,0x00}, //
{0x1D,0x00}, //
{0x20,0x33}, //
{0x21,0xaa}, //
{0x22,0xa6}, //
{0x23,0xb0}, //
{0x31,0x99}, //
{0x32,0x00}, //
{0x33,0x00}, //
{0x34,0x3C}, //
{0x50,0x21}, //
{0x54,0x30}, //
{0x56,0xfe}, //
{0x62,0x78}, //
{0x63,0x9E}, //
{0x64,0x78}, //
{0x65,0x9E}, //
{0x72,0x7A}, //
{0x73,0x9A}, //
{0x74,0x7A}, //
{0x75,0x9A}, //
{0x82,0x09}, // 
{0x84,0x09}, // 
{0x86,0x09}, // 
{0xa0,0x03}, //
{0xa8,0x1D}, //
{0xaa,0x49}, //
{0xb9,0x8A}, // 
{0xbb,0x8A}, // 
{0xbc,0x04}, // 
{0xbd,0x10}, // 
{0xbe,0x04}, // 
{0xbf,0x10}, //
     
//Page10
{0x03,0x10},//
{0x10,0x03},//ISPCTL1
{0x12,0x30},//Y offet, dy offseet enable
{0x40,0x80},
{0x41,0x22},//24->22
{0x50,0xa5},//
     
{0x60,0x1f},
{0x61,0x80},
{0x62,0x80},
{0x63,0xf0},
{0x64,0x80},
     
//Page11
{0x03,0x11},
{0x10,0x99},
{0x11,0x0e},
{0x21,0x29},
{0X50,0x05},
{0x60,0x0f},
{0x62,0x43},
{0x63,0x63},
{0x74,0x08},
{0x75,0x08},
     
//Page12
{0x03,0x12},
{0x40,0x23},
{0x41,0x3b},
{0x50,0x05},
{0x70,0x1d},
{0x74,0x05},
{0x75,0x04},
{0x77,0x20},
{0x78,0x10},
{0x91,0x34},
{0xb0,0xc9},
{0xd0,0xb1},
     
//Page13
{0x03,0x13},
{0x10,0x3b},
{0x11,0x03},
{0x12,0x00},
{0x13,0x02},
{0x14,0x18},
{0x20,0x02},
{0x21,0x01},
{0x23,0x24},
{0x24,0x06},
{0x25,0x4a},
{0x28,0x00},
{0x29,0x78},
{0x30,0xff},
{0x80,0x0D},
{0x81,0x13},
     
{0x83,0x5d},
     
{0x90,0x03},
{0x91,0x02},
{0x93,0x3d},
{0x94,0x03},
{0x95,0x8f},
     
//Page14
{0x03,0x14},
{0x10,0x01},
{0x20,0x90},
{0x21,0xa0},
{0x22,0x53},
{0x23,0x40},
{0x24,0x3e},
     
//Page 15 CmC
{0x03,0x15},
{0x10,0x03},
     
{0x14,0x3c},
{0x16,0x2c},
{0x17,0x2f},
     
{0x30,0xcb},
{0x31,0x61},
{0x32,0x16},
{0x33,0x23},
{0x34,0xce},
{0x35,0x2b},
{0x36,0x01},
{0x37,0x34},
{0x38,0x75},

{0x40,0x87},
{0x41,0x18},
{0x42,0x91},
{0x43,0x94},
{0x44,0x9f},
{0x45,0x33},
{0x46,0x00},
{0x47,0x94},
{0x48,0x14},
     
     
{0x03,0x16},
{0x10,0x01},
{0x30,0x00},
{0x31,0x09},
{0x32,0x1b},
{0x33,0x35},
{0x34,0x5d},
{0x35,0x7a},
{0x36,0x93},
{0x37,0xa7},
{0x38,0xb8},
{0x39,0xc6},
{0x3a,0xd2},
{0x3b,0xe4},
{0x3c,0xf1},
{0x3d,0xf9},
{0x3e,0xff},
     
     
//Page20 AE
{0x03,0x20},
{0x10,0x0c},
{0x11,0x04},
     
{0x20,0x01},
{0x28,0x3f},
{0x29,0xa3},
{0x2a,0x93},
{0x2b,0xf5},
     
{0x30,0xf8},
     
{0x60,0xc0},
     
{0x68,0x34},
{0x69,0x6e},
{0x6A,0x28},
{0x6B,0xc8},
     
{0x70,0x42}, //Y LVL
     
{0x78,0x12}, //yth1
{0x79,0x16}, //yth2
{0x7A,0x23}, //yth3
     
{0x83,0x00}, //EXP Normal 33.33 fps 
{0x84,0xaf}, 
{0x85,0xc8}, 
{0x86,0x00}, //EXPMin 6000.00 fps
{0x87,0xfa}, 
{0x88,0x01}, //EXP Max 20.00 fps 
{0x89,0x24}, 
{0x8a,0xf8},   
{0x8B,0x3a}, //EXP100 
{0x8C,0x98}, 
{0x8D,0x30}, //EXP120 
{0x8E,0xd4}, 
     
{0x8f,0x04},
{0x90,0x93},
     
{0x91,0x01}, //EXP Fix 15.00 fps
{0x92,0x7c}, 
{0x93,0xdc}, 
     
{0x98,0x8C},
{0x99,0x23},
     
{0x9c,0x06}, //EXP Limit 857.14 fps 
{0x9d,0xd6}, 
{0x9e,0x00}, //EXP Unit 
{0x9f,0xfa}, 
     
{0xb0,0x14}, //
{0xb1,0x14}, // 08->14
{0xb2,0xe0}, // a0->f0->e0
{0xb3,0x14}, //
{0xb4,0x14}, //
{0xb5,0x38}, //
{0xb6,0x26}, //
{0xb7,0x20}, //
{0xb8,0x1d}, //
{0xb9,0x1b}, //
{0xba,0x1a}, //
{0xbb,0x19}, //
{0xbc,0x19}, //
{0xbd,0x18}, //
     
{0xc0,0x14},
     
{0xc8,0x70},
{0xc9,0x80},
     
//Page 22 AWB
{0x03,0x22},
{0x10,0xe2},
{0x11,0x26},
{0x21,0x00},
     
{0x30,0x80},
{0x31,0x80},
{0x38,0x11},
{0x39,0x33},
{0x40,0xf0},
{0x41,0x33},
{0x42,0x33},
{0x43,0xf3},
{0x44,0x55},
{0x45,0x44},
{0x46,0x02},
     
{0x80,0x45},
{0x81,0x20},
{0x82,0x48},
     
{0x83,0x54},//54
{0x84,0x22},//22 29 27
{0x85,0x52},//54 53 50
{0x86,0x20},//22
     
{0x87,0x4c},
{0x88,0x34},
{0x89,0x30},
{0x8a,0x22},
     
{0x8b,0x00},
{0x8d,0x22},
{0x8e,0x71},
     
     
{0x8f,0x63},
{0x90,0x60},
{0x91,0x5c},
{0x92,0x59},
{0x93,0x55},
{0x94,0x50},
{0x95,0x48},
{0x96,0x3e},
{0x97,0x37},
{0x98,0x30},
{0x99,0x29},
{0x9a,0x26},
{0x9b,0x09},
    
{0x03,0x22},
{0x10,0xfb},
     
{0x03,0x20},
{0x10,0x9c},
     
{0x03,0x00},
{0x01,0xf0},

{0xFF, 0xFF},  
CAMACQ_SUB_BTM_OF_DATA

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


{0x03,0x00},
{0x50,0x05}, //Test Pattern

{0x03,0x11},
{0x10,0x98},

{0x03,0x12},
{0x40,0x22},
{0x70,0x1c},

{0x03,0x13},
{0x10,0x3a},
{0x80,0x0c},

{0x03,0x14},
{0x10,0x00},

{0x03,0x15},
{0x10,0x02},

{0x03,0x16},
{0x10,0x00},

{0x03,0x20},
{0x10,0x0c},

{0x03,0x22},
{0x10,0x7b},


CAMACQ_SUB_BTM_OF_DATA

}
#endif /* _CAMACQ_API_C_ */
;

GLOBAL const U8 reg_sub_dtp_off[][2]
#if defined(_CAMACQ_API_C_)
={

{0x03,0x00},
{0x50,0x00}, //Test Pattern

{0x03,0x11},
{0x10,0x99},

{0x03,0x12},
{0x40,0x23},
{0x70,0x1d},

{0x03,0x13},
{0x10,0x3b},
{0x80,0x0d},

{0x03,0x14},
{0x10,0x01},

{0x03,0x15},
{0x10,0x03},

{0x03,0x16},
{0x10,0x01},

{0x03,0x20},
{0x10,0x8c},

{0x03,0x22},
{0x10,0xfb},


CAMACQ_SUB_BTM_OF_DATA

}
#endif /* _CAMACQ_API_C_ */
;


#undef GLOBAL

#endif /* _CAMACQ_SR200PC10_H_ */
