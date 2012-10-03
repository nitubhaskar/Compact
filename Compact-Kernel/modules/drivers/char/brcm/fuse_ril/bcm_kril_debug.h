/****************************************************************************
*
*     Copyright (c) 2009 Broadcom Corporation
*
*   Unless you and Broadcom execute a separate written software license 
*   agreement governing use of this software, this software is licensed to you 
*   under the terms of the GNU General Public License version 2, available 
*    at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL"). 
*
*   Notwithstanding the above, under no circumstances may you combine this 
*   software in any way with any other Broadcom software provided under a license 
*   other than the GPL, without Broadcom's express prior written consent.
*
****************************************************************************/

#ifndef _BCM_KRIL_DEBUG_H
#define _BCM_KRIL_DEBUG_H

#include "config.h"

extern int KRIL_GetASSERTFlag(void);
#define KRIL_ASSERT(flag) if ((KRIL_GetASSERTFlag() & flag)) __bug(__FILE__, __LINE__);

#define DBG_ERROR    0x0100
#define DBG_INFO     BCMLOG_ANDROID_KRIL_BASIC
#define DBG_TRACE    BCMLOG_ANDROID_KRIL_DETAIL
#define DBG_TRACE2   BCMLOG_ANDROID_KRIL_DETAIL

#define KRIL_DEBUG(level,fmt,args...)  if (BCMLOG_LogIdIsEnabled(level) || (level == DBG_ERROR)) printk( "%s:: " fmt, __FUNCTION__, ##args );

#endif //_BCM_KRIL_DEBUG_H
