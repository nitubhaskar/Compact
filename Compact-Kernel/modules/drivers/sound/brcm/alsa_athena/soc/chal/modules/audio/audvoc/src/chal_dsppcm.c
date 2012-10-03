/*****************************************************************************
*  Copyright 2001 - 2011 Broadcom Corporation.  All rights reserved.
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
 

#include "chal_types.h"
#include "chal_common.h"
#include "brcm_rdb_dsp_pcm.h"
#include "chal_dsppcm.h"
#include "brcm_rdb_util.h"


static CHAL_HANDLE handle = NULL;

/**
*
*  @brief  Initialize CHAL DSP PCM
*
*  @param  baseAddr  (in) mapped base address 
*
*  @return CHAL handle
*****************************************************************************/
CHAL_HANDLE chal_dsppcm_init(cUInt32 baseAddr)
{
    //chal_dprintf( CDBG_INFO, "chal_dsppcm_init, base=0x%x\n", baseAddr);
  
    // Don't re-init the block
    if (handle) {
        chal_dprintf(CDBG_ERRO, "ERROR: chal_dsppcm_init: already initialized\n");
        return handle;
    }
        
    handle = (CHAL_HANDLE)baseAddr;
         
    return handle;
}

/**
*
*  @brief  De-Initialize CHAL DSP PCM
*
*  @param  handle  (in) chal handle
*
*  @return none
*****************************************************************************/
cVoid chal_dsppcm_deinit(CHAL_HANDLE handle)
{
    //chal_dprintf( CDBG_INFO, "chal_dsppcm_deinit\n");  
  
    handle = NULL;
}


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
cVoid chal_dsppcm_set_mode(CHAL_HANDLE handle, CHAL_DSPPCM_Mode mode, cUInt8 bitfactor, cUInt8 datafactor)
{
    cUInt32 base;
	cUInt16 val;

	if (!handle) {
        chal_dprintf(CDBG_ERRO, "ERROR: chal_dsppcm_set_mode: not initialized\n");
        return;
    }

    //chal_dprintf( CDBG_INFO, "chal_dsppcm_set_mode: %d\n", mode);  

    base = (cUInt32)handle;
 
	val = (mode << DSP_PCM_PCMRATE_R_MOD_SHIFT) & DSP_PCM_PCMRATE_R_MOD_MASK;
    if (mode == CHAL_DSPPCM_Mode_User)
	{
		val |= (bitfactor << DSP_PCM_PCMRATE_R_BITFACTOR_SHIFT) & DSP_PCM_PCMRATE_R_BITFACTOR_MASK;
		val |= (datafactor << DSP_PCM_PCMRATE_R_DATAFACTOR_SHIFT) & DSP_PCM_PCMRATE_R_DATAFACTOR_MASK;
	}

	BRCM_WRITE_REG(base, DSP_PCM_PCMRATE_R, val);
}



