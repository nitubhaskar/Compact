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
#include "bcm_kril_common.h"
#include "bcm_kril_capi2_handler.h"
#include "bcm_kril_cmd_handler.h"
#include "bcm_kril_ioctl.h"

#include "capi2_stk_ds.h"
#include "capi2_pch_msg.h"
#include "capi2_gen_msg.h"
#include "capi2_reqrep.h"
#include "brcm_urilc_cmd.h"

//Common Define
const UInt16 Select_Mf_Path = APDUFILEID_MF;
#define SIM_GetMfPathLen() ( sizeof(Select_Mf_Path) / sizeof(UInt16) )
#define SIM_GetMfPath() (&Select_Mf_Path)

static UInt8 schnlIDs[CHNL_IDS_SIZE];
static UInt8 scodings[CHNL_IDS_SIZE];

extern Boolean gFdnSkip[DUAL_SIM_SIZE];

static const SIMSMSMesgStatus_t Kril_SMSMsgStatuss[] =
{
    SIMSMSMESGSTATUS_UNREAD,
    SIMSMSMESGSTATUS_READ,
    SIMSMSMESGSTATUS_UNSENT,
    SIMSMSMESGSTATUS_SENT,
};

static unsigned char sms_rec_no = 0;


void ParseSmscStr2BCD(char* pp, char *tempBuf, int length)
{
	int i;
	int lo_nibble, hi_nibble;
	for(i =0; i < length; i+=2)
	{
		lo_nibble = tempBuf[i+0];
		hi_nibble = tempBuf[i+1];
		if((i+1)==length)
			{
				hi_nibble = 0x0f;
				lo_nibble = tempBuf[i+0];
			}
		if((lo_nibble >= 0x30) && (lo_nibble <= 0x39))
			lo_nibble -= 0x30;
		else if(lo_nibble == 0x2a)/* charecter '*' will be translate to bcd value 0xa  */
			lo_nibble = 0x0a;
		else if(lo_nibble == 0x23)/* charecter '#' will be translate to bcd value 0xb  */
			lo_nibble = 0x0b;
		else if(lo_nibble == 0x70)/* charecter 'p' (DTMF) will be translate to bcd value 0xc  */
			lo_nibble = 0x0c;
		else if(lo_nibble == 0x3f)/* charecter '?' (Wild) will be translate to bcd value 0xd  */
			lo_nibble = 0x0d;
		else lo_nibble = 0x0f;


		if((hi_nibble >= 0x30) && (hi_nibble <= 0x39))
			hi_nibble -= 0x30;
		else if(hi_nibble == 0x2a)/* charecter '*' will be translate to bcd value 0xa  */
			hi_nibble = 0x0a;
		else if(hi_nibble == 0x23)/* charecter '#' will be translate to bcd value 0xb  */
			hi_nibble = 0x0b;
		else if(hi_nibble == 0x70)/* charecter 'p' (DTMF) will be translate to bcd value 0xc  */
			hi_nibble = 0x0c;
		else if(hi_nibble == 0x3f)/* charecter '?' (Wild) will be translate to bcd value 0xd  */
			hi_nibble = 0x0d;
		else hi_nibble = 0x0f;
		pp[i/2] = (lo_nibble) | (hi_nibble<<4);
	}
}

//***********************************************************************
/*
*	Gets the next non-empty SIM SMS record, starting at index inIndex.
*
*  @param  inIndex (in)      0-based index to start looking at.
*  @param  inStatus (in)     Status of message.  See TS 27.005 3.1, "<stat>":
*        0 = "REC UNREAD"
*        1 = "REC READ"
*        2 = "STO UNSENT"
*        3 = "STO SENT"
*
*   @return The 1-based index of the next non-empty SIM SMS record,
*           or 0 if none found.
*/
//***********************************************************************
static UInt8 GetNextValidRec(SimNumber_t SimId, UInt8 inIndex, int inStatus)
{
    UInt8 recNum = 0;
    UInt8 simRecords = KRIL_GetTotalSMSInSIM(SimId);

    if (inStatus < 4)
    {
        SIMSMSMesgStatus_t msgStatus = Kril_SMSMsgStatuss[inStatus];
        int index;

        // Find the first record that matches the requested status
        for (index = 1; index <= simRecords; index++)
        {
            if (GetSMSMesgStatus(SimId, index) == msgStatus)
            {
                recNum = index;
                break;
            }
        }
    }
    else if (inIndex < simRecords)
    {
        // List all messages.
        recNum = inIndex + 1;
    }

    return recNum;
}

void KRIL_SendSMSHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
 
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_ERROR, "[SEND_SMS] handler_state:0x%4X::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
        else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
        {
            StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
            KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }
 
    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilSendMsgInfo_t *tdata = (KrilSendMsgInfo_t *)pdata->ril_cmd->data;
            UInt8 *pszMsg = NULL;
            Sms_411Addr_t sca;
            
            // Check if need skip FDN
            if (TRUE == gFdnSkip[pdata->ril_cmd->SimId])
            {
                gFdnSkip[pdata->ril_cmd->SimId] = FALSE;
                
                CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_LOCAL_SS_ELEM_FDN_CHECK);
                pdata->handler_state = BCM_SMS_SET_FDN_MODE;
                return;
            }
            
            memset(&sca, 0, sizeof(Sms_411Addr_t));
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSendSMSResponse_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            pdata->rsp_len = sizeof(KrilSendSMSResponse_t);
            memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
            sca.Toa = tdata->Toa;
            sca.Len = tdata->Len;
            memcpy(sca.Val, tdata->Val, sca.Len);
            pszMsg = (UInt8 *) tdata->Pdu;
#if 0
            int a;
            for(a = 0 ; a < tdata->Length; a++)
                KRIL_DEBUG(DBG_ERROR, "[SEND_SMS] pdu[%d]:0x%02x\n", a, tdata->Pdu[a]);
#endif
            KRIL_DEBUG(DBG_ERROR, "[SEND_SMS] Toa:%d Len:%d\n", sca.Toa, sca.Len);

            KRIL_DEBUG(DBG_INFO, "Toa:%d Len:%d\n", sca.Toa, sca.Len);
            CAPI2_SmsApi_SendSMSPduReq(InitClientInfo(pdata->ril_cmd->SimId), tdata->Length, pszMsg, &sca);
            pdata->handler_state = BCM_SMS_RESPONSE_AND_RESET_FDN;
        }
        break;

        case BCM_SMS_SET_FDN_MODE:
        {
            CAPI2_MS_Element_t *rsp = (CAPI2_MS_Element_t*)capi2_rsp->dataBuf;
            KrilSMSInfo_t *pSmsInfo = (KrilSMSInfo_t*)pdata->cmdContext;
            CAPI2_MS_Element_t data;
            
            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR, "rsp is NULL. Error!!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            if (!pSmsInfo)
            {
                KRIL_DEBUG(DBG_ERROR, "pSmsInfo is NULL. Error!!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            memset(pSmsInfo, 0x00, sizeof(KrilSMSInfo_t));
            
            // Backup fdnmode
            pSmsInfo->fdnmode = rsp->data_u.u8Data;
            KRIL_DEBUG(DBG_INFO, "Original fdnmode:%d\n", pSmsInfo->fdnmode);
            
            // Set FDN mode as CONFIG_MODE_SUPPRESS to skip FDN
            pSmsInfo->isfdnskip = TRUE;

            memset(&data, 0, sizeof(CAPI2_MS_Element_t));
            data.inElemType = MS_LOCAL_SS_ELEM_FDN_CHECK;
            data.data_u.u8Data = CONFIG_MODE_SUPPRESS;
            CAPI2_MsDbApi_SetElement(InitClientInfo(pdata->ril_cmd->SimId), &data);
            pdata->handler_state = BCM_SendCAPI2Cmd;
        }
        break;

        case BCM_SMS_RESPONSE_AND_RESET_FDN:
        {
            SmsSubmitRspMsg_t* rsp = (SmsSubmitRspMsg_t*) capi2_rsp->dataBuf;
            KrilSendSMSResponse_t *rdata = (KrilSendSMSResponse_t *)pdata->bcm_ril_rsp;
            KrilSMSInfo_t *pSmsInfo = (KrilSMSInfo_t*)pdata->cmdContext;
            
            KRIL_DEBUG(DBG_INFO, "InternalErrCause:%d NetworkErrCause:0x%x submitRspType:%d tpMr:%d\n", rsp->InternalErrCause, rsp->NetworkErrCause, rsp->submitRspType, rsp->tpMr);
            
            if (!pSmsInfo)
            {
                KRIL_DEBUG(DBG_ERROR, "pSmsInfo is NULL. Error!!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if(MS_MN_SMS_NO_ERROR == rsp->NetworkErrCause)
            {
                if (rsp->InternalErrCause != RESULT_OK)
                {
                    pdata->result = RILErrorResult(rsp->InternalErrCause);
                    rdata->errorCode = rsp->NetworkErrCause;
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    rdata->messageRef = rsp->tpMr;
                    rdata->errorCode = rsp->NetworkErrCause;
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else
            {
                if (rsp->InternalErrCause != RESULT_OK)
                {
                    pdata->result = RILErrorResult(rsp->InternalErrCause);
                }
                rdata->errorCode = rsp->NetworkErrCause;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            
            if (TRUE == pSmsInfo->isfdnskip)
            {
                CAPI2_MS_Element_t data;
                
                // Backup the handler_state 
                pSmsInfo->handler_state = pdata->handler_state;
                
                // Set FDN mode as original mode
                memset(&data, 0, sizeof(CAPI2_MS_Element_t));
                data.inElemType = MS_LOCAL_SS_ELEM_FDN_CHECK;
                data.data_u.u8Data = pSmsInfo->fdnmode;
                CAPI2_MsDbApi_SetElement(InitClientInfo(pdata->ril_cmd->SimId), &data);
                
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KrilSMSInfo_t *pSmsInfo = (KrilSMSInfo_t*)pdata->cmdContext;
            
            if (!pSmsInfo)
            {
                KRIL_DEBUG(DBG_ERROR, "pSmsInfo is NULL. Error!!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            pdata->handler_state = pSmsInfo->handler_state;
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "[SEND_SMS] handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

static Boolean sMoreMessageToSend = FALSE; // keep the value to check whether we need to enable the more message to send
static UInt32 sSMSExpectMoreState = 0; // store the handler state
void KRIL_SendSMSExpectMoreHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "(KRIL_SendSMSExpectMoreHandler)handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
        else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
        {
            StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
            CAPI2_MS_Element_t data;
            memset((UInt8*)&data, 0x00, sizeof(CAPI2_MS_Element_t));
            data.inElemType = MS_LOCAL_SMS_ELEM_MORE_MESSAGE_TO_SEND;
            data.data_u.u8Data = 0; // disable the more message to send
            CAPI2_MsDbApi_SetElement(InitClientInfo(pdata->ril_cmd->SimId), &data);
            KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
            sSMSExpectMoreState = BCM_ErrorCAPI2Cmd;
            pdata->handler_state = BCM_SMS_DisableMoreMessageToSend;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            if (FALSE == sMoreMessageToSend)
            {
                CAPI2_MS_Element_t data;
                memset((UInt8*)&data, 0x00, sizeof(CAPI2_MS_Element_t));
                data.inElemType = MS_LOCAL_SMS_ELEM_MORE_MESSAGE_TO_SEND;
                data.data_u.u8Data = 1; // enable the more message to send
                sMoreMessageToSend = TRUE;
                CAPI2_MsDbApi_SetElement(InitClientInfo(pdata->ril_cmd->SimId), &data);
                pdata->handler_state = BCM_SMS_StartMultiSMSTransfer;
            }
            else
            {
                KrilSendMsgInfo_t *tdata = (KrilSendMsgInfo_t *)pdata->ril_cmd->data;
                UInt8 *pszMsg = NULL;
                Sms_411Addr_t sca; 
                memset(&sca, 0, sizeof(Sms_411Addr_t));
                pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSendSMSResponse_t), GFP_KERNEL);
                if(!pdata->bcm_ril_rsp) {
                    KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
                }
                pdata->rsp_len = sizeof(KrilSendSMSResponse_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                sca.Toa = tdata->Toa;
                sca.Len = tdata->Len;
                memcpy(sca.Val, tdata->Val, sca.Len);
                pszMsg = (UInt8 *) tdata->Pdu;
                KRIL_DEBUG(DBG_INFO, "Toa:%d Len:%d\n", sca.Toa, sca.Len);
                CAPI2_SmsApi_SendSMSPduReq(InitClientInfo(pdata->ril_cmd->SimId), tdata->Length, pszMsg, &sca);
                pdata->handler_state = BCM_RESPCAPI2Cmd;				
            }
            break;
        }

        case BCM_SMS_StartMultiSMSTransfer:
        {
            CAPI2_SmsApi_StartMultiSmsTransferReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_SMS_SendSMSPduReq;
            break;
        }
        
        case BCM_SMS_SendSMSPduReq:
        {
            KrilSendMsgInfo_t *tdata = (KrilSendMsgInfo_t *)pdata->ril_cmd->data;
            UInt8 *pszMsg = NULL;
            Sms_411Addr_t sca; memset(&sca, 0, sizeof(Sms_411Addr_t));
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSendSMSResponse_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            pdata->rsp_len = sizeof(KrilSendSMSResponse_t);
            memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
            sca.Toa = tdata->Toa;
            sca.Len = tdata->Len;
            memcpy(sca.Val, tdata->Val, sca.Len);
            pszMsg = (UInt8 *) tdata->Pdu;
#if 0
            int a;
            for(a = 0 ; a <= tdata->Length; a++)
                KRIL_DEBUG(DBG_INFO, "pdu[%d]:0x%x\n", a, tdata->Pdu[a]);
#endif
            KRIL_DEBUG(DBG_INFO, "Toa:%d Len:%d\n", sca.Toa, sca.Len);
            CAPI2_SmsApi_SendSMSPduReq(InitClientInfo(pdata->ril_cmd->SimId), tdata->Length, pszMsg, &sca);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            SmsSubmitRspMsg_t* rsp = (SmsSubmitRspMsg_t*) capi2_rsp->dataBuf;
            KrilSendSMSResponse_t *rdata = (KrilSendSMSResponse_t *)pdata->bcm_ril_rsp;
            KRIL_DEBUG(DBG_INFO, "InternalErrCause:%d NetworkErrCause:0x%x submitRspType:%d tpMr:%d\n", rsp->InternalErrCause, rsp->NetworkErrCause, rsp->submitRspType, rsp->tpMr);
            if(MS_MN_SMS_NO_ERROR == rsp->NetworkErrCause)
            {
                if (rsp->InternalErrCause != RESULT_OK)
                {
                    pdata->result = RILErrorResult(rsp->InternalErrCause);
                    rdata->errorCode = rsp->NetworkErrCause;
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    rdata->messageRef = rsp->tpMr;
                    rdata->errorCode = rsp->NetworkErrCause;
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else
            {
                if (rsp->InternalErrCause != RESULT_OK)
                {
                    pdata->result = RILErrorResult(rsp->InternalErrCause);
                }
                rdata->errorCode = rsp->NetworkErrCause;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            if (KRIL_IsMoreSendSMSCmd(pdata->ril_cmd->SimId) == FALSE)
            {
                CAPI2_MS_Element_t data;
                memset((UInt8*)&data, 0x00, sizeof(CAPI2_MS_Element_t));
                data.inElemType = MS_LOCAL_SMS_ELEM_MORE_MESSAGE_TO_SEND;
                data.data_u.u8Data = 0; // disable the more message to send
                sSMSExpectMoreState = pdata->handler_state;
                CAPI2_MsDbApi_SetElement(InitClientInfo(pdata->ril_cmd->SimId), &data);
                pdata->handler_state = BCM_SMS_DisableMoreMessageToSend;
            }
        }
        break;

        case BCM_SMS_DisableMoreMessageToSend:
        {
            CAPI2_SmsApi_StopMultiSmsTransferReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_SMS_StopMultiSMSTransfer;
            break;
        }

        case BCM_SMS_StopMultiSMSTransfer:
        {
            sMoreMessageToSend = FALSE;
            pdata->handler_state = sSMSExpectMoreState;
            sSMSExpectMoreState = 0;
        }
        break;
 
        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_WriteSMSToSIMHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
  
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

	
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            if(TRUE == GetIsRevClass2SMS(pdata->ril_cmd->SimId) && BCM_RESPCAPI2Cmd == pdata->handler_state) // send ack to network only for writeing Class 2 SMS in SIM fail
            {
                CAPI2_SmsApi_SendAckToNetwork(InitClientInfo(pdata->ril_cmd->SimId), KRIL_GetSmsMti(pdata->ril_cmd->SimId), SMS_ACK_ERROR);
                pdata->handler_state = BCM_SMS_SendAckToNetwork;
                return;
            }
            KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, FALSE);
            KRIL_GetUpdateSMSNumber(pdata->ril_cmd->SimId);// if more update SMS in queue, need to trigger the cmd queue event to process next SMS body
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }
  
    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            //for CIB version, message are written to SIM, which
            //includes status (first byte) + pdu
            UInt8 szMessage[SMSMESG_DATA_SZ+1];

            UInt16 rec_no;
            KrilWriteMsgInfo_t *tdata = (KrilWriteMsgInfo_t *)pdata->ril_cmd->data;
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilMsgIndexInfo_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            memset(szMessage, SIM_RAW_EMPTY_VALUE, SMSMESG_DATA_SZ+1); // Fill the 0xFF in the struct
            pdata->rsp_len = sizeof(KrilMsgIndexInfo_t);
            memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);

            //for CIB version, message are written to SIM, which
            //includes status (first byte) + pdu
            szMessage[0] = Kril_SMSMsgStatuss[tdata->MsgState];
            memcpy(&(szMessage[1]), tdata->Pdu, tdata->Length);
#if 0
            int i;
            for (i =0 ; i < tdata->Length; i++)
                KRIL_DEBUG(DBG_INFO, "szMessage[%d]:0x%x\n", i, szMessage[i]);
#endif
            KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, TRUE);
			if( tdata->index == 0xff )
			{
	            rec_no = CheckFreeSMSIndex(pdata->ril_cmd->SimId);
			}
			else	// for replace type
			{
	            rec_no = tdata->index;
			}
			
	        KRIL_DEBUG(DBG_ERROR, "[WRITE_SMS] rec_no:%d\n", rec_no);
            if (rec_no != SMS_FULL)
            {
                //CAPI2_SIM_SubmitLinearEFileUpdateReq() does not give rec_no
                //back in the resp message, set the record no in resp now
                KrilMsgIndexInfo_t *rdata = (KrilMsgIndexInfo_t *)pdata->bcm_ril_rsp;
                rdata->index = rec_no;
                //CAPI2_SimApi_SubmitLinearEFileUpdateReq() rec_no is 1 based
                //CheckFreeSMSIndex() is 1 based, AT is 0 based.
                CAPI2_SimApi_SubmitLinearEFileUpdateReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, rec_no, szMessage, (SMSMESG_DATA_SZ+1), SIM_GetMfPathLen(), SIM_GetMfPath());
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else
            {
                if(GetIsRevClass2SMS(pdata->ril_cmd->SimId) != TRUE)
                {
                    KRIL_SendNotify(pdata->ril_cmd->SimId, BRCM_RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
                    KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, FALSE);
                    KRIL_GetUpdateSMSNumber(pdata->ril_cmd->SimId);// if more update SMS in queue, need to trigger the cmd queue event to process next SMS
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    if (TRUE == KRIL_GetMESMSAvailable())
                    {
                        CAPI2_SmsApi_SendAckToNetwork(InitClientInfo(pdata->ril_cmd->SimId), KRIL_GetSmsMti(pdata->ril_cmd->SimId), SMS_ACK_PROTOCOL_ERROR);
                        pdata->handler_state = BCM_SMS_SendAckToNetwork;
                    }
                    else
                    {
                        CAPI2_SmsApi_SendAckToNetwork(InitClientInfo(pdata->ril_cmd->SimId), KRIL_GetSmsMti(pdata->ril_cmd->SimId), SMS_ACK_MEM_EXCEEDED);
                        pdata->handler_state = BCM_SIM_UpdateSMSCapExceededFlag;
                    }
                    KRIL_SendNotify(pdata->ril_cmd->SimId, BRCM_RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
                }
            }
        }
        break;
  
        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->msgType == MSG_SIM_EFILE_UPDATE_RSP)
            {
                SIM_EFILE_UPDATE_RESULT_t* pSimResult = (SIM_EFILE_UPDATE_RESULT_t*)capi2_rsp->dataBuf;
                KrilWriteMsgInfo_t *tdata = (KrilWriteMsgInfo_t *)pdata->ril_cmd->data;
                KrilMsgIndexInfo_t *rdata = (KrilMsgIndexInfo_t *)pdata->bcm_ril_rsp;
                rdata->result = RILErrorSIMResult(pSimResult->result);

                if(GetIsRevClass2SMS(pdata->ril_cmd->SimId) != TRUE)
                {
                    if (pSimResult->result == SIMACCESS_SUCCESS)
                    {
                        //CAPI2_SIM_SubmitLinearEFileUpdateReq() does not give rec_no
                        //back in the resp message, use the one in the original request
                        SetSMSMesgStatus(pdata->ril_cmd->SimId, (UInt8)(rdata->index), Kril_SMSMsgStatuss[tdata->MsgState]);
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
                    }
                    else
                    {
                        KRIL_DEBUG(DBG_ERROR, "pSimResult: %d\n", pSimResult->result);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    }
                    KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, FALSE);
                    KRIL_GetUpdateSMSNumber(pdata->ril_cmd->SimId);// if more update SMS in queue, need to trigger the cmd queue event to process next SMS
                }
                else // for Class 2 SMS
                {
                    if(pSimResult->result == SIMACCESS_SUCCESS)
                    {
                        KrilMsgIndexInfo_t msg;
                        msg.result = RILErrorSIMResult(pSimResult->result);
                        msg.index = rdata->index;
                        KRIL_DEBUG(DBG_ERROR, "[WRITE_SMS] Class2> msg.result:%d, msg.index:%d\n", msg.result, msg.index);
                        SetSMSMesgStatus(pdata->ril_cmd->SimId, (UInt8)(rdata->index), Kril_SMSMsgStatuss[tdata->MsgState]);
                        if (1 == tdata->MoreSMSToReceive)
                        {
                            CAPI2_SmsApi_SendAckToNetwork(InitClientInfo(pdata->ril_cmd->SimId), KRIL_GetSmsMti(pdata->ril_cmd->SimId), SMS_ACK_SUCCESS);
                            pdata->handler_state = BCM_SMS_SendAckToNetwork;
                        }
                        else
                        {
                            SetIsRevClass2SMS(pdata->ril_cmd->SimId, FALSE);
                            KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, FALSE);
                            KRIL_GetUpdateSMSNumber(pdata->ril_cmd->SimId);// if more update SMS in queue, need to trigger the cmd queue event to process next SMS
                            pdata->handler_state = BCM_FinishCAPI2Cmd;
                        }
                        KRIL_SendNotify(pdata->ril_cmd->SimId, BRCM_RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM, &msg, sizeof(KrilMsgIndexInfo_t));
                    }
                    else
                    {
                        CAPI2_SmsApi_SendAckToNetwork(InitClientInfo(pdata->ril_cmd->SimId), KRIL_GetSmsMti(pdata->ril_cmd->SimId), SMS_ACK_ERROR);
                        pdata->handler_state = BCM_SMS_SendAckToNetwork;
                    }
                }
            }
            else
            {
                KRIL_SetSMSToSIMTID(capi2_rsp->tid);
            }
        }
        break;

        case BCM_SIM_UpdateSMSCapExceededFlag:
        {
            UInt8 data[]={0xFE};
            CAPI2_SimApi_SubmitBinaryEFileUpdateReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMSS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, 1, data, 1, SIM_GetMfPathLen(), SIM_GetMfPath());
            pdata->handler_state = BCM_SMS_SendAckToNetwork;
        }
        break;

        case BCM_SMS_SendAckToNetwork:
        {
            SetIsRevClass2SMS(pdata->ril_cmd->SimId, FALSE);
            KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, FALSE);
            KRIL_GetUpdateSMSNumber(pdata->ril_cmd->SimId);// if more update SMS in queue, need to trigger the cmd queue event to process next SMS
            pdata->handler_state = BCM_FinishCAPI2Cmd;
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_DeleteSMSOnSIMHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
 
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
 
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, FALSE);
            KRIL_GetUpdateSMSNumber(pdata->ril_cmd->SimId);// if more update SMS in queue, need to trigger the cmd queue event to process next SMS body
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }
 
    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            int *index = (int *)(pdata->ril_cmd->data);

            //for CIB version, message are written to SIM, which
            //includes status (first byte) + pdu
            UInt8 szMessage[SMSMESG_DATA_SZ+1];
            memset(szMessage,SIM_RAW_EMPTY_VALUE, SMSMESG_DATA_SZ+1);
            szMessage[0] = SIMSMSMESGSTATUS_FREE;

            KRIL_DEBUG(DBG_ERROR, "[DELEVE_SMS] index:%d\n", *index);
            KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, TRUE);

            //CAPI2_SIM_SubmitLinearEFileUpdateReq() does not give rec_no
            //back in the resp message, we borrow bcm_ril_resp here
            //to save index info. Will need to delete this during finish up
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilMsgIndexInfo_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;                
            }
            else
            {
                memset(pdata->bcm_ril_rsp, 0, sizeof(KrilMsgIndexInfo_t));
                ((KrilMsgIndexInfo_t*)pdata->bcm_ril_rsp)->index = *index;    
                //CAPI2_SimApi_SubmitLinearEFileUpdateReq() rec_no is 1 based
                CAPI2_SimApi_SubmitLinearEFileUpdateReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, *index, szMessage, (SMSMESG_DATA_SZ+1), SIM_GetMfPathLen(), SIM_GetMfPath());
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;
 
        case BCM_RESPCAPI2Cmd:
        {
            if ( capi2_rsp->msgType == MSG_SIM_EFILE_UPDATE_RSP)
            {
                SIM_EFILE_UPDATE_RESULT_t* pSimResult = (SIM_EFILE_UPDATE_RESULT_t*)capi2_rsp->dataBuf;
                if (pSimResult->result == SIMACCESS_SUCCESS)
                {
                    //CAPI2_SIM_SubmitLinearEFileUpdateReq() does not give rec_no
                    //back in the resp message, use the one in the original request
                    SetSMSMesgStatus(pdata->ril_cmd->SimId, (UInt8)(((KrilMsgIndexInfo_t*)pdata->bcm_ril_rsp)->index), SIMSMSMESGSTATUS_FREE);
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR, "pSimResult: %d\n", pSimResult->result);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                //since we just borrowed pdata->bcm_ril_rsp for saving
                //index info, we don't need it anymore.
                kfree(pdata->bcm_ril_rsp);
                pdata->bcm_ril_rsp = NULL;
                KRIL_SetInUpdateSMSInSIMHandler(pdata->ril_cmd->SimId, FALSE);
                KRIL_GetUpdateSMSNumber(pdata->ril_cmd->SimId);// if more update SMS in queue, need to trigger the cmd queue event to process next SMS
             }
             else
             {
                 KRIL_SetSMSToSIMTID(capi2_rsp->tid);
             }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_SMSAcknowledgeHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilMsgAckInfo_t *tdata = (KrilMsgAckInfo_t *)pdata->ril_cmd->data;
            SmsAckNetworkType_t SmsAckNetworkType;

            KRIL_DEBUG(DBG_INFO, "AckType:0x%x FailCause:%d\n", tdata->AckType, tdata->FailCause);
            if (1 == tdata->AckType)
            {
                SmsAckNetworkType = SMS_ACK_SUCCESS;
            }
            else
            {
                if (0xd3 == tdata->FailCause)
                {
                    SmsAckNetworkType = SMS_ACK_MEM_EXCEEDED;
                }
                else if (0xd0 == tdata->FailCause || 0xd1 == tdata->FailCause)
                {
                    SmsAckNetworkType = SMS_ACK_PROTOCOL_ERROR;
                }
                else
                {
                    SmsAckNetworkType = SMS_ACK_ERROR;
                }
            }
            CAPI2_SmsApi_SendAckToNetwork(InitClientInfo(pdata->ril_cmd->SimId), KRIL_GetSmsMti(pdata->ril_cmd->SimId), SmsAckNetworkType);
            pdata->handler_state = BCM_SIM_UpdateSMSCapExceededFlag;
        }
        break;

        case BCM_SIM_UpdateSMSCapExceededFlag:
        {
            KrilMsgAckInfo_t *tdata = (KrilMsgAckInfo_t *)pdata->ril_cmd->data;
            if (0xd3 == tdata->FailCause)
            {
                UInt8 data[]={0xFE}; // SMS full
                CAPI2_SimApi_SubmitBinaryEFileUpdateReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMSS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, 1, data, 1, SIM_GetMfPathLen(), SIM_GetMfPath());
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else
            {
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            pdata->handler_state = BCM_FinishCAPI2Cmd;
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_GetSMSCAddressHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
 
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
 
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }
 
    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            pdata->bcm_ril_rsp = kmalloc(sizeof(krilGetSMSCAddress_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {    
                pdata->rsp_len = sizeof(krilGetSMSCAddress_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                CAPI2_SmsApi_GetSMSrvCenterNumber(InitClientInfo(pdata->ril_cmd->SimId), USE_DEFAULT_SCA_NUMBER);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            break;
        }
 
        case BCM_RESPCAPI2Cmd:
        {
            SmsAddress_t *rsp = (SmsAddress_t *)capi2_rsp->dataBuf;
            krilGetSMSCAddress_t *rdata = (krilGetSMSCAddress_t *)pdata->bcm_ril_rsp;
			char pdu[SMS_MAX_DIGITS+1];
			int len, i, index;

			//Byte#0 - address length (in bytes)
			len = strlen(rsp->Number);
			if (len%2 == 0){
				len = len/2 +1;
			} else{
				len = len/2 +2;
			}
			pdu[0]= len;

 	        //Byte#1 - address type
	        if(145 == rsp->TypeOfAddress){
				pdu[1]=0x91;
			} else{
				pdu[1]=0x81;
			}
 	        
			//Byte#2 - Address in BCD format (convert string address to BCD)
			ParseSmscStr2BCD(&(pdu[2]),(char *)rsp->Number,strlen(rsp->Number));
			
			index = 0;
			for(i = 0; i < len+1; i++){
				sprintf((char *)&rdata->smsc[(index++)*2],"%02X",(unsigned char)pdu[i]);
			}

            KRIL_DEBUG(DBG_ERROR, "BCM_RESPCAPI2Cmd:: smsc:%s TypeOfAddress:%d\n", rdata->smsc, rsp->TypeOfAddress);
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_SetSMSCAddressHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
  
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }
  
    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilSetSMSCAddress_t *tdata = (KrilSetSMSCAddress_t *)pdata->ril_cmd->data;
            SmsAddress_t psca;
            strcpy(psca.Number, tdata->smsc);
            if('+' == tdata->smsc[0])
                psca.TypeOfAddress = 145;
            else
                psca.TypeOfAddress = 129;

            KRIL_DEBUG(DBG_INFO, "TypeOfAddress:%d Number:%s...!\n", psca.TypeOfAddress, psca.Number);
            CAPI2_SmsApi_SendSMSSrvCenterNumberUpdateReq(InitClientInfo(pdata->ril_cmd->SimId), &psca, USE_DEFAULT_SCA_NUMBER);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }
  
        case BCM_RESPCAPI2Cmd:
        {
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_ReportSMSMemoryStatusHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            int available = *((int *)pdata->ril_cmd->data);
            if (available != KRIL_GetMESMSAvailable())
            {
                KRIL_SetMESMSAvailable(available);
                pdata->handler_state = BCM_FinishCAPI2Cmd;
                if (TRUE == available) // if memory is available, need to send a indication to network and update the EF-SMSS status in SIM.
                {
                    CAPI2_SimApi_SubmitBinaryEFileReadReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMSS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, 1, 1, SIM_GetMfPathLen(), SIM_GetMfPath());
                    pdata->handler_state = BCM_SIM_UpdateSMSCapExceededFlag;
                }
            }
            else
            {
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            break;
        }

        case BCM_SIM_UpdateSMSCapExceededFlag:
        {
            SIM_EFILE_DATA_t *rsp = (SIM_EFILE_DATA_t *)capi2_rsp->dataBuf;
            if (rsp->result != SIMACCESS_SUCCESS)  // read SMS status error
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
	            if (0xFE == *(rsp->ptr))  // SMS full
	            {
		            UInt8 data[]={0xFF};
		            CAPI2_SimApi_SubmitBinaryEFileUpdateReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMSS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, 1, data, 1, SIM_GetMfPathLen(), SIM_GetMfPath());
		            pdata->handler_state = BCM_SMS_SendMemAvailInd;
	            }
	            else
	            {
	                pdata->handler_state = BCM_FinishCAPI2Cmd;
	            }
            }
            break;
        }

        case BCM_SMS_SendMemAvailInd:
        {
            SIM_EFILE_UPDATE_RESULT_t *rsp = (SIM_EFILE_UPDATE_RESULT_t *)capi2_rsp->dataBuf;
            KRIL_DEBUG(DBG_INFO, "Update EF-SMSS result:%d\n", rsp->result);
            CAPI2_SmsApi_SendMemAvailInd(InitClientInfo(pdata->ril_cmd->SimId));

            if (CheckFreeSMSIndex(pdata->ril_cmd->SimId) == SMS_FULL)
            {
                KRIL_SendNotify(pdata->ril_cmd->SimId, BRCM_RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
            }
            else
            {
                KRIL_SendNotify(pdata->ril_cmd->SimId, RIL_UNSOL_SIM_SMS_STORAGE_AVAILALE, NULL, 0);
            }
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }

        case BCM_RESPCAPI2Cmd:
        {
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_GetBroadcastSmsHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);

    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilGsmBroadcastGetSmsConfigInfo_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {    
                pdata->rsp_len = sizeof(KrilGsmBroadcastGetSmsConfigInfo_t);
                CAPI2_SmsApi_GetCBMI(InitClientInfo(pdata->ril_cmd->SimId));
                pdata->handler_state = BCM_SMS_GetCBMI;
            }
            break;
        }

        case BCM_SMS_GetCBMI:
        {
            UInt8 i;
            KrilGsmBroadcastGetSmsConfigInfo_t *rdata = (KrilGsmBroadcastGetSmsConfigInfo_t *)pdata->bcm_ril_rsp;
            SMS_CB_MSG_IDS_t *rsp = (SMS_CB_MSG_IDS_t *)capi2_rsp->dataBuf;

            if(capi2_rsp->result != RESULT_OK)
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
            pdata->rsp_len = rsp->nbr_of_msg_id_ranges * sizeof(iKrilGetCBSMSConfigInfo_t);

            for(i = 0 ; i < rsp->nbr_of_msg_id_ranges ; i++)
            {
                rdata->content[i].fromServiceId = rsp->msg_id_range_list.A[i].start_pos;
                rdata->content[i].toServiceId = rsp->msg_id_range_list.A[i].stop_pos;
                rdata->content[i].selected = 1;
                KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "start_pos:0x%x stop_pos:0x%x\n", rsp->msg_id_range_list.A[i].start_pos, rsp->msg_id_range_list.A[i].stop_pos);
            }

            KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "nbr_of_msg_id_ranges:%d\n",rsp->nbr_of_msg_id_ranges);
            *(UInt8*)(pdata->cmdContext) = rsp->nbr_of_msg_id_ranges;

            CAPI2_SmsApi_GetCbLanguage(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }

        case BCM_RESPCAPI2Cmd:
        {
            UInt8 i,j;
            UInt8 k = 0;
            UInt8 record = 0;
            SMS_CB_MSG_ID_RANGE_LIST_t map_CodeScheme;

            if(capi2_rsp->result != RESULT_OK )
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            else
            {
                KrilGsmBroadcastGetSmsConfigInfo_t *rdata = (KrilGsmBroadcastGetSmsConfigInfo_t *)pdata->bcm_ril_rsp;
                MS_T_MN_CB_LANGUAGES* rsp = (MS_T_MN_CB_LANGUAGES *)capi2_rsp->dataBuf;

                if (0 < rsp->nbr_of_languages)
                {
                    if (1 == rsp->nbr_of_languages) //only one set of code scheme for each set of language pairs
                    {
                        for (i = 0; i < *(UInt8*)(pdata->cmdContext); i++)
                        {
                            rdata->content[i].fromCodeScheme = rsp->language_list.A[0];
                            rdata->content[i].toCodeScheme = rsp->language_list.A[rsp->nbr_of_languages - 1];
                            KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "fromCodeScheme:0x%x toCodeScheme:0x%x\n", rsp->language_list.A[0], rsp->language_list.A[rsp->nbr_of_languages - 1]);
                        }
                    }
                    else
                    {
                        for (j = 0; j < rsp->nbr_of_languages; j++)
                        {
                            if ((0 == j)||(k != record)) //this item of code scheme is a starting number of a new set
                            {
                                map_CodeScheme.A[k].start_pos = rsp->language_list.A[j];
                            }
                            if ((rsp->nbr_of_languages - 1) == j) //for the last item of code scheme list
                            {
                                map_CodeScheme.A[k].stop_pos = rsp->language_list.A[j];
                                break;
                            }
                            if (rsp->language_list.A[j+1] == (rsp->language_list.A[j] + 1)) //this item is a continuous number from previous one
                            {
                                map_CodeScheme.A[k].stop_pos = rsp->language_list.A[j];
                                record = k;
                            }
                            else if (rsp->language_list.A[j+1] != (rsp->language_list.A[j] + 1)) //the stop of this code scheme pair
                            {
                                map_CodeScheme.A[k].stop_pos = rsp->language_list.A[j];
                                k++;
                            }
                        }

                        if ((k+1) != *(UInt8*)(pdata->cmdContext)) //just for check
                        {
                            KRIL_DEBUG(DBG_ERROR, "Nbr of msg id:%d, Nbr of code scheme:%d\n", *(UInt8*)(pdata->cmdContext), k+1);
                        }

                        for (i = 0; i < *(UInt8*)(pdata->cmdContext); i++)
                        {
                            j = i;
                            if (k < i) //number of code scheme is smaller than number of message id 
                                j = k;
                            rdata->content[i].fromCodeScheme = map_CodeScheme.A[j].start_pos;
                            rdata->content[i].toCodeScheme = map_CodeScheme.A[j].stop_pos;
                            KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "fromCodeScheme:0x%x toCodeScheme:0x%x\n", rdata->content[i].fromCodeScheme, rdata->content[i].toCodeScheme);
                        }
                    }
                }
                KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "nbr_of_languages:%d\n", rsp->nbr_of_languages);
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            break;
        }

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_SetBroadcastSmsHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);

    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilGsmBroadcastSmsConfigInfo_t *tdata = (KrilGsmBroadcastSmsConfigInfo_t *)pdata->ril_cmd->data;
            UInt8* pchnlIDs;
            UInt8* pcodings;
            UInt8  mode;            
            
            mode = tdata->selected;
            memset(schnlIDs, 0, CHNL_IDS_SIZE);
            if(strlen(tdata->mids) != 0) {         
                pchnlIDs = tdata->mids;
                memcpy(schnlIDs, pchnlIDs, CHNL_IDS_SIZE);                
            }
            else{                
                pchnlIDs = NULL;
            }

            memset(scodings, 0, CHNL_IDS_SIZE);
            if(strlen(tdata->dcss) != 0) {         
                pcodings = tdata->dcss;
                memcpy(scodings, pcodings, CHNL_IDS_SIZE);                
            }
            else{
                pcodings = NULL;                
            }

            KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "mode:%d\n", mode);
            if (NULL != pchnlIDs){
                KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "mid:%s\n", schnlIDs);
            }
            if (NULL != pcodings){
                KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "dcs:%s\n", scodings);            
            }
            CAPI2_SmsApi_SetCellBroadcastMsgTypeReq(InitClientInfo(pdata->ril_cmd->SimId), mode, pchnlIDs, pcodings);               
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }

        case BCM_RESPCAPI2Cmd:
        {
            if(capi2_rsp->result != RESULT_OK )
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_SmsBroadcastActivationHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);

    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            UInt8 mode = *((UInt8 *)pdata->ril_cmd->data);
            KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "mode:%d \n", mode);

            if (0 == mode) //Active
            {
                CAPI2_SmsApi_SetCellBroadcastMsgTypeReq(InitClientInfo(pdata->ril_cmd->SimId), mode, schnlIDs, scodings);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else //turn-off
            {
                CAPI2_SmsApi_SetCellBroadcastMsgTypeReq(InitClientInfo(pdata->ril_cmd->SimId), mode, NULL, NULL);
                pdata->handler_state = BCM_SMS_SetCBOff;
            }
            break;
        }

        case BCM_SMS_SetCBOff:
        {
            if(capi2_rsp->result != RESULT_OK )
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            KRIL_DEBUG(DBG_ERROR/*DBG_INFO*/, "BCM_SMS_SetCBOff: StopReceivingCellBroadcastReq\n");
            CAPI2_SmsApi_StopReceivingCellBroadcastReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }

        case BCM_RESPCAPI2Cmd:
        {
            if(capi2_rsp->result != RESULT_OK )
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}


void KRIL_QuerySMSInSIMHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            CAPI2_SimApi_SubmitEFileInfoReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, SIM_GetMfPathLen(), SIM_GetMfPath());
            pdata->handler_state = BCM_SIM_SubmitRecordEFileReadReq;
        }
        break;

        case BCM_SIM_SubmitRecordEFileReadReq:
        {
            SIM_EFILE_INFO_t *rsp = (SIM_EFILE_INFO_t *) capi2_rsp->dataBuf;
            if (SIMACCESS_SUCCESS == rsp->result)
            {
                *(UInt8*)(pdata->cmdContext) = 1;
                KRIL_SetTotalSMSInSIM(pdata->ril_cmd->SimId, rsp->file_size/rsp->record_length);
                KRIL_DEBUG(DBG_INFO, "KRIL_GetTotalSMSInSIM:%d rec_no:%d\n", KRIL_GetTotalSMSInSIM(pdata->ril_cmd->SimId), *(UInt8*)(pdata->cmdContext));
                CAPI2_SimApi_SubmitRecordEFileReadReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, *(UInt8*)(pdata->cmdContext), SMSMESG_DATA_SZ+1, SIM_GetMfPathLen(), SIM_GetMfPath());
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            SIM_EFILE_DATA_t *rsp = (SIM_EFILE_DATA_t *) capi2_rsp->dataBuf;
            if (SIMACCESS_SUCCESS == rsp->result)
            {
                SetSMSMesgStatus(pdata->ril_cmd->SimId, *(UInt8*)(pdata->cmdContext), *rsp->ptr);
                KRIL_DEBUG(DBG_INFO, "KRIL_GetTotalSMSInSIM:%d GetSMSMesgStatus[%d]:%d\n", KRIL_GetTotalSMSInSIM(pdata->ril_cmd->SimId), *(UInt8 *)pdata->cmdContext, GetSMSMesgStatus(pdata->ril_cmd->SimId, *(UInt8 *)pdata->cmdContext));
                (*(UInt8 *)pdata->cmdContext)++;
                if((*(UInt8 *)pdata->cmdContext) <= KRIL_GetTotalSMSInSIM(pdata->ril_cmd->SimId))
                {
                    CAPI2_SimApi_SubmitRecordEFileReadReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, *(UInt8*)(pdata->cmdContext), SMSMESG_DATA_SZ+1, SIM_GetMfPathLen(), SIM_GetMfPath());
                }
                else
                {
                    if (SMS_FULL == CheckFreeSMSIndex(pdata->ril_cmd->SimId))
                    {
                        KRIL_SendNotify(pdata->ril_cmd->SimId, BRCM_RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
                    }
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_GetSmsSimMaxCapacityHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    KrilSimSmsCapacity_t* pCapacity;
    UInt8 totalSlots;
    UInt8 usedSlots;
    UInt8 index;

    totalSlots = KRIL_GetTotalSMSInSIM(pdata->ril_cmd->SimId);
    usedSlots = 0;

    for (index = 1; index <= totalSlots; index++)
    {
        if (GetSMSMesgStatus(pdata->ril_cmd->SimId, index) != SIMSMSMESGSTATUS_FREE)
        {
            usedSlots++;
        }
    }

    pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimSmsCapacity_t), GFP_KERNEL);
    if(!pdata->bcm_ril_rsp) {
        KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
        pdata->handler_state = BCM_ErrorCAPI2Cmd;             
    }
    else
    {
        pCapacity = pdata->bcm_ril_rsp;
        pCapacity->totalSlots = totalSlots;
        pCapacity->usedSlots = usedSlots;    
        pdata->rsp_len = sizeof(KrilSimSmsCapacity_t);
        pdata->handler_state = BCM_FinishCAPI2Cmd;
    }
}

// For URILC only
void KRIL_ReadSmsOnSimHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            int recNum = *(int *)pdata->ril_cmd->data;
            recNum++;   // 1-based index
            CAPI2_SimApi_SubmitRecordEFileReadReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, recNum, SMSMESG_DATA_SZ+1, SIM_GetMfPathLen(), SIM_GetMfPath());
            
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            SIM_EFILE_DATA_t *rsp = (SIM_EFILE_DATA_t *) capi2_rsp->dataBuf;
            if (SIMACCESS_SUCCESS == rsp->result)
            {
                int recNum = *(int *)pdata->ril_cmd->data;
                KrilReadMsgRsp_t* pMsgContents = NULL;
                SIMSMSMesgStatus_t msgStatus;

                pdata->bcm_ril_rsp = kmalloc(sizeof(KrilReadMsgRsp_t), GFP_KERNEL);
                if(!pdata->bcm_ril_rsp) {
                    KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;             
                    return;
                }
                pdata->rsp_len = sizeof(KrilReadMsgRsp_t);
                pMsgContents = (KrilReadMsgRsp_t*)pdata->bcm_ril_rsp;
                memset(pMsgContents->mesg_data, SIM_RAW_EMPTY_VALUE, BCM_SMSMSG_SZ); // Fill the 0xFF in the struct
                
                // Get the status byte, which is the first byte of the array
                msgStatus = (SIMSMSMesgStatus_t)(*(rsp->ptr));
                memcpy( pMsgContents->mesg_data, rsp->ptr, BCM_SMSMSG_SZ );

                // If the message is unread, mark it as read now.
                if (msgStatus == SIMSMSMESGSTATUS_UNREAD)
                {
                    KRIL_DEBUG(DBG_INFO, "msg[%d] marked as read\n", recNum);
                    msgStatus = SIMSMSMESGSTATUS_READ;
                    SetSMSMesgStatus(pdata->ril_cmd->SimId, recNum, msgStatus);
                }
                pMsgContents->recNum = recNum;
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_INFO, "error result: %d\n", rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_ListSmsOnSimHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            int stat = *(int *)pdata->ril_cmd->data;
            UInt8 recNum = 0;   // 1-based record number to start listing

            KRIL_DEBUG(DBG_INFO, "stat:%d\n", stat);
            recNum = GetNextValidRec(pdata->ril_cmd->SimId, 0, stat);
            if (recNum > 0)
            {
                UInt8* pSavedRecIndex = (UInt8 *)(pdata->cmdContext);
                *pSavedRecIndex = recNum - 1;
                CAPI2_SimApi_SubmitRecordEFileReadReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, recNum, SMSMESG_DATA_SZ+1, SIM_GetMfPathLen(), SIM_GetMfPath());
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_INFO, "No SIM records match status %d\n", stat);
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            SIM_EFILE_DATA_t *rsp = (SIM_EFILE_DATA_t *) capi2_rsp->dataBuf;
            if (SIMACCESS_SUCCESS == rsp->result)
            {
                UInt8* pSavedRecIndex = (UInt8 *)(pdata->cmdContext);         
                KrilReadMsgRsp_t msgContents;
                UInt8 recIndex = *pSavedRecIndex;  // 0-based record number
                UInt8 simRecords = KRIL_GetTotalSMSInSIM(pdata->ril_cmd->SimId);
                int stat = *(int *)pdata->ril_cmd->data;
                SIMSMSMesgStatus_t msgStatus;

                // Get the status byte, which is the first byte of the array
                msgStatus = (SIMSMSMesgStatus_t)(*(rsp->ptr));
              
                if (msgStatus != SIMSMSMESGSTATUS_FREE)
                {
                    // Allocate the response data
                    memset(msgContents.mesg_data, SIM_RAW_EMPTY_VALUE, BCM_SMSMSG_SZ); // Fill the 0xFF in the struct
                    memcpy(msgContents.mesg_data, rsp->ptr, rsp->data_len);
                    msgContents.recNum = recIndex;

                    KRIL_SendNotify(pdata->ril_cmd->SimId, URILC_UNSOL_RESP_LIST_SMS_ON_SIM, &msgContents, sizeof(KrilReadMsgRsp_t));
                    
                    // If the message is unread, mark it as read now.
                    if (msgStatus == SIMSMSMESGSTATUS_UNREAD)
                    {
                        KRIL_DEBUG(DBG_INFO, "msg[%d] marked as read\n", recIndex);
                        msgStatus = SIMSMSMESGSTATUS_READ;
                        SetSMSMesgStatus(pdata->ril_cmd->SimId, recIndex, msgStatus);
                    }
                }
                else
                {
                    KRIL_DEBUG(DBG_INFO, "skipping empty record %d\n", recIndex);
                }

                recIndex++;
                if (recIndex < simRecords)
                {
                    // Search for next valid record
                    UInt8 recNum;

                    recNum = GetNextValidRec(pdata->ril_cmd->SimId, recIndex, stat);
                    if (recNum > 0)
                    {
                        *pSavedRecIndex = recNum - 1;
                    
                        KRIL_DEBUG(DBG_INFO, "Request SMS recNum %d\n", recNum);

                        CAPI2_SimApi_SubmitRecordEFileReadReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, recNum, SMSMESG_DATA_SZ+1, SIM_GetMfPathLen(), SIM_GetMfPath());
                        pdata->handler_state = BCM_RESPCAPI2Cmd;
                    }
                    else
                    {
                        // Done
                        KRIL_DEBUG(DBG_INFO, "No more SIM records match status %d\n", stat);
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
                    }
                }
                else
                {
                    // Done
                    KRIL_DEBUG(DBG_INFO, "No more SIM records match\n");
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else
            {
                KRIL_DEBUG(DBG_INFO, "error result: %d\n", rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_SendStoredSmsHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            int recNum = *(int *)pdata->ril_cmd->data;
            recNum++;   // 1-based index

            KRIL_DEBUG(DBG_INFO, "sending msg at %d\n", recNum);

            CAPI2_SimApi_SubmitRecordEFileReadReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID, APDUFILEID_EF_SMS, (KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_2G)?APDUFILEID_DF_TELECOM : APDUFILEID_USIM_ADF, recNum, SMSMESG_DATA_SZ+1, SIM_GetMfPathLen(), SIM_GetMfPath());
            pdata->handler_state = BCM_SIM_SubmitRecordEFileReadReq;
        }
        break;

        case BCM_SIM_SubmitRecordEFileReadReq:
        {
            SIM_EFILE_DATA_t *rsp = (SIM_EFILE_DATA_t *) capi2_rsp->dataBuf;
            if (SIMACCESS_SUCCESS == rsp->result)
            {
                UInt8 *pszMsg = (rsp->ptr + 1); // skip the status byte
                UInt8 *pdu;
                UInt8 scaLen;
                int pduLen;

                // Skip the SCA
                scaLen = ((Sms_411Addr_t *)pszMsg)->Len;
                pdu = (UInt8*)(&pszMsg[scaLen+1]);
                pduLen = SMSMESG_DATA_SZ-scaLen;

                CAPI2_SmsApi_SendSMSPduReq(InitClientInfo(pdata->ril_cmd->SimId), pduLen, pdu, NULL);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_INFO, "error result: %d\n", rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            SmsSubmitRspMsg_t* rsp = (SmsSubmitRspMsg_t*) capi2_rsp->dataBuf;
            KrilSendSMSResponse_t *rdata;
            KRIL_DEBUG(DBG_INFO, "InternalErrCause:%d NetworkErrCause:0x%x submitRspType:%d tpMr:%d\n", rsp->InternalErrCause, rsp->NetworkErrCause, rsp->submitRspType, rsp->tpMr);

            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSendSMSResponse_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
                return;
            }
            pdata->rsp_len = sizeof(KrilSendSMSResponse_t);
            memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
            rdata = (KrilSendSMSResponse_t*)pdata->bcm_ril_rsp;
            
#ifdef CONFIG_BRCM_FUSE_RIL_CIB
            // enum renamed in CIB
            // **FIXME** MAG - check how this will affect user space... (if at all)
            if(MS_MN_SMS_NO_ERROR == rsp->NetworkErrCause)
#else
            if(MN_SMS_NO_ERROR == rsp->NetworkErrCause)
#endif
            {
                if (rsp->InternalErrCause != RESULT_OK)
                {
                    pdata->result = RILErrorResult(rsp->InternalErrCause);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    rdata->messageRef = rsp->tpMr;
                    rdata->errorCode = rsp->NetworkErrCause;
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else
            {
                if (rsp->InternalErrCause != RESULT_OK)
                {
                    pdata->result = RILErrorResult(rsp->InternalErrCause);
                }
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_GetElemCscsHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
	
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_LOCAL_PHCTRL_ELEM_CSCS);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*) capi2_rsp->dataBuf;

            pdata->rsp_len = sizeof(UInt8)*10;
            pdata->bcm_ril_rsp = kmalloc(pdata->rsp_len, GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                strncpy(pdata->bcm_ril_rsp, rsp->data_u.u10Bytes, 10);    
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_GetElemMoreMsgToSendHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_LOCAL_SMS_ELEM_MORE_MESSAGE_TO_SEND);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*) capi2_rsp->dataBuf;
            UInt8* pUInt8Rsp;

            pdata->rsp_len = sizeof(UInt8);
            pdata->bcm_ril_rsp = kmalloc(pdata->rsp_len, GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pUInt8Rsp = (UInt8*)pdata->bcm_ril_rsp;
                *pUInt8Rsp = rsp->data_u.u8Data;    
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_SetElemMoreMsgToSendHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            CAPI2_MS_Element_t data;
            UInt8 *tdata = (UInt8 *)pdata->ril_cmd->data;

            memset((UInt8*)&data, 0x00, sizeof(CAPI2_MS_Element_t));
            data.inElemType = MS_LOCAL_SMS_ELEM_MORE_MESSAGE_TO_SEND;
            data.data_u.u8Data = *tdata;

            CAPI2_MsDbApi_SetElement(InitClientInfo(pdata->ril_cmd->SimId), &data);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            pdata->handler_state = BCM_FinishCAPI2Cmd;
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_StartMultiSmsTxHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            CAPI2_SmsApi_StartMultiSmsTransferReq( InitClientInfo(pdata->ril_cmd->SimId) );
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            pdata->handler_state = BCM_FinishCAPI2Cmd;
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

// For URILC only
void KRIL_StopMultiSmsTxHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    
    if (capi2_rsp != NULL)
    {
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::result:%d\n", pdata->handler_state, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            CAPI2_SmsApi_StopMultiSmsTransferReq( InitClientInfo(pdata->ril_cmd->SimId) );
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            pdata->handler_state = BCM_FinishCAPI2Cmd;
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}

