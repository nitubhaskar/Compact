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
*   @file   msc.h
*
*   @brief  This file contains definitions for Mobile Station Controller 
*			(MSC) function prototypes.
*
****************************************************************************/

#ifndef _MSC_MSC_H_
#define _MSC_MSC_H_




//******************************************************************************
// Function Prototypes
//******************************************************************************
void MSC_Init( void );					// Initialize the top-level unit

void MSC_Shutdown( void );				// Shut down the top-level unit

void MSC_Register( void );				// Register call-back functions

void MSC_Run( void );					// Create and execute the top-level task

void MSC_Post_Msg(InterTaskMsg_t *msg); 

Boolean MS_IsSimInsertedSent(ClientInfo_t *inClientInfoPtr); // Return whether SIM inserted indication has been sent

void startStatsticInfoTimer(void);
void stopStatsticInfoTimer(void);

void MS_LoadMSDBNvData(void);

void MS_SetAttachSt(Boolean cs_flag, AttachState_t attach_state);

AttachState_t MS_GetAttachSt(Boolean cs_flag, SimNumber_t sim_id);

void MS_IsDetachedState( Boolean *cs_detached_sim1, Boolean *ps_detached_sim1, 
						 Boolean *cs_detached_sim2, Boolean *ps_detached_sim2 );

void MS_SetSystemState(SimNumber_t sim_id, SystemState_t state);

SystemState_t MS_GetSystemState(SimNumber_t sim_id);

Boolean MS_IsSystemOn(UInt8 sim_id);

#endif

