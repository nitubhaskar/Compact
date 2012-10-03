/*******************************************************************************
* Copyright 2011 Broadcom Corporation.  All rights reserved.
*
* 	@file	 arch/arm/plat-bcmap/csl/csl_dsi.c
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
/**
*
*   @file   csl_dsi.c
*
*   @brief  DSI Controller Implementation 
*
*   DSI Controller Implementation
*
****************************************************************************/

#define __CSL_DSI_USE_INT__
#define __KERNEL__

#include <linux/string.h>
#include <linux/workqueue.h>
#include <linux/time.h>
#include <linux/interrupt.h>
#include <plat/mobcom_types.h>
#include <plat/osabstract/ostypes.h>
#include <plat/osabstract/ostask.h>
#include <plat/osabstract/ossemaphore.h>
#include <plat/osabstract/osqueue.h>
#include <mach/irqs.h>
#include <mach/memory.h>
#include <linux/kernel.h>
#include <asm/io.h>


#include <plat/dma_drv.h>
#include <plat/rdb/brcm_rdb_syscfg.h>
#include <plat/rdb/brcm_rdb_lcdc.h>
#include <plat/rdb/brcm_rdb_sysmap.h>
#include <plat/chal/chal_common.h>
#include <plat/chal/chal_dsi.h>
#include <plat/csl/csl_lcd.h>
#include <plat/csl/csl_dsi.h>
#include <plat/types.h>
#include <plat/osdal_os.h>
#include <plat/osdal_os_driver.h>
#include <plat/osdal_os_service.h>


#define DSI_INITIALIZED         0x13579BDF
#define DSI_INST_COUNT          (UInt32)1
#define DSI_MAX_CLIENT          4
#define DSI_CM_MAX_HANDLES      4

// MIPIDSI and CMI clocks share enable bit and
// so also share this config struct:
typedef struct
{
    cUInt32 speedMIPIDSI;
    cUInt32 speedCMI;
} CHAL_CLK_CFG_MIPIDSI_CMI_T;

// MIPIDSIAFE clock uses this config struct:
typedef struct
{
    cUInt8 dsiDiv;
    cUInt8 cam2Div;
    cUInt8 cam1Div;
} CHAL_CLK_CFG_MIPIDSIAFE_T;


// DSI Core clk tree configuration / settings
typedef struct {
    UInt32   hsPllReq_MHz;   // in:  PLL freq requested
    UInt32  escClkIn_MHz;   // in:  ESC clk in requested
    UInt32   hsPllSet_MHz;   // out: PLL freq set
    UInt32   hsBitClk_MHz;   // out: end HS bit clock
    UInt32   escClk_MHz;     // out: ESC clk after req inDIV
    UInt32  hsClkDiv;       // out: HS  CLK Divider Register value 
    UInt32  escClkDiv;      // out: ESC CLK Divider Register value
    UInt32  hsPll_P1;       // out: ATHENA's PLL setting
    UInt32  hsPll_N_int;    // out: ATHENA's PLL setting
    UInt32  hsPll_N_frac;   // out: ATHENA's PLL setting
} DSI_CLK_CFG_T;

// DSI Client 
typedef struct {
    CSL_LCD_HANDLE          lcdH;
    Boolean                 open;    
    Boolean                 hasLock;
} DSI_CLIENT_t, *DSI_CLIENT;

// DSI Command Mode 
typedef struct {
    CHAL_DSI_CM_CFG_t       chalCmCfg;
    DSI_CLIENT              client;
    Boolean                 configured;      
    Boolean                 dcsUseContinue;  
    UInt32                  dcsCmndStart;    
    UInt32                  dcsCmndCont;     
    CHAL_DSI_DE1_COL_MOD_t  cm;              
    CHAL_DSI_TE_MODE_t      teMode;
    UInt32                  bpp_wire;         
    UInt32                  bpp_dma;          
    UInt32                  wc_rshift;        
} DSI_CM_HANDLE_t, *DSI_CM_HANDLE;


// DSI Interface 
typedef struct
{
    CHAL_HANDLE             chalH;
    UInt32                  bus;
    UInt32                  init;
    UInt32                  initOnce;
    Boolean                 ulps;
    UInt32                  dsiCoreRegAddr;
    UInt32                  clients;
    DSI_CLK_CFG_T           clkCfg;        // HS & ESC Clk configuration
    Task_t                  updReqT;
    Queue_t                 updReqQ;
    Semaphore_t             semaDsiW;      
    Semaphore_t             semaDsi;
    Semaphore_t             semaInt;
    Semaphore_t             semaDma;
	irq_handler_t			lisr;
    void                    (*hisr) (void);
    void                    (*task) (void);
    void                    (*dma_cb) ( DMADRV_CALLBACK_STATUS_t );
#ifndef __KERNEL__
    Interrupt_t             iHisr;
#endif
	UInt32					interruptId;
	spinlock_t 				bcm_dsi_spin_Lock;
    DSI_CLIENT_t            client [ DSI_MAX_CLIENT ];
    DSI_CM_HANDLE_t         chCm [ DSI_CM_MAX_HANDLES ];  
} DSI_HANDLE_t, *DSI_HANDLE;

typedef struct {
    Dma_Buffer_List         dmaBuffList;   // local DMA config                 
    Dma_Data                dmaData;       // local DMA config
    Dma_Chan_Info           dmaChInfo;     // local DMA config
    DMA_CHANNEL             dmaCh;         // local DMA config
    DSI_HANDLE              dsiH;          // CSL DSI handle
    DSI_CLIENT              clientH;       // CSL DSI Client handle
    CSL_LCD_UPD_REQ_T       updReq;        // update Request
} DSI_UPD_REQ_MSG_T;

static DSI_HANDLE_t         dsiBus[DSI_INST_COUNT];

#define DSI_PKT_SIZE_MAX    (0xFFFF - 1)  // 16 bits = 65535 - 1 (DCS cmnd)
#define DSI_PKT_RPT_MAX     0x3FFF        // 14 bits = 16383

#define FLUSH_Q_SIZE        1

#define TX_PKT_ENG_0        ((UInt8)0)
#define TX_PKT_ENG_1        ((UInt8)1)


// move to \public\msconsts.h
// IDLE, LOWEST, BELOW_NORMAL, NORMAL, ABOVE_NORMAL, HIGHEST        
#define TASKPRI_DSI             (TPriority_t)(ABOVE_NORMAL)
#define STACKSIZE_DSI           STACKSIZE_BASIC*5
#define HISRSTACKSIZE_DSISTAT  (256 + RESERVED_STACK_FOR_LISR)

static Boolean        cslDsiClkTreeInit( DSI_HANDLE dsiH, const DSI_CLK_T *hsClk, 
                        const DSI_CLK_T *escClk );

static Boolean        cslDsiClkTreeEna ( DSI_HANDLE dsiH );

static void           cslDsiEnaIntEvent( DSI_HANDLE dsiH, 
                        UInt32 intEventMask );
static CSL_LCD_RES_T  cslDsiWaitForStatAny_Poll( DSI_HANDLE dsiH, UInt32 mask,
                        UInt32 *intStat, UInt32 tout_msec );
static CSL_LCD_RES_T  cslDsiWaitForInt ( DSI_HANDLE dsiH, UInt32 tout_msec  );
static CSL_LCD_RES_T  cslDsiDmaStart ( DSI_UPD_REQ_MSG_T* updMsg );
static CSL_LCD_RES_T  cslDsiDmaStop ( DSI_UPD_REQ_MSG_T* updMsg );
static void           cslDsiClearAllFifos( DSI_HANDLE dsiH );

static void DumpDSIRegister();


static UInt32         dsiIntStat;

#define CSL_DSI0_IRQ        IRQ_DSI
#define CSL_DSI0_BASE_ADDR  DSI_BASE_ADDR

static OSDAL_CLK_HANDLE usbclk = NULL;
static OSDAL_CLK_HANDLE dsiclk = NULL;
static OSDAL_CLK_HANDLE mipidsi = NULL;

/////////////////////////////////////////////////////////////////////////////////////
#define VMALLOC_END (PAGE_OFFSET + 0x28000000)
#define PHYS_OFFSET     (CONFIG_BCM_RAM_BASE+CONFIG_BCM_RAM_START_RESERVED_SIZE)

#define IO_START_PA                     (0x34000000) /* HUB clock manager reg base */
#define IO_START_VA                     (VMALLOC_END)

#ifndef HW_IO_PHYS_TO_VIRT
#define HW_IO_PHYS_TO_VIRT(phys)        ((phys) - IO_START_PA + IO_START_VA)
#endif
#define HW_IO_VIRT_TO_PHYS(virt)        ((virt) - IO_START_VA + IO_START_PA)
#define CONSISTENT_DMA_SIZE SZ_4M
//////////////////////////////////////////////////////////////////////////////////////

//#define printk(format, arg...)	do {   } while (0)

//*****************************************************************************
//
// Function Name:  cslDsi0Stat_LISR
// 
// Description:    DSI Controller 0 LISR 
//
//*****************************************************************************
#ifdef __KERNEL__
static irqreturn_t cslDsi0Stat_LISR ( int i, void * j)
#else
static void cslDsi0Stat_LISR ( void )
#endif
{
       DSI_HANDLE dsiH = &dsiBus[0];
    
    dsiIntStat = chal_dsi_get_int ( dsiH->chalH ); 

    if (dsiIntStat & ~CHAL_DSI_ISTAT_TXPKT1_DONE)
	    printk(KERN_ERR "DSI int stat is 0x%08x\n", dsiIntStat);

    chal_dsi_ena_int ( dsiH->chalH, 0 );
    chal_dsi_clr_int ( dsiH->chalH, 0xFFFFFFFF );
    
    OSSEMAPHORE_Release ( dsiH->semaInt );

#ifdef __KERNEL__
return IRQ_HANDLED;
#endif
}

//*****************************************************************************
//
// Function Name:  cslDsi0Stat_HISR
// 
// Description:    DSI Controller 0 HISR 
//
//*****************************************************************************
static void cslDsi0Stat_HISR ( void )
{
    DSI_HANDLE dsiH = &dsiBus[0];

    OSSEMAPHORE_Release ( dsiH->semaInt );
}


//*****************************************************************************
//
// Function Name:  cslDsi0EofDma
// 
// Description:    DSI Controller 0 EOF DMA 
//
//*****************************************************************************
static void cslDsi0EofDma ( DMADRV_CALLBACK_STATUS_t status )
{
    DSI_HANDLE dsiH = &dsiBus[0];

    if( status != DMADRV_CALLBACK_OK )
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsi0EofDma(): ERR DMA!\n\r");
    }

    OSSEMAPHORE_Release ( dsiH->semaDma );
}

//*****************************************************************************
//
// Function Name:  cslDsi0UpdateTask
// 
// Description:    DSI Controller 0 Update Task
//
//*****************************************************************************
static void cslDsi0UpdateTask (void)
{
    DSI_UPD_REQ_MSG_T   updMsg;
    OSStatus_t          osStat;
    CSL_LCD_RES_T       res;
    DSI_HANDLE          dsiH = &dsiBus[0];

    for (;;)
    {
        res  = CSL_LCD_OK;
        
        // Wait for update request
        OSQUEUE_Pend ( dsiH->updReqQ, (QMsg_t*)&updMsg, TICKS_FOREVER);

        // Wait For signal from eof DMA
        osStat = OSSEMAPHORE_Obtain ( dsiH->semaDma, 
            TICKS_IN_MILLISECONDS ( updMsg.updReq.timeOut_ms ) );
        
        if ( osStat != OSSTATUS_SUCCESS)
        {
            if( osStat == OSSTATUS_TIMEOUT )
            {
                LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsi0UpdateTask: "
                    "TIMED OUT While waiting for EOF DMA\n\r" ); 
                res = CSL_LCD_OS_TOUT;
            }    
            else
            {
                LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsi0UpdateTask: "
                    "OS ERR While waiting for EOF DMA\n\r" ); 
                res = CSL_LCD_OS_ERR;
            }  
            
            cslDsiDmaStop( &updMsg );
        }
        
        if ( res == CSL_LCD_OK )
        {
            res = cslDsiWaitForInt( dsiH, updMsg.updReq.timeOut_ms );
        } 
        else
        {
            cslDsiWaitForInt( dsiH, 1 );        
        }    
        
        chal_dsi_de1_enable ( dsiH->chalH, FALSE  );
        chal_dsi_cmnd_start ( dsiH->chalH, 0, FALSE );
        
        if( !updMsg.clientH->hasLock ) OSSEMAPHORE_Release ( dsiH->semaDsi );
        
        if( updMsg.updReq.cslLcdCb )
        {
            updMsg.updReq.cslLcdCb (res, updMsg.clientH,updMsg.updReq.cslLcdCbRef);
        } 
        else
        {
            LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsi0UpdateTask: "
                "Callback EQ NULL, Skipping\n\r" ); 
        }    
    }
} // cslDsi0UpdateTask


//*****************************************************************************
//
// Function Name:  cslDsiOsInit
// 
// Description:    DSI COntroller OS Interface Init
//
//*****************************************************************************
Boolean cslDsiOsInit ( DSI_HANDLE dsiH )
{
    Boolean   res = TRUE;

	int ret;
   
    // Update Request Queue
    dsiH->updReqQ = OSQUEUE_Create ( FLUSH_Q_SIZE, 
        sizeof(DSI_UPD_REQ_MSG_T), OSSUSPEND_PRIORITY);
    if (!dsiH->updReqQ)
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiOsInit(): "
            "OSQUEUE_Create failed\n");
        res = FALSE;
    }
    else
    {
        OSQUEUE_ChangeName ( dsiH->updReqQ, dsiH->bus ? "Dsi1Q":"Dsi0Q" );
    }    

    // Update Request Task
    dsiH->updReqT = OSTASK_Create ( dsiH->task, 
        dsiH->bus ? (TName_t)"Dsi1T":(TName_t)"Dsi0T",
        TASKPRI_DSI, STACKSIZE_DSI );
    if (!dsiH->updReqT)
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiOsInit(): Create Task failure\n");
        res = FALSE;
    }
    
    // DSI Global Data Semaphore
    dsiH->semaDsiW = OSSEMAPHORE_Create ( 1, OSSUSPEND_PRIORITY );
    if (!dsiH->semaDsiW)
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiOsInit: ERR Sema Creation!\n");
        res = FALSE;
    }
    else
    {
        OSSEMAPHORE_ChangeName ( dsiH->semaDsiW, 
            dsiH->bus ? "Dsi1W":"Dsi0W" );
    }
    
    // DSI Interface Semaphore
    dsiH->semaDsi = OSSEMAPHORE_Create (1, OSSUSPEND_PRIORITY);
    if (!dsiH->semaDsi)
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiOsInit: ERR Sema Creation!\n");
        res = FALSE;
    }
    else
    {
        OSSEMAPHORE_ChangeName ( dsiH->semaDsi, dsiH->bus ? "Dsi1":"Dsi0" );
    }

    // DSI Interrupt Event Semaphore
    dsiH->semaInt = OSSEMAPHORE_Create (0, OSSUSPEND_PRIORITY);
    if (!dsiH->semaInt)
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiOsInit: ERR Sema Creation!\n");
        res = FALSE;
    }
    else
    {
        OSSEMAPHORE_ChangeName ( dsiH->semaInt, dsiH->bus ? "Dsi1Int":"Dsi0Int");
    }

    // EndOfDma Semaphore
    dsiH->semaDma = OSSEMAPHORE_Create (0, OSSUSPEND_PRIORITY);
    if (!dsiH->semaDma)
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiOsInit: ERR Sema Creation!\n");
        res = FALSE;
    }
    else
    {
        OSSEMAPHORE_ChangeName ( dsiH->semaDma, 
            dsiH->bus ? "Dsi1Dma":"Dsi0Dma" );
    }

#ifndef __KERNEL__
    // DSI Controller Interrupt
    dsiH->iHisr = OSINTERRUPT_Create( 
                        (IEntry_t)dsiH->hisr, 
                        dsiH->bus ? (IName_t)"Dsi1":(IName_t)"Dsi0",
                        IPRIORITY_MIDDLE, HISRSTACKSIZE_DSISTAT );
#endif

#ifdef __KERNEL__
 	ret = request_irq(dsiH->interruptId, dsiH->lisr, IRQF_DISABLED |
			    IRQF_NO_SUSPEND, "BRCM DSI CSL", NULL);
	if (ret < 0) {
		pr_err("%s(%s:%u)::request_irq failed IRQ %d\n",
		       __FUNCTION__, __FILE__, __LINE__, dsiH->interruptId);
		goto free_irq;
	}
#else
        IRQ_Register ( dsiH->interruptId, dsiH->lisr );
#endif

    return ( res );

#ifdef __KERNEL__
free_irq:
	free_irq(dsiH->interruptId, NULL);
    return ( res );
#endif	
} // cslLcdcOsInit

//*****************************************************************************
//
// Function Name:   cslDsiDmaStart
//
// Description:     Set Lcd Dma channel and LinkList
//                  
//*****************************************************************************
static CSL_LCD_RES_T cslDsiDmaStart ( DSI_UPD_REQ_MSG_T* updMsg )
{
    UInt32          width;
    UInt32          height;
    CSL_LCD_RES_T   res = CSL_LCD_OK;
        
    // Reserve Channel
    if ( DMADRV_Obtain_Channel( 
            DMA_CLIENT_MEMORY, 
            DMA_CLIENT_DSI_CM, 
            &updMsg->dmaCh ) != DMADRV_STATUS_OK )
    {        
        LCD_DBG ( LCD_DBG_ID,  
            "[CSL DSI] cslDsiDmaStart(): ERROR Obtaining DMA Channel\r\n");
        return CSL_LCD_DMA_ERR;
    }

    // Configure Channel
    updMsg->dmaChInfo.srcID          = DMA_CLIENT_MEMORY;
    updMsg->dmaChInfo.dstID          = DMA_CLIENT_DSI_CM;
    updMsg->dmaChInfo.type           = DMA_FCTRL_MEM_TO_PERI;
    updMsg->dmaChInfo.srcBstSize     = DMA_BURST_SIZE_16;
    updMsg->dmaChInfo.dstBstSize     = DMA_BURST_SIZE_16;
    updMsg->dmaChInfo.srcDataWidth   = DMA_DATA_SIZE_32BIT;
    updMsg->dmaChInfo.dstDataWidth   = DMA_DATA_SIZE_32BIT;
    updMsg->dmaChInfo.incMode        = DMA_INC_MODE_SRC;
    updMsg->dmaChInfo.xferCompleteCb = (DmaDrv_Callback)updMsg->dsiH->dma_cb;
    updMsg->dmaChInfo.freeChan       = TRUE;
    updMsg->dmaChInfo.prot           = 0;
    updMsg->dmaChInfo.bCircular      = FALSE;
    updMsg->dmaChInfo.alignment      = DMA_ALIGNMENT_32;

    if ( DMADRV_Config_Channel (updMsg->dmaCh, &updMsg->dmaChInfo) 
        != DMADRV_STATUS_OK )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] cslDsiDmaStart(): ERROR DMADRV_Config_Channel, "
            "DmaCh[%d]\r\n", updMsg->dmaCh );
        return CSL_LCD_DMA_ERR;
    }    

    width  = updMsg->updReq.lineLenP *updMsg->updReq.buffBpp;
    height = updMsg->updReq.lineCount;
    
    // build LLI
    updMsg->dmaBuffList.buffers[0].srcAddr   = (UInt32)updMsg->updReq.buff;
    updMsg->dmaBuffList.buffers[0].destAddr  = 0x084a0140;//chal_dsi_de1_get_dma_address ( updMsg->dsiH->chalH );
    updMsg->dmaBuffList.buffers[0].length    = width * height;
    updMsg->dmaBuffList.buffers[0].bRepeat   = 0;
    updMsg->dmaBuffList.buffers[0].interrupt = 1;
    updMsg->dmaData.numBuffer  = 1;
    updMsg->dmaData.pBufList   = (Dma_Buffer_List *)&updMsg->dmaBuffList;

    if ( DMADRV_Bind_Data(updMsg->dmaCh, &updMsg->dmaData ) 
        != DMADRV_STATUS_OK )
    {
        LCD_DBG ( LCD_DBG_ID,  
            "[CSL DSI] cslDsiDmaStart(): ERROR DMADRV_Bind_Data, "
            "DmaCh[%d]\r\n", updMsg->dmaCh );
        return CSL_LCD_DMA_ERR;
    }    

	if ( DMADRV_Start_Transfer ( updMsg->dmaCh ) != DMADRV_STATUS_OK )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] cslDsiDmaStart(): ERROR DMADRV_Start_Transfer, "
            "DmaCh[%d]\r\n", updMsg->dmaCh );
        return CSL_LCD_DMA_ERR;
    }

	//DumpDMACRegister();

    return ( res );
} // cslDsiDmaStart   

//*****************************************************************************
//
// Function Name:   cslLcdcDmaStop
//
// Description:     Stops DMA
//                  
//*****************************************************************************
static CSL_LCD_RES_T cslDsiDmaStop ( DSI_UPD_REQ_MSG_T* updMsg )
{
    CSL_LCD_RES_T res = CSL_LCD_OK;

    LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiDmaStop: DMA-CH[%d]\n\r", updMsg->dmaCh);

    LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiDmaStop: "
        "INT      0x%08X\n\r", dsiIntStat );
    LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiDmaStop: "
        "INT STAT 0x%08X\n\r", 
        chal_dsi_get_status ( updMsg->dsiH->chalH ) ); 

//    DMADRV_Stop_TransferEx((DMA_CHANNEL)updMsg->dmaCh);
    DMADRV_Force_Shutdown_Channel((DMA_CHANNEL)updMsg->dmaCh);
    DMADRV_Release_Channel((DMA_CHANNEL)updMsg->dmaCh);
    cslDsiClearAllFifos ( updMsg->dsiH );
    updMsg->dmaCh = DMA_CHANNEL_INVALID;
    return res;
} // cslDsiDmaStop


//*****************************************************************************
//
// Function Name:  cslDsiClkTreeInit
// 
// Description:    Init CLK Tree For HighSpeed & Escape Clocks
//
//*****************************************************************************
static Boolean cslDsiClkTreeInit( 
    DSI_HANDLE      dsiH,
    const DSI_CLK_T       *in_hsClk,   // HS  Input clock & Divider setting
    const DSI_CLK_T       *in_escClk   // ESC Input clock & Divider setting
    )
{
    UInt32 hsClkSet;  // Base HS Clk Set ( PLL if available )
    DSI_CLK_T       hsClk = *in_hsClk; 
    DSI_CLK_T       escClk = *in_escClk; 

    printk(KERN_ERR "hsclk->clk=0x%08x  div=0x%08x, and esc->clk = 0x%08x and its div = 0x%08x\n",
		hsClk.clkIn_MHz, hsClk.clkInDiv, escClk.clkIn_MHz, escClk.clkInDiv);
		
    //--- H S  clock -------------------------------------------------------
    if ( hsClk.clkIn_MHz  > 1000 )
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiClkTreeInit(): "
            "ERR DSI PLL, Valid Input Range: < 1000, Inc 4MHz\r\n" ); 
        return ( FALSE );      
    }    
    // ATHENA for now, set PLL base to 4MHz ( = 24/6 ) so we can do simple
    // increments of HS by 4[Mbps]
    dsiH->clkCfg.hsPll_P1       = 6;
    dsiH->clkCfg.hsPll_N_frac   = 0;
    dsiH->clkCfg.hsPll_N_int    = (UInt32)((float) hsClk.clkIn_MHz / 4);
    hsClkSet = (float)4 * dsiH->clkCfg.hsPll_N_int;
    dsiH->clkCfg.hsPllReq_MHz = (float)hsClk.clkIn_MHz;
    dsiH->clkCfg.hsPllSet_MHz = hsClkSet;
    
    // we can divide in the range of 1 to 15 ( == reg )
    if ( !(hsClk.clkInDiv >=1 && hsClk.clkInDiv <=15) )
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiClkTreeInit(): "
            "ERR Invalid HS Clk Div, Valid Range: 1-15, inc 1\r\n" ); 
        return ( FALSE );    
    }
    // calculate HS Bit Clock & init HS clk div value     
    dsiH->clkCfg.hsBitClk_MHz = hsClkSet / hsClk.clkInDiv;
    dsiH->clkCfg.hsClkDiv     = hsClk.clkInDiv;
    
    LCD_DBG ( LCD_DBG_ID, "[CSL DSI] INFO-HS -CLK: "
        "InClk[%u] InDiv[%d], InSet[%u] => %u[Mhz]\r\n",
        hsClk.clkIn_MHz, hsClk.clkInDiv, 
        hsClkSet              , dsiH->clkCfg.hsBitClk_MHz ); 
    
    //--- E S C  clock -----------------------------------------------------
    // ZEUS & ATHENA - ESC clk is fixed to 156
    if ( escClk.clkIn_MHz != 156 )
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiClkTreeInit(): "
            "ERR ESC CLK In, Valid Input: 156[MHz]\r\n" ); 
        return ( FALSE );      
    }    
    // need it later since HERA can have multiple esc clk source
    dsiH->clkCfg.escClkIn_MHz = escClk.clkIn_MHz;

    // we can divide by 2,4,6,8,10,12,14,16 ( == (reg+1) * 2)
    if ( !(escClk.clkInDiv >=2 && escClk.clkInDiv <=16) )
        return ( FALSE );      
    if ( (escClk.clkInDiv % 2) != 0 )
        return ( FALSE );      
        
    dsiH->clkCfg.escClkDiv  = (escClk.clkInDiv >> 1 ) - 1; 
    dsiH->clkCfg.escClk_MHz = (float)escClk.clkIn_MHz / escClk.clkInDiv;


    LCD_DBG ( LCD_DBG_ID, "[CSL DSI] INFO-ESC-CLK: "
        "InClk[%u] InDiv[%d]                => %u[Mhz]\r\n",
        escClk.clkIn_MHz, escClk.clkInDiv, dsiH->clkCfg.escClk_MHz ); 

    return ( TRUE );
}

static Boolean cslDsiClkTreeEna( DSI_HANDLE dsiH )
{

#define SYSCFG_PERIPH_LCD_AHB_CLK_EN 0x0888012c
#define SYSCFG_DSICR 0x08880038
#define CLK_ANALOG_PHASES_ENABLE  0x08140140
#define CLK_DSIPLL_DIVIDERS 0x08140224
#define CLK_DSIPLL_FRAC_DIVIDERS 0x08140228
#define CLK_DSIPLL_ENABLE  0x08140220
#define CLK_DSIPLL_channel_disables 0x0814023C
#define CLK_MIPI_DSI_AFE_CLK_DIV 0x0814013C
#define CLK_MIPI_DSI_CTRL 0x08140134
#define CLK_MIPIDSI_CMI_CLK_EN 0x0814012C

    void                        *clkDrv;
    CHAL_CLK_CFG_MIPIDSI_CMI_T  escClkDiv;
    CHAL_CLK_CFG_MIPIDSIAFE_T   hsClkDiv;
	OSDAL_Status ret = OSDAL_ERR_OK;

	writel(0x0, IO_ADDRESS(SYSCFG_PERIPH_LCD_AHB_CLK_EN) ); //disable lcd ahb clk
	//-------------------------------------------------------------------------
    // 0x0888_0038   SYSCFG_DSICR (SYSCFG_DSICR_DSI_EN)
    //-------------------------------------------------------------------------
    // Start DSI AHB clock
    //*(volatile UInt32*) 0x08880038  = 1;
  	writel(1, IO_ADDRESS(SYSCFG_DSICR));
    //               D S I   P L L
    // 0x0814_0140   CLK_ANALOG_PHASES_ENABLE (0=enable)
	//*(volatile UInt32*) 0x08140140 &= ~0x00000002; // we share it with USB & TVPLL
	writel(readl(IO_ADDRESS(CLK_ANALOG_PHASES_ENABLE)) &~(0x00000002), IO_ADDRESS(CLK_ANALOG_PHASES_ENABLE));
    //-------------------------------------------------------------------------
    //               D S I   P L L
    // 0x0814_0224   CLK_DSIPLL_DIVIDERS        P1 (DEF=1) &  NINT(DEF=15)
    //               DEF = 24x15 =360 Mbps
    // 0x0814_0228   CLK_DSIPLL_FRAC_DIVIDERS   NFRAC (DEF=0)
    // 0x0814_0220   DSI PLL ENA - CLK_DSIPLL_ENABLE (PWR UP PLL)
    //               DEFAULT = 0 = DISABLE
    // 0x0814_023C   CLK_DSIPLL_channel_disables  DEF VALUE = 1 = DISABLE
    //-------------------------------------------------------------------------
    //*(volatile UInt32*) 0x08140224  =  
    //    ((dsiH->clkCfg.hsPll_N_int)<< 8) | (dsiH->clkCfg.hsPll_P1 & 0x0000000F);
    //*(volatile UInt32*) 0x08140228  = dsiH->clkCfg.hsPll_N_frac;
    //*(volatile UInt32*) 0x08140220  = 0x00000001;
    //*(volatile UInt32*) 0x0814023C  = 0x00000000;
   writel(((dsiH->clkCfg.hsPll_N_int)<< 8) | (dsiH->clkCfg.hsPll_P1 & 0x0000000F), IO_ADDRESS(CLK_DSIPLL_DIVIDERS));
   writel(dsiH->clkCfg.hsPll_N_frac, IO_ADDRESS(CLK_DSIPLL_FRAC_DIVIDERS));
   writel(0x00000001, IO_ADDRESS(CLK_DSIPLL_ENABLE));
   writel(0x00000000, IO_ADDRESS(CLK_DSIPLL_channel_disables));

    // wait for 400us for PLL to LOCK
   OSTASK_Sleep (TICKS_IN_MILLISECONDS (1) );

    // SET HS  C L K Divider 
    //  o cHAL has problems, sets all clocks (CSI1/CSI2/DSI)
    //  o DSI = 0x...._.X..
   hsClkDiv.dsiDiv  = dsiH->clkCfg.hsClkDiv;
   //hsClkDiv.cam1Div = x;
   //hsClkDiv.cam2Div = x;
//  CLKDRV_Set_Clock   ( clkDrv, CLK_MIPIDSI_AFE, (void *)&hsClkDiv );
    // HARDOCDED - Do not touch CAM DIVs until CLK_DRV handles this correctly
//    *(volatile UInt32*) 0x0814013C &= ~0x00000F00; 
//    *(volatile UInt32*) 0x0814013C |= (dsiH->clkCfg.hsClkDiv << 8 ); 
   writel(readl(IO_ADDRESS(CLK_MIPI_DSI_AFE_CLK_DIV)) &~(0x00000F00), IO_ADDRESS(CLK_MIPI_DSI_AFE_CLK_DIV));
   writel(readl(IO_ADDRESS(CLK_MIPI_DSI_AFE_CLK_DIV)) | (dsiH->clkCfg.hsClkDiv << 8 ), IO_ADDRESS(CLK_MIPI_DSI_AFE_CLK_DIV));
    // Set E S C  C L K  Divider & Start ESC Clock
   escClkDiv.speedMIPIDSI = dsiH->clkCfg.escClkDiv;
    //-------------------------------------------------------------------------
    // 0x0814_0134   Set E S C  C L K  Divider
    //-------------------------------------------------------------------------
   // *(volatile UInt32*) 0x08140134 = dsiH->clkCfg.escClkDiv;
   writel(dsiH->clkCfg.escClkDiv, IO_ADDRESS(CLK_MIPI_DSI_CTRL));
    //-------------------------------------------------------------------------
    // 0x0814_012C   E S C  C L K  Ena ???
    //               o CLK_MIPIDSI_CMI_CLK_EN    VALUE = 0x0000_0001
    //-------------------------------------------------------------------------
    //*(volatile UInt32*) 0x0814012C = 0x00000001;
   writel(0x00000001, IO_ADDRESS(CLK_MIPIDSI_CMI_CLK_EN));
   return ( TRUE );
}

//*****************************************************************************
//
// Function Name:  cslDsiWaitForStatAny_Poll
// 
// Description:    
//                 
//*****************************************************************************
static CSL_LCD_RES_T  cslDsiWaitForStatAny_Poll( 
    DSI_HANDLE  dsiH, 
    UInt32      statMask, 
    UInt32*     intStat,
    UInt32      mSec 
    )
{
    CSL_LCD_RES_T   res  = CSL_LCD_OK;
    UInt32          stat = 0;
    
    stat = chal_dsi_get_status (dsiH->chalH);
    
    while ( (stat & statMask ) == 0 )
    {
        stat = chal_dsi_get_status (dsiH->chalH);
    }    
        
    if( intStat != NULL )
        *intStat = stat;    

    return ( res );    
}


//*****************************************************************************
//
// Function Name:  cslDsiClearAllFifos
// 
// Description:    Clear All DSI FIFOs
//                 
//*****************************************************************************
static void cslDsiClearAllFifos( DSI_HANDLE dsiH )
{
    UInt32 fifoMask;

    fifoMask =   CHAL_DSI_CTRL_CLR_LANED_FIFO        
               | CHAL_DSI_CTRL_CLR_PIX_BYTEC_FIFO
               | CHAL_DSI_CTRL_CLR_CMD_PIX_BYTEC_FIFO
               | CHAL_DSI_CTRL_CLR_PIX_DATA_FIFO     
               | CHAL_DSI_CTRL_CLR_CMD_DATA_FIFO;     

    chal_dsi_clr_fifo ( dsiH->chalH, fifoMask );
}

//*****************************************************************************
//
// Function Name:  cslDsiEnaIntEvent
// 
// Description:    event bits set to "1" will be enabled, 
//                 rest of the events will be disabled
//                 
//*****************************************************************************
static void cslDsiEnaIntEvent( DSI_HANDLE dsiH, UInt32 intEventMask )
{
    chal_dsi_ena_int ( dsiH->chalH, intEventMask );
}


//*****************************************************************************
//
// Function Name:  cslDsiDisInt
// 
// Description:    
//                 
//*****************************************************************************
static void cslDsiDisInt ( DSI_HANDLE dsiH )
{
    chal_dsi_ena_int ( dsiH->chalH, 0 );
    chal_dsi_clr_int ( dsiH->chalH, 0xFFFFFFFF );
}

//*****************************************************************************
//
// Function Name:  cslDsiWaitForInt
// 
// Description:    
//                 
//*****************************************************************************
static CSL_LCD_RES_T cslDsiWaitForInt( DSI_HANDLE dsiH, UInt32 tout_msec )
{
    OSStatus_t      osRes;
    CSL_LCD_RES_T   res = CSL_LCD_OK;

    osRes = OSSEMAPHORE_Obtain ( dsiH->semaInt, tout_msec );

    if ( osRes != OSSTATUS_SUCCESS )
    {
        cslDsiDisInt ( dsiH );
        
        if( osRes == OSSTATUS_TIMEOUT )
        {
            LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiWaitForInt: "
                "ERR Timed Out!\n\r" ); 
            res = CSL_LCD_OS_TOUT;
        }    
        else
        {
            LCD_DBG ( LCD_DBG_ID, "[CSL DSI] cslDsiWaitForInt: "
                "ERR OS Err...\n\r" ); 
            res = CSL_LCD_OS_ERR;
        }    
    } 
    return ( res );
}



//*****************************************************************************
//
// Function Name:  cslDsiBtaRecover
// 
// Description:    Recover From Failed BTA
//                 ZEUS Workaround, Check ATHENA
//                 
//*****************************************************************************
static CSL_LCD_RES_T  cslDsiBtaRecover ( DSI_HANDLE dsiH )
{
    CSL_LCD_RES_T  res = CSL_LCD_OK;
    
    chal_dsi_phy_state  ( dsiH->chalH, PHY_TXSTOP );
    
    OSTASK_Sleep (TICKS_IN_MILLISECONDS (1) );
    
    chal_dsi_clr_fifo ( dsiH->chalH, 
        CHAL_DSI_CTRL_CLR_CMD_DATA_FIFO | CHAL_DSI_CTRL_CLR_LANED_FIFO );
    
    chal_dsi_phy_state ( dsiH->chalH, PHY_CORE );
    return ( res );
}



//*****************************************************************************
//                 
// Function Name:  CSL_DSI_Lock
// 
// Description:    Lock DSI Interface For Client
//                 
//*****************************************************************************
void CSL_DSI_Lock ( CSL_LCD_HANDLE client )
{
    DSI_CLIENT clientH = (DSI_CLIENT) client;
    DSI_HANDLE dsiH    = (DSI_HANDLE) clientH->lcdH;
    
    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );
    
    OSSEMAPHORE_Obtain ( dsiH->semaDsi, TICKS_FOREVER );
    clientH->hasLock = TRUE;
    
    OSSEMAPHORE_Release ( dsiH->semaDsiW );
}

//*****************************************************************************
//                 
// Function Name:  CSL_DSI_Unlock
// 
// Description:    Release Client's DSI Interface Lock
//                 
//*****************************************************************************
void CSL_DSI_Unlock ( CSL_LCD_HANDLE client )
{
    DSI_CLIENT clientH = (DSI_CLIENT) client;
    DSI_HANDLE dsiH    = (DSI_HANDLE) clientH->lcdH;
    
    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );
    
    OSSEMAPHORE_Release ( dsiH->semaDsi );
    clientH->hasLock = FALSE;
    
    OSSEMAPHORE_Release ( dsiH->semaDsiW );
}

//*****************************************************************************
//                 
// Function Name:  CSL_DSI_SendPacketTrigger  
// 
// Description:    Send TRIGGER Message
//                 
//*****************************************************************************
CSL_LCD_RES_T CSL_DSI_SendTrigger ( 
    CSL_LCD_HANDLE  client, 
    UInt8           trig 
    )
{
    DSI_HANDLE      dsiH;
    DSI_CLIENT      clientH;
    CSL_LCD_RES_T   res;
    
    clientH = (DSI_CLIENT) client;
    dsiH    = (DSI_HANDLE) clientH->lcdH;

    if ( dsiH->ulps ) return CSL_LCD_BAD_STATE;

    if( !clientH->hasLock ) OSSEMAPHORE_Obtain ( dsiH->semaDsi, TICKS_FOREVER );
    
    chal_dsi_clr_status ( dsiH->chalH, 0xFFFFFFFF );

#ifdef __CSL_DSI_USE_INT__    
    cslDsiEnaIntEvent( dsiH, (UInt32)CHAL_DSI_ISTAT_TXPKT1_DONE );
#endif    
    chal_dsi_send_trig ( dsiH->chalH, TX_PKT_ENG_0, trig );

#ifdef __CSL_DSI_USE_INT__    
    res = cslDsiWaitForInt( dsiH, 100 );
#else    
    res = cslDsiWaitForStatAny_Poll( dsiH, 
            CHAL_DSI_STAT_TXPKT1_DONE, NULL, 100 );
#endif


    if( !clientH->hasLock ) OSSEMAPHORE_Release ( dsiH->semaDsi );
    return ( res );
}

//*****************************************************************************
//                 
// Function Name:  CSL_DSI_SendPacket  ( for now only POLLED )
// 
// Description:    Send DSI Packet with an option to end it with BTA
//                 If BTA is requested, command.reply cannot be NULL
//
//*****************************************************************************
CSL_LCD_RES_T CSL_DSI_SendPacket ( 
    CSL_LCD_HANDLE  client, 
    pCSL_DSI_CMND   command 
    )
{
    DSI_HANDLE      dsiH;
    DSI_CLIENT      clientH;
    CSL_LCD_RES_T   res = CSL_LCD_OK;
    CHAL_DSI_CMND_t cmnd;
    CHAL_DSI_RES_t  chalRes;
    UInt32          stat;
    UInt32          event;
   
    clientH = (DSI_CLIENT) client;
    dsiH    = (DSI_HANDLE) clientH->lcdH;
    
    if ( dsiH->ulps ) 
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_SendPacket(): ERR, "
            "Link Is In ULPS \r\n", command->vc ); 
        return CSL_LCD_BAD_STATE;
    }    
    if ( command->endWithBta && (command->reply == NULL) )
    {
    
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_SendPacket(): ERR, "
            "BTA Requested But pReply Eq NULL\r\n", command->vc ); 
        return CSL_LCD_ERR;
    } 
    
    cmnd.dsiCmnd     = command->dsiCmnd;
    cmnd.msg         = command->msg;     
    cmnd.msgLen      = command->msgLen;
    cmnd.vc          = command->vc;       
    cmnd.isLP        = command->isLP;   
    cmnd.isLong      = command->isLong;   
    cmnd.vmWhen      = CHAL_DSI_CMND_WHEN_BEST_EFFORT;
    cmnd.endWithBta  = command->endWithBta;
    
    if( !clientH->hasLock ) OSSEMAPHORE_Obtain ( dsiH->semaDsi, TICKS_FOREVER );
    
    chal_dsi_clr_status ( dsiH->chalH, 0xFFFFFFFF );

#ifdef __CSL_DSI_USE_INT__    
    if ( command->endWithBta  )
    {
        event =   CHAL_DSI_ISTAT_PHY_RX_TRIG
                | CHAL_DSI_ISTAT_RX2_PKT                   
                | CHAL_DSI_ISTAT_RX1_PKT;                   
    } 
    else
    {
        event =   CHAL_DSI_ISTAT_TXPKT1_DONE;
    }
    cslDsiEnaIntEvent( dsiH, event );
#else
    if ( command->endWithBta  )
    {
        event =   CHAL_DSI_STAT_PHY_RX_TRIG
                | CHAL_DSI_STAT_RX2_PKT                   
                | CHAL_DSI_STAT_RX1_PKT;                   
    } 
    else
    {
        event =   CHAL_DSI_STAT_TXPKT1_DONE;
    }
#endif    

    chalRes = chal_dsi_send_cmnd ( dsiH->chalH, TX_PKT_ENG_0, &cmnd );
 
    if( chalRes !=  CHAL_DSI_OK )
    {
        if( !clientH->hasLock ) OSSEMAPHORE_Release ( dsiH->semaDsi );
        return ( CSL_LCD_INT_ERR );    
    }

#ifdef __CSL_DSI_USE_INT__    
    res  = cslDsiWaitForInt( dsiH, 100 );
    stat = chal_dsi_get_status (dsiH->chalH);
#else    
    res = cslDsiWaitForStatAny_Poll( dsiH, event, &stat, 100 );
#endif

    if( command->endWithBta )
    {
        if( res != CSL_LCD_OK )
        {
            LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_SendPacket(): WARNING, "
                "VC[%d] Probable BTA TimeOut, Recovering ...\r\n", cmnd.vc ); 
            cslDsiBtaRecover ( dsiH );
        }
        else
        {
            chalRes = chal_dsi_read_reply( dsiH->chalH, stat, 
                        (pCHAL_DSI_REPLY)command->reply );
            if( chalRes == CHAL_DSI_RX_NO_PKT )
            {
                LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_SendPacket(): WARNING, "
                    "VC[%d] BTA No Data Received ...\r\n", cmnd.vc ); 
                res = CSL_LCD_INT_ERR;
            }    
        }
    }
    
    chal_dsi_cmnd_start ( dsiH->chalH, TX_PKT_ENG_0, FALSE );

    if( !clientH->hasLock ) OSSEMAPHORE_Release ( dsiH->semaDsi );
        
    return ( res );    
}


//*****************************************************************************
//                 
// Function Name:  CSL_DSI_OpenCmVc
//                    
// Description:    Open (configure) Command Mode VC
//                 Returns VC Command Mode handle
//
//*****************************************************************************
CSL_LCD_RES_T CSL_DSI_OpenCmVc ( 
    CSL_LCD_HANDLE      client, 
    pCSL_DSI_CM_VC      dsiCmVcCfg, 
    CSL_LCD_HANDLE*     dsiCmVcH     
    ) 
{
    DSI_HANDLE      dsiH;
    DSI_CLIENT      clientH;
    DSI_CM_HANDLE   cmVcH;    
    CSL_LCD_RES_T   res = CSL_LCD_OK;
    Boolean         colCfgValid = TRUE;  
    UInt32          i;
    
    clientH = (DSI_CLIENT) client;
    dsiH    = (DSI_HANDLE) clientH->lcdH;
    
    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );
    
    if( dsiCmVcCfg->vc > 3 ) 
    {
        *dsiCmVcH = NULL;
        OSSEMAPHORE_Release ( dsiH->semaDsiW );
        return ( CSL_LCD_ERR );
    }
        
    for (i=0; i<DSI_CM_MAX_HANDLES; i++ )
    {
        if( !dsiH->chCm[i].configured )
            break;
    }    
    
    if( i >= DSI_CM_MAX_HANDLES )
    {
        *dsiCmVcH = NULL;
        return ( CSL_LCD_INST_COUNT );
    }
    else
    {
       cmVcH = &dsiH->chCm[i];
    }
    
    cmVcH->chalCmCfg.vc       = dsiCmVcCfg->vc;         
    cmVcH->chalCmCfg.dsiCmnd  = dsiCmVcCfg->dsiCmnd;
    cmVcH->chalCmCfg.isLP     = dsiCmVcCfg->isLP; 
    cmVcH->chalCmCfg.vmWhen   = CHAL_DSI_CMND_WHEN_BEST_EFFORT;
    cmVcH->chalCmCfg.start    = TRUE;
    
    cmVcH->dcsCmndStart = dsiCmVcCfg->dcsCmndStart;
    cmVcH->dcsCmndCont  = dsiCmVcCfg->dcsCmndCont;
 
    if (dsiCmVcCfg->dcsCmndStart == dsiCmVcCfg->dcsCmndCont )
    {
        cmVcH->dcsUseContinue    = FALSE;
        cmVcH->chalCmCfg.dcsCmnd = dsiCmVcCfg->dcsCmndStart;   
    }
    else
    {
        cmVcH->dcsUseContinue    = TRUE;
        cmVcH->chalCmCfg.dcsCmnd = dsiCmVcCfg->dcsCmndStart;   
    }

    // TE configuration - for now we support only DSI Ctrlr SYNC through 
    // dedicated external TE input
    switch ( dsiCmVcCfg->teCfg.teInType )
    {
        case DSI_TE_NONE:
            break;    
        case DSI_TE_CTRLR_INPUT_0:
            #if ( defined (_ATHENA_)&& (CHIP_REVISION >= 20) ) 
            chal_dsi_te_cfg ( dsiH->chalH, CHAL_DSI_TE_IN_0, 
                (pCHAL_DSI_TE_CFG)dsiCmVcCfg->teCfg.teInCfg );
            #endif            
            cmVcH->chalCmCfg.isTE = TRUE;  
            cmVcH->teMode = TE_EXT_0;
            break;
        case DSI_TE_CTRLR_INPUT_1:
            #if ( defined (_ATHENA_)&& (CHIP_REVISION >= 20) ) 
            chal_dsi_te_cfg ( dsiH->chalH, CHAL_DSI_TE_IN_1, 
                (pCHAL_DSI_TE_CFG)dsiCmVcCfg->teCfg.teInCfg );
            #endif            
            cmVcH->chalCmCfg.isTE = TRUE;  
            cmVcH->teMode = TE_EXT_1;
            break;
        case DSI_TE_CTRLR_TRIG:
#if ( defined (_ATHENA_) )
            cmVcH->chalCmCfg.isTE = TRUE;  
            cmVcH->teMode = TE_TRIG;
            break;
#else
            OSSEMAPHORE_Release ( dsiH->semaDsiW );
            LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_OpenCmVc(): ERR, "
                  "DSI TRIG Not Supported Yet\r\n" ); 
            return ( CSL_LCD_ERR );    
#endif            
        default:
            LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_OpenCmVc(): ERR, "
                "Invalid TE Config Type [%d]\r\n", dsiCmVcCfg->teCfg.teInType ); 
            OSSEMAPHORE_Release ( dsiH->semaDsiW );
            return ( CSL_LCD_ERR );    
    }
 
//  DSI_WC_PIXEL_CNT = PKT_PIXEL_COUNT >> wc_rshift
//  Represents number of required AHB transactions per DSI packet
//  All modes except DE1_CM_565 ( 2x565 pixels in 1 AHB cycle, thus PIXEL/2 )
//  require 1 DMA CYCLE / pixel

    switch ( dsiCmVcCfg->cm_in )
    {
        // 1x888 pixel per 32-bit word (MSB DontCare)
        case LCD_IF_CM_I_RGB888U:         
            switch ( dsiCmVcCfg->cm_out )
            {
                case LCD_IF_CM_O_RGB565:
                    cmVcH->bpp_dma   = 4;  
                    cmVcH->bpp_wire  = 2;
                    cmVcH->wc_rshift = 0;
                    cmVcH->cm = DE1_CM_888U_TO_565;
                    break;
                case LCD_IF_CM_O_RGB666:
                case LCD_IF_CM_O_RGB888:
                    cmVcH->bpp_dma   = 4;  
                    cmVcH->bpp_wire  = 3;
                    cmVcH->wc_rshift = 0;
                    cmVcH->cm = DE1_CM_888U;
                    break;
                default:
                    colCfgValid = FALSE;
                    break;
            }    
            break;
        
        // 2x565 pixels per 32-bit word
        case LCD_IF_CM_I_RGB565P:         
            switch ( dsiCmVcCfg->cm_out )
            {
                case LCD_IF_CM_O_RGB565:
                    cmVcH->bpp_dma   = 2;  
                    cmVcH->bpp_wire  = 2;
                    cmVcH->wc_rshift = 1;
                    cmVcH->cm = DE1_CM_565;
                    break;
                default:
                    colCfgValid = FALSE;
                    break;
            }    
            break;
            
        // 1x565 pixel per 32-bit word (LSB)
        case LCD_IF_CM_I_RGB565U_LSB:    
            switch ( dsiCmVcCfg->cm_out )
            {
                case LCD_IF_CM_O_RGB565:
                    cmVcH->bpp_dma   = 4;  
                    cmVcH->bpp_wire  = 2;
                    cmVcH->wc_rshift = 0;
                    cmVcH->cm = DE1_CM_565_LSB;
                    break;
                default:
                    colCfgValid = FALSE;
                    break;
            }    
            break;
            
        default:    
            colCfgValid = FALSE;
            break;
    }    

    if( !colCfgValid )
    {
        cmVcH->configured = FALSE;
        cmVcH = NULL;
        res = CSL_LCD_COL_MODE;
    }
    else
    {    
        cmVcH->configured = TRUE;
        cmVcH->client = clientH;
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_OpenCmVc(): OK, "
            "VC[%d], TE[%s]\r\n", 
            cmVcH->chalCmCfg.vc, cmVcH->chalCmCfg.isTE ? "YES":"NO" ); 
    }    
     
    *dsiCmVcH = (CSL_LCD_HANDLE)cmVcH;   
    OSSEMAPHORE_Release ( dsiH->semaDsiW );
    return ( res );
} 

//*****************************************************************************
//
// Function Name: CSL_DSI_CloseCmVc
// 
// Description:   Close Command Mode VC handle
//                 
//*****************************************************************************
CSL_LCD_RES_T CSL_DSI_CloseCmVc ( CSL_LCD_HANDLE vcH )
{
    CSL_LCD_RES_T   res     = CSL_LCD_OK;
    DSI_CM_HANDLE   dsiChH  = (DSI_CM_HANDLE ) vcH;
    DSI_HANDLE      dsiH    = (DSI_HANDLE) dsiChH->client->lcdH;
    
    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );
    
    dsiChH->configured = FALSE;
    LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_CloseCmVc(): "
        "VC[%d] Closed\r\n", dsiChH->chalCmCfg.vc ); 

    OSSEMAPHORE_Release ( dsiH->semaDsiW );

    return ( res );
}
//*****************************************************************************
//
// Function Name: CSL_DSI_UpdateCmVc
// 
// Description:   A T H E N A's  Command Mode - DMA Frame Update
//                
//*****************************************************************************
CSL_LCD_RES_T CSL_DSI_UpdateCmVc ( 
    CSL_LCD_HANDLE      vcH, 
    pCSL_LCD_UPD_REQ    req 
    )
{
    CSL_LCD_RES_T       res = CSL_LCD_OK;
    DSI_HANDLE          dsiH;
    DSI_CLIENT          clientH;
    DSI_CM_HANDLE       dsiChH;
    OSStatus_t          osStat;
    UInt32              pktSizeBytes;
    UInt32              pktCount;
    UInt32              pktWc;
    DSI_UPD_REQ_MSG_T   updMsgCm;
    Boolean             isTE;
    
    dsiChH  = ( DSI_CM_HANDLE ) vcH;
    clientH = ( DSI_CLIENT ) dsiChH->client;
    dsiH    = ( DSI_HANDLE ) clientH->lcdH;
    
    if ( dsiH->ulps ) return CSL_LCD_BAD_STATE;
    
    pktSizeBytes = (req->lineLenP * dsiChH->bpp_wire);
    if( pktSizeBytes > DSI_PKT_SIZE_MAX )
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_UpdateCmVc: "
            "ERR Packet Size !\n\r" ); 
        return ( CSL_LCD_ERR );    
    }    
    
    pktCount = req->lineCount;
    if( pktCount > DSI_PKT_RPT_MAX )
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_UpdateCmVc: "
            "ERR Packet Repeat Count !\n\r" ); 
        return ( CSL_LCD_ERR );    
    }
        
    pktWc = req->lineLenP >> dsiChH->wc_rshift;
    if( !clientH->hasLock ) OSSEMAPHORE_Obtain ( dsiH->semaDsi, TICKS_FOREVER );
    
    chal_dsi_clr_status ( dsiH->chalH, 0xFFFFFFFF );
        
    updMsgCm.updReq  = *req;
    updMsgCm.dsiH    = dsiH; 
    updMsgCm.clientH = clientH; 
    if( (res = cslDsiDmaStart ( &updMsgCm )) != CSL_LCD_OK )
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_UpdateCmVc: "
            "ERR Failed To Start DMA!\n\r" ); 
        if( !clientH->hasLock ) OSSEMAPHORE_Release ( dsiH->semaDsi );
        return ( res );    
    }
    // set TE mode
    isTE = dsiChH->chalCmCfg.isTE;
    chal_dsi_te_mode ( dsiH->chalH, dsiChH->teMode );

    // set DE1 ColorMode
    chal_dsi_de1_set_cm ( dsiH->chalH, dsiChH->cm );               
    // enable DE1
    chal_dsi_de1_enable ( dsiH->chalH, TRUE  );

    // DSI PACKET SIZE IN BYTEs
    dsiChH->chalCmCfg.pktSizeBytes = pktSizeBytes;

    // SET NUMBER OF AHB TRANSACTIONs REQUIRED PER DSI PACKET, NA to HERA                    
    chal_dsi_de1_set_wc ( dsiH->chalH, pktWc );

//    if( dsiChH->teMode == TE_TRIG )
//    {
//        // Test code for TE TRIG msg - 
//        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_UpdateCmVc: "
//            "Sending BTA ...\n\r" ); 
//        chal_dsi_clr_status ( dsiH->chalH, 0xFFFFFFFF );
//        chal_dsi_send_bta ( dsiH->chalH, TX_PKT_ENG_0 );
//        cslDsiWaitForStatAny_Poll( dsiH, CHAL_DSI_STAT_TXPKT1_END, NULL, 100 );
//    }
    
    if( dsiChH->dcsUseContinue && (pktCount > 1) )
    {
        // DCS START + DCS CONTINUE
        dsiChH->chalCmCfg.dcsCmnd  = dsiChH->dcsCmndStart;
        dsiChH->chalCmCfg.pktCount = 1;

        // send first line with START CMND
#ifdef __CSL_DSI_USE_INT__  
        //cslDsiEnaIntEvent( dsiH, (UInt32)(CHAL_DSI_ISTAT_TXPKT1_DONE | CHAL_DSI_ISTAT_ERR_CONT_LP1 | CHAL_DSI_ISTAT_ERR_CONT_LP0 |
		//	CHAL_DSI_ISTAT_ERR_CONTROL | CHAL_DSI_ISTAT_ERR_SYNC_ESC)); 
		cslDsiEnaIntEvent( dsiH, (UInt32)CHAL_DSI_ISTAT_TXPKT1_DONE);
#else
        chal_dsi_clr_status ( dsiH->chalH, 0xFFFFFFFF );
#endif        
        chal_dsi_de1_send ( dsiH->chalH, TX_PKT_ENG_0, &dsiChH->chalCmCfg );  

#ifdef __CSL_DSI_USE_INT__  
        res = cslDsiWaitForInt( dsiH, 100 );
#else    
        cslDsiWaitForStatAny_Poll( dsiH, CHAL_DSI_STAT_TXPKT1_DONE, NULL, 100 );
#endif       
        // deactivate PKTC.Start
        chal_dsi_cmnd_start ( dsiH->chalH, TX_PKT_ENG_0, FALSE );

        // send rest of the frame with CONTINUE CMND
        dsiChH->chalCmCfg.dcsCmnd  = dsiChH->dcsCmndCont;
        dsiChH->chalCmCfg.pktCount = pktCount - 1;
        
        if ( isTE ) dsiChH->chalCmCfg.isTE = FALSE;  
    } 
    else
    {
        dsiChH->chalCmCfg.pktCount = pktCount;
    }

    // send rest of the frame 
	//cslDsiEnaIntEvent( dsiH, (UInt32)(CHAL_DSI_ISTAT_TXPKT1_DONE | CHAL_DSI_ISTAT_ERR_CONT_LP1 | CHAL_DSI_ISTAT_ERR_CONT_LP0 |
	//		CHAL_DSI_ISTAT_ERR_CONTROL | CHAL_DSI_ISTAT_ERR_SYNC_ESC));

    cslDsiEnaIntEvent ( dsiH, (UInt32)CHAL_DSI_ISTAT_TXPKT1_DONE );

    chal_dsi_de1_send ( dsiH->chalH, TX_PKT_ENG_0, &dsiChH->chalCmCfg );  
    
    // restore TE mode config - complex, simplify
    dsiChH->chalCmCfg.isTE = isTE;  
    
    if ( req->cslLcdCb == NULL )
    {   
        osStat = OSSEMAPHORE_Obtain ( dsiH->semaDma, 
            TICKS_IN_MILLISECONDS( req->timeOut_ms ) );

        if ( osStat != OSSTATUS_SUCCESS)
        {
            cslDsiDmaStop ( &updMsgCm );
            if( osStat == OSSTATUS_TIMEOUT )
            {
                LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_UpdateCmVc: "
                    "ERR Timed Out Waiting For EOF DMA!\n\r" ); 
                res = CSL_LCD_OS_TOUT;
            }    
            else
            {
                LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_UpdateCmVc: "
                    "ERR OS Err...\n\r" ); 
                res = CSL_LCD_OS_ERR;
            }    
        } 

        //wait for interface to drain
        if( res == CSL_LCD_OK )
        {
            res = cslDsiWaitForInt( dsiH, 100 );
        }    
        else    
        {
            cslDsiWaitForInt( dsiH, 1 );        
        }
            
        // deactivate PKTC.Start
        chal_dsi_cmnd_start ( dsiH->chalH, TX_PKT_ENG_0, FALSE );
        chal_dsi_de1_enable ( dsiH->chalH, FALSE  );

        if( !clientH->hasLock ) OSSEMAPHORE_Release ( dsiH->semaDsi );
    }    
    else
    {   // non-blocking
        osStat = OSQUEUE_Post ( dsiH->updReqQ, 
                    (QMsg_t*) &updMsgCm, TICKS_NO_WAIT );
                    
        if ( osStat != OSSTATUS_SUCCESS)
        {
            if( osStat == OSSTATUS_TIMEOUT )
                res = CSL_LCD_OS_TOUT;
            else
                res = CSL_LCD_OS_ERR;
        }
    }
    return ( res );
} // CSL_DSI_UpdateCmVc


//*****************************************************************************
//                 
// Function Name:  CSL_DSI_CloseClient
// 
// Description:    Close Client Interface 
//
//*****************************************************************************
CSL_LCD_RES_T  CSL_DSI_CloseClient ( CSL_LCD_HANDLE client )
{

    DSI_HANDLE     dsiH;
    DSI_CLIENT     clientH;
    CSL_LCD_RES_T  res;
    
    clientH = (DSI_CLIENT) client;
    dsiH    = (DSI_HANDLE) clientH->lcdH;

    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );
    
    if( clientH->hasLock )
    {
        LCD_DBG ( LCD_DBG_ID,   
            "[CSL DSI] CSL_DSI_CloseClient(): ERR Client LOCK Active!\r\n" ); 
        res = CSL_LCD_ERR;
    }    
    else
    {
        clientH->open = FALSE;    
        dsiH->clients--;
        res = CSL_LCD_OK;
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_CloseClient(): "
            "Clients Left[%d]\r\n", dsiH->clients ); 
    }
    
    OSSEMAPHORE_Release ( dsiH->semaDsiW );
    
    return ( res );
}

//*****************************************************************************
//                 
// Function Name:  CSL_DSI_OpenClient
// 
// Description:    Register Client Of DSI interface 
//
//*****************************************************************************
CSL_LCD_RES_T  CSL_DSI_OpenClient ( 
    UInt32              bus, 
    CSL_LCD_HANDLE*     clientH
    )
{
    DSI_HANDLE      dsiH;
    CSL_LCD_RES_T   res;
    UInt32          i;
    
    if ( bus >= DSI_INST_COUNT )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] CSL_DSI_OpenClient(): ERR Invalid Bus Id!\r\n" ); 
        *clientH = (CSL_LCD_HANDLE) NULL;        
        return ( CSL_LCD_BUS_ID );    
    } 
    
    dsiH = (DSI_HANDLE) &dsiBus[bus];
    
    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );
    
    if( dsiH->init != DSI_INITIALIZED )
    {
        res = CSL_LCD_NOT_INIT;
    }
    else
    {
        for ( i=0; i<DSI_MAX_CLIENT; i++)
        {
            if( !dsiH->client[i].open )
            {
                dsiH->client[i].lcdH = &dsiBus[bus];
                dsiH->client[i].open = TRUE;   
                *clientH = (CSL_LCD_HANDLE) &dsiH->client[i];
                break;
            }
        }
        if( i >= DSI_MAX_CLIENT )
        {
            LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_OpenClient(): ERR, "
                "Max Client Count Reached[%d]\r\n", DSI_MAX_CLIENT ); 
            res = CSL_LCD_INST_COUNT;
        } 
        else
        {
            dsiH->clients++;
            res = CSL_LCD_OK;
        }    
    }
    
    if( res != CSL_LCD_OK )
    {
        *clientH = (CSL_LCD_HANDLE) NULL;        
    } 
    else
    {
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_OpenClient(): OK, "
            "Client Count[%d]\r\n", dsiH->clients ); 
    }   
    
    OSSEMAPHORE_Release ( dsiH->semaDsiW );
    
    return ( res );
}

//*****************************************************************************
//
// Function Name: CSL_DSI_Ulps
// 
// Description:   Enter / Exit ULPS on Clk & Data Line(s)
//                
//*****************************************************************************
CSL_LCD_RES_T  CSL_DSI_Ulps ( CSL_LCD_HANDLE client, Boolean on )
{
    CSL_LCD_RES_T  res = CSL_LCD_OK;
    DSI_HANDLE     dsiH;
    DSI_CLIENT     clientH;
    
    clientH = (DSI_CLIENT) client;
    dsiH    = (DSI_HANDLE) clientH->lcdH;

    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );
    
    if( !clientH->hasLock ) OSSEMAPHORE_Obtain ( dsiH->semaDsi, TICKS_FOREVER );
    
    if ( on && !dsiH->ulps )
    {
        chal_dsi_phy_state  ( dsiH->chalH, PHY_ULPS );
        dsiH->ulps = TRUE;
    } 
    else
    {
        if ( dsiH->ulps )
        { 
            chal_dsi_phy_state  ( dsiH->chalH, PHY_CORE );
            cslDsiWaitForStatAny_Poll( dsiH, CHAL_DSI_STAT_PHY_D0_STOP, 
                NULL, 10 );
            dsiH->ulps = FALSE;
        }
    }
    
    if( !clientH->hasLock ) OSSEMAPHORE_Release ( dsiH->semaDsi );
    
    OSSEMAPHORE_Release ( dsiH->semaDsiW );
    
    return ( res );
} // CSL_DSI_Ulps 


//*****************************************************************************
//                 
// Function Name:  CSL_DSI_Close
// 
// Description:    Close DSI Controller
//
//*****************************************************************************
CSL_LCD_RES_T  CSL_DSI_Close ( UInt32 bus )
{
    DSI_HANDLE          dsiH;
    CSL_LCD_RES_T       res = CSL_LCD_OK;

    dsiH = (DSI_HANDLE) &dsiBus[bus];

    OSSEMAPHORE_Obtain ( dsiH->semaDsi , TICKS_FOREVER );
    OSSEMAPHORE_Obtain ( dsiH->semaDsiW, TICKS_FOREVER );

    if( dsiH->init != DSI_INITIALIZED )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] CSL_DSI_Close(): DSI Interface Not Init\r\n" ); 
        res = CSL_LCD_ERR;
        goto CSL_DSI_CloseRet;
    }    
    
    if( dsiH->clients != 0 )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] CSL_DSI_Close(): DSI Interface Client Count != 0\n" ); 
        res = CSL_LCD_ERR;
        goto CSL_DSI_CloseRet;
    }    
    
    chal_dsi_off ( dsiH->chalH );
    chal_dsi_phy_afe_off ( dsiH->chalH );
    dsiH->init = ~DSI_INITIALIZED;

    LCD_DBG ( LCD_DBG_ID, 
        "[CSL DSI] CSL_DSI_Close(): DONE For DSI Bus[%d]\r\n", bus ); 

CSL_DSI_CloseRet:    
    OSSEMAPHORE_Release ( dsiH->semaDsiW );
    OSSEMAPHORE_Release ( dsiH->semaDsi  );
    
    return ( res );
}

//*****************************************************************************
//                 
// Function Name:  CSL_DSI_Init
// 
// Description:    Init DSI Controller
//
//*****************************************************************************
CSL_LCD_RES_T  CSL_DSI_Init ( const pCSL_DSI_CFG dsiCfg )
{
    CSL_LCD_RES_T       res = CSL_LCD_OK;
    DSI_HANDLE          dsiH;
    
    CHAL_DSI_MODE_t     chalMode;
    CHAL_DSI_INIT_t     chalInit;
    CHAL_DSI_AFE_CFG_t  chalAfeCfg;                     

    if ( dsiCfg->bus >= DSI_INST_COUNT )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] CSL_DSI_Init(): ERR Invalid Bus Id!\r\n" ); 
        return ( CSL_LCD_BUS_ID );    
    }

    dsiH = (DSI_HANDLE) &dsiBus[dsiCfg->bus];
    if( dsiH->init == DSI_INITIALIZED )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] CSL_DSI_Init(): DSI Interface Already Init\r\n" ); 
        res = CSL_LCD_IS_OPEN;
    }    

    if( dsiH->initOnce != DSI_INITIALIZED )
    {
        memset ( dsiH, 0, sizeof(DSI_HANDLE_t) );
        
        dsiH->bus = dsiCfg->bus;

		spin_lock_init(&(dsiH->bcm_dsi_spin_Lock));
		
        if( dsiH->bus == 0 )
        {
            dsiH->dsiCoreRegAddr = IO_ADDRESS(CSL_DSI0_BASE_ADDR);
            dsiH->interruptId    = CSL_DSI0_IRQ;
            dsiH->lisr           = cslDsi0Stat_LISR;
            dsiH->hisr           = cslDsi0Stat_HISR;
            dsiH->task           = cslDsi0UpdateTask;
            dsiH->dma_cb         = cslDsi0EofDma;
        }
    
        if ( !cslDsiOsInit ( dsiH ) )
        {
            LCD_DBG ( LCD_DBG_ID, 
                "[CSL DSI] CSL_DSI_Init(): ERROR OS Init!\r\n" ); 
            return ( CSL_LCD_OS_ERR );    
        }
        else
        {
            dsiH->initOnce = DSI_INITIALIZED;    
        }
    }    
    
    // Init User Controlled Values
    chalInit.dlCount         = dsiCfg->dlCount;
    chalInit.clkContinuous   = dsiCfg->enaContClock;
    
    chalMode.enaContClock    = dsiCfg->enaContClock;  // 2'nd time ?
    chalMode.enaRxCrc        = dsiCfg->enaRxCrc;     
    chalMode.enaRxEcc        = dsiCfg->enaRxEcc;     
    chalMode.enaHsTxEotPkt   = dsiCfg->enaHsTxEotPkt;
    chalMode.enaLpTxEotPkt   = dsiCfg->enaLpTxEotPkt;
    chalMode.enaLpRxEotPkt   = dsiCfg->enaLpRxEotPkt;
    
    // Init HARD-CODED Settings
    chalAfeCfg.afeCtaAdj     = 7;      // 0 - 15
    chalAfeCfg.afePtaAdj     = 7;      // 0 - 15
    chalAfeCfg.afeBandGapOn  = TRUE;   
    chalAfeCfg.afeDs2xClkEna = FALSE;  

#if !( defined (_ATHENA_)&& (CHIP_REVISION == 10) ) 
    chalAfeCfg.afeClkIdr     = 6;      // 0 - 7  DEF 6
    chalAfeCfg.afeDlIdr      = 6;      // 0 - 7  DEF 6
#endif
 
    // Init Values for HS/ESC Clock Trees 
    if(!cslDsiClkTreeInit (dsiH, &dsiCfg->hsBitClk, &dsiCfg->escClk) )
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] CSL_DSI_Init(): ERROR HS/ESC Clk Init!\r\n" ); 
        return ( CSL_LCD_ERR );    
    }
    if ( cslDsiClkTreeEna( dsiH ) )
    {   
        dsiH->chalH = chal_dsi_init ( dsiH->dsiCoreRegAddr, &chalInit );

        if( dsiH->chalH == NULL )
        {
            LCD_DBG ( LCD_DBG_ID, 
                "[CSL DSI] CSL_DSI_Init(): ERROR in cHal Init!\r\n" ); 
            res = CSL_LCD_ERR;    
        }
        else
        {
            
            chal_dsi_phy_afe_on ( dsiH->chalH, &chalAfeCfg );
            // as per HERA rdb clksel must be done before ANY timing is set
            chal_dsi_on ( dsiH->chalH, &chalMode );

			printk("chal_dsi_set_timing() dsiH->clkCfg.escClk_MHz:%d dsiH->clkCfg.hsBitClk_MHz:%d dsiCfg->lpBitRate_Mbps:%d ", 
				dsiH->clkCfg.escClk_MHz,
				dsiH->clkCfg.hsBitClk_MHz,
				dsiCfg->lpBitRate_Mbps);

            if( !chal_dsi_set_timing ( 
                    dsiH->chalH, 
                    dsiCfg->dPhySpecRev,
                    dsiH->clkCfg.escClk_MHz,
                    dsiH->clkCfg.hsBitClk_MHz,
                    dsiCfg->lpBitRate_Mbps ) )
            {    
                LCD_DBG ( LCD_DBG_ID, 
                    "[CSL DSI] CSL_DSI_Init(): "
                    "ERROR In Timing Calculation!\r\n" ); 
                res = CSL_LCD_ERR;    
            }
            else
            {
                chal_dsi_de1_set_dma_thresh ( dsiH->chalH, 32 );
                
                cslDsiClearAllFifos ( dsiH );
                // wait for STOP state
                OSTASK_Sleep (TICKS_IN_MILLISECONDS (1) );
            }
        } 
    } 
    else
    {
        LCD_DBG ( LCD_DBG_ID, 
            "[CSL DSI] CSL_DSI_Init(): ERROR Enabling DSI Clocks\r\n" ); 
        res = CSL_LCD_ERR;    
    }
    
    if( res == CSL_LCD_OK )
    {    
        LCD_DBG ( LCD_DBG_ID, "[CSL DSI] CSL_DSI_Init(): OK\r\n" ); 
        dsiH->init = DSI_INITIALIZED;
        dsiH->bus  = dsiCfg->bus;
    }
    else
    {
        dsiH->init = 0;
    }    
    
    return ( res );
} // CSL_DSI_Init



int Log_DebugPrintf(UInt16 logID, char* fmt, ...)
{

	char p[255];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(p, 255, fmt, ap);
	va_end(ap);

	printk(p);


	return 1;
}




