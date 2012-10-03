/*****************************************************************************
*  Copyright 2001 - 20011 Broadcom Corporation.  All rights reserved.
*
*  Unless you and Broadcom execute a separate written software license
*  agreement governing use of this software, this software is licensed to you
*  under the terms of the GNU General Public License version 2, available at
*  http://www.gnu.org/licenses/old-license/gpl-2.0.html (the "GPL").
*
*  Notwithstanding the above, under no circumstances may you combine this
*  software in any way with any other Broadcom software provided under a
*  license other than the GPL, without Broadcom's express prior written
*  consent.
*
*****************************************************************************/
/**
*
* @file  chal_dsppcm.h
*
* @brief DSP PCM cHAL interface
*
* @note
*****************************************************************************/

#ifndef _CHAL_DSPPCM_H_
#define _CHAL_DSPPCM_H_

#ifdef __cplusplus
extern "C" {
#endif


/**
 * @addtogroup cHAL_Interface 
 * @{
 */

 
typedef enum
{
    CHAL_DSPPCM_Mode_8KHz,  ///< 0: PCM works in 8K mode. (Bit rate 200K, Data rate 8K)
    CHAL_DSPPCM_Mode_16KHz, ///< 1: PCM works in 16K mode. (Bit rate 400K, Data rate 16K)
    CHAL_DSPPCM_Mode_User   ///< 2: PCM works in the user defined mode
} CHAL_DSPPCM_Mode;


/**
*
*  @brief  Initialize CHAL DSP PCM
*
*  @param  baseAddr  (in) mapped base address 
*
*  @return CHAL handle
*****************************************************************************/
CHAL_HANDLE chal_dsppcm_init(cUInt32 baseAddr);

/**
*
*  @brief  De-Initialize CHAL DSP PCM
*
*  @param  handle  (in) chal handle
*
*  @return none
*****************************************************************************/
cVoid chal_dsppcm_deinit(CHAL_HANDLE handle);

/**
*
*  @brief  Set PCM mode
*
*  @param  handle  (in) chal handle
*          mode  (in) mode to set
*          bitfactor/datafactor  (in) used for user mode
*
*  @return none
*****************************************************************************/
cVoid chal_dsppcm_set_mode(CHAL_HANDLE handle, CHAL_DSPPCM_Mode mode, cUInt8 bitfactor, cUInt8 datafactor);



#ifdef __cplusplus
}
#endif
#endif

