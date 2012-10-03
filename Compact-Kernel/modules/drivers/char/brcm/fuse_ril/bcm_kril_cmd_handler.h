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

#ifndef _BCM_KRIL_CMD_HANDLER_H
#define _BCM_KRIL_CMD_HANDLER_H

#include "bcm_kril_common.h"
#include "bcm_kril.h"

// Common capi2 handler state
#define BCM_SendCAPI2Cmd       0xFFFC
#define BCM_RESPCAPI2Cmd       0xFFFD
#define BCM_FinishCAPI2Cmd     0xFFFE
#define BCM_ErrorCAPI2Cmd      0xFFFF

/*+20110418 BCOM PATCH FOR DebugScreen*/
#define BCM_MeasureReport 0xFFFA
/*-20110418 BCOM PATCH FOR DebugScreen*/


#define BCM_TID_INIT   0X00
#define BCM_TID_MAX    0XFFFFF
#define BCM_TID_SEND   0XFFFF
#define BCM_TID_RECV   0x0000
#define BCM_TID_NOTIFY 0X00

#define DUAL_SIM_SIZE  (SIM_DUAL_SECOND + 1)
//---------------------------------------------------------
// RAT Defines
//---------------------------------------------------------
#define RAT_NOT_AVAILABLE  0
#define RAT_GSM            1
#define RAT_UMTS           2

#define RX_SIGNAL_INFO_UNKNOWN 0xFF  

#define INTERNATIONAL_CODE  '+'     ///< '+' at the beginning
#define TOA_International   145
#define TOA_Unknown         129

// for SIM
#define SIM_IMAGE_INSTANCE_SIZE         9
#define SIM_IMAGE_INSTANCE_COUNT_SIZE   1

// for SMS
#define SMS_FULL      0xFF

// For STK
#define STK_REFRESH              (0x01)
#define STK_MORETIME             (0x02)
#define STK_POLLINTERVAL         (0x03)
#define STK_POLLINGOFF           (0x04)
#define STK_EVENTLIST            (0x05)
#define STK_SETUPCALL            (0x10)
#define STK_SENDSS               (0x11)
#define STK_SENDUSSD             (0x12)
#define STK_SENDSMS              (0x13)
#define STK_SENDDTMF             (0x14)
#define STK_LAUNCHBROWSER        (0x15)
#define STK_PLAYTONE             (0x20)
#define STK_DISPLAYTEXT          (0x21)
#define STK_GETINKEY             (0x22)
#define STK_GETINPUT             (0x23)
#define STK_SELECTITEM           (0x24)
#define STK_SETUPMENU            (0x25)
#define STK_LOCALINFO            (0x26)
#define STK_SETUPIDLEMODETEXT    (0x28)
#define STK_RUNATCOMMAND         (0x34)
#define STK_LANGUAGENOTIFICATION (0x35)
#define STK_OPENCHANNEL          (0x40)
#define STK_CLOSECHANNEL         (0x41)
#define STK_RECEIVEDATA          (0x42)
#define STK_SENDDATA             (0x43)
#define STK_CHANNELSTATUS        (0x44)

/* BCD coded IMEI length: 7 bytes for 14 digits, but first nibble and last nibble are not used (set to 0) 
 * in our system parameter convention. So total 8 bytes. */
#define BCD_IMEI_LEN  8

typedef struct
{
    struct list_head     list;
    KRIL_Response_t     result_info;
}KRIL_ResultQueue_t;


typedef struct // for command queue
{
    struct list_head list;
    struct mutex mutex;
    UInt32 cmd;
    struct work_struct commandq;
    struct workqueue_struct *cmd_wq;
    KRIL_Command_t *ril_cmd;
} KRIL_CmdQueue_t;


typedef struct  // for capi2 info
{
    struct list_head list;
    UInt32 tid;
    UInt8 clientID;
    MsgType_t msgType;
    SimNumber_t SimId;
    UInt32 DialogId;
    Result_t result;
    void *dataBuf;
    UInt32 dataLength;
    ResultDataBufHandle_t dataBufHandle;
} Kril_CAPI2Info_t;


typedef struct // for response queue
{
    spinlock_t lock;
    struct work_struct responseq;
    struct workqueue_struct *rsp_wq;
    Kril_CAPI2Info_t capi2_head;
} KRIL_RespWq_t;


typedef struct // for notify queue
{
    spinlock_t lock;
    struct work_struct notifyq;
    struct workqueue_struct *notify_wq;
    Kril_CAPI2Info_t capi2_head;
} KRIL_NotifyWq_t;


typedef struct kril_cmd // Command list
{
    struct list_head list;
    struct mutex mutex;
    UInt32 cmd;
    UInt32 tid;
    UInt32 handler_state;
    BRIL_Errno result;      //Response result
    KRIL_Command_t *ril_cmd;
    void *bcm_ril_rsp;
    UInt32 rsp_len;
    void (*capi2_handler)(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp);
    void* cmdContext; // may be used by KRIL command handlers to store context information across CAPI2 calls to CP
} KRIL_CmdList_t;


typedef struct kril_capi2_handler_fn_t
{
    int    cmd;
    void   (*capi2_handler)(void *ril_cmd, Kril_CAPI2Info_t *ril_rsp);
    int    contextSize;
} kril_capi2_handler_fn_t;


typedef struct
{
    dev_t devnum;
    int kril_state;
    struct semaphore kril_sem;
    struct cdev cdev;
    void __iomem *apcp_shmem;
    struct timer_list timeout;
    wait_queue_head_t read_wait;
    int timeout_done;
    spinlock_t           recv_lock;
    struct file          *file;
    wait_queue_head_t    recv_wait;
    struct mutex         recv_mutex;
}KRIL_Param_t;


// For cmd context structure
/**
Store the Call index
**/
typedef struct
{
    UInt8 ActiveIndex;
    Boolean activeMPTY;
    UInt8 WaitIndex;
    UInt8 HeldIndex;
    Boolean heldMPTY;
    UInt8 MptyIndex;
} KrilCallIndex_t;

/**
Store the DTMF information
**/
typedef struct
{
    UInt8 DTMFCallIndex;
    CCallState_t inCcCallState;
} KrilDTMFInfo_t;

/**
Store the SIM ID and DTMF information
**/
typedef struct
{
    UInt8 Index;
    SimNumber_t SimId;
} KrilSetPreferNWInfo_t;

/**
Store the Call Type information
**/
typedef struct
{
    CCallType_t CallType;
} KrilAnswerInfo_t;

/**
Selection path information for a SIM/USIM file
**/
typedef struct
{
    UInt8        path_len;
    const UInt16 *select_path;
} KrilSimDedFilePath_t;

void KRIL_InitHandler(void);
void KRIL_CommandThread(struct work_struct *data);
void KRIL_ResponseHandler(struct work_struct *data);
void KRIL_NotifyHandler(struct work_struct *data);

void ProcessNotification(Kril_CAPI2Info_t *entry);

void KRIL_Capi2HandleRespCbk(UInt32 tid, UInt8 clientID, MsgType_t msgType, SimNumber_t SimId, UInt32 DialogId, Result_t result, void *dataBuf, UInt32 dataLength,ResultDataBufHandle_t dataBufHandle);
void KRIL_Capi2HandleAckCbk(UInt32 tid, UInt8 clientid, RPC_ACK_Result_t ackResult, UInt32 ackUsrData);
void KRIL_Capi2HandleFlowCtrl(RPC_FlowCtrlEvent_t event, UInt8 channel);

ClientInfo_t* InitClientInfo(SimNumber_t SimId);
void SetClientID(UInt8 ClientID);
UInt32 GetClientID(void);
UInt32 GetNewTID(void);
UInt32 GetTID(void);

Boolean IsNeedToWait(SimNumber_t SimId, unsigned long CmdID);
Boolean IsBasicCapi2LoggingEnable(void);

// for PDP
UInt8 ReleasePdpContext(SimNumber_t SimId, UInt8 cid);
UInt8 FindPdpCid(SimNumber_t SimId);

// for call
void KRIL_SetIncomingCallIndex(SimNumber_t SimId, UInt8 IncomingCallIndex);
UInt8 KRIL_GetIncomingCallIndex(SimNumber_t SimId);
void KRIL_SetWaitingCallIndex(SimNumber_t SimId, UInt8 IncomingCallIndex);
UInt8 KRIL_GetWaitingCallIndex(SimNumber_t SimId);
void KRIL_SetCallType(SimNumber_t SimId, int index, CCallType_t theCallType);
CCallType_t KRIL_GetCallType(SimNumber_t SimId, int index);
void KRIL_SetCallState(SimNumber_t SimId, int index, BRIL_CallState theCallState);
BRIL_CallState KRIL_GetCallState(SimNumber_t SimId, int index);
void KRIL_ClearCallNumPresent(SimNumber_t SimId);
void KRIL_SetCallNumPresent(SimNumber_t SimId, int index, PresentationInd_t present);
PresentationInd_t KRIL_GetCallNumPresent(SimNumber_t SimId, int index);
void KRIL_SetInHoldCallHandler(SimNumber_t SimId, Boolean CallHandler);
Boolean KRIL_GetInHoldCallHandler(SimNumber_t SimId);
void KRIL_SetIsNeedMakeCall(SimNumber_t SimId, Boolean MakeCall);
Boolean KRIL_GetIsNeedMakeCall(SimNumber_t SimId);
void KRIL_SetHungupForegroundResumeBackgroundEndMPTY(UInt32 tid);
UInt32 KRIL_GetHungupForegroundResumeBackgroundEndMPTY(void);
void KRIL_SetLastCallFailCause(SimNumber_t SimId, BRIL_LastCallFailCause inCause);
BRIL_LastCallFailCause KRIL_GetLastCallFailCause(SimNumber_t SimId);
BRIL_LastCallFailCause KRIL_MNCauseToRilError(Cause_t inMNCause);

// for Network
void KRIL_SetInSetPrferredDataConnectionHandler(Boolean state);
void KRIL_SetInSetPrferredNetworkTypeHandler(Boolean state);
Boolean KRIL_GetInSetPrferredNetworkTypeHandler(void);
Boolean KRIL_SetPreferredNetworkType(SimNumber_t SimId, int PreferredNetworkType);
void KRIL_SetIsNeedSetPreferNetwrokType(Boolean state);
Boolean KRIL_GetIsNeedSetPreferNetwrokType(void);
int KRIL_GetPreferredNetworkType(SimNumber_t SimId);
void KRIL_SetLocationUpdateStatus(SimNumber_t SimId, int LocationUpdateStatus);
int KRIL_GetLocationUpdateStatus(SimNumber_t SimId);
void KRIL_SetRestrictedState(SimNumber_t SimId, int RestrictedState);
int KRIL_GetRestrictedState(SimNumber_t SimId);

// for SS
void KRIL_SetServiceClassValue(int ss_class);
int KRIL_GetServiceClassValue(void);

// for SIM
void KRIL_SetSimAppType(SimNumber_t SimId, SIM_APPL_TYPE_t simAppType);
SIM_APPL_TYPE_t KRIL_GetSimAppType(SimNumber_t SimId);

// for SMS
Boolean QuerySMSinSIMHandle(KRIL_CmdList_t *listentry, Kril_CAPI2Info_t *entry);
void KRIL_SetSmsMti(SimNumber_t SimId, SmsMti_t SmsMti);
SmsMti_t KRIL_GetSmsMti(SimNumber_t SimId);
void SetIsRevClass2SMS(SimNumber_t SimId, Boolean value);
Boolean GetIsRevClass2SMS(SimNumber_t SimId);
void KRIL_SetInSendSMSHandler(SimNumber_t SimId, Boolean SMSHandler);
Boolean KRIL_GetInSendSMSHandler(SimNumber_t SimId);
void KRIL_SetMESMSAvailable(Boolean IsSMSMEAvailable);
Boolean KRIL_GetMESMSAvailable(void);
void KRIL_SetTotalSMSInSIM(SimNumber_t SimId, UInt8 TotalSMSInSIM);
UInt8 KRIL_GetTotalSMSInSIM(SimNumber_t SimId);
UInt8 CheckFreeSMSIndex(SimNumber_t SimId);
void SetSMSMesgStatus(SimNumber_t SimId, UInt8 Index, SIMSMSMesgStatus_t status);
SIMSMSMesgStatus_t GetSMSMesgStatus(SimNumber_t SimId, UInt8 Index);
void KRIL_IncrementSendSMSNumber(SimNumber_t SimId);
void KRIL_DecrementSendSMSNumber(SimNumber_t SimId);
Int8 KRIL_GetSendSMSNumber(SimNumber_t SimId);
void KRIL_SetInUpdateSMSInSIMHandler(SimNumber_t SimId, Boolean SMSHandler);
Boolean KRIL_GetInUpdateSMSInSIMHandler(SimNumber_t SimId);
void KRIL_IncrementUpdateSMSNumber(SimNumber_t SimId);
void KRIL_DecrementUpdateSMSNumber(SimNumber_t SimId);
Int8 KRIL_GetUpdateSMSNumber(SimNumber_t SimId);

void KRIL_SetSMSToSIMTID(UInt32 Tid);
UInt32 KRIL_GetSMSToSIMTID(void);
void KRIL_SetServingCellTID(UInt32 tid);
UInt32 KRIL_GetServingCellTID(void);
void KRIL_SetInNeighborCellHandler( Boolean inHandler );
Boolean KRIL_GetInNeighborCellHandler(void);

void KRIL_SetInSetupPDPHandler( Boolean inHandler );
Boolean KRIL_GetInSetupPDPHandler(void);

void KRIL_SetNetworkSelectTID(UInt32 tid);
UInt32 KRIL_GetNetworkSelectTID(void);
void KRIL_SetInNetworkSelectHandler( Boolean inHandler );
Boolean KRIL_GetInNetworkSelectHandler(void);
UInt16 KRIL_USSDSeptet2Octet(UInt8 *p_src, UInt8 *p_dest, UInt16 num_of_Septets);

/*+20110418 BCOM PATCH FOR DebugScreen*/
void KRIL_SetMeasureReportTID(UInt32 tid);
UInt32 KRIL_GetMeasureReportTID(void);
void KRIL_SetInMeasureReportHandler( Boolean inHandler );
Boolean KRIL_GetInMeasureReportHandler(void);
/*-20110418 BCOM PATCH FOR DebugScreen*/

void KRIL_SetHandling2GOnlyRequest( SimNumber_t SimId, Boolean inHandler );
Boolean KRIL_GetHandling2GOnlyRequest(SimNumber_t SimId);

void KRIL_SetSsSrvReqTID(UInt32 tid);
UInt32 KRIL_GetSsSrvReqTID(void);

void KRIL_SetASSERTFlag(int flag);
int KRIL_GetASSERTFlag(void);

void HexDataToHexStr(char *HexString, const UInt8 *HexData, UInt16 length);
void RawDataPrintfun(UInt8* rawdata, UInt16 datalen, char* showstr);

void KRIL_CmdQueueWork(void);

void KRIL_SendNotify(SimNumber_t SimId, int CmdID, void *rsp_data, UInt32 rsp_len);

// for error cause transform
BRIL_Errno RILErrorResult(Result_t err);
BRIL_Errno RILErrorSIMResult(SIMAccess_t err);
#endif //_BCM_KRIL_CMD_HANDLER_H
