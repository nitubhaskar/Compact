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

#ifdef  VOICESOLUTION_DUMP
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/file.h>
#endif

typedef struct
{
	Int16 * pUldata;
	Int16 * pDldata;
	Int16 * pOutput_buf;
	
}TVoIF_CallBackData;

static TVoIF_CallBackData sgCallbackData;

//
// APIs 
//
#define	DIAMONDVOICE_FRAME	160

extern void DiamondVoice_Exe(void);
extern void DiamondVoice_Config(int mode, short *Speech_buf, short *Noise_buf, short *Output_buf);

#ifdef  VOICESOLUTION_DUMP
#define DIAMOND_BEFORE_FILE_PATH "/data/Diamond_Before.pcm"
#define DIAMOND_AFTER_FILE_PATH "/data/Diamond_After.pcm"

typedef struct 
{
    struct file *fp_in, *fp_out;
    struct work_struct work_write;   
}TVoif_Dump;

static TVoif_Dump sgDumpData;

static void work_output_thread(struct work_struct *work)
{
//    printk("\n===============  work_output_thread  =============== \n");

    sgDumpData.fp_in->f_op->write( sgDumpData.fp_in, (const char __user *)sgCallbackData.pDldata, sizeof(Int16) * DIAMONDVOICE_FRAME, &sgDumpData.fp_in->f_pos );
    sgDumpData.fp_out->f_op->write( sgDumpData.fp_out, (const char __user *)sgCallbackData.pOutput_buf, sizeof(Int16) * DIAMONDVOICE_FRAME, &sgDumpData.fp_out->f_pos );
}
#endif

static void VOIF_CB_Fxn (Int16 * ulData, Int16 *dlData, UInt32 sampleCount, UInt8 isCall16K)
{

//    printk("\n===============  VOIF_CB_Fxn  ===============isCall16K %d\n", isCall16K);

    void (*pDiamondVoiceExe)() = symbol_get(DiamondVoice_Exe);

    if ( isCall16K ) return; /* Not Support WB case */

//    printk("\n===============1  VOIF_CB_Fxn  dlData= 0x%p, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x===============\n", dlData, dlData[0],dlData[1],dlData[2],dlData[3],dlData[4],dlData[5],dlData[6],dlData[7]);

	memcpy(sgCallbackData.pUldata, ulData, sizeof(Int16) * DIAMONDVOICE_FRAME);
	memcpy(sgCallbackData.pDldata, dlData, sizeof(Int16) * DIAMONDVOICE_FRAME);

    pDiamondVoiceExe();

    memcpy(dlData, sgCallbackData.pOutput_buf, sizeof(Int16) * DIAMONDVOICE_FRAME);

//    printk("\n===============2  VOIF_CB_Fxn  dlData= 0x%p, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x===============\n", dlData, dlData[0],dlData[1],dlData[2],dlData[3],dlData[4],dlData[5],dlData[6],dlData[7]);

#ifdef  VOICESOLUTION_DUMP
        schedule_work(&sgDumpData.work_write);
#endif

    return;
}

// Start voif 
void VoIF_init(AudioMode_t mode)
{
    short DiamondVoice_Mode;
    short (*pDiamondVoice_Config)(int , short *, short *, short *) = symbol_get(DiamondVoice_Config);

    printk("\n===============  VoIF_init : AudioMode= %d ===============\n", mode);

    sgCallbackData.pOutput_buf = (Int16 *)vmalloc(sizeof(Int16) * DIAMONDVOICE_FRAME);
    sgCallbackData.pDldata = (Int16 *)vmalloc(sizeof(Int16) * DIAMONDVOICE_FRAME);
    sgCallbackData.pUldata = (Int16 *)vmalloc(sizeof(Int16) * DIAMONDVOICE_FRAME);

    DiamondVoice_Mode = pDiamondVoice_Config(mode, sgCallbackData.pDldata, sgCallbackData.pUldata, sgCallbackData.pOutput_buf);    

    if ( DiamondVoice_Mode != 0x00 )
    {
        printk("\nDiamondVoice_Mode=%d\n", DiamondVoice_Mode);
        AUDDRV_VOIF_Start (VOIF_CB_Fxn);

#ifdef VOICESOLUTION_DUMP
        sgDumpData.fp_in = filp_open(DIAMOND_BEFORE_FILE_PATH,  O_WRONLY |O_TRUNC |O_LARGEFILE |O_CREAT,0666);
        sgDumpData.fp_out = filp_open(DIAMOND_AFTER_FILE_PATH,  O_WRONLY |O_TRUNC |O_LARGEFILE |O_CREAT,0666);
        INIT_WORK(&sgDumpData.work_write, work_output_thread);
#endif

    }
}

void VoIF_Deinit()
{
    AUDDRV_VOIF_Stop ();

	printk("\n===============  VoIF_Deinit  ===============\n");

	vfree(sgCallbackData.pOutput_buf);
	vfree(sgCallbackData.pDldata);
	vfree(sgCallbackData.pUldata);
	
#ifdef VOICESOLUTION_DUMP
    if ( sgDumpData.fp_in )
        filp_close(sgDumpData.fp_in, NULL);

    if ( sgDumpData.fp_out )
        filp_close(sgDumpData.fp_out, NULL);
#endif
	
    return;
}
 

