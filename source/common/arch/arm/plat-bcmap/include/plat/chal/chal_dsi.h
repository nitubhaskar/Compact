/*****************************************************************************
*  Copyright 2001 - 2008 Broadcom Corporation.  All rights reserved.
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
*  @file  chal_dsi.h
*
*  @brief ATHENA DSI Controller cHAL interface
*
*  @note
*****************************************************************************/

/* Requires the following header files before its inclusion in a c file
#include "chal_common.h"
*/

#ifndef __CHAL_DSI_H__
#define __CHAL_DSI_H__

#ifdef __cplusplus
extern "C" {
#endif

#define CHIP_REVISION 20

/**
 * @addtogroup cHAL_Interface 
 * @{
 */

/**
*
*  Packet Schedule Type; Applicable if Video Mode Interface is Active
*
*****************************************************************************/
#define CHAL_DSI_CMND_WHEN_BEST_EFFORT      0  
#define CHAL_DSI_CMND_WHEN_HBP_HFP          1  
#define CHAL_DSI_CMND_WHEN_HA_AFTER_BURST   2  
#define CHAL_DSI_CMND_WHEN_VBP              3  

/**
*
*  DSI Core FIFOs 
*
*****************************************************************************/
#define CHAL_DSI_CTRL_CLR_LANED_FIFO            (0x00000080)  //      
#define CHAL_DSI_CTRL_CLR_PIX_BYTEC_FIFO        (0x00000040)  //      
#define CHAL_DSI_CTRL_CLR_CMD_PIX_BYTEC_FIFO    (0x00000020)  //      
#define CHAL_DSI_CTRL_CLR_PIX_DATA_FIFO         (0x00000010)  //      
#define CHAL_DSI_CTRL_CLR_CMD_DATA_FIFO         (0x00000008)  //      

/**
*
*  DSI Core Interrupt & Status  
*
*****************************************************************************/
#define CHAL_DSI_ISTAT_ERR_FIFO                 (0x01000000)  //24           
#define CHAL_DSI_ISTAT_TXPKT1_DONE              (0x00800000)  //23    
#define CHAL_DSI_ISTAT_PHY_DIR_REV2FWD          (0x00400000)  //22           
#define CHAL_DSI_ISTAT_PHY_D1_ULPS              (0x00200000)  //21           
#define CHAL_DSI_ISTAT_PHY_D1_STOP              (0x00100000)  //20           
#define CHAL_DSI_ISTAT_PHY_RX_LPDT              (0x00080000)  //19           
#define CHAL_DSI_ISTAT_PHY_RX_TRIG              (0x00040000)  //18           
#define CHAL_DSI_ISTAT_PHY_D0_ULPS              (0x00020000)  //17           
#define CHAL_DSI_ISTAT_PHY_D0_LPDT              (0x00010000)  //16           
#define CHAL_DSI_ISTAT_PHY_DIR_FWD2REV          (0x00008000)  //15           
#define CHAL_DSI_ISTAT_PHY_D0_STOP              (0x00004000)  //14    
#define CHAL_DSI_ISTAT_PHY_CLK_ULPS             (0x00002000)  //13           
#define CHAL_DSI_ISTAT_PHY_CLK_HS               (0x00001000)  //12           
#define CHAL_DSI_ISTAT_PHY_CLK_STOP             (0x00000800)  //11           
#define CHAL_DSI_ISTAT_PR_TO                    (0x00000400)  //10           
#define CHAL_DSI_ISTAT_TA_TO                    (0x00000200)  //09           
#define CHAL_DSI_ISTAT_LPRX_TO                  (0x00000100)  //08           
#define CHAL_DSI_ISTAT_HSTX_TO                  (0x00000080)  //07           
#define CHAL_DSI_ISTAT_ERR_CONT_LP1             (0x00000040)  //06           
#define CHAL_DSI_ISTAT_ERR_CONT_LP0             (0x00000020)  //05           
#define CHAL_DSI_ISTAT_ERR_CONTROL              (0x00000010)  //04           
#define CHAL_DSI_ISTAT_ERR_SYNC_ESC             (0x00000008)  //03           
#define CHAL_DSI_ISTAT_RX2_PKT                  (0x00000004)  //02           
#define CHAL_DSI_ISTAT_RX1_PKT                  (0x00000002)  //01           
#define CHAL_DSI_ISTAT_TXPKT1_END               (0x00000001)  //00    

#define CHAL_DSI_STAT_ERR_FIFO                  CHAL_DSI_ISTAT_ERR_FIFO       
#define CHAL_DSI_STAT_TXPKT1_DONE               CHAL_DSI_ISTAT_TXPKT1_DONE    
#define CHAL_DSI_STAT_PHY_DIR_REV2FWD           CHAL_DSI_ISTAT_PHY_DIR_REV2FWD
#define CHAL_DSI_STAT_PHY_D1_ULPS               CHAL_DSI_ISTAT_PHY_D1_ULPS    
#define CHAL_DSI_STAT_PHY_D1_STOP               CHAL_DSI_ISTAT_PHY_D1_STOP    
#define CHAL_DSI_STAT_PHY_RX_LPDT               CHAL_DSI_ISTAT_PHY_RX_LPDT    
#define CHAL_DSI_STAT_PHY_RX_TRIG               CHAL_DSI_ISTAT_PHY_RX_TRIG    
#define CHAL_DSI_STAT_PHY_D0_ULPS               CHAL_DSI_ISTAT_PHY_D0_ULPS    
#define CHAL_DSI_STAT_PHY_D0_LPDT               CHAL_DSI_ISTAT_PHY_D0_LPDT    
#define CHAL_DSI_STAT_PHY_DIR_FWD2REV           CHAL_DSI_ISTAT_PHY_DIR_FWD2REV
#define CHAL_DSI_STAT_PHY_D0_STOP               CHAL_DSI_ISTAT_PHY_D0_STOP    
#define CHAL_DSI_STAT_PHY_CLK_ULPS              CHAL_DSI_ISTAT_PHY_CLK_ULPS   
#define CHAL_DSI_STAT_PHY_CLK_HS                CHAL_DSI_ISTAT_PHY_CLK_HS     
#define CHAL_DSI_STAT_PHY_CLK_STOP              CHAL_DSI_ISTAT_PHY_CLK_STOP   
#define CHAL_DSI_STAT_PR_TO                     CHAL_DSI_ISTAT_PR_TO          
#define CHAL_DSI_STAT_TA_TO                     CHAL_DSI_ISTAT_TA_TO          
#define CHAL_DSI_STAT_LPRX_TO                   CHAL_DSI_ISTAT_LPRX_TO        
#define CHAL_DSI_STAT_HSTX_TO                   CHAL_DSI_ISTAT_HSTX_TO        
#define CHAL_DSI_STAT_ERR_CONT_LP1              CHAL_DSI_ISTAT_ERR_CONT_LP1   
#define CHAL_DSI_STAT_ERR_CONT_LP0              CHAL_DSI_ISTAT_ERR_CONT_LP0   
#define CHAL_DSI_STAT_ERR_CONTROL               CHAL_DSI_ISTAT_ERR_CONTROL    
#define CHAL_DSI_STAT_ERR_SYNC_ESC              CHAL_DSI_ISTAT_ERR_SYNC_ESC   
#define CHAL_DSI_STAT_RX2_PKT                   CHAL_DSI_ISTAT_RX2_PKT        
#define CHAL_DSI_STAT_RX1_PKT                   CHAL_DSI_ISTAT_RX1_PKT        
#define CHAL_DSI_STAT_TXPKT1_END                CHAL_DSI_ISTAT_TXPKT1_END     
  
/**
*
*  D-PHY State Control Modes
*
*****************************************************************************/
typedef enum
{
     PHY_TXSTOP  = 1,        ///< PHY FORCE TX-STOP 
     PHY_ULPS    = 2,        ///< PHY FORCE ULPS 
     PHY_CORE    = 3,        ///< PHY under Core Control (remove any forced state)
} CHAL_DSI_PHY_STATE_t; 
  
/**
*
*  Display Engine 1 Color Modes (Command Mode Interface)
*
*****************************************************************************/
typedef enum
{
    DE1_CM_888U_TO_565  = 0, ///< IN 4B OUT 2B
    DE1_CM_565_LSB      = 1, ///< IN 4B OUT 2B  1 565 pixel per AHB per (only lower 16bits used, no conv)
    DE1_CM_565          = 2, ///< IN 4B OUT 4B  2 565 pixel per AHB
    DE1_CM_888U         = 3, ///< IN 4B OUT 3B  1 888 pixel per AHB (upper byte not used)
} CHAL_DSI_DE1_COL_MOD_t;
  

#if (CHIP_REVISION >= 20)

/**
*
*  TE Inputs
*
*****************************************************************************/
#define CHAL_DSI_TE_IN_0                 (0)   ///< DSI TE In 0
#define CHAL_DSI_TE_IN_1                 (1)   ///< DSI TE In 1

/**
*
*  TE Modes
*
*****************************************************************************/
#define CHAL_DSI_TE_MODE_VSYNC          (0)   ///< TE MODE vsync       (mode 1)    
#define CHAL_DSI_TE_MODE_VSYNC_HSYNC    (1)   ///< TE MODE vsync_hsync (mode 2)

/**
*
*  TE Polarity for VSYNC & HSYNC
*
*****************************************************************************/
#define CHAL_DSI_TE_ACT_POL_LO          (0)   ///< Active LO ( VSYNC & HSYNC )
#define CHAL_DSI_TE_ACT_POL_HI          (1)   ///< Active HI ( VSYNC & HSYNC )

/**
*
*  TECTL Input Configuration
*
*****************************************************************************/
typedef struct 
{
    UInt32     te_mode;      // te mode ( VSYNC or VSYNC & HSYNC )
    UInt32     sync_pol;     // sync polarity
    UInt32     vsync_width;  // vsync_width [tectl_clk_count]
    UInt32     hsync_line;   // hsync_line
} CHAL_DSI_TE_CFG_t, *pCHAL_DSI_TE_CFG;
#endif // #if (CHIP_REVISION >= 20)

/**
*
*  TE Mode 
*
*****************************************************************************/
typedef enum
{
    TE_TRIG             = 0, ///< TRIGGER MESSAGE
    TE_EXT_0            = 1, ///< TE Input 0
    TE_EXT_1            = 2, ///< TE Input 1
} CHAL_DSI_TE_MODE_t;


/**
*
*  DSI message Definition
*
*****************************************************************************/
typedef struct
{
    cUInt32   vc;            ///< Dest. Virtual Channel
    cBool     isLP;          ///< Low Power | High Speed
    cBool     isLong;        ///< LONG | SHORT
    cUInt32   vmWhen;        ///< if Video Mode active, when
    cUInt32   dsiCmnd;       ///< DSI command to send
    cUInt8*   msg;           ///< Up to 2 SHORT, Up to 8 Byte LONG message, FIFO
    cUInt32   msgLen;        ///< Max 8 ?
    cBool     endWithBta;    ///< end with BTA
} CHAL_DSI_CMND_t, *pCHAL_DSI_CMND; 

/**
*
*  Command Mode Interface
*
*****************************************************************************/
typedef struct {
    cUInt32             vc;            ///< Dest. Virtual Channel 
    cBool               isLP;          ///< Low Power | High Speed
    cBool               isTE;          ///< TE synced
    cUInt32             vmWhen;        ///< if Video Mode active, when
    cUInt32             dsiCmnd;       ///< DSI Command Type
    cUInt32             dcsCmnd;       ///< DCS Command
    cUInt32             pktSizeBytes;  ///< packet payload size exc dcs&dsi cmnds
    cUInt32             pktCount;      ///< repeat count
    cBool               start;         ///< start 
} CHAL_DSI_CM_CFG_t, *pCHAL_DSI_CM_CFG;


/**
*
*  Status of Received Message
*
*****************************************************************************/
typedef enum
{
    ERR_RX_OK              = 0x0001,
    ERR_RX_CRC             = 0x0010,   
    ERR_RX_MULTI_BIT       = 0x0020,   
    ERR_RX_ECC             = 0x0040,   
    ERR_RX_CORRECTABLE     = 0x0080,    
    ERR_RX_PKT_INCOMPLETE  = 0x0100,
} CHAL_DSI_RX_STAT_t; 


/**
*
*  DSI CORE CLOCK SELECTION
*
*****************************************************************************/
typedef enum 
{
    CHAL_DSI_BIT_CLK_DIV_BY_8 = 0,  ///< BYTE CLOCK
    CHAL_DSI_BIT_CLK_DIV_BY_4 = 1,  ///< DDR2
    CHAL_DSI_BIT_CLK_DIV_BY_2 = 2,  ///< DDR
} CHAL_DSI_CLK_SEL_t, *pCHAL_DSI_CLK_SEL; 


#define RX_TYPE_TRIG          1
#define RX_TYPE_READ_REPLY    2   
#define RX_TYPE_ERR_REPLY     4   

/**
*
*  DSI Replay
*
*****************************************************************************/
typedef struct
{
    cUInt8              type;
    cUInt8              trigger;
    cUInt8              readReplyDt;
    cUInt8              readReplySize;
    cUInt32             readReplyRxStat;
    cUInt8*             pReadReply;
    cUInt8              errReportDt;
    cUInt32             errReportRxStat;
    cUInt16             errReport;
} CHAL_DSI_REPLY_t, *pCHAL_DSI_REPLY; 

/**
*
*  DSI cHal Init 
*
*****************************************************************************/
typedef struct
{
    cUInt32         dlCount;          ///< DL count
    cBool           clkContinuous;    ///< Continuous clock
} CHAL_DSI_INIT_t, *pCHAL_DSI_INIT; 

/**
*
*  DSI Core Configuration Flags 
*
*****************************************************************************/
typedef struct
{
    cBool           enaContClock;     ///< DSI-PHY-C
    cBool           enaRxCrc;         ///< DSI-CTRL
    cBool           enaRxEcc;         ///< DSI-CTRL
    cBool           enaHsTxEotPkt;    ///< DSI-CTRL
    cBool           enaLpTxEotPkt;    ///< DSI-CTRL
    cBool           enaLpRxEotPkt;    ///< DSI-CTRL
} CHAL_DSI_MODE_t, *pCHAL_DSI_MODE;

/**
*
*  DSI AnalogFrontEnd Configuration Flags 
*
*****************************************************************************/
typedef struct
{
    cUInt32         afeCtaAdj;        ///< DSI-PHY-AFE-C
    cUInt32         afePtaAdj;        ///< DSI-PHY-AFE-C
    cBool           afeBandGapOn;     ///< DSI-PHY-AFE-C
    cBool           afeDs2xClkEna;    ///< DSI-PHY-AFE-C                   
#if (CHIP_REVISION >= 20)
    cUInt32         afeClkIdr;
    cUInt32         afeDlIdr;
#endif    
} CHAL_DSI_AFE_CFG_t, *pCHAL_DSI_AFE_CFG;

/**
*
*  DSI Core Timing Settings
*
*****************************************************************************/
typedef struct
{
    cUInt32         esc2lpRatio;      ///< DSI-PHY-C
    
	cUInt32         hsInit_hsClk;     ///< TIMING  	   
	cUInt32         hsWup_hsClk;	  ///< TIMING
	cUInt32         lpWup_hsClk;	  ///< TIMING
	cUInt32         hsCpre_hsClk;	  ///< TIMING
	cUInt32         hsCzero_hsClk;    ///< TIMING
	cUInt32         hsCpost_hsClk;    ///< TIMING
	cUInt32         hsCtrail_hsClk;   ///< TIMING

	cUInt32         hsLpx_hsClk;	  ///< TIMING HS REQ Time
    cUInt32         hsPre_hsClk;	  ///< TIMING
	cUInt32         hsZero_hsClk;	  ///< TIMING
	cUInt32         hsTrail_hsClk;    ///< TIMING
    cUInt32         hsExit_hsClk;	  ///< TIMING
    
    cUInt32         lpx_escClk;		  ///< TIMING x[esc_clk] this is [lpx] Any LP REQ Timing
    cUInt32         lpTaGo_lpxCnt;    ///< TIMING x[lpx]
    cUInt32         lpTaSure_lpxCnt;  ///< TIMING x[lpx]                     
    cUInt32         lpTaGet_lpxCnt;   ///< TIMING x[lpx]                     
} CHAL_DSI_TIMING_t, *pCHAL_DSI_TIMING;


/**
*
*  DSI cHal Result
*
*****************************************************************************/
typedef enum 
{
    CHAL_DSI_OK         = 0,  ///< OK
    CHAL_DSI_ERR        = 1,  ///< Unspecified
    CHAL_DSI_MSG_SIZE   = 2,  ///< Unsupported msg size
    CHAL_DSI_CMND_DONE  = 3,  ///< CMND DONE not set
    CHAL_DSI_RX_NO_PKT  = 4,  ///< NO RX PACKETs PENDING
    CHAL_DSI_LINE_COUNT = 5,  ///< Wrong Number Of Interface Data Lines
} CHAL_DSI_RES_t; 

/**
*
*  @brief    Force D-PHY State
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 state   (in)  D-PHY State
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_phy_state ( CHAL_HANDLE handle, 
        CHAL_DSI_PHY_STATE_t state );

/**
*
*  @brief    Set DSI Interrupt Enable
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 intMask (in)  Enable/Disable Int Mask; 1=ENA 0=DIS
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_ena_int ( CHAL_HANDLE handle, cUInt32  intMask );

/**
*
*  @brief    Get Current DSI Interrupt 
*
*  @param	 handle  (in)  DSI cHAL handle
*
*  @return	 cUInt32 (out) Current DSI interrupt event
*
*  @note     
*****************************************************************************/                           
cUInt32 chal_dsi_get_int ( CHAL_HANDLE handle );

/**
*
*  @brief    Clears DSI Interrupts 
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 intMask (in)  Int Clear Mask; 1=CLEAR
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_clr_int ( CHAL_HANDLE handle, cUInt32  intMask );


/**
*
*  @brief    Clears Selected DSI FIFOs
*
*  @param	 handle   (in)  DSI cHAL handle
*  @param	 fifoMask (in)  FIFO Select Mask; 1=CLEAR
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_clr_fifo ( CHAL_HANDLE handle, cUInt32  fifoMask );


/**
*
*  @brief    Clear DSI Interrupt Status
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 intMask (in)  Int Clear Mask; 1=CLEAR
*
*  @return	 cUint32 (out) Current DSI interrupt status
*
*  @note     
*****************************************************************************/                           
cUInt32 chal_dsi_get_status ( CHAL_HANDLE handle );

/**
*
*  @brief    Get DSI Interrupt Status
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 intMask (in)  Int Clear Mask; 1=CLEAR
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_clr_status ( CHAL_HANDLE handle, cUInt32  statMask );
             
/**
*
*  @brief    Set/Clear CMND_EN of DSI CMD PKTC Register
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param    txEng   (in)  TX PKT Engine, 0 for ATHENA
*  @param	 start   (in)  TRUE=Start Command FALSE=Clear CMND_EN bit
*
*  @return	 void
*
*  @note     Start/Stop DSI Command configured through PKTH & PKTC reg. intf.
*****************************************************************************/                           
cVoid chal_dsi_cmnd_start ( CHAL_HANDLE handle, cUInt8 txEng, cBool start );   


#if (CHIP_REVISION >= 20)
/**
*
*  @brief    Configure TE 
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 teIn    (in)  DSI TE Input
*  @param	 teCfg   (in)  TE Configration
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cInt32 chal_dsi_te_cfg ( CHAL_HANDLE handle, UInt32 teIn, pCHAL_DSI_TE_CFG teCfg );
#endif // #if (CHIP_REVISION >= 20)

/**
*
*  @brief    Set TE SYNC mode
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 teMode  (in)  TE SYNC Mode
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_te_mode ( CHAL_HANDLE handle, CHAL_DSI_TE_MODE_t teMode );

/**
*
*  @brief    Set DMA Threshold For Display Engine 1 FIFO
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 thresh  (in)  Threshold 
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_de1_set_dma_thresh ( CHAL_HANDLE handle, cUInt32 thresh );

/**
*
*  @brief    Get DMA Address Of Display Engine 1 FIFO
*
*  @param	 handle  (in)  DSI cHAL handle
*
*  @return	 cUInt32 (out) DMA Address
*
*  @note     
*****************************************************************************/                           
cUInt32 chal_dsi_de1_get_dma_address( CHAL_HANDLE handle );

/**
*
*  @brief    Set Display Engine 1 DMA Transfer Size
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 wc      (in)  DMA Transfer Cycle Count
*
*  @return	 void 
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_de1_set_wc ( CHAL_HANDLE handle, cUInt32 wc );   

/**
*
*  @brief    Set Display Engine 1 Color Mode
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 cm      (in)  DE1 Color Mode Configuration
*
*  @return	 void 
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_de1_set_cm ( CHAL_HANDLE handle, CHAL_DSI_DE1_COL_MOD_t cm );

/**
*
*  @brief    Enable/Disable Display Engine 1
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 ena     (in)  TRUE=enable FALSE=disable
*
*  @return	 void 
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_de1_enable ( CHAL_HANDLE handle, cBool ena );

/**
*
*  @brief    Send Using Command Mode Display Intf of Display Engine 1
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param    txEng   (in)  TX PKT Engine, 0 for ATHENA
*  @param	 cmCfg   (in)  Command Mode Interface Configuration
*
*  @return	 result  (out) Status Of Send Request 
*
*  @note     
*****************************************************************************/                           
CHAL_DSI_RES_t chal_dsi_de1_send ( 
    CHAL_HANDLE      handle, 
    cUInt8           txEng,
    pCHAL_DSI_CM_CFG cmCfg );   

/**
*
*  @brief    Send DSI Command Using PKTH & PKTC Interface
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param    txEng   (in)  TX PKT Engine, 0 for ATHENA
*  @param	 cmnd    (in)  DSI Command Definition
*
*  @return	 result  (out) Status Of Send Request 
*
*  @note     
*****************************************************************************/                           
CHAL_DSI_RES_t chal_dsi_send_cmnd ( 
    CHAL_HANDLE     handle, 
    cUInt8          txEng,
    pCHAL_DSI_CMND  cmnd );   

/**
*
*  @brief    Send Bus Turn Around Request
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param    txEng   (in)  TX PKT Engine, 0 for ATHENA
*
*  @return	 cVoid
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_send_bta ( CHAL_HANDLE handle, cUInt8 txEng );   

/**
*
*  @brief    Send Trigger
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param    txEng   (in)  TX PKT Engine, 0 for ATHENA
*  @param	 trig    (in)  8-bit TRIGGER message
*
*  @return	 cVoid
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_send_trig ( CHAL_HANDLE handle, cUInt8 txEng, cUInt8 trig );   

/**
*
*  @brief    Read Reply
*
*  @param	 handle  (in)      DSI cHAL handle
*  @param	 intStat (in)      Interrupt Status collected at command Done
*  @param	 msg     (in/out)  DSI Reply
*
*  @return	 result  (out) Status Of Send Request 
*
*  @note     
*****************************************************************************/                           
CHAL_DSI_RES_t chal_dsi_read_reply ( 
    CHAL_HANDLE         handle, 
    cUInt32             intStat,  
    pCHAL_DSI_REPLY     reply 
    );

/**
*
*  @brief    Configure DSI Interface Settings
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 dsiMode (in)  DSI Interface settings
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_on ( CHAL_HANDLE handle, pCHAL_DSI_MODE dsiMode );

/**
*
*  @brief    Disable DSI Interface
*
*  @param	 handle  (in)  DSI cHAL handle
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_off ( CHAL_HANDLE handle );

/**
*
*  @brief    Configure DSI AnalogFrontEnd 
*
*  @param	 handle  (in)  DSI cHAL handle
*  @param	 afeCfg  (in)  DSI AFE settings
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_phy_afe_on ( CHAL_HANDLE handle, pCHAL_DSI_AFE_CFG afeCfg  );

/**
*
*  @brief    Disable DSI AnalogFrontEnd Interface
*
*  @param	 handle  (in)  DSI cHAL handle
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
cVoid chal_dsi_phy_afe_off ( CHAL_HANDLE handle );

/**
*
*  @brief    Configure DSI Timing
*
*  @param	 handle         (in)  DSI cHAL handle
*  @param	 DPHY_SpecRev   (in)  DSI D-PHY Spec Rev
*  @param	 escClk_MHz     (in)  System Setting For ESC_CLK
*  @param	 hsBitRate_Mbps (in)  Target HS Bit Rate
*  @param	 lpBitRate_Mbps (in)  Target LP Bit Rate
*
*  @return	 cBool
*
*  @note     
*****************************************************************************/                           
cBool chal_dsi_set_timing ( 
    CHAL_HANDLE  handle,
    cUInt32      DPHY_SpecRev,
    float        escClk_MHz,              
    float        hsBitRate_Mbps,  
    float        lpBitRate_Mbps  
    );

/**
*
*  @brief    Init DSI cHal
*
*  @param	 handle  (in)  DSI cHal handle
*  @param	 init    (in)  DSI cHal Init Settings
*
*  @return	 void
*
*  @note     
*****************************************************************************/                           
CHAL_HANDLE chal_dsi_init ( cUInt32 baseAddress, pCHAL_DSI_INIT dsiInit );
/** @} */

#ifdef __cplusplus
}
#endif

#endif // __CHAL_DSI_H__
