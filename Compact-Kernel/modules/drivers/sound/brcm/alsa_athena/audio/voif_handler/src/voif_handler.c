/*******************************************************************************************
Copyright 2010 Broadcom Corporation.  All rights reserved.

Unless you and Broadcom execute a separate written software license agreement
governing use of this software, this software is licensed to you under the
terms of the GNU General Public License version 2, available at
http://www.gnu.org/copyleft/gpl.html (the "GPL").

Notwithstanding the above, under no circumstances may you combine this software
in any way with any other Broadcom software provided under a license other than
the GPL, without Broadcom's express prior written consent.
*******************************************************************************************/
/**
*
*   @file   voif_handler.c
*
*   @brief  PCM data interface to DSP. 
*           It is used to hook with customer's downlink voice processing module. 
*           Customer will implement this.
*
****************************************************************************/


#include "mobcom_types.h"
#include "audio_consts.h"
#include "voif_handler.h"
#include "audio_vdriver_voif.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

typedef struct
{
	struct work_struct mwork; //worker thread data structure
	struct workqueue_struct *pWorkqueue_VoIF;
}TVoIF_ThreadData;

typedef struct
{
	Int16 * pUldata;
	Int16 * pDldata;
	Int16 * pOutput_buf;
	
}TVoIF_CallBackData;

static TVoIF_ThreadData sgVoIFThreadData;
static TVoIF_CallBackData sgCallbackData;

//
// APIs 
//
#define	DIAMONDVOICE_FRAME	160

extern void DiamondVoice_Exe(void);
extern void DiamondVoice_Config(int mode, short *Speech_buf, short *Noise_buf, short *Output_buf);

static void VOIF_CB_Fxn (Int16 * ulData, Int16 *dlData, UInt32 sampleCount, UInt8 isCall16K)
{

//    printk("\n===============  VOIF_CB_Fxn  ===============isCall16K %d\n", isCall16K);

    void (*pDiamondVoiceExe)() = symbol_get(DiamondVoice_Exe);

    if ( isCall16K ) return; /* Not Support WB case */

	memcpy(sgCallbackData.pUldata, ulData, sizeof(Int16) * DIAMONDVOICE_FRAME);
	memcpy(sgCallbackData.pDldata, dlData, sizeof(Int16) * DIAMONDVOICE_FRAME);

    pDiamondVoiceExe();

    memcpy(dlData, sgCallbackData.pOutput_buf, sizeof(Int16) * DIAMONDVOICE_FRAME);

    return;
}

// Start voif 
void VoIF_init(AudioMode_t mode)
{
    short DiamondVoice_Mode;
    short (*pDiamondVoice_Config)(int , short *, short *, short *) = symbol_get(DiamondVoice_Config);

	printk("\n===============  VoIF_init  ===============\n");

	sgCallbackData.pOutput_buf = (Int16 *)vmalloc(sizeof(Int16) * DIAMONDVOICE_FRAME);
	sgCallbackData.pDldata = (Int16 *)vmalloc(sizeof(Int16) * DIAMONDVOICE_FRAME);
	sgCallbackData.pUldata = (Int16 *)vmalloc(sizeof(Int16) * DIAMONDVOICE_FRAME);

    DiamondVoice_Mode = pDiamondVoice_Config(mode, sgCallbackData.pDldata, sgCallbackData.pUldata, sgCallbackData.pOutput_buf);    

    if ( DiamondVoice_Mode != 0x00 )
    AUDDRV_VOIF_Start (VOIF_CB_Fxn);
}

void VoIF_Deinit()
{
    AUDDRV_VOIF_Stop ();

	printk("\n===============  VoIF_Deinit  ===============\n");

	vfree(sgCallbackData.pOutput_buf);
	vfree(sgCallbackData.pDldata);
	vfree(sgCallbackData.pUldata);
	
    return;
}
 

