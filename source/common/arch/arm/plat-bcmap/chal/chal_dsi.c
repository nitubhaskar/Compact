/***************************************************************************
*
* Copyright 2004 - 2009 Broadcom Corporation.  All rights reserved.
*
* Unless you and Broadcom execute a separate written software license
* agreement governing use of this software, this software is licensed to you
* under the terms of the GNU General Public License version 2, available at
* http://www.gnu.org/copyleft/gpl.html (the "GPL").
*
* Notwithstanding the above, under no circumstances may you combine this
* software in any way with any other Broadcom software provided under a
* license other than the GPL, without Broadcom's express prior written
* consent.
*
****************************************************************************/
/**
*  @file   chal_dsi.c
*
*  @brief  ATHENA DSI Controller cHAL source code file.
*
*  @note
*
****************************************************************************/

#include "plat/chal/chal_common.h"
#include "plat/chal/chal_dsi.h"
#include "plat/rdb/brcm_rdb_sysmap.h"
#include "plat/rdb/brcm_rdb_syscfg.h"
#include "plat/chal/chal_dsi_ath_b0_reg.h"
#include "plat/rdb/brcm_rdb_util.h"
#include <asm/io.h>

#define CHIP_REVISION 20

typedef struct
{
    cUInt32   baseAddr;
    cInt32    dlCount;
    cBool     clkContinuous;
}CHAL_DSI_HANDLE_st, *CHAL_DSI_HANDLE;

//*****************************************************************************
// Local Variables
//*****************************************************************************
static CHAL_DSI_HANDLE_st dsi_dev[1];       

//*****************************************************************************
// Local Functions
//*****************************************************************************
//SETs REGISTER BIT FIELD; VALUE IS 0 BASED 
#define DSI_REG_FIELD_SET(r,f,d)        \
    ( ((BRCM_REGTYPE(r))(d) << BRCM_FIELDSHIFT(r,f) ) & BRCM_FIELDMASK(r,f) )

//SETs REGISTER BITs Defined WITH MASK
#define DSI_REG_WRITE_MASKED(b,r,m,d)   \
    ( BRCM_WRITE_REG(b,r,(BRCM_READ_REG(b,r) & (~m)) | d) )

// NO OF DATA LINES SUPPORTED
#define DSI_DL_COUNT                1

// DSI COMMAND TYPE
#define CMND_CTRL_CMND_PKT          0               
#define CMND_CTRL_CMND_PKTwBTA      1               
#define CMND_CTRL_TRIG              2               
#define CMND_CTRL_BTA               3               
                   
// DSI PACKET SOURCE                                       
#define DSI_PKT_SRC_CMND_FIFO       0               
#define DSI_PKT_SRC_DE0             1               
#define DSI_PKT_SRC_DE1             2               
#define DSI_PKT_SRC_INV             3                

#define DSI_DT_VC_MASK              0x000000C0      
#define DSI_DT_VC_MASK_SHIFT        6               
#define DSI_DT_MASK                 0x0000003F      

// D-PHY Timing Record
typedef struct
{
    char*   name;           
    UInt32  timeBase;         
    UInt32  mode;             
    UInt32  time_min1_ns;
    UInt32  time_min1_ui;
    UInt32  time_min2_ns;
    UInt32  time_min2_ui;
    UInt32  time_max_ns;
    UInt32  time_max_ui;
    UInt32  counter_min;
    UInt32  counter_max;
    UInt32  counter_step;
    UInt32  counter_offs;
    UInt32  counter;         // <out> calculated value of the register
    float   period;          // <out> dbg
} DSI_COUNTER;

//--- Counter Mode Flags
// record has MAX value set
#define DSI_C_HAS_MAX        1  
// record MIN value is MAX of 2 values 
#define DSI_C_MIN_MAX_OF_2   2  

//--- Counter timeBase Flags
// ESC2LPDT entry - must be first record
#define DSI_C_TIME_ESC2LPDT  0   
// counts in HS Bit Clk
#define DSI_C_TIME_HS        1   
// counts in ESC CLKs
#define DSI_C_TIME_ESC       2   

// DSI Core Timing Registers
typedef enum
{
    DSI_C_ESC2LP_RATIO = 0,
    DSI_C_HS_INIT,    
    DSI_C_HS_WAKEUP,  
    DSI_C_LP_WAKEUP,  
    DSI_C_HS_CLK_PRE,
    DSI_C_HS_CLK_PREPARE,
    DSI_C_HS_CLK_ZERO,
    DSI_C_HS_CLK_POST,
    DSI_C_HS_CLK_TRAIL,
    DSI_C_HS_LPX,     
    DSI_C_HS_PRE,     
    DSI_C_HS_ZERO,       
    DSI_C_HS_TRAIL,     
    DSI_C_HS_EXIT,       
    DSI_C_LPX,               
    DSI_C_LP_TA_GO,     
    DSI_C_LP_TA_SURE, 
    DSI_C_LP_TA_GET,
    DSI_C_MAX,   
} DSI_TIMING_C;


//--------------------------------------------------------------
// D-PHY 0.92 Timing
//   o changed T-INIT to 1ms(from1us) to fix LM2550 
//     start-up issues
//--------------------------------------------------------------
DSI_COUNTER  dsi_dphy_0_92[] =
{                         
    // LP Data Symbol Rate Calc - MUST BE FIRST RECORD
    { "ESC2LP_RATIO" , DSI_C_TIME_ESC2LPDT, 0,
            0,0,0,0,  0,0,  0,0x0000003F,  1,1,  0 },

    // min = 100[us] + 0[UI]         
    { "HS_INIT"      , DSI_C_TIME_HS,  0,
            1000000,0,0,0,  0,0,  0,0xFFFFFFFF,  4,4,  0 },      

    // min = 1[ms] + 0[UI]   
    { "HS_WAKEUP"    , DSI_C_TIME_HS,  0,
            1000000,0,0,0,  0,0,  0,0xFFFFFFFF,  4,4,  0 },      

    // min = 1[ms] + 0[UI]   
    { "LP_WAKEUP"    , DSI_C_TIME_ESC, 0,
            1000000,0,0,0,  0,0,  0,0xFFFFFFFF,  1,1, 0 },      

// A0 did not have PREPARE but only PRE ???     
//    // min = 38[ns] + 0[UI]   max= 95[ns] + 0[UI]   
//    { "HS_CLK_PRE"   , DSI_C_TIME_HS,  DSI_C_HAS_MAX,
//            38,0,0,0,  95,0,  0,0x000003FF,  4,4,  0 },      

    // min = 0[ns] + 8[UI]
    { "HS_CLK_PRE"   , DSI_C_TIME_HS,  0,
            0,8,0,0,      0,0,  0,0x000003FF,  4,4,  0 },      

    // min = 38[ns] + 0[UI]   max= 95[ns] + 0[UI]   
    { "HS_CLK_PREPARE"   , DSI_C_TIME_HS, DSI_C_HAS_MAX,
            38,0,0,0,    95,0,  0,0x000003FF,  4,4,  0 },      


    // min = 262[ns] + 0[UI]        
    { "HS_CLK_ZERO"  , DSI_C_TIME_HS,  0,
            262,0,0,0,  0,0,  0,0x000003FF,  4,4,  0 },      

    // min =  60[ns] + 52[UI]       
    { "HS_CLK_POST"  , DSI_C_TIME_HS,  0,
            60,52,0,0,  0,0,  0,0x000003FF,  4,4,  0 },      

    // min =  60[ns] + 0[UI]        
    { "HS_CLK_TRAIL" , DSI_C_TIME_HS,  0,
            60,0,0,0,  0,0,  0,0x000003FF,  4,4,  0 },      

    // min =  50[ns] + 0[UI]        
    { "HS_LPX"       , DSI_C_TIME_HS,  0,
            50,0,0,0,  0,0,  0,0x000003FF,  4,4,  0 },      

    // min = 40[ns] + 4[UI]      max= 85[ns] + 6[UI]        
    { "HS_PRE"       , DSI_C_TIME_HS,  DSI_C_HAS_MAX,
            40,4,0,0,  85,6,  0,0x000003FF,  4,4,  0 },      

    // min = 105[ns] + 6[UI]        
    { "HS_ZERO"      , DSI_C_TIME_HS,  0,
            105,6,0,0, 0,0,  0,0x000003FF,  4,4, 0 },      
//          205,6,0,0, 0,0,  0,0x000003FF,  4,4, 0 },      

    // min = max(0[ns]+32[UI],60[ns]+16[UI])  n=4     
    { "HS_TRAIL"     , DSI_C_TIME_HS,  DSI_C_MIN_MAX_OF_2,
            0,32,60,16, 0,0, 0,0x000003FF,  4,4,  0 },      

    // min = 100[ns] + 0[UI]         
    { "HS_EXIT"      , DSI_C_TIME_HS,  0,
            100,0,0,0,  0,0,  0,0x000003FF,  4,4,  0 },      

    // min = 50[ns] + 0[UI]       
    { "LPX"          , DSI_C_TIME_ESC, 0,
            50,0,0,0,  0,0, 0,0x000000FF,  1,1,  0 },      

    // min = 4*[Tlpx]  max = 4[Tlpx], set to 4         
    { "LP-TA-GO"     , DSI_C_TIME_ESC, 0,
            200,0,0,0,  0,0, 0,0x000000FF,  1,1,  0 },      

    // min = 1*[Tlpx]  max = 2[Tlpx], set to 2         
    { "LP-TA-SURE"   , DSI_C_TIME_ESC, 0,
            100,0,0,0,  0,0,  0,0x000000FF,  1,1,  0 },      

    // min = 5*[Tlpx]  max = 5[Tlpx], set to 5         
    { "LP-TA-GET"    , DSI_C_TIME_ESC, 0,
            250,0,0,0,  0,0,  0,0x000000FF,  1,1,  0 },      
};


//*****************************************************************************
//
// Function Name: chal_dsi_init
//
// Description:   Initialize DSI Controller  and software interface
//
//*****************************************************************************
CHAL_HANDLE chal_dsi_init ( cUInt32 baseAddr, pCHAL_DSI_INIT dsiInit )
{
    CHAL_DSI_HANDLE pDev = NULL;
         
    chal_dprintf (CDBG_INFO, "chal_dsi_init\n");

    if ( dsiInit->dlCount > DSI_DL_COUNT )
    {
        chal_dprintf (CDBG_ERRO, "ERROR: chal_dsi_init: DataLine Count\n");
        return (CHAL_HANDLE) NULL;
    }
    
    pDev                = (CHAL_DSI_HANDLE) &dsi_dev[0];
    pDev->baseAddr      = baseAddr;
    pDev->dlCount       = dsiInit->dlCount;
    pDev->clkContinuous = dsiInit->clkContinuous;
    
    return (CHAL_HANDLE) pDev;
}


//****************************************************************************
//
//  Function Name:  chal_dsi_phy_state
//                  
//  Description:    PHY State Control
//                  Controls Clock & Data Line States 
//
//  CLK lane cannot be directed to STOP state. It is normally in stop state 
//  if cont clock is not enabled.
//  CLK lane will not transition to ULPS if cont clk is enabled.
//  When ULPS is requested and cont clock was enabled it will be disabled
//  at same time as ULPS is activated, and re-enabled upon ULPS exit
//                  
//****************************************************************************
cVoid  chal_dsi_phy_state ( 
    CHAL_HANDLE             handle, 
    CHAL_DSI_PHY_STATE_t    state 
    )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    cUInt32         regMask = 0;
    cUInt32         regVal  = 0;
    
    regMask =  DSI_REG_FIELD_SET( DSI_PHYC, FORCE_TXSTOP_0, 1 )
             | DSI_REG_FIELD_SET( DSI_PHYC, TXULPSCLK     , 1 )    
             | DSI_REG_FIELD_SET( DSI_PHYC, TXULPSESC_0   , 1 )
             | DSI_REG_FIELD_SET( DSI_PHYC, TX_HSCLK_CONT , 1 );

    switch ( state )
    {
    case PHY_TXSTOP:
        regVal =  DSI_REG_FIELD_SET( DSI_PHYC, FORCE_TXSTOP_0, 1 );
        break;
    case PHY_ULPS:
        regVal =  DSI_REG_FIELD_SET( DSI_PHYC, TXULPSCLK  , 1 )
                | DSI_REG_FIELD_SET( DSI_PHYC, TXULPSESC_0, 1 );
        break;
    case PHY_CORE:
    default:
        if ( pDev->clkContinuous )
            regVal  = DSI_REG_FIELD_SET( DSI_PHYC, TX_HSCLK_CONT , 1 );
        else
            regVal  = 0;
        break;
    }

    DSI_REG_WRITE_MASKED( pDev->baseAddr, DSI_PHYC, regMask, regVal );
}


//****************************************************************************
//
//  Function Name:  chal_dsi_set_afe_off
//                  
//  Description:    Power Down PHY AFE (Analog Front End)
//                  
//****************************************************************************
cVoid  chal_dsi_phy_afe_off ( CHAL_HANDLE handle )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    cUInt32         afeVal  = 0;
    cUInt32         afeMask = 0;


    afeMask =  DSI_PHY_AFEC_PD_CLANE_MASK
             | DSI_PHY_AFEC_PD_DLANE0_MASK
             | DSI_PHY_AFEC_PD_BG_MASK
             | DSI_PHY_AFEC_PD_MASK;

    afeVal =   DSI_REG_FIELD_SET( DSI_PHY_AFEC, PD       , 1 )  // Pwr Down AFE
             | DSI_REG_FIELD_SET( DSI_PHY_AFEC, PD_CLANE , 1 )  // Pwr Down CL
             | DSI_REG_FIELD_SET( DSI_PHY_AFEC, PD_DLANE0, 1 )  // Pwr Down DLx
             | DSI_REG_FIELD_SET( DSI_PHY_AFEC, PD_BG    , 1 ); // Pwr Down BG

    DSI_REG_WRITE_MASKED ( pDev->baseAddr, DSI_PHY_AFEC, afeMask, afeVal );
}
//****************************************************************************
//
//  Function Name:  chal_dsi_phy_afe_on
//                  
//  Description:    Configure & Enable PHY-AFE  (Analog Front End)
//                  Power Up CLK & DLs
//****************************************************************************
cVoid chal_dsi_phy_afe_on ( CHAL_HANDLE handle, pCHAL_DSI_AFE_CFG afeCfg )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    cUInt32         afeVal  = 0;
    cUInt32         afeMask = 0;
    cUInt32         i;
    
#if (CHIP_REVISION >= 20)
    afeMask =  DSI_PHY_AFEC_CTATADJ_MASK
             | DSI_PHY_AFEC_PTATADJ_MASK
             | DSI_PHY_AFEC_FS2X_EN_CLK_MASK
             | DSI_PHY_AFEC_RESET_MASK
             | DSI_PHY_AFEC_PD_CLANE_MASK
             | DSI_PHY_AFEC_PD_DLANE0_MASK
             | DSI_PHY_AFEC_PD_BG_MASK
             | DSI_PHY_AFEC_PD_MASK
             | DSI_PHY_AFEC_IDR_CLANE_MASK
             | DSI_PHY_AFEC_IDR_DLANE0_MASK;
    
    afeVal =   DSI_REG_FIELD_SET( DSI_PHY_AFEC, CTATADJ   , afeCfg->afeCtaAdj )
             | DSI_REG_FIELD_SET( DSI_PHY_AFEC, PTATADJ   , afeCfg->afePtaAdj )
             | DSI_REG_FIELD_SET( DSI_PHY_AFEC, IDR_CLANE , afeCfg->afeClkIdr )
             | DSI_REG_FIELD_SET( DSI_PHY_AFEC, IDR_DLANE0, afeCfg->afeDlIdr  );
#else
    afeMask =  DSI_PHY_AFEC_CTATADJ_MASK
             | DSI_PHY_AFEC_PTATADJ_MASK
             | DSI_PHY_AFEC_FS2X_EN_CLK_MASK
             | DSI_PHY_AFEC_RESET_MASK
             | DSI_PHY_AFEC_PD_CLANE_MASK
             | DSI_PHY_AFEC_PD_DLANE0_MASK
             | DSI_PHY_AFEC_PD_BG_MASK
             | DSI_PHY_AFEC_PD_MASK;
    
    afeVal =   DSI_REG_FIELD_SET( DSI_PHY_AFEC, CTATADJ   , afeCfg->afeCtaAdj )
             | DSI_REG_FIELD_SET( DSI_PHY_AFEC, PTATADJ   , afeCfg->afePtaAdj );
#endif

    if( !afeCfg->afeBandGapOn ) 
        afeVal |= DSI_REG_FIELD_SET( DSI_PHY_AFEC, PD_BG, 1 );
 
    if( afeCfg->afeDs2xClkEna )
        afeVal |= DSI_REG_FIELD_SET( DSI_PHY_AFEC, FS2X_EN_CLK, 1 );

    afeVal |= DSI_PHY_AFEC_RESET_MASK;

    // reset AFE + PWR-UP
    DSI_REG_WRITE_MASKED ( pDev->baseAddr, DSI_PHY_AFEC, afeMask, afeVal );
    for ( i=0; i<100; i++ ) {}
    // remove RESET
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_PHY_AFEC, RESET, 0 );
}

//*****************************************************************************
//
// Function Name:  chalDsiTimingDivAndRoundUp
// 
// Description:    DSI Timing Counters - Divide & RoundUp Utility
//                 Checks for Counter Value Overflow
//*****************************************************************************
static Boolean chalDsiTimingDivAndRoundUp ( 
    DSI_COUNTER *       pDsiC,
    cUInt32             i,         // DSI counter index
    float               dividend, 
    float               divisor 
    )
{
    float   counter_f;
    cUInt32 counter;
    
    counter_f  = dividend / divisor;
    counter    = (UInt32)counter_f;
    
    if( counter_f != (float)counter ) 
        counter++;

    if ( (counter % pDsiC[i].counter_step) != 0 )
        counter += pDsiC[i].counter_step;
    
    counter  = counter & (~(pDsiC[i].counter_step - 1));
    
    counter -= pDsiC[i].counter_offs;
    
    pDsiC[i].counter = counter;
    
    if ( counter > pDsiC[i].counter_max ) 
    {
        chal_dprintf ( CDBG_ERRO, "[cHAL DSI] chalDsiTimingDivAndRoundUp: "
            "%s counter value overflow\n\r",  pDsiC[i].name );
        return ( FALSE );
    } 
    else
    {
        return ( TRUE );
    }
}


//*****************************************************************************
//
// Function Name:  chal_dsi_set_timing
// 
// Description:    Calculate Values Of DSI Timing Counters
//
//                 <in> escClk_MHz      ESC CLK after divider
//                 <in> hsBitRate_Mbps  HS Bit Rate ( eq to DSI PLL/dsiPllDiv ) 
//                 <in> lpBitRate_Mbps  LP Bit Rate, Max 10 Mbps 
//                   
//*****************************************************************************
cBool chal_dsi_set_timing ( 
    CHAL_HANDLE         handle,
    cUInt32             DPHY_SpecRev,
    float               escClk_MHz,              
    float               hsBitRate_Mbps,  
    float               lpBitRate_Mbps  
    )
{
    Boolean res = FALSE;
    
    float           time_min;
    float           time_min1;
    float           time_min2;
    float           time_max;
    float           period;
    float           ui_ns;
    float           escClk_ns;
    cUInt32         i;
    DSI_COUNTER *   pDsiC;
    CHAL_DSI_HANDLE pDev;

	chal_dprintf ( CDBG_INFO, "chal_dsi_set_timing() escClk_MHz:%f hsBitRate_Mbps:%f lpBitRate_Mbps:%f ", escClk_MHz, hsBitRate_Mbps, lpBitRate_Mbps);
    
    pDev        = (CHAL_DSI_HANDLE) handle;
    ui_ns       = 1 / hsBitRate_Mbps  * 1000;
    escClk_ns   = 1 / escClk_MHz * 1000;

    switch ( DPHY_SpecRev )
    {
        case 1:
            pDsiC = &dsi_dphy_0_92[0];
            break;
        default:
            return ( FALSE );
    }

    // LP Symbol Data Rate = esc_clk / esc2lp_ratio
    if( ! chalDsiTimingDivAndRoundUp( pDsiC, DSI_C_ESC2LP_RATIO, 
        (float)escClk_MHz, (float)lpBitRate_Mbps * 2) )
    {
        return ( FALSE );
    }            
       
    for( i=1; i < DSI_C_MAX; i++ )
    {
        // Period_min1 [ns]
        time_min1 =  (float)pDsiC[i].time_min1_ns 
                   + (float)pDsiC[i].time_min1_ui * ui_ns;

        // Period_min2 [ns]
        if( pDsiC[i].mode & DSI_C_MIN_MAX_OF_2 )
            time_min2 = (float)pDsiC[i].time_min2_ns 
                      + (float)pDsiC[i].time_min2_ui * ui_ns;
        else
            time_min2 = 0;
        
        // Period_min [ns] = max(min1, min2)       
        if( time_min1 >= time_min2 )
            time_min = time_min1;
        else
            time_min = time_min2;    

        // Period_max [ns]
        if( pDsiC[i].mode & DSI_C_HAS_MAX )
            time_max = (float)pDsiC[i].time_max_ns 
                     + (float)pDsiC[i].time_max_ui * ui_ns;
        else
            time_max = 0;
        
        // Period_units [ns]
        if ( pDsiC[i].timeBase & DSI_C_TIME_HS )
            period = ui_ns;
        else if ( pDsiC[i].timeBase & DSI_C_TIME_ESC )
            period = escClk_ns;
        else 
            period = 0;
        
        
        pDsiC[i].period = period;
        
        if( period != 0 )
        {
            res = chalDsiTimingDivAndRoundUp ( pDsiC, i, time_min, period );
            if( !res )
                return ( res );
                
            if( pDsiC[i].mode & DSI_C_HAS_MAX )
            {
                if( ((float)pDsiC[i].counter * period ) > time_max )
                {
                    chal_dprintf ( CDBG_ERRO, "[cHAL DSI] chal_dsi_set_timing: "
                        "%s violates MAX D-PHY Spec allowed value\n\r", 
                        pDsiC[i].name );
                    return ( FALSE );
                }
            }
        }              
    }    


    for( i=0; i < DSI_C_MAX; i++ )
    {
        if ( pDsiC[i].timeBase == DSI_C_TIME_ESC2LPDT )
        {
            chal_dprintf ( CDBG_INFO, "[cHAL DSI] chal_dsi_set_timing: "
                "%13s %7d\n\r", pDsiC[i].name, pDsiC[i].counter );
        } 
        else
        {
            chal_dprintf ( CDBG_INFO, "[cHAL DSI] chal_dsi_set_timing: "
                "%13s %7d %10.2f[ns]\n\r", 
                pDsiC[i].name, 
                pDsiC[i].counter, 
                ((float)pDsiC[i].counter + pDsiC[i].counter_offs) 
                * pDsiC[i].period );
        }
    }

    chal_dprintf (CDBG_INFO, "\r\n[cHAL DSI] chal_dsi_set_timing: "
        "HS_DATA_RATE %6.2f[Mbps]\r\n",  hsBitRate_Mbps );
    chal_dprintf (CDBG_INFO, "[cHAL DSI] chal_dsi_set_timing: "
        "LP_DATA_RATE %6.2f[Mbps]\n\r", 
          escClk_MHz 
        / 2 / (   pDsiC [ DSI_C_ESC2LP_RATIO ].counter 
                + pDsiC [ DSI_C_ESC2LP_RATIO ].counter_offs) );
                

    // set ESC 2 LPDT ratio
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_PHYC     , 
        ESC_CLK_LPDT, pDsiC [ DSI_C_ESC2LP_RATIO ].counter );

    // INIT
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_INIT  , 
        HS_INIT   , pDsiC [ DSI_C_HS_INIT ].counter );
    
    // ULPS WakeUp    
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_WUP   , 
        HS_WUP    , pDsiC [ DSI_C_HS_WAKEUP ].counter);
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_LP_WUP   , 
        LP_WUP    , pDsiC [ DSI_C_LP_WAKEUP ].counter );


    // HS CLK
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_CPRE  , 
        HS_CPRE   , pDsiC [ DSI_C_HS_CLK_PRE ].counter );
#if (CHIP_REVISION >= 20)
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_CPREPARE  , 
        HS_CLK_PREP, pDsiC [ DSI_C_HS_CLK_PREPARE ].counter );
#endif        
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_CZERO , 
        HS_CZERO  , pDsiC [ DSI_C_HS_CLK_ZERO ].counter );
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_CPOST , 
        HS_CPOST  , pDsiC [ DSI_C_HS_CLK_POST ].counter );
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_CTRAIL, 
        HS_CTRAIL , pDsiC [ DSI_C_HS_CLK_TRAIL ].counter );

    // HS
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_LPX   , 
        HS_LPX    , pDsiC [ DSI_C_HS_LPX ].counter );
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_PRE   , 
        HS_PRE    , pDsiC [ DSI_C_HS_PRE ].counter );
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_ZERO  , 
        HS_ZERO   , pDsiC [ DSI_C_HS_ZERO ].counter );
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_TRAIL , 
        HS_TRAIL  , pDsiC [ DSI_C_HS_TRAIL ].counter );
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_HS_EXIT  , 
        HS_EXIT   , pDsiC [ DSI_C_HS_EXIT ].counter  );
    
    // ESC 
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_LPX      , 
        LPX       , pDsiC [ DSI_C_LPX ].counter );
        
    // TURN_AROUND     
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_TA_GO    , 
        TA_GO     , pDsiC [ DSI_C_LP_TA_GO ].counter);
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_TA_SURE  , 
        TA_SURE   , pDsiC [ DSI_C_LP_TA_SURE ].counter );
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_TA_GET   , 
        TA_GET    , pDsiC [ DSI_C_LP_TA_GET ].counter );

    return ( TRUE );
}


//****************************************************************************
//
//  Function Name:  chal_dsi_off
//                  
//  Description:    Disable DSI core 
//                  
//****************************************************************************
cVoid  chal_dsi_off ( CHAL_HANDLE handle )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;

    //DSI_PHYC_PHY_CLANE_EN   = 0;
    //DSI_PHYC_PHY_DLANE0_EN  = 0;
    BRCM_WRITE_REG ( pDev->baseAddr, DSI_PHYC, 0 );
    
    // DSI_CTRL_DSI_EN = 0
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_CTRL, DSI_EN, 0 );
}


//****************************************************************************
//
//  Function Name:  chal_dsi_te_mode
//                  
//  Description:    Set TE SYNC mode
//                  
//****************************************************************************
cVoid chal_dsi_te_mode ( CHAL_HANDLE handle, CHAL_DSI_TE_MODE_t teMode )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_CTRL, TE_TRIGC, teMode );
}


#if (CHIP_REVISION >= 20)
//*****************************************************************************
//
// Function Name:  chal_dsi_te_cfg
// 
// Description:    Configure & Enable TE Input
//                 
//*****************************************************************************
cInt32 chal_dsi_te_cfg ( CHAL_HANDLE handle, UInt32 teIn, pCHAL_DSI_TE_CFG teCfg )
{
    
    #define TE_VSWIDTH_MAX   (DSI_TE0_VSWIDTH_TE_VSWIDTH_MASK >> DSI_TE0_VSWIDTH_TE_VSWIDTH_SHIFT)
    #define TE_HSLINE_MAX    (DSI_TE0C_HSLINE_MASK >> DSI_TE0C_HSLINE_SHIFT)
    
    CHAL_DSI_HANDLE pDev                = (CHAL_DSI_HANDLE) handle;
    cUInt32         te_ctrl_reg_val     = 0;
    cUInt32         te_ctrl_reg_mask    = 0;

    te_ctrl_reg_mask |=   DSI_TE0C_MODE_MASK   
                        | DSI_TE0C_HSLINE_MASK 
                        | DSI_TE0C_POL_MASK    
                        | DSI_TE0C_TE_EN_MASK;  

    if( teCfg->sync_pol == CHAL_DSI_TE_ACT_POL_HI )
    {
        te_ctrl_reg_val |= DSI_TE0C_POL_MASK;
    }
    
    if ( teCfg->te_mode == CHAL_DSI_TE_MODE_VSYNC_HSYNC )
    {
        te_ctrl_reg_val |= DSI_TE0C_MODE_MASK; 
    
        if ( teCfg->vsync_width > TE_VSWIDTH_MAX )
        {
            chal_dprintf (CDBG_ERRO, "chal_dsi_te_cfg: "
                "VSYNC Width Value[0x%08X] Overflow, Max[0x%08X]\n", 
                teCfg->vsync_width, TE_VSWIDTH_MAX );
            return ( -1 );
        }    
        if ( teCfg->hsync_line > TE_HSLINE_MAX )
        {
            chal_dprintf (CDBG_ERRO, "chal_dsi_te_cfg: "
                "HSYNC Line Value[0x%08X] Overflow, Max[0x%08X]\n", 
                teCfg->hsync_line, TE_HSLINE_MAX  );
            return ( -1 );
        }
        
        te_ctrl_reg_val |= ( teCfg->hsync_line << DSI_TE0C_HSLINE_SHIFT );
    }    
    
    te_ctrl_reg_val |= DSI_TE0C_TE_EN_MASK;
    
    switch ( teIn )
    {
        case CHAL_DSI_TE_IN_0:
            BRCM_WRITE_REG ( pDev->baseAddr, DSI_TE0_VSWIDTH, teCfg->vsync_width );
            BRCM_WRITE_REG ( pDev->baseAddr, DSI_TE0C, te_ctrl_reg_val );
            break;
        case CHAL_DSI_TE_IN_1:
            BRCM_WRITE_REG ( pDev->baseAddr, DSI_TE1_VSWIDTH, teCfg->vsync_width );
            BRCM_WRITE_REG ( pDev->baseAddr, DSI_TE1C, te_ctrl_reg_val );
            break;
        default:
            chal_dprintf (CDBG_ERRO, "chal_dsi_te_cfg: "
                "Invalid TE Input[%d]\n", teIn );
            return ( -1 );    
    }
    
    return ( 0 );
}
#endif

//****************************************************************************
//
//  Function Name:  chal_dsi_on
//                  
//  Description:    
//                  
//****************************************************************************
cVoid chal_dsi_on ( CHAL_HANDLE handle, pCHAL_DSI_MODE dsiMode )
{
    
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    cUInt32         mask = 0;
    cUInt32         ctrl = 0;

    // DSI_CTRL_CAL_BYTE_EN
    // DSI_CTRL_INV_BYTE_EN
    
    // DSI_CTRL_SOFT_RESET_CFG
    // DSI_CTRL_DSI_EN 
    
    // DSI_CTRL_CLR_LANED_FIFO        
    // DSI_CTRL_CLR_PIX_BYTEC_FIFO    
    // DSI_CTRL_CLR_CMD_PIX_BYTEC_FIFO
    // DSI_CTRL_CLR_PIX_DATA_FIFO     
    // DSI_CTRL_CLR_CMD_DATA_FIFO     
    
    // DSI-CTRL  Configure Modes & enable the core  
    // -------------------------------------------

    mask =   DSI_REG_FIELD_SET( DSI_CTRL, NO_DISP_CRC   , 1 )
           | DSI_REG_FIELD_SET( DSI_CTRL, NO_DISP_ECC   , 1 )
           | DSI_REG_FIELD_SET( DSI_CTRL, RX_LPDT_EOT_EN, 1 )
           | DSI_REG_FIELD_SET( DSI_CTRL, LPDT_EOT_EN   , 1 )
           | DSI_REG_FIELD_SET( DSI_CTRL, HSDT_EOT_EN   , 1 )
//         | DSI_REG_FIELD_SET( DSI_CTRL, HS_CLKC       , 3 )       
           | DSI_REG_FIELD_SET( DSI_CTRL, DSI_EN        , 1 );
         

    if( !dsiMode->enaRxCrc )
        ctrl |= DSI_REG_FIELD_SET( DSI_CTRL, NO_DISP_CRC, 1 );
        
    if( !dsiMode->enaRxEcc )
        ctrl |= DSI_REG_FIELD_SET( DSI_CTRL, NO_DISP_ECC, 1 );
    
    if( dsiMode->enaHsTxEotPkt )
        ctrl |= DSI_REG_FIELD_SET( DSI_CTRL, HSDT_EOT_EN, 1 );
    
    if( dsiMode->enaLpTxEotPkt )
        ctrl |= DSI_REG_FIELD_SET( DSI_CTRL, LPDT_EOT_EN, 1 );
    
    if( dsiMode->enaLpRxEotPkt )
        ctrl |= DSI_REG_FIELD_SET( DSI_CTRL, RX_LPDT_EOT_EN, 1 );

    ctrl |= DSI_REG_FIELD_SET( DSI_CTRL, DSI_EN, 1 );
    
    DSI_REG_WRITE_MASKED ( pDev->baseAddr, DSI_CTRL, mask, ctrl );
       
    // PHY-C  Configure & Enable D-PHY Interface
    // -----------------------------------------
    mask = 0;
    ctrl = 0;

    // DSI_PHYC_TXULPSCLK

    // DSI_PHYC_FORCE_TXSTOP_0
    // DSI_PHYC_FORCE_TXSTOP_1

    // DSI_PHYC_TXULPSESC_0
    // DSI_PHYC_TXULPSESC_1

    mask =   DSI_PHYC_TX_HSCLK_CONT_MASK 
           | DSI_PHYC_PHY_CLANE_EN_MASK  
           | DSI_PHYC_PHY_DLANE0_EN_MASK; 

    if ( dsiMode->enaContClock )
        ctrl |= DSI_REG_FIELD_SET( DSI_PHYC, TX_HSCLK_CONT, 1 );
    
    ctrl |= DSI_REG_FIELD_SET( DSI_PHYC, PHY_CLANE_EN , 1 );
    ctrl |= DSI_REG_FIELD_SET( DSI_PHYC, PHY_DLANE0_EN, 1 );

    DSI_REG_WRITE_MASKED ( pDev->baseAddr, DSI_PHYC, mask, ctrl );
}



//****************************************************************************
//
//  Function Name:  chal_dsi_ena_int
//                  
//  Description:    Enable(b'1b) | Disable(b'0') DSI Interrupts
//                  
//****************************************************************************
cVoid  chal_dsi_ena_int ( CHAL_HANDLE handle, cUInt32 intMask )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;

    BRCM_WRITE_REG ( pDev->baseAddr, DSI_INT_EN, intMask );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_get_int
//                  
//  Description:    Get Pending Interrupts
//                  
//****************************************************************************
cUInt32  chal_dsi_get_int ( CHAL_HANDLE handle )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;

    return ( BRCM_READ_REG ( pDev->baseAddr, DSI_INT_STAT ) );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_clr_int
//                  
//  Description:    Clear Pending Interrupt
//                  
//****************************************************************************
cVoid  chal_dsi_clr_int ( CHAL_HANDLE handle, cUInt32 intMask )
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    
    BRCM_WRITE_REG ( pDev->baseAddr, DSI_INT_STAT, intMask );
}


//****************************************************************************
//
//  Function Name:  chal_dsi_clr_fifo
//                  
//  Description:    Clear Sleleceted DSI FIFOs
//                  
//****************************************************************************
cVoid  chal_dsi_clr_fifo ( CHAL_HANDLE handle, cUInt32  fifoMask )
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    
    DSI_REG_WRITE_MASKED ( pDev->baseAddr, DSI_CTRL, fifoMask, fifoMask );
}


//****************************************************************************
//
//  Function Name:  chal_dsi_get_status
//                  
//  Description:    Int Status is collection of Int event (set until cleared)
//                  and status (real time state) flags
//                  
//****************************************************************************
cUInt32 chal_dsi_get_status ( CHAL_HANDLE handle )
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    
    return ( BRCM_READ_REG ( pDev->baseAddr, DSI_STAT ) );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_clr_status
//                  
//  Description:    Int Status is collection of Int event (set until cleared)
//                  and status (real time state) flags
//                  1 - clears event bits
//****************************************************************************
cVoid chal_dsi_clr_status ( CHAL_HANDLE handle, cUInt32 statMask )
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    
    BRCM_WRITE_REG ( pDev->baseAddr, DSI_STAT, statMask );
}

 

//****************************************************************************
//
//  Function Name:  chal_dsi_send_bta
//                  
//  Description:    Turn Bus Around
//                  
//****************************************************************************
cVoid chal_dsi_send_bta ( 
    CHAL_HANDLE handle, 
    cUInt8      txEng    // NA to ATHENA
    )   
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    cUInt32         pktc   = 0;
    
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_CTRL   , CMND_CTRL_BTA );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_REPEAT , 1 );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_MODE   , 1 );  // LowPower
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_TX_TIME, 
                CHAL_DSI_CMND_WHEN_BEST_EFFORT );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, DISPLAY_NO , 
                DSI_PKT_SRC_CMND_FIFO );     
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_EN     , 1 );

    BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_PKTC, pktc );
}


//****************************************************************************
//
//  Function Name:  chal_dsi_send_trig
//                  
//  Description:    Send TRIGGER Message
//                  
//****************************************************************************
cVoid chal_dsi_send_trig ( 
    CHAL_HANDLE handle, 
    cUInt8      txEng,   // NA to ATHENA
    cUInt8      trig )   
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    cUInt32         pktc   = 0;
    
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_CTRL   , CMND_CTRL_TRIG );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_REPEAT , 1 );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_MODE   , 1 );  // LowPower
    
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_TX_TIME, 
                CHAL_DSI_CMND_WHEN_BEST_EFFORT );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, DISPLAY_NO , 
                DSI_PKT_SRC_CMND_FIFO );     
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_EN     , 1 );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, TRIG_CMD   , trig );
    
    BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_PKTC, pktc );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_send_cmnd
//                  
//  Description:    Send   SHORT CMND - up to 2 Byte Parms - LSB first on wire
//                       | LONG  CMND - up to 8 Byte Parms - LSB first on wire
//                  
//****************************************************************************
CHAL_DSI_RES_t chal_dsi_send_cmnd ( 
    CHAL_HANDLE     handle, 
    cUInt8          txEng,   // NA to ATHENA
    pCHAL_DSI_CMND  cmnd )   
{
    CHAL_DSI_HANDLE pDev   = (CHAL_DSI_HANDLE) handle;
    cUInt32      i;
    cUInt32      pktc   = 0;
    cUInt32      pkth   = 0;
    cUInt32      dsi_DT = 0;

    if( !cmnd->isLong && cmnd->msgLen > 2 )
        return ( CHAL_DSI_MSG_SIZE );
        
    if( cmnd->isLong && cmnd->msgLen > 8 )
        return ( CHAL_DSI_MSG_SIZE );

    if( cmnd->isLP   ) 
      pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_MODE , 1 );
    //pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_MODE , 0 );
	
    if( cmnd->isLong ) 
        pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_TYPE , 1 );
    
    if( cmnd->endWithBta ) 
        pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_CTRL, 
                    CMND_CTRL_CMND_PKTwBTA );
    else    
        pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_CTRL, 
                    CMND_CTRL_CMND_PKT );
    
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_REPEAT , 1 );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_TX_TIME, cmnd->vmWhen );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, DISPLAY_NO , DSI_PKT_SRC_CMND_FIFO );     
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_EN , 1 );

    dsi_DT = ((cmnd->vc & 0x00000003) << 6) | cmnd->dsiCmnd;

    if ( cmnd->isLong ) 
    {
        // for long, parm byteCount && byteCount from CMND DATA FIFO are same, 
        pkth =   DSI_REG_FIELD_SET ( DSI_CMD_PKTH, WC_FIFO,  cmnd->msgLen )
               | DSI_REG_FIELD_SET ( DSI_CMD_PKTH, WC_PARAM, cmnd->msgLen );
    } 
    else
    {
        // SHORT MSG IS FIXED IN SIZE (always 2  parms)
        pkth  = cmnd->msg[0];
//      if( cmnd->msgLen == 2 )
        pkth |= (cmnd->msg[1] << 8); 

        pkth = DSI_REG_FIELD_SET ( DSI_CMD_PKTH, WC_PARAM, pkth );
    }
  
    pkth |= dsi_DT;

    BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_PKTH, pkth );
    
    // if LONG PKT, fill CMND DATA FIFO 
    if( cmnd->isLong ) 
    {
        for( i=0; i < cmnd->msgLen; i++ )
        {
            BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_DATA_FIFO, cmnd->msg[i] );
        }   
    }   

    BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_PKTC, pktc );

    return ( CHAL_DSI_OK );
}


//****************************************************************************
//
//  Function Name:  chal_dsi_read_reply
//                  
//  Description:    
//                  
//****************************************************************************

CHAL_DSI_RES_t chal_dsi_read_reply ( 
    CHAL_HANDLE         handle, 
    cUInt32             intStat, // core pending interrupts or interrupt stat
    pCHAL_DSI_REPLY     reply 
    )
{
    #define	DSI_DT_P2H_SH_ACK_ERR_RPT	(0x02)    
    #define	DSI_DT_P2H_SH_GEN_RD_1B	    (0x11)    
    #define	DSI_DT_P2H_DCS_SH_RD_1B	    (0x21)    

    CHAL_DSI_HANDLE pDev     = (CHAL_DSI_HANDLE) handle;
    cUInt32         rxStat;
    cUInt32         pkth;
    cUInt32         size;
    cUInt32         i;
    cUInt32         replyDT; 
    
    reply->type = 0;
    
    if (    (intStat & DSI_STAT_RX1_PKT_MASK) 
         || (intStat & DSI_STAT_PHY_RXTRIG_MASK) )
    {
        pkth = BRCM_READ_REG ( pDev->baseAddr, DSI_RX1_PKTH );
    
        if( intStat & DSI_STAT_PHY_RXTRIG_MASK )
        {
            reply->type    = RX_TYPE_TRIG;
            reply->trigger = pkth & DSI_RX1_PKTH_DT_LP_CMD_MASK;
        } 
        else
        {
            rxStat  = 0;
            replyDT = pkth & DSI_RX1_PKTH_DT_LP_CMD_MASK;
            
            // either DATA REPLY or ERROR STAT
            if( (replyDT & 0x3F) == DSI_DT_P2H_SH_ACK_ERR_RPT )
            {
                // ERROR STAT
                reply->type  |= RX_TYPE_ERR_REPLY;
                reply->errReportDt = replyDT;
        
                if( pkth & DSI_RX1_PKTH_DET_ERR_MASK    ) 
                    rxStat |= ERR_RX_MULTI_BIT;     
                if( pkth & DSI_RX1_PKTH_ECC_ERR_MASK    ) 
                    rxStat |= ERR_RX_ECC;           
                if( pkth & DSI_RX1_PKTH_COR_ERR_MASK    ) 
                    rxStat |= ERR_RX_CORRECTABLE;   
                if( pkth & DSI_RX1_PKTH_INCOMP_PKT_MASK ) 
                    rxStat |= ERR_RX_PKT_INCOMPLETE;
        
                reply->errReportRxStat = rxStat;
 
                if( (rxStat == 0) || (rxStat == ERR_RX_CORRECTABLE) )
                    reply->errReportRxStat |= ERR_RX_OK;
                                    
                
                // ALWAYS SHORT - NO CRC
                reply->errReport = (pkth & DSI_RX1_PKTH_WC_PARAM_MASK) >> 
                    DSI_RX1_PKTH_WC_PARAM_SHIFT;
            }
            else
            {
                // DATA REPLY
                reply->type    = RX_TYPE_READ_REPLY;
                //reply->readReplyDt = pkth & DSI_RX1_PKTH_DT_LP_CMD_MASK;
                reply->readReplyDt = replyDT;
                
                if( pkth & DSI_RX1_PKTH_CRC_ERR_MASK    ) 
                    rxStat |= ERR_RX_CRC;           
                if( pkth & DSI_RX1_PKTH_DET_ERR_MASK    ) 
                    rxStat |= ERR_RX_MULTI_BIT;     
                if( pkth & DSI_RX1_PKTH_ECC_ERR_MASK    ) 
                    rxStat |= ERR_RX_ECC;           
                if( pkth & DSI_RX1_PKTH_COR_ERR_MASK    ) 
                    rxStat |= ERR_RX_CORRECTABLE;   
                if( pkth & DSI_RX1_PKTH_INCOMP_PKT_MASK ) 
                    rxStat |= ERR_RX_PKT_INCOMPLETE;

                reply->readReplyRxStat = rxStat;
                if( (rxStat == 0) || (rxStat == ERR_RX_CORRECTABLE))
                    reply->readReplyRxStat |= ERR_RX_OK;
            
                if ( pkth & DSI_RX1_PKTH_PKT_TYPE_MASK )
                { 
                    // LONG, MAX 8 bytes of parms
                    size = (pkth & DSI_RX1_PKTH_WC_PARAM_MASK) >> 8;
                    for ( i=0; i < size; i++ )
                    {   
                        reply->pReadReply[i] =      
                            BRCM_READ_REG ( pDev->baseAddr, DSI_CMD_DATA_FIFO );
                    } 
                    reply->readReplySize = size; 
                }
                else
                { 
                    // SHORT  PKT - MAX 2 BYTEs, NO CRC
                    reply->pReadReply[0] = 
                        (pkth & DSI_RX1_PKTH_WC_PARAM_MASK) >> 8;
                    reply->pReadReply[1] = 
                        (pkth & DSI_RX1_PKTH_WC_PARAM_MASK) >> 16;
                    if(  ((reply->readReplyDt & 0x3F)==DSI_DT_P2H_SH_GEN_RD_1B)
                       ||((reply->readReplyDt & 0x3F)==DSI_DT_P2H_DCS_SH_RD_1B))
                    {
                        reply->readReplySize = 1;
                    }    
                    else
                    {
                        reply->readReplySize = 2;
                    }    
                }
            }        
        }
    }

    if ( intStat & DSI_STAT_RX2_PKT_MASK )
    {
        // we can only get ERROR STAT here
        reply->type  |= RX_TYPE_ERR_REPLY;
    
        pkth = BRCM_READ_REG ( pDev->baseAddr, DSI_RX2_SHORT_PKT );
        
        rxStat = 0;
        
        if( pkth & DSI_RX2_SHORT_PKT_DET_ERR_MASK    ) 
            rxStat |= ERR_RX_MULTI_BIT;     
        if( pkth & DSI_RX2_SHORT_PKT_ECC_ERR_MASK    ) 
            rxStat |= ERR_RX_ECC;           
        if( pkth & DSI_RX2_SHORT_PKT_COR_ERR_MASK    ) 
            rxStat |= ERR_RX_CORRECTABLE;   
        if( pkth & DSI_RX2_SHORT_PKT_INCOMP_PKT_MASK ) 
            rxStat |= ERR_RX_PKT_INCOMPLETE;
        
        reply->errReportRxStat = rxStat;
 
        if( (rxStat == 0) || (rxStat == ERR_RX_CORRECTABLE) )
            reply->errReportRxStat |= ERR_RX_OK;
                                    
        reply->errReportDt = pkth & DSI_RX2_SHORT_PKT_DT_LP_CMD_MASK;
        // ALWAYS SHORT - NO CRC
        reply->errReport = (pkth & DSI_RX2_SHORT_PKT_WC_PARAM_MASK) >> 
            DSI_RX2_SHORT_PKT_WC_PARAM_SHIFT;
    }
    
    if ( reply->type == 0 )
        return (CHAL_DSI_RX_NO_PKT);
    else
        return (CHAL_DSI_OK);
}    

//****************************************************************************
//
//  Function Name:  chal_dsi_de1_get_dma_address
//                  
//  Description:    this is DE1 related
//                  
//****************************************************************************
UInt32 chal_dsi_de1_get_dma_address ( CHAL_HANDLE handle )
{
    CHAL_DSI_HANDLE dsiH = (CHAL_DSI_HANDLE) handle;
    
    return ( BRCM_REGADDR ( dsiH->baseAddr, DSI_PIX_FIFO0 ));
}

//****************************************************************************
//
//  Function Name:  chal_dsi_de1_set_dma_thresh
//                  
//  Description:    this is DE1 related
//                  
//****************************************************************************
cVoid chal_dsi_de1_set_dma_thresh ( CHAL_HANDLE handle, cUInt32 thresh )
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;

    BRCM_WRITE_REG ( pDev->baseAddr, DSI_DMA_THRE, thresh );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_cmnd_start
//                  
//  Description:    ENABLE | DISABLE  Command Interface
//                  
//****************************************************************************
cVoid chal_dsi_cmnd_start ( 
    CHAL_HANDLE handle, 
    cUInt8      txEng,   // NA to ATHENA
    cBool       start )   
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    
    if ( start )
        BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_CMD_PKTC, CMD_EN, 1 );
    else    
        BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_CMD_PKTC, CMD_EN, 0 );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_de1_set_wc
//                  
//  Description:    Display Engine 1
//                  Sets number of DMA cycles per DSI packet 
//                  
//****************************************************************************
cVoid chal_dsi_de1_set_wc ( CHAL_HANDLE handle, cUInt32 wc )   
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_WC_PIXEL_CNT, WC_PIXEL_CNT, wc );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_de1_set_cm
//                  
//  Description:    
//                  
//****************************************************************************
cVoid chal_dsi_de1_set_cm ( CHAL_HANDLE handle, CHAL_DSI_DE1_COL_MOD_t cm )
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    
    BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_DISP1_CTRL, PFORMAT, cm );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_de1_enable
//                  
//  Description:    Ena | Dis Color Engine 1
//                  
//****************************************************************************
cVoid chal_dsi_de1_enable ( CHAL_HANDLE handle, cBool ena )
{
    CHAL_DSI_HANDLE pDev = (CHAL_DSI_HANDLE) handle;
    
    if ( ena )
        BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_DISP1_CTRL, EN, 1 );
    else    
        BRCM_WRITE_REG_FIELD ( pDev->baseAddr, DSI_DISP1_CTRL, EN, 0 );
}

//****************************************************************************
//
//  Function Name:  chal_dsi_de1_send
//                  
//  Description:    Display Engine 1 Send
//                  Sends Packet(s) using Display Engine 1     
//                  (pixels from PIXEL FIFO)
//****************************************************************************
CHAL_DSI_RES_t chal_dsi_de1_send ( 
    CHAL_HANDLE      handle, 
    cUInt8           txEng,   // NA to ATHENA
    pCHAL_DSI_CM_CFG cmCfg 
    )   
{
    CHAL_DSI_HANDLE pDev         = (CHAL_DSI_HANDLE) handle;
    cUInt32         pktc         = 0;
    cUInt32         pkth         = 0;
    cUInt32         dsi_DT       = 0;
    cUInt32         dcsCmndCount = 0;

    if ( cmCfg->dcsCmnd != NULL )
        dcsCmndCount = 1;    
    
    //================================   
    // PKTH    
    //================================   
    dsi_DT = ((cmCfg->vc & 0x00000003) << 6) | cmCfg->dsiCmnd;
    pkth  = dsi_DT;    
    pkth |=  DSI_REG_FIELD_SET( DSI_CMD_PKTH, WC_PARAM, 
                cmCfg->pktSizeBytes+dcsCmndCount )
           | DSI_REG_FIELD_SET( DSI_CMD_PKTH, WC_FIFO , dcsCmndCount );



    //================================   
    // PKTC 
    //================================   
    if( cmCfg->isLP ) 
        pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_MODE  , 1 );

    if( cmCfg->isTE ) 
        pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_TE_EN , 1 );

    // always long
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_TYPE , 1 );

    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_CTRL   , CMND_CTRL_CMND_PKT);
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_REPEAT , cmCfg->pktCount );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_TX_TIME, cmCfg->vmWhen );
    pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, DISPLAY_NO , DSI_PKT_SRC_DE1 );
     
    if( cmCfg->start ) 
    {
        pktc |= DSI_REG_FIELD_SET( DSI_CMD_PKTC, CMD_EN , 1 );
    }
     
    //================================   
    // DCS Cmnd to CMND FIFO
    //================================   
    if ( dcsCmndCount == 1 )
    {
        BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_DATA_FIFO, cmCfg->dcsCmnd );
    }
    
    //================================   
    // Execute 
    //================================   
    BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_PKTH, pkth );
    BRCM_WRITE_REG ( pDev->baseAddr, DSI_CMD_PKTC, pktc );
    return ( CHAL_DSI_OK );
}

            
    
