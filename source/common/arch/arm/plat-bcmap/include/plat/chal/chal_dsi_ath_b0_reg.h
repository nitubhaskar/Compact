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
/************************************************************************************************/

#ifndef __BRCM_RDB_DSI_H__
#define __BRCM_RDB_DSI_H__

/*
   OCT 04 2010
   B0 RDB is currently not usable
   The file is temp fix until we have correct RDB in place checked in by BSP team
   Known B0 RDB Issues:
     DSI_CTRL       : missing TE_TRIG   & HS_CLKC
     DSI_CMD_PKTC   : missing CMD_TE_EN                 
     HS TIMIG       : registers have wrong shift (2 instead 0)                 
     DSI_WC_PIXEL   : missing completly               
*/

#define DSI_CTRL_OFFSET                                                   0x00000000
#define DSI_CTRL_TYPE                                                     UInt32    
#define DSI_CTRL_RESERVED_MASK                                            0xFFFC0000
#define    DSI_CTRL_TE_TRIGC_SHIFT                                        16        
#define    DSI_CTRL_TE_TRIGC_MASK                                         0x00030000
#define    DSI_CTRL_HS_CLKC_SHIFT                                         14        
#define    DSI_CTRL_HS_CLKC_MASK                                          0x0000C000
#define    DSI_CTRL_RX_LPDT_EOT_EN_SHIFT                                  13        
#define    DSI_CTRL_RX_LPDT_EOT_EN_MASK                                   0x00002000
#define    DSI_CTRL_LPDT_EOT_EN_SHIFT                                     12        
#define    DSI_CTRL_LPDT_EOT_EN_MASK                                      0x00001000
#define    DSI_CTRL_HSDT_EOT_EN_SHIFT                                     11        
#define    DSI_CTRL_HSDT_EOT_EN_MASK                                      0x00000800
#define    DSI_CTRL_SOFT_RESET_CFG_SHIFT                                  10        
#define    DSI_CTRL_SOFT_RESET_CFG_MASK                                   0x00000400
#define    DSI_CTRL_CAL_BYTE_EN_SHIFT                                     9         
#define    DSI_CTRL_CAL_BYTE_EN_MASK                                      0x00000200
#define    DSI_CTRL_INV_BYTE_EN_SHIFT                                     8         
#define    DSI_CTRL_INV_BYTE_EN_MASK                                      0x00000100
#define    DSI_CTRL_CLR_LANED_FIFO_SHIFT                                  7         
#define    DSI_CTRL_CLR_LANED_FIFO_MASK                                   0x00000080
#define    DSI_CTRL_CLR_PIX_BYTEC_FIFO_SHIFT                              6         
#define    DSI_CTRL_CLR_PIX_BYTEC_FIFO_MASK                               0x00000040
#define    DSI_CTRL_CLR_CMD_PIX_BYTEC_FIFO_SHIFT                          5         
#define    DSI_CTRL_CLR_CMD_PIX_BYTEC_FIFO_MASK                           0x00000020
#define    DSI_CTRL_CLR_PIX_DATA_FIFO_SHIFT                               4         
#define    DSI_CTRL_CLR_PIX_DATA_FIFO_MASK                                0x00000010
#define    DSI_CTRL_CLR_CMD_DATA_FIFO_SHIFT                               3         
#define    DSI_CTRL_CLR_CMD_DATA_FIFO_MASK                                0x00000008
#define    DSI_CTRL_NO_DISP_CRC_SHIFT                                     2         
#define    DSI_CTRL_NO_DISP_CRC_MASK                                      0x00000004
#define    DSI_CTRL_NO_DISP_ECC_SHIFT                                     1         
#define    DSI_CTRL_NO_DISP_ECC_MASK                                      0x00000002
#define    DSI_CTRL_DSI_EN_SHIFT                                          0         
#define    DSI_CTRL_DSI_EN_MASK                                           0x00000001

#define DSI_CMD_PKTC_OFFSET                                               0x00000004
#define DSI_CMD_PKTC_TYPE                                                 UInt32
#define DSI_CMD_PKTC_RESERVED_MASK                                        0x00000000
#define    DSI_CMD_PKTC_TRIG_CMD_SHIFT                                    24
#define    DSI_CMD_PKTC_TRIG_CMD_MASK                                     0xFF000000
#define    DSI_CMD_PKTC_CMD_REPEAT_SHIFT                                  10
#define    DSI_CMD_PKTC_CMD_REPEAT_MASK                                   0x00FFFC00
#define    DSI_CMD_PKTC_DISPLAY_NO_SHIFT                                  8
#define    DSI_CMD_PKTC_DISPLAY_NO_MASK                                   0x00000300
#define    DSI_CMD_PKTC_CMD_TX_TIME_SHIFT                                 6
#define    DSI_CMD_PKTC_CMD_TX_TIME_MASK                                  0x000000C0
#define    DSI_CMD_PKTC_CMD_CTRL_SHIFT                                    4
#define    DSI_CMD_PKTC_CMD_CTRL_MASK                                     0x00000030
#define    DSI_CMD_PKTC_CMD_MODE_SHIFT                                    3
#define    DSI_CMD_PKTC_CMD_MODE_MASK                                     0x00000008
#define    DSI_CMD_PKTC_CMD_TYPE_SHIFT                                    2
#define    DSI_CMD_PKTC_CMD_TYPE_MASK                                     0x00000004
#define    DSI_CMD_PKTC_CMD_TE_EN_SHIFT                                   1
#define    DSI_CMD_PKTC_CMD_TE_EN_MASK                                    0x00000002
#define    DSI_CMD_PKTC_CMD_EN_SHIFT                                      0
#define    DSI_CMD_PKTC_CMD_EN_MASK                                       0x00000001

#define DSI_CMD_PKTH_OFFSET                                               0x00000008
#define DSI_CMD_PKTH_TYPE                                                 UInt32
#define DSI_CMD_PKTH_RESERVED_MASK                                        0x00000000
#define    DSI_CMD_PKTH_WC_FIFO_SHIFT                                     24
#define    DSI_CMD_PKTH_WC_FIFO_MASK                                      0xFF000000
#define    DSI_CMD_PKTH_WC_PARAM_SHIFT                                    8
#define    DSI_CMD_PKTH_WC_PARAM_MASK                                     0x00FFFF00
#define    DSI_CMD_PKTH_DT_SHIFT                                          0
#define    DSI_CMD_PKTH_DT_MASK                                           0x000000FF

#define DSI_RX1_PKTH_OFFSET                                               0x0000000C
#define DSI_RX1_PKTH_TYPE                                                 UInt32
#define DSI_RX1_PKTH_RESERVED_MASK                                        0x0C000000
#define    DSI_RX1_PKTH_CRC_ERR_SHIFT                                     31
#define    DSI_RX1_PKTH_CRC_ERR_MASK                                      0x80000000
#define    DSI_RX1_PKTH_DET_ERR_SHIFT                                     30
#define    DSI_RX1_PKTH_DET_ERR_MASK                                      0x40000000
#define    DSI_RX1_PKTH_ECC_ERR_SHIFT                                     29
#define    DSI_RX1_PKTH_ECC_ERR_MASK                                      0x20000000
#define    DSI_RX1_PKTH_COR_ERR_SHIFT                                     28
#define    DSI_RX1_PKTH_COR_ERR_MASK                                      0x10000000
#define    DSI_RX1_PKTH_INCOMP_PKT_SHIFT                                  25
#define    DSI_RX1_PKTH_INCOMP_PKT_MASK                                   0x02000000
#define    DSI_RX1_PKTH_PKT_TYPE_SHIFT                                    24
#define    DSI_RX1_PKTH_PKT_TYPE_MASK                                     0x01000000
#define    DSI_RX1_PKTH_WC_PARAM_SHIFT                                    8
#define    DSI_RX1_PKTH_WC_PARAM_MASK                                     0x00FFFF00
#define    DSI_RX1_PKTH_DT_LP_CMD_SHIFT                                   0
#define    DSI_RX1_PKTH_DT_LP_CMD_MASK                                    0x000000FF

#define DSI_RX2_SHORT_PKT_OFFSET                                          0x00000010
#define DSI_RX2_SHORT_PKT_TYPE                                            UInt32
#define DSI_RX2_SHORT_PKT_RESERVED_MASK                                   0x0C000000
#define    DSI_RX2_SHORT_PKT_CRC_ERR_SHIFT                                31
#define    DSI_RX2_SHORT_PKT_CRC_ERR_MASK                                 0x80000000
#define    DSI_RX2_SHORT_PKT_DET_ERR_SHIFT                                30
#define    DSI_RX2_SHORT_PKT_DET_ERR_MASK                                 0x40000000
#define    DSI_RX2_SHORT_PKT_ECC_ERR_SHIFT                                29
#define    DSI_RX2_SHORT_PKT_ECC_ERR_MASK                                 0x20000000
#define    DSI_RX2_SHORT_PKT_COR_ERR_SHIFT                                28
#define    DSI_RX2_SHORT_PKT_COR_ERR_MASK                                 0x10000000
#define    DSI_RX2_SHORT_PKT_INCOMP_PKT_SHIFT                             25
#define    DSI_RX2_SHORT_PKT_INCOMP_PKT_MASK                              0x02000000
#define    DSI_RX2_SHORT_PKT_PKT_TYPE_SHIFT                               24
#define    DSI_RX2_SHORT_PKT_PKT_TYPE_MASK                                0x01000000
#define    DSI_RX2_SHORT_PKT_WC_PARAM_SHIFT                               8
#define    DSI_RX2_SHORT_PKT_WC_PARAM_MASK                                0x00FFFF00
#define    DSI_RX2_SHORT_PKT_DT_LP_CMD_SHIFT                              0
#define    DSI_RX2_SHORT_PKT_DT_LP_CMD_MASK                               0x000000FF

#define DSI_CMD_DATA_FIFO_OFFSET                                          0x00000014
#define DSI_CMD_DATA_FIFO_TYPE                                            UInt32
#define DSI_CMD_DATA_FIFO_RESERVED_MASK                                   0xFFFFFF00
#define    DSI_CMD_DATA_FIFO_CMD_DATA_SHIFT                               0
#define    DSI_CMD_DATA_FIFO_CMD_DATA_MASK                                0x000000FF

#define DSI_DISP0_CTRL_OFFSET                                             0x00000018
#define DSI_DISP0_CTRL_TYPE                                               UInt32
#define DSI_DISP0_CTRL_RESERVED_MASK                                      0x80000000
#define    DSI_DISP0_CTRL_ST_END_SHIFT                                    30
#define    DSI_DISP0_CTRL_ST_END_MASK                                     0x40000000
#define    DSI_DISP0_CTRL_FRAMEC_SHIFT                                    29
#define    DSI_DISP0_CTRL_FRAMEC_MASK                                     0x20000000
#define    DSI_DISP0_CTRL_PIX_CLK_DIV_SHIFT                               13
#define    DSI_DISP0_CTRL_PIX_CLK_DIV_MASK                                0x1FFFE000
#define    DSI_DISP0_CTRL_LP_STOP_CTRL_SHIFT                              11
#define    DSI_DISP0_CTRL_LP_STOP_CTRL_MASK                               0x00001800
#define    DSI_DISP0_CTRL_HACTIVE_NULL_SHIFT                              10
#define    DSI_DISP0_CTRL_HACTIVE_NULL_MASK                               0x00000400
#define    DSI_DISP0_CTRL_VBLP_CTRL_SHIFT                                 9
#define    DSI_DISP0_CTRL_VBLP_CTRL_MASK                                  0x00000200
#define    DSI_DISP0_CTRL_HBLP_CTRL_SHIFT                                 8
#define    DSI_DISP0_CTRL_HBLP_CTRL_MASK                                  0x00000100
#define    DSI_DISP0_CTRL_VC_SHIFT                                        6
#define    DSI_DISP0_CTRL_VC_MASK                                         0x000000C0
#define    DSI_DISP0_CTRL_PFORMAT_SHIFT                                   2
#define    DSI_DISP0_CTRL_PFORMAT_MASK                                    0x0000003C
#define    DSI_DISP0_CTRL_MODE_SHIFT                                      1
#define    DSI_DISP0_CTRL_MODE_MASK                                       0x00000002
#define    DSI_DISP0_CTRL_EN_SHIFT                                        0
#define    DSI_DISP0_CTRL_EN_MASK                                         0x00000001

#define DSI_DISP1_CTRL_OFFSET                                             0x0000001C
#define DSI_DISP1_CTRL_TYPE                                               UInt32
#define DSI_DISP1_CTRL_RESERVED_MASK                                      0xFFFFFFF8
#define    DSI_DISP1_CTRL_PFORMAT_SHIFT                                   1
#define    DSI_DISP1_CTRL_PFORMAT_MASK                                    0x00000006
#define    DSI_DISP1_CTRL_EN_SHIFT                                        0
#define    DSI_DISP1_CTRL_EN_MASK                                         0x00000001

#define DSI_DMA_THRE_OFFSET                                               0x00000024
#define DSI_DMA_THRE_TYPE                                                 UInt32
#define DSI_DMA_THRE_RESERVED_MASK                                        0xFFFFFE00
#define    DSI_DMA_THRE_DMA_THRESH_SHIFT                                  0
#define    DSI_DMA_THRE_DMA_THRESH_MASK                                   0x000001FF

#define DSI_INT_STAT_OFFSET                                               0x00000028
#define DSI_INT_STAT_TYPE                                                 UInt32
#define DSI_INT_STAT_RESERVED_MASK                                        0xFE000000
#define    DSI_INT_STAT_FIFO_ERR_SHIFT                                    24
#define    DSI_INT_STAT_FIFO_ERR_MASK                                     0x01000000
#define    DSI_INT_STAT_CMD_CTRL_DONE_SHIFT                               23
#define    DSI_INT_STAT_CMD_CTRL_DONE_MASK                                0x00800000
#define    DSI_INT_STAT_PHY_DIR_REV_TOFWD_SHIFT                           22
#define    DSI_INT_STAT_PHY_DIR_REV_TOFWD_MASK                            0x00400000
#define    DSI_INT_STAT_PHY_D1_ULPS_SHIFT                                 21
#define    DSI_INT_STAT_PHY_D1_ULPS_MASK                                  0x00200000
#define    DSI_INT_STAT_PHY_D1_STOP_SHIFT                                 20
#define    DSI_INT_STAT_PHY_D1_STOP_MASK                                  0x00100000
#define    DSI_INT_STAT_PHY_RXLPDT_SHIFT                                  19
#define    DSI_INT_STAT_PHY_RXLPDT_MASK                                   0x00080000
#define    DSI_INT_STAT_PHY_RXTRIG_SHIFT                                  18
#define    DSI_INT_STAT_PHY_RXTRIG_MASK                                   0x00040000
#define    DSI_INT_STAT_PHY_D0_ULPS_SHIFT                                 17
#define    DSI_INT_STAT_PHY_D0_ULPS_MASK                                  0x00020000
#define    DSI_INT_STAT_PHY_D0_LPDT_SHIFT                                 16
#define    DSI_INT_STAT_PHY_D0_LPDT_MASK                                  0x00010000
#define    DSI_INT_STAT_PHY_DIR_FWD_TOREV_SHIFT                           15
#define    DSI_INT_STAT_PHY_DIR_FWD_TOREV_MASK                            0x00008000
#define    DSI_INT_STAT_PHY_D0_STOP_SHIFT                                 14
#define    DSI_INT_STAT_PHY_D0_STOP_MASK                                  0x00004000
#define    DSI_INT_STAT_PHY_CLK_ULPS_SHIFT                                13
#define    DSI_INT_STAT_PHY_CLK_ULPS_MASK                                 0x00002000
#define    DSI_INT_STAT_PHY_CLK_HS_SHIFT                                  12
#define    DSI_INT_STAT_PHY_CLK_HS_MASK                                   0x00001000
#define    DSI_INT_STAT_PHY_CLK_STOP_SHIFT                                11
#define    DSI_INT_STAT_PHY_CLK_STOP_MASK                                 0x00000800
#define    DSI_INT_STAT_PR_TO_SHIFT                                       10
#define    DSI_INT_STAT_PR_TO_MASK                                        0x00000400
#define    DSI_INT_STAT_TA_TO_SHIFT                                       9
#define    DSI_INT_STAT_TA_TO_MASK                                        0x00000200
#define    DSI_INT_STAT_LPRX_TO_SHIFT                                     8
#define    DSI_INT_STAT_LPRX_TO_MASK                                      0x00000100
#define    DSI_INT_STAT_HSTX_TO_SHIFT                                     7
#define    DSI_INT_STAT_HSTX_TO_MASK                                      0x00000080
#define    DSI_INT_STAT_ERRCONTLP1_SHIFT                                  6
#define    DSI_INT_STAT_ERRCONTLP1_MASK                                   0x00000040
#define    DSI_INT_STAT_ERRCONTLP0_SHIFT                                  5
#define    DSI_INT_STAT_ERRCONTLP0_MASK                                   0x00000020
#define    DSI_INT_STAT_ERRCONTROL_SHIFT                                  4
#define    DSI_INT_STAT_ERRCONTROL_MASK                                   0x00000010
#define    DSI_INT_STAT_ERRSYNCESC_SHIFT                                  3
#define    DSI_INT_STAT_ERRSYNCESC_MASK                                   0x00000008
#define    DSI_INT_STAT_RX2_PKT_SHIFT                                     2
#define    DSI_INT_STAT_RX2_PKT_MASK                                      0x00000004
#define    DSI_INT_STAT_RX1_PKT_SHIFT                                     1
#define    DSI_INT_STAT_RX1_PKT_MASK                                      0x00000002
#define    DSI_INT_STAT_CMD_PKT_SHIFT                                     0
#define    DSI_INT_STAT_CMD_PKT_MASK                                      0x00000001

#define DSI_INT_EN_OFFSET                                                 0x0000002C
#define DSI_INT_EN_TYPE                                                   UInt32
#define DSI_INT_EN_RESERVED_MASK                                          0xFE000000
#define    DSI_INT_EN_FIFO_ERR_SHIFT                                      24
#define    DSI_INT_EN_FIFO_ERR_MASK                                       0x01000000
#define    DSI_INT_EN_CMD_CTRL_DONE_SHIFT                                 23
#define    DSI_INT_EN_CMD_CTRL_DONE_MASK                                  0x00800000
#define    DSI_INT_EN_PHY_DIR_REV_TO_FWD_SHIFT                            22
#define    DSI_INT_EN_PHY_DIR_REV_TO_FWD_MASK                             0x00400000
#define    DSI_INT_EN_PHY_D1_ULPS_SHIFT                                   21
#define    DSI_INT_EN_PHY_D1_ULPS_MASK                                    0x00200000
#define    DSI_INT_EN_PHY_D1_STOP_SHIFT                                   20
#define    DSI_INT_EN_PHY_D1_STOP_MASK                                    0x00100000
#define    DSI_INT_EN_PHY_RXLPDT_SHIFT                                    19
#define    DSI_INT_EN_PHY_RXLPDT_MASK                                     0x00080000
#define    DSI_INT_EN_PHY_RXTRIG_SHIFT                                    18
#define    DSI_INT_EN_PHY_RXTRIG_MASK                                     0x00040000
#define    DSI_INT_EN_PHY_D0_ULPS_SHIFT                                   17
#define    DSI_INT_EN_PHY_D0_ULPS_MASK                                    0x00020000
#define    DSI_INT_EN_PHY_D0_LPDT_SHIFT                                   16
#define    DSI_INT_EN_PHY_D0_LPDT_MASK                                    0x00010000
#define    DSI_INT_EN_PHY_DIR_FWD_TO_REV_SHIFT                            15
#define    DSI_INT_EN_PHY_DIR_FWD_TO_REV_MASK                             0x00008000
#define    DSI_INT_EN_PHY_D0_STOP_SHIFT                                   14
#define    DSI_INT_EN_PHY_D0_STOP_MASK                                    0x00004000
#define    DSI_INT_EN_PHY_CLK_ULPS_SHIFT                                  13
#define    DSI_INT_EN_PHY_CLK_ULPS_MASK                                   0x00002000
#define    DSI_INT_EN_PHY_CLK_HS_SHIFT                                    12
#define    DSI_INT_EN_PHY_CLK_HS_MASK                                     0x00001000
#define    DSI_INT_EN_PHY_CLK_STOP_SHIFT                                  11
#define    DSI_INT_EN_PHY_CLK_STOP_MASK                                   0x00000800
#define    DSI_INT_EN_PR_TO_SHIFT                                         10
#define    DSI_INT_EN_PR_TO_MASK                                          0x00000400
#define    DSI_INT_EN_TA_TO_SHIFT                                         9
#define    DSI_INT_EN_TA_TO_MASK                                          0x00000200
#define    DSI_INT_EN_LRX_H_TO_SHIFT                                      8
#define    DSI_INT_EN_LRX_H_TO_MASK                                       0x00000100
#define    DSI_INT_EN_HTX_TO_SHIFT                                        7
#define    DSI_INT_EN_HTX_TO_MASK                                         0x00000080
#define    DSI_INT_EN_ERRCONTLP1_SHIFT                                    6
#define    DSI_INT_EN_ERRCONTLP1_MASK                                     0x00000040
#define    DSI_INT_EN_ERRCONTLP0_SHIFT                                    5
#define    DSI_INT_EN_ERRCONTLP0_MASK                                     0x00000020
#define    DSI_INT_EN_ERRCONTROL_SHIFT                                    4
#define    DSI_INT_EN_ERRCONTROL_MASK                                     0x00000010
#define    DSI_INT_EN_ERRSYNCESC_SHIFT                                    3
#define    DSI_INT_EN_ERRSYNCESC_MASK                                     0x00000008
#define    DSI_INT_EN_RX2_PKT_SHIFT                                       2
#define    DSI_INT_EN_RX2_PKT_MASK                                        0x00000004
#define    DSI_INT_EN_RX1_PKT_SHIFT                                       1
#define    DSI_INT_EN_RX1_PKT_MASK                                        0x00000002
#define    DSI_INT_EN_CMD_PKT_SHIFT                                       0
#define    DSI_INT_EN_CMD_PKT_MASK                                        0x00000001

#define DSI_STAT_OFFSET                                                   0x00000030
#define DSI_STAT_TYPE                                                     UInt32
#define DSI_STAT_RESERVED_MASK                                            0xFE400000
#define    DSI_STAT_FIFO_ERR_SHIFT                                        24
#define    DSI_STAT_FIFO_ERR_MASK                                         0x01000000
#define    DSI_STAT_CMD_CTRL_DONE_SHIFT                                   23
#define    DSI_STAT_CMD_CTRL_DONE_MASK                                    0x00800000
#define    DSI_STAT_PHY_D1_ULPS_SHIFT                                     21
#define    DSI_STAT_PHY_D1_ULPS_MASK                                      0x00200000
#define    DSI_STAT_PHY_D1_STOP_SHIFT                                     20
#define    DSI_STAT_PHY_D1_STOP_MASK                                      0x00100000
#define    DSI_STAT_PHY_RXLPDT_SHIFT                                      19
#define    DSI_STAT_PHY_RXLPDT_MASK                                       0x00080000
#define    DSI_STAT_PHY_RXTRIG_SHIFT                                      18
#define    DSI_STAT_PHY_RXTRIG_MASK                                       0x00040000
#define    DSI_STAT_PHY_D0_ULPS_SHIFT                                     17
#define    DSI_STAT_PHY_D0_ULPS_MASK                                      0x00020000
#define    DSI_STAT_PHY_D0_LPDT_SHIFT                                     16
#define    DSI_STAT_PHY_D0_LPDT_MASK                                      0x00010000
#define    DSI_STAT_PHY_DIR_FWD_TOREV_SHIFT                               15
#define    DSI_STAT_PHY_DIR_FWD_TOREV_MASK                                0x00008000
#define    DSI_STAT_PHY_D0_STOP_SHIFT                                     14
#define    DSI_STAT_PHY_D0_STOP_MASK                                      0x00004000
#define    DSI_STAT_PHY_CLK_ULPS_SHIFT                                    13
#define    DSI_STAT_PHY_CLK_ULPS_MASK                                     0x00002000
#define    DSI_STAT_PHY_CLK_HS_SHIFT                                      12
#define    DSI_STAT_PHY_CLK_HS_MASK                                       0x00001000
#define    DSI_STAT_PHY_CLK_STOP_SHIFT                                    11
#define    DSI_STAT_PHY_CLK_STOP_MASK                                     0x00000800
#define    DSI_STAT_PR_TO_SHIFT                                           10
#define    DSI_STAT_PR_TO_MASK                                            0x00000400
#define    DSI_STAT_TA_TO_SHIFT                                           9
#define    DSI_STAT_TA_TO_MASK                                            0x00000200
#define    DSI_STAT_LPRX_TO_SHIFT                                         8
#define    DSI_STAT_LPRX_TO_MASK                                          0x00000100
#define    DSI_STAT_HSTX_TO_SHIFT                                         7
#define    DSI_STAT_HSTX_TO_MASK                                          0x00000080
#define    DSI_STAT_ERRCONTLP1_SHIFT                                      6
#define    DSI_STAT_ERRCONTLP1_MASK                                       0x00000040
#define    DSI_STAT_ERRCONTLP0_SHIFT                                      5
#define    DSI_STAT_ERRCONTLP0_MASK                                       0x00000020
#define    DSI_STAT_ERRCONTROL_SHIFT                                      4
#define    DSI_STAT_ERRCONTROL_MASK                                       0x00000010
#define    DSI_STAT_ERRSYNCESC_SHIFT                                      3
#define    DSI_STAT_ERRSYNCESC_MASK                                       0x00000008
#define    DSI_STAT_RX2_PKT_SHIFT                                         2
#define    DSI_STAT_RX2_PKT_MASK                                          0x00000004
#define    DSI_STAT_RX1_PKT_SHIFT                                         1
#define    DSI_STAT_RX1_PKT_MASK                                          0x00000002
#define    DSI_STAT_CMD_PKT_SHIFT                                         0
#define    DSI_STAT_CMD_PKT_MASK                                          0x00000001

#define DSI_HSTX_TO_CNT_OFFSET                                            0x00000034
#define DSI_HSTX_TO_CNT_TYPE                                              UInt32
#define DSI_HSTX_TO_CNT_RESERVED_MASK                                     0xFF000000
#define    DSI_HSTX_TO_CNT_HSTX_TO_SHIFT                                  0
#define    DSI_HSTX_TO_CNT_HSTX_TO_MASK                                   0x00FFFFFF

#define DSI_LPRX_TO_CNT_OFFSET                                            0x00000038
#define DSI_LPRX_TO_CNT_TYPE                                              UInt32
#define DSI_LPRX_TO_CNT_RESERVED_MASK                                     0xFF000000
#define    DSI_LPRX_TO_CNT_LPRX_TO_SHIFT                                  0
#define    DSI_LPRX_TO_CNT_LPRX_TO_MASK                                   0x00FFFFFF

#define DSI_TA_TO_CNT_OFFSET                                              0x0000003C
#define DSI_TA_TO_CNT_TYPE                                                UInt32
#define DSI_TA_TO_CNT_RESERVED_MASK                                       0xFF000000
#define    DSI_TA_TO_CNT_TA_TO_SHIFT                                      0
#define    DSI_TA_TO_CNT_TA_TO_MASK                                       0x00FFFFFF

#define DSI_PR_TO_CNT_OFFSET                                              0x00000040
#define DSI_PR_TO_CNT_TYPE                                                UInt32
#define DSI_PR_TO_CNT_RESERVED_MASK                                       0xFF000000
#define    DSI_PR_TO_CNT_PR_TO_SHIFT                                      0
#define    DSI_PR_TO_CNT_PR_TO_MASK                                       0x00FFFFFF

#define DSI_PHYC_OFFSET                                                   0x00000044
#define DSI_PHYC_TYPE                                                     UInt32
#define DSI_PHYC_RESERVED_MASK                                            0xFFFC0888
#define    DSI_PHYC_ESC_CLK_LPDT_SHIFT                                    12
#define    DSI_PHYC_ESC_CLK_LPDT_MASK                                     0x0003F000
#define    DSI_PHYC_TX_HSCLK_CONT_SHIFT                                   10
#define    DSI_PHYC_TX_HSCLK_CONT_MASK                                    0x00000400
#define    DSI_PHYC_TXULPSCLK_SHIFT                                       9
#define    DSI_PHYC_TXULPSCLK_MASK                                        0x00000200
#define    DSI_PHYC_PHY_CLANE_EN_SHIFT                                    8
#define    DSI_PHYC_PHY_CLANE_EN_MASK                                     0x00000100
#define    DSI_PHYC_FORCE_TXSTOP_1_SHIFT                                  6
#define    DSI_PHYC_FORCE_TXSTOP_1_MASK                                   0x00000040
#define    DSI_PHYC_TXULPSESC_1_SHIFT                                     5
#define    DSI_PHYC_TXULPSESC_1_MASK                                      0x00000020
#define    DSI_PHYC_PHY_DLANE1_EN_SHIFT                                   4
#define    DSI_PHYC_PHY_DLANE1_EN_MASK                                    0x00000010
#define    DSI_PHYC_FORCE_TXSTOP_0_SHIFT                                  2
#define    DSI_PHYC_FORCE_TXSTOP_0_MASK                                   0x00000004
#define    DSI_PHYC_TXULPSESC_0_SHIFT                                     1
#define    DSI_PHYC_TXULPSESC_0_MASK                                      0x00000002
#define    DSI_PHYC_PHY_DLANE0_EN_SHIFT                                   0
#define    DSI_PHYC_PHY_DLANE0_EN_MASK                                    0x00000001

#define DSI_HS_INIT_OFFSET                                                0x00000048
#define DSI_HS_INIT_TYPE                                                  UInt32
#define DSI_HS_INIT_RESERVED_MASK                                         0xFE000003
#define    DSI_HS_INIT_HS_INIT_SHIFT                                      0
#define    DSI_HS_INIT_HS_INIT_MASK                                       0x01FFFFFC

#define DSI_HS_EXIT_OFFSET                                                0x0000004C
#define DSI_HS_EXIT_TYPE                                                  UInt32
#define DSI_HS_EXIT_RESERVED_MASK                                         0xFFFFFC03
#define    DSI_HS_EXIT_HS_EXIT_SHIFT                                      0
#define    DSI_HS_EXIT_HS_EXIT_MASK                                       0x000003FC

#define DSI_HS_LPX_OFFSET                                                 0x00000050
#define DSI_HS_LPX_TYPE                                                   UInt32
#define DSI_HS_LPX_RESERVED_MASK                                          0xFFFFFC03
#define    DSI_HS_LPX_HS_LPX_SHIFT                                        0
#define    DSI_HS_LPX_HS_LPX_MASK                                         0x000003FC

#define DSI_HS_CPRE_OFFSET                                                0x00000054
#define DSI_HS_CPRE_TYPE                                                  UInt32
#define DSI_HS_CPRE_RESERVED_MASK                                         0xFFFFFC03
#define    DSI_HS_CPRE_HS_CPRE_SHIFT                                      0
#define    DSI_HS_CPRE_HS_CPRE_MASK                                       0x000003FC

#define DSI_HS_CZERO_OFFSET                                               0x00000058
#define DSI_HS_CZERO_TYPE                                                 UInt32
#define DSI_HS_CZERO_RESERVED_MASK                                        0xFFFFFC03
#define    DSI_HS_CZERO_HS_CZERO_SHIFT                                    0
#define    DSI_HS_CZERO_HS_CZERO_MASK                                     0x000003FC

#define DSI_HS_CPOST_OFFSET                                               0x0000005C
#define DSI_HS_CPOST_TYPE                                                 UInt32
#define DSI_HS_CPOST_RESERVED_MASK                                        0xFFFFFC03
#define    DSI_HS_CPOST_HS_CPOST_SHIFT                                    0
#define    DSI_HS_CPOST_HS_CPOST_MASK                                     0x000003FC

#define DSI_HS_CTRAIL_OFFSET                                              0x00000060
#define DSI_HS_CTRAIL_TYPE                                                UInt32
#define DSI_HS_CTRAIL_RESERVED_MASK                                       0xFFFFFC03
#define    DSI_HS_CTRAIL_HS_CTRAIL_SHIFT                                  0
#define    DSI_HS_CTRAIL_HS_CTRAIL_MASK                                   0x000003FC

#define DSI_HS_WUP_OFFSET                                                 0x00000064
#define DSI_HS_WUP_TYPE                                                   UInt32
#define DSI_HS_WUP_RESERVED_MASK                                          0xFE000003
#define    DSI_HS_WUP_HS_WUP_SHIFT                                        0
#define    DSI_HS_WUP_HS_WUP_MASK                                         0x01FFFFFC

#define DSI_HS_PRE_OFFSET                                                 0x00000068
#define DSI_HS_PRE_TYPE                                                   UInt32
#define DSI_HS_PRE_RESERVED_MASK                                          0xFFFFFC03
#define    DSI_HS_PRE_HS_PRE_SHIFT                                        0
#define    DSI_HS_PRE_HS_PRE_MASK                                         0x000003FC

#define DSI_HS_ZERO_OFFSET                                                0x0000006C
#define DSI_HS_ZERO_TYPE                                                  UInt32
#define DSI_HS_ZERO_RESERVED_MASK                                         0xFFFFFC03
#define    DSI_HS_ZERO_HS_ZERO_SHIFT                                      0
#define    DSI_HS_ZERO_HS_ZERO_MASK                                       0x000003FC

#define DSI_HS_TRAIL_OFFSET                                               0x00000070
#define DSI_HS_TRAIL_TYPE                                                 UInt32
#define DSI_HS_TRAIL_RESERVED_MASK                                        0xFFFFFC03
#define    DSI_HS_TRAIL_HS_TRAIL_SHIFT                                    0
#define    DSI_HS_TRAIL_HS_TRAIL_MASK                                     0x000003FC

#define DSI_LPX_OFFSET                                                    0x00000074
#define DSI_LPX_TYPE                                                      UInt32
#define DSI_LPX_RESERVED_MASK                                             0xFFFFFF00
#define    DSI_LPX_LPX_SHIFT                                              0
#define    DSI_LPX_LPX_MASK                                               0x000000FF

#define DSI_LP_WUP_OFFSET                                                 0x00000078
#define DSI_LP_WUP_TYPE                                                   UInt32
#define DSI_LP_WUP_RESERVED_MASK                                          0xFF000000
#define    DSI_LP_WUP_LP_WUP_SHIFT                                        0
#define    DSI_LP_WUP_LP_WUP_MASK                                         0x00FFFFFF

#define DSI_TA_GO_OFFSET                                                  0x0000007C
#define DSI_TA_GO_TYPE                                                    UInt32
#define DSI_TA_GO_RESERVED_MASK                                           0xFFFFFF00
#define    DSI_TA_GO_TA_GO_SHIFT                                          0
#define    DSI_TA_GO_TA_GO_MASK                                           0x000000FF

#define DSI_TA_SURE_OFFSET                                                0x00000080
#define DSI_TA_SURE_TYPE                                                  UInt32
#define DSI_TA_SURE_RESERVED_MASK                                         0xFFFFFF00
#define    DSI_TA_SURE_TA_SURE_SHIFT                                      0
#define    DSI_TA_SURE_TA_SURE_MASK                                       0x000000FF

#define DSI_TA_GET_OFFSET                                                 0x00000084
#define DSI_TA_GET_TYPE                                                   UInt32
#define DSI_TA_GET_RESERVED_MASK                                          0xFFFFFF00
#define    DSI_TA_GET_TA_GET_SHIFT                                        0
#define    DSI_TA_GET_TA_GET_MASK                                         0x000000FF

#define DSI_PHY_AFEC_OFFSET                                               0x00000088
#define DSI_PHY_AFEC_TYPE                                                 UInt32
#define DSI_PHY_AFEC_RESERVED_MASK                                        0xF8038000
#define    DSI_PHY_AFEC_IDR_DLANE1_SHIFT                                  24
#define    DSI_PHY_AFEC_IDR_DLANE1_MASK                                   0x07000000
#define    DSI_PHY_AFEC_IDR_DLANE0_SHIFT                                  21
#define    DSI_PHY_AFEC_IDR_DLANE0_MASK                                   0x00E00000
#define    DSI_PHY_AFEC_IDR_CLANE_SHIFT                                   18
#define    DSI_PHY_AFEC_IDR_CLANE_MASK                                    0x001C0000
#define    DSI_PHY_AFEC_RESET_SHIFT                                       14
#define    DSI_PHY_AFEC_RESET_MASK                                        0x00004000
#define    DSI_PHY_AFEC_PD_SHIFT                                          13
#define    DSI_PHY_AFEC_PD_MASK                                           0x00002000
#define    DSI_PHY_AFEC_PD_BG_SHIFT                                       12
#define    DSI_PHY_AFEC_PD_BG_MASK                                        0x00001000
#define    DSI_PHY_AFEC_PD_DLANE1_SHIFT                                   11
#define    DSI_PHY_AFEC_PD_DLANE1_MASK                                    0x00000800
#define    DSI_PHY_AFEC_PD_DLANE0_SHIFT                                   10
#define    DSI_PHY_AFEC_PD_DLANE0_MASK                                    0x00000400
#define    DSI_PHY_AFEC_PD_CLANE_SHIFT                                    9
#define    DSI_PHY_AFEC_PD_CLANE_MASK                                     0x00000200
#define    DSI_PHY_AFEC_FS2X_EN_CLK_SHIFT                                 8
#define    DSI_PHY_AFEC_FS2X_EN_CLK_MASK                                  0x00000100
#define    DSI_PHY_AFEC_PTATADJ_SHIFT                                     4
#define    DSI_PHY_AFEC_PTATADJ_MASK                                      0x000000F0
#define    DSI_PHY_AFEC_CTATADJ_SHIFT                                     0
#define    DSI_PHY_AFEC_CTATADJ_MASK                                      0x0000000F

#define DSI_TSTSEL_OFFSET                                                 0x0000008C
#define DSI_TSTSEL_TYPE                                                   UInt32
#define DSI_TSTSEL_RESERVED_MASK                                          0xFFFFFFF0
#define    DSI_TSTSEL_TEST_SELEC_SHIFT                                    0
#define    DSI_TSTSEL_TEST_SELEC_MASK                                     0x0000000F
#define       DSI_TSTSEL_TEST_SELEC_CMD_ZERO                              0x00000000
#define       DSI_TSTSEL_TEST_SELEC_CMD_ONE                               0x00000001
#define       DSI_TSTSEL_TEST_SELEC_CMD_TWO                               0x00000002
#define       DSI_TSTSEL_TEST_SELEC_CMD_THREE                             0x00000003
#define       DSI_TSTSEL_TEST_SELEC_CMD_FOUR                              0x00000004
#define       DSI_TSTSEL_TEST_SELEC_CMD_FIVE                              0x00000005
#define       DSI_TSTSEL_TEST_SELEC_CMD_SIX                               0x00000006
#define       DSI_TSTSEL_TEST_SELEC_CMD_SEVEN                             0x00000007
#define       DSI_TSTSEL_TEST_SELEC_CMD_EIGHT                             0x00000008
#define       DSI_TSTSEL_TEST_SELEC_CMD_NINE                              0x00000009
#define       DSI_TSTSEL_TEST_SELEC_CMD_TEN                               0x0000000A
#define       DSI_TSTSEL_TEST_SELEC_CMD_ELEVEN                            0x0000000B

#define DSI_TSTMON_OFFSET                                                 0x00000090
#define DSI_TSTMON_TYPE                                                   UInt32
#define DSI_TSTMON_RESERVED_MASK                                          0x00000000
#define    DSI_TSTMON_UNION_SHIFT                                         0
#define    DSI_TSTMON_UNION_MASK                                          0xFFFFFFFF

#define DSI_EOT_PKT_OFFSET                                                0x00000094
#define DSI_EOT_PKT_TYPE                                                  UInt32
#define DSI_EOT_PKT_RESERVED_MASK                                         0xC0000000
#define    DSI_EOT_PKT_EOT_SHIFT                                          0
#define    DSI_EOT_PKT_EOT_MASK                                           0x3FFFFFFF

#define DSI_PHY_ACTRL_OFFSET                                              0x00000098
#define DSI_PHY_ACTRL_TYPE                                                UInt32
#define DSI_PHY_ACTRL_RESERVED_MASK                                       0xFFFFF000
#define    DSI_PHY_ACTRL_DLANE1_ACTRL_SHIFT                               8
#define    DSI_PHY_ACTRL_DLANE1_ACTRL_MASK                                0x00000F00
#define    DSI_PHY_ACTRL_DLANE0_ACTRL_SHIFT                               4
#define    DSI_PHY_ACTRL_DLANE0_ACTRL_MASK                                0x000000F0
#define    DSI_PHY_ACTRL_CLANE_ACTRL_SHIFT                                0
#define    DSI_PHY_ACTRL_CLANE_ACTRL_MASK                                 0x0000000F

#define DSI_HS_CPREPARE_OFFSET                                            0x0000009C
#define DSI_HS_CPREPARE_TYPE                                              UInt32
#define DSI_HS_CPREPARE_RESERVED_MASK                                     0xFFFFFC03
#define    DSI_HS_CPREPARE_HS_CLK_PREP_SHIFT                              0
#define    DSI_HS_CPREPARE_HS_CLK_PREP_MASK                               0x000003FC

#define DSI_HS_ANLAT_OFFSET                                               0x000000A0
#define DSI_HS_ANLAT_TYPE                                                 UInt32
#define DSI_HS_ANLAT_RESERVED_MASK                                        0xFFFFFFC3
#define    DSI_HS_ANLAT_HS_ANLAT_SHIFT                                    0
#define    DSI_HS_ANLAT_HS_ANLAT_MASK                                     0x0000003C

#define DSI_TE0C_OFFSET                                                   0x000000B0
#define DSI_TE0C_TYPE                                                     UInt32
#define DSI_TE0C_RESERVED_MASK                                            0xFFF80000
#define    DSI_TE0C_HSLINE_SHIFT                                          3
#define    DSI_TE0C_HSLINE_MASK                                           0x0007FFF8
#define    DSI_TE0C_POL_SHIFT                                             2
#define    DSI_TE0C_POL_MASK                                              0x00000004
#define    DSI_TE0C_MODE_SHIFT                                            1
#define    DSI_TE0C_MODE_MASK                                             0x00000002
#define    DSI_TE0C_TE_EN_SHIFT                                           0
#define    DSI_TE0C_TE_EN_MASK                                            0x00000001

#define DSI_TE0_VSWIDTH_OFFSET                                            0x000000B4
#define DSI_TE0_VSWIDTH_TYPE                                              UInt32
#define DSI_TE0_VSWIDTH_RESERVED_MASK                                     0x00000000
#define    DSI_TE0_VSWIDTH_TE_VSWIDTH_SHIFT                               0
#define    DSI_TE0_VSWIDTH_TE_VSWIDTH_MASK                                0xFFFFFFFF

#define DSI_TE1C_OFFSET                                                   0x000000B8
#define DSI_TE1C_TYPE                                                     UInt32
#define DSI_TE1C_RESERVED_MASK                                            0xFFF80000
#define    DSI_TE1C_HSLINE_SHIFT                                          3
#define    DSI_TE1C_HSLINE_MASK                                           0x0007FFF8
#define    DSI_TE1C_POL_SHIFT                                             2
#define    DSI_TE1C_POL_MASK                                              0x00000004
#define    DSI_TE1C_MODE_SHIFT                                            1
#define    DSI_TE1C_MODE_MASK                                             0x00000002
#define    DSI_TE1C_TE_EN_SHIFT                                           0
#define    DSI_TE1C_TE_EN_MASK                                            0x00000001

#define DSI_TE1_VSWIDTH_OFFSET                                            0x000000BC
#define DSI_TE1_VSWIDTH_TYPE                                              UInt32
#define DSI_TE1_VSWIDTH_RESERVED_MASK                                     0x00000000
#define    DSI_TE1_VSWIDTH_TE_VSWIDTH_SHIFT                               0
#define    DSI_TE1_VSWIDTH_TE_VSWIDTH_MASK                                0xFFFFFFFF

#define DSI_CMD_PKTC2_OFFSET                                              0x000000C0
#define DSI_CMD_PKTC2_TYPE                                                UInt32
#define DSI_CMD_PKTC2_RESERVED_MASK                                       0x00000002
#define    DSI_CMD_PKTC2_TRIG_CMD_SHIFT                                   24
#define    DSI_CMD_PKTC2_TRIG_CMD_MASK                                    0xFF000000
#define    DSI_CMD_PKTC2_CMD_REPEAT_SHIFT                                 10
#define    DSI_CMD_PKTC2_CMD_REPEAT_MASK                                  0x00FFFC00
#define    DSI_CMD_PKTC2_DISPLAY_NO_SHIFT                                 8
#define    DSI_CMD_PKTC2_DISPLAY_NO_MASK                                  0x00000300
#define    DSI_CMD_PKTC2_CMD_TX_TIME_SHIFT                                6
#define    DSI_CMD_PKTC2_CMD_TX_TIME_MASK                                 0x000000C0
#define    DSI_CMD_PKTC2_CMD_CTRL_SHIFT                                   4
#define    DSI_CMD_PKTC2_CMD_CTRL_MASK                                    0x00000030
#define    DSI_CMD_PKTC2_CMD_MODE_SHIFT                                   3
#define    DSI_CMD_PKTC2_CMD_MODE_MASK                                    0x00000008
#define    DSI_CMD_PKTC2_CMD_TYPE_SHIFT                                   2
#define    DSI_CMD_PKTC2_CMD_TYPE_MASK                                    0x00000004
#define    DSI_CMD_PKTC2_CMD_TE_EN_SHIFT                                  1
#define    DSI_CMD_PKTC2_CMD_TE_EN_MASK                                   0x00000002
#define    DSI_CMD_PKTC2_CMD_EN_SHIFT                                     0
#define    DSI_CMD_PKTC2_CMD_EN_MASK                                      0x00000001

#define DSI_PIX_FIFO0_OFFSET                                              0x00000140
#define DSI_PIX_FIFO0_TYPE                                                UInt32
#define DSI_PIX_FIFO0_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO0_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO0_PIX_MASK                                         0xFFFFFFFF

#define DSI_PIX_FIFO1_OFFSET                                              0x00000144
#define DSI_PIX_FIFO1_TYPE                                                UInt32
#define DSI_PIX_FIFO1_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO1_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO1_PIX_MASK                                         0xFFFFFFFF

#define DSI_PIX_FIFO2_OFFSET                                              0x00000148
#define DSI_PIX_FIFO2_TYPE                                                UInt32
#define DSI_PIX_FIFO2_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO2_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO2_PIX_MASK                                         0xFFFFFFFF

#define DSI_PIX_FIFO3_OFFSET                                              0x0000014C
#define DSI_PIX_FIFO3_TYPE                                                UInt32
#define DSI_PIX_FIFO3_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO3_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO3_PIX_MASK                                         0xFFFFFFFF

#define DSI_PIX_FIFO4_OFFSET                                              0x00000150
#define DSI_PIX_FIFO4_TYPE                                                UInt32
#define DSI_PIX_FIFO4_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO4_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO4_PIX_MASK                                         0xFFFFFFFF

#define DSI_PIX_FIFO5_OFFSET                                              0x00000154
#define DSI_PIX_FIFO5_TYPE                                                UInt32
#define DSI_PIX_FIFO5_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO5_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO5_PIX_MASK                                         0xFFFFFFFF

#define DSI_PIX_FIFO6_OFFSET                                              0x00000158
#define DSI_PIX_FIFO6_TYPE                                                UInt32
#define DSI_PIX_FIFO6_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO6_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO6_PIX_MASK                                         0xFFFFFFFF

#define DSI_PIX_FIFO7_OFFSET                                              0x0000015C
#define DSI_PIX_FIFO7_TYPE                                                UInt32
#define DSI_PIX_FIFO7_RESERVED_MASK                                       0x00000000
#define    DSI_PIX_FIFO7_PIX_SHIFT                                        0
#define    DSI_PIX_FIFO7_PIX_MASK                                         0xFFFFFFFF

#define DSI_WC_PIXEL_CNT_OFFSET                                           0x00000288
#define DSI_WC_PIXEL_CNT_TYPE                                             UInt32
#define DSI_WC_PIXEL_CNT_RESERVED_MASK                                    0xFFFF8000
#define    DSI_WC_PIXEL_CNT_WC_PIXEL_CNT_SHIFT                            0
#define    DSI_WC_PIXEL_CNT_WC_PIXEL_CNT_MASK                             0x00007FFF

#endif /* __BRCM_RDB_DSI_H__ */


