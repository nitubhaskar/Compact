/************************************************************************************************/
/*                                                                                              */
/*  Copyright 2010  Broadcom Corporation                                                        */
/*                                                                                              */
/*     Unless you and Broadcom execute a separate written software license agreement governing  */
/*     use of this software, this software is licensed to you under the terms of the GNU        */
/*     General Public License version 2 (the GPL), available at                                 */
/*                                                                                              */
/*          http://www.broadcom.com/licenses/GPLv2.php                                          */
/*                                                                                              */
/*     with the following added to such license:                                                */
/*                                                                                              */
/*     As a special exception, the copyright holders of this software give you permission to    */
/*     link this software with independent modules, and to copy and distribute the resulting    */
/*     executable under terms of your choice, provided that you also meet, for each linked      */
/*     independent module, the terms and conditions of the license of that module.              */
/*     An independent module is a module which is not derived from this software.  The special  */
/*     exception does not apply to any modifications of the software.                           */
/*                                                                                              */
/*     Notwithstanding the above, under no circumstances may you combine this software in any   */
/*     way with any other Broadcom software provided under a license other than the GPL,        */
/*     without Broadcom's express prior written consent.                                        */
/*                                                                                              */
/*     Date     : Generated on 9/22/2010 18:51:34                                             */
/*     RDB file : //ATHENA/                                                                   */
/************************************************************************************************/

#ifndef __BRCM_RDB_DSP_PCM_H__
#define __BRCM_RDB_DSP_PCM_H__

#define DSP_PCM_PCMRATE_R_OFFSET                                          0x00000A84
#define DSP_PCM_PCMRATE_R_TYPE                                            UInt16
#define DSP_PCM_PCMRATE_R_RESERVED_MASK                                   0x00000000
#define    DSP_PCM_PCMRATE_R_MOD_SHIFT                                    14
#define    DSP_PCM_PCMRATE_R_MOD_MASK                                     0x0000C000
#define    DSP_PCM_PCMRATE_R_DATAFACTOR_SHIFT                             7
#define    DSP_PCM_PCMRATE_R_DATAFACTOR_MASK                              0x00003F80
#define    DSP_PCM_PCMRATE_R_BITFACTOR_SHIFT                              0
#define    DSP_PCM_PCMRATE_R_BITFACTOR_MASK                               0x0000007F

#define DSP_PCM_PCMDIR_R_OFFSET                                           0x00000F4C
#define DSP_PCM_PCMDIR_R_TYPE                                             UInt16
#define DSP_PCM_PCMDIR_R_RESERVED_MASK                                    0x00000007
#define    DSP_PCM_PCMDIR_R_PCMDIR_SHIFT                                  3
#define    DSP_PCM_PCMDIR_R_PCMDIR_MASK                                   0x0000FFF8

#define DSP_PCM_PCMDOR_R_OFFSET                                           0x00000F4E
#define DSP_PCM_PCMDOR_R_TYPE                                             UInt16
#define DSP_PCM_PCMDOR_R_RESERVED_MASK                                    0x00000007
#define    DSP_PCM_PCMDOR_R_PCMDOR_SHIFT                                  3
#define    DSP_PCM_PCMDOR_R_PCMDOR_MASK                                   0x0000FFF8

#define DSP_PCM_PCMRATE_OFFSET                                            0x0000E542
#define DSP_PCM_PCMRATE_TYPE                                              UInt16
#define DSP_PCM_PCMRATE_RESERVED_MASK                                     0x00000000
#define    DSP_PCM_PCMRATE_MOD_SHIFT                                      14
#define    DSP_PCM_PCMRATE_MOD_MASK                                       0x0000C000
#define    DSP_PCM_PCMRATE_DATAFACTOR_SHIFT                               7
#define    DSP_PCM_PCMRATE_DATAFACTOR_MASK                                0x00003F80
#define    DSP_PCM_PCMRATE_BITFACTOR_SHIFT                                0
#define    DSP_PCM_PCMRATE_BITFACTOR_MASK                                 0x0000007F

#define DSP_PCM_PCMDIR_OFFSET                                             0x0000E7A6
#define DSP_PCM_PCMDIR_TYPE                                               UInt16
#define DSP_PCM_PCMDIR_RESERVED_MASK                                      0x00000007
#define    DSP_PCM_PCMDIR_PCMDIR_SHIFT                                    3
#define    DSP_PCM_PCMDIR_PCMDIR_MASK                                     0x0000FFF8

#define DSP_PCM_PCMDOR_OFFSET                                             0x0000E7A7
#define DSP_PCM_PCMDOR_TYPE                                               UInt16
#define DSP_PCM_PCMDOR_RESERVED_MASK                                      0x00000007
#define    DSP_PCM_PCMDOR_PCMDOR_SHIFT                                    3
#define    DSP_PCM_PCMDOR_PCMDOR_MASK                                     0x0000FFF8

#endif /* __BRCM_RDB_DSP_PCM_H__ */


