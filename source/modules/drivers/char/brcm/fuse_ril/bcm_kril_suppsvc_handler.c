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

extern CallIndex_t gUssdID[DUAL_SIM_SIZE];
extern CallIndex_t gPreviousUssdID[DUAL_SIM_SIZE];
extern UInt32      gDialogID; // To use the same dialogID in the same USSD session

typedef struct
{
    UInt8                         *name;
    SS_CallBarType_t   type;
} CBfacid_t;


//
// Call barring types array
//
SS_CallBarType_t ssBarringTypes[] =
{
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_OUT_ALL,				///< All outgoing 
  SS_CALLBAR_TYPE_OUT_INTL	,			///< Outgoing international
  SS_CALLBAR_TYPE_OUT_INTL_EXCL_HPLMN,	///< Outgoing international excluding HomePLMN (home country)
  SS_CALLBAR_TYPE_INC_ALL,				///< All incoming
  SS_CALLBAR_TYPE_INC_ROAM_OUTSIDE_HPLMN,	///< All incoming when roaming outside HomePLMN (home country)
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_NOTSPECIFIED,
  SS_CALLBAR_TYPE_ALL,					///< All calls(incoming and outgoing)
  SS_CALLBAR_TYPE_OUTG,					///< Outgoing
  SS_CALLBAR_TYPE_INC						///< Incoming	  
};

SS_Code_t ssBarringCodes[] =
{
  SS_CODE_ALL_SS,
  SS_CODE_ALL_SS,
  SS_CODE_ALL_SS,
  SS_CODE_ALL_SS,
  SS_CODE_BAOC,				///< All outgoing
  SS_CODE_BOIC	,			///< Outgoing international
  SS_CODE_BOIC_EXC_PLMN,	///< Outgoing international excluding HomePLMN (home country)
  SS_CODE_BAIC,				///< All incoming
  SS_CODE_BAIC_ROAM,	///< All incoming when roaming outside HomePLMN (home country)
  SS_CODE_ALL_SS,
  SS_CODE_ALL_SS,
  SS_CODE_ALL_SS,
  SS_CODE_ALL_SS,
  SS_CODE_ALL_CALL_RESTRICTION,	///< All calls(incoming and outgoing)
  SS_CODE_BOC,					///< Outgoing
  SS_CODE_BIC					///< Incoming
};

static const SS_Code_t Kril_FwdReasonCode[] =
{
    SS_CODE_CFU,
    SS_CODE_CFB,
    SS_CODE_CFNRY,
    SS_CODE_CFNRC,
    SS_CODE_ALL_FORWARDING,
    SS_CODE_ACF,
};

static const SS_Operation_t Kril_FwdModeToOp[] =
{
    SS_OPERATION_CODE_DEACTIVATE,
    SS_OPERATION_CODE_ACTIVATE,
    SS_OPERATION_CODE_INTERROGATE,
    SS_OPERATION_CODE_REGISTER,
    SS_OPERATION_CODE_ERASE,
};

static const SS_Mode_t Kril_FwdMode[] =
{
    SS_MODE_DISABLE,
    SS_MODE_ENABLE,
    SS_MODE_INTERROGATE,
    SS_MODE_REGISTER,
    SS_MODE_ERASE,
};

/* ATC Service Class values defined in GSM 07.07 */
typedef enum
{
	ATC_NOT_SPECIFIED = 0,
	ATC_VOICE = 1,
	ATC_DATA = 2,
	ATC_FAX = 4,
	ATC_SMS = 8,
	ATC_DATA_CIRCUIT_SYNC = 16,
	ATC_DATA_CIRCUIT_ASYNC = 32,
	ATC_DPA = 64,
	ATC_DPAD = 128,
} ATC_CLASS_t;

SS_SvcCls_t GetServiceClass (int InfoClass)
{
    SS_SvcCls_t  svcCls = SS_SVCCLS_UNKNOWN;

    switch(InfoClass)
    {
        case 1:
            svcCls = SS_SVCCLS_SPEECH;
        break;

        case 2:
            svcCls = SS_SVCCLS_DATA;
        break;

        case 4:
            svcCls = SS_SVCCLS_FAX;
        break;

        case 5:
            svcCls = SS_SVCCLS_ALL_TELE_SERVICES;
        break;

        case 8:
            svcCls = SS_SVCCLS_SMS;
        break;

        case 10:
            svcCls = SS_SVCCLS_ALL_SYNC_SERVICES;
        break;

        case 11:
            svcCls = SS_SVCCLS_ALL_BEARER_SERVICES;
        break;

        case 13: // need to check
            svcCls = SS_SVCCLS_ALL_TELE_SERVICES;
        break;

        case 16:
            svcCls = SS_SVCCLS_DATA_CIRCUIT_SYNC;
        break;

        case 32:
            svcCls = SS_SVCCLS_DATA_CIRCUIT_ASYNC;
        break;

        case 64:
            svcCls = SS_SVCCLS_DEDICATED_PACKET;
        break;

        case 80:
            svcCls = SS_SVCCLS_ALL_SYNC_SERVICES;
        break;

        case 128:
            svcCls = SS_SVCCLS_DEDICATED_PAD;
        break;

        case 135 : 
            svcCls = SS_SVCCLS_ALL_TELESERVICE_EXCEPT_SMS;
        break;

        case 160:
            svcCls = SS_SVCCLS_ALL_ASYNC_SERVICES ;
        break;

        case 240:
            svcCls = SS_SVCCLS_ALL_BEARER_SERVICES;
        break;

        default:
            svcCls = SS_SVCCLS_NOTSPECIFIED;
    }
    return svcCls;
}

void SS_SvcCls2BasicSrvGroup(int                 inInfoClass,
							BasicSrvGroup_t*	inBasicSrvPtr)
{
    SS_SvcCls_t svcCls = GetServiceClass(inInfoClass);
	switch (svcCls)
	{
		case SS_SVCCLS_NOTSPECIFIED:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_UNSPECIFIED;
			break;

		case SS_SVCCLS_SPEECH:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_SPEECH_TRANSMISSION_SERVICES;
			break;

		case SS_SVCCLS_ALT_SPEECH:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_AUXILIARY_TELEPHONY;
			break;

		case SS_SVCCLS_DATA://?
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_DATA_TELE_SERVICES;
			break;

		case SS_SVCCLS_FAX:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_FACIMILE_TRANSMISSION_SERVICES;
			break;

		case SS_SVCCLS_ALL_TELESERVICE_EXCEPT_SMS:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_TELE_SRV_EXCEPT_SMS;
			break;

		case SS_SVCCLS_SMS:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_SHORT_MESSAGE_SERVICES;
			break;

		case SS_SVCCLS_DATA_CIRCUIT_SYNC:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_DATA_CIRCUIT_SYNCH;
			break;

		case SS_SVCCLS_DATA_CIRCUIT_ASYNC:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_DATA_CIRCUIT_ASYNCH;
			break;

		case SS_SVCCLS_DEDICATED_PACKET: //Abdi Check this convertion
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_DATA_CDA_SERVICES;
			break;

		case SS_SVCCLS_ALL_SYNC_SERVICES:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_SYNCHRONOUS_SERVICES;
			break;

		case SS_SVCCLS_DEDICATED_PAD: //Abdi Check this convertion
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_PAD_ACCESS_CA_SERVICES;
			break;

		case SS_SVCCLS_ALL_ASYNC_SERVICES:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_ASYNCHRONOUS_SERVICES;
			break;

		case SS_SVCCLS_ALL_BEARER_SERVICES:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_BEARER_SERVICES;
			break;

		case SS_SVCCLS_ALL_TELE_SERVICES:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_TELE_SERVICES;
			break;

		case SS_SVCCLS_PLMN_SPEC_TELE_SVC:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_TELE_SERVICES;
			*(TeleSrv_t*)&inBasicSrvPtr->content = TELE_SRV_ALL_PLMN_SPECIFIC_SERVICES;
			break;

		case SS_SVCCLS_SPEECH_FOLLOW_BY_DATA_CDA:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_SPEECH_FOLLOWED_DATA_CDA;
			break;

		case SS_SVCCLS_SPEECH_FOLLOW_BY_DATA_CDS:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_BEARER_SERVICES;
			*(BearerSrv_t*)&inBasicSrvPtr->content = BEARER_SRV_ALL_SPEECH_FOLLOWED_DATA_CDS;
			break;

		case SS_SVCCLS_UNKNOWN:
		default:
			inBasicSrvPtr->type = BASIC_SERVICE_TYPE_UNKNOWN;
			break;
	}
}

int SvcClassToATClass (SS_SvcCls_t InfoClass)
{
    ATC_CLASS_t ATCClass;

    switch((int)InfoClass)
    {
        case SS_SVCCLS_NOTSPECIFIED:
            ATCClass = ATC_NOT_SPECIFIED;
            break;

        case SS_SVCCLS_SPEECH:
        case SS_SVCCLS_ALT_SPEECH:
            ATCClass = ATC_VOICE;
            break;

        case SS_SVCCLS_ALL_TELE_SERVICES:
            ATCClass = ATC_VOICE | ATC_FAX | ATC_SMS;
            break;

        case SS_SVCCLS_ALL_TELESERVICE_EXCEPT_SMS:
            ATCClass = ATC_VOICE | ATC_FAX;
            break;

        case SS_SVCCLS_DATA:
            ATCClass = ATC_DATA;
            break;

        case SS_SVCCLS_FAX:
            ATCClass = ATC_FAX;
            break;

        case SS_SVCCLS_SMS:
            ATCClass = ATC_SMS;
            break;

        case SS_SVCCLS_DATA_CIRCUIT_SYNC:
            ATCClass = ATC_DATA_CIRCUIT_SYNC;
            break;

        case SS_SVCCLS_DATA_CIRCUIT_ASYNC:
            ATCClass = ATC_DATA_CIRCUIT_ASYNC;
            break;

        case SS_SVCCLS_DEDICATED_PACKET:
            ATCClass = ATC_DPA;
            break;

        case SS_SVCCLS_DEDICATED_PAD:
            ATCClass = ATC_DPAD;
            break;

        case SS_SVCCLS_ALL_SYNC_SERVICES:
            ATCClass = ATC_DATA_CIRCUIT_SYNC |ATC_DPA;
            break;

        case SS_SVCCLS_ALL_ASYNC_SERVICES:
            ATCClass = ATC_DATA_CIRCUIT_ASYNC |ATC_DPAD;
            break;

        case SS_SVCCLS_ALL_BEARER_SERVICES:
            ATCClass = ATC_DATA_CIRCUIT_SYNC | ATC_DATA_CIRCUIT_ASYNC |ATC_DPA |ATC_DPAD;
            break;

        default:
            ATCClass = ATC_NOT_SPECIFIED;
            break;
    }
    return ATCClass;
}

int StdClassToATClass (int InfoClass)
{
    //**FixMe** We should really skip the intermediate conversion to old SS api types
    // and just do the conversion here.  For now, just do the intermediate conversion
    // (copied from ss_api_old.c).
    SS_SvcCls_t svcCls = GetServiceClass(InfoClass);
    return SvcClassToATClass(svcCls);
}

SS_SvcCls_t GetSsClassFromSrvGrp(BasicSrvGroup_t* inBasicSrvGroup)
{
	SS_SvcCls_t SvcClass = SS_SVCCLS_UNKNOWN;

	if(inBasicSrvGroup->type == BASIC_SERVICE_TYPE_BEARER_SERVICES )
	{
		switch(inBasicSrvGroup->content)
		{
			case BEARER_SRV_ALL_BEARER_SERVICES:
				SvcClass = SS_SVCCLS_ALL_BEARER_SERVICES;
				break;

			case BEARER_SRV_ALL_DATA_CDA_SERVICES:
			case BEARER_SRV_DATA_CDA_300_BPS:
			case BEARER_SRV_DATA_CDA_1200_BPS:
			case BEARER_SRV_DATA_CDA_1200_75_BPS:
			case BEARER_SRV_DATA_CDA_2400_BPS:
			case BEARER_SRV_DATA_CDA_4800_BPS:
			case BEARER_SRV_DATA_CDA_9600_BPS:
			case BEARER_SRV_ALL_DATA_CIRCUIT_ASYNCH:
			case BEARER_SRV_ALL_ALTER_SPEECH_DATA_CDA:
				SvcClass = SS_SVCCLS_DATA_CIRCUIT_ASYNC;
				break;

			case BEARER_SRV_ALL_DATA_CDS_SERVICES:
			case BEARER_SRV_DATA_CDS_1200_BPS:
			case BEARER_SRV_DATA_CDS_2400_BPS:
			case BEARER_SRV_DATA_CDS_4800_BPS:
			case BEARER_SRV_DATA_CDS_9600_BPS:
			case BEARER_SRV_ALL_ALTER_SPEECH_DATA_CDS:
				SvcClass = SS_SVCCLS_DATA_CIRCUIT_SYNC;
				break;

			case BEARER_SRV_ALL_PAD_ACCESS_CA_SERVICES:
			case BEARER_SRV_PAD_ACCESS_CA_300_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_1200_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_1200_75_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_2400_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_4800_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_9600_BPS:
				SvcClass = SS_SVCCLS_DEDICATED_PACKET;
				break;

			case BEARER_SRV_ALL_DATA_PDS_SERVICES:
			case BEARER_SRV_DATA_PDS_2400_BPS:
			case BEARER_SRV_DATA_PDS_4800_BPS:
			case BEARER_SRV_DATA_PDS_9600_BPS:
				SvcClass = SS_SVCCLS_DEDICATED_PACKET;
				break;

			case BEARER_SRV_ALL_SPEECH_FOLLOWED_DATA_CDA:
				SvcClass = SS_SVCCLS_SPEECH_FOLLOW_BY_DATA_CDA;
				break;

			case BEARER_SRV_ALL_SPEECH_FOLLOWED_DATA_CDS:
				SvcClass = SS_SVCCLS_SPEECH_FOLLOW_BY_DATA_CDS;
				break;

			case BEARER_SRV_ALL_DATA_CIRCUIT_SYNCH:
				SvcClass = SS_SVCCLS_DATA_CIRCUIT_SYNC;
				break;

			case BEARER_SRV_ALL_ASYNCHRONOUS_SERVICES:
				SvcClass = SS_SVCCLS_ALL_ASYNC_SERVICES;
				break;

			case BEARER_SRV_ALL_SYNCHRONOUS_SERVICES:
				SvcClass = SS_SVCCLS_ALL_SYNC_SERVICES;
				break;

			case BEARER_SRV_ALL_PLMN_SPECIFIC_BASIC_SRV:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_1:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_2:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_3:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_4:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_5:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_6:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_7:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_8:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_9:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_A:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_B:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_C:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_D:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_E:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_F:
				SvcClass = SS_SVCCLS_UNKNOWN; // Need to investigate if we this maps somewhere else.
				break;

			default:
				break;
		}
	}
	else if(inBasicSrvGroup->type == BASIC_SERVICE_TYPE_TELE_SERVICES)
	{
		switch(inBasicSrvGroup->content)
		{
			case TELE_SRV_ALL_TELE_SERVICES:
				SvcClass = SS_SVCCLS_ALL_TELE_SERVICES;
				break;

			case TELE_SRV_ALL_SPEECH_TRANSMISSION_SERVICES:
			case TELE_SRV_TELEPHONY:
			case TELE_SRV_EMERGENCY_CALLS:
				SvcClass = SS_SVCCLS_SPEECH;
				break;

			case TELE_SRV_ALL_SHORT_MESSAGE_SERVICES:
			case TELE_SRV_SHORT_MESSAGE_MT_PP:
			case TELE_SRV_SHORT_MESSAGE_MO_PP:
				SvcClass = SS_SVCCLS_SMS;
				break;

			case TELE_SRV_ALL_AUXILIARY_TELEPHONY:
				SvcClass = SS_SVCCLS_ALT_SPEECH;
				break;

			case TELE_SRV_ALL_FACIMILE_TRANSMISSION_SERVICES:
			case TELE_SRV_FACIMILE_GROUP_3_ALTER_SPEECH:
			case TELE_SRV_AUTOMATIC_FACIMILE_GROUP_3:
			case TELE_SRV_FACIMILE_GROUP_4:
				SvcClass = SS_SVCCLS_FAX;
				break;

			case TELE_SRV_ALL_DATA_TELE_SERVICES:
				SvcClass = SS_SVCCLS_DATA;
				break;

			case TELE_SRV_ALL_TELE_SRV_EXCEPT_SMS:
				SvcClass = SS_SVCCLS_ALL_TELESERVICE_EXCEPT_SMS;
				break;

			case TELE_SRV_TELEPHONY_AND_ALL_SYNC_SERVICES:
				SvcClass = SS_SVCCLS_ALL_SYNC_SERVICES;	// ?? need to verify
				break;

			case TELE_SRV_VOICE_GROUP_CALL_SERVICE:
				SvcClass = SS_SVCCLS_UNKNOWN;	// need to investigate
				break;
			case TELE_SRV_VOICE_BROADCAST_SERVICE:
				SvcClass = SS_SVCCLS_UNKNOWN;	// need to investigate
				break;

			case TELE_SRV_ALL_PLMN_SPECIFIC_SERVICES:
			case TELE_SRV_PLMN_SPECIFIC_1:
				SvcClass = SS_SVCCLS_ALT_SPEECH;
				break;

			case TELE_SRV_PLMN_SPECIFIC_2:
			case TELE_SRV_PLMN_SPECIFIC_3:
			case TELE_SRV_PLMN_SPECIFIC_4:
			case TELE_SRV_PLMN_SPECIFIC_5:
			case TELE_SRV_PLMN_SPECIFIC_6:
			case TELE_SRV_PLMN_SPECIFIC_7:
			case TELE_SRV_PLMN_SPECIFIC_8:
			case TELE_SRV_PLMN_SPECIFIC_9:
			case TELE_SRV_PLMN_SPECIFIC_A:
			case TELE_SRV_PLMN_SPECIFIC_B:
			case TELE_SRV_PLMN_SPECIFIC_C:
			case TELE_SRV_PLMN_SPECIFIC_D:
			case TELE_SRV_PLMN_SPECIFIC_E:
			case TELE_SRV_PLMN_SPECIFIC_F:
				SvcClass = SS_SVCCLS_PLMN_SPEC_TELE_SVC;
				break;

			default:
				break;
		}
	}
	if(SvcClass == SS_SVCCLS_UNKNOWN)
	{
		KRIL_DEBUG(DBG_INFO, "SSGetSsClassfromSvcGrp(): Error could not map the input Service Grp type = %d\n",inBasicSrvGroup->type);
		KRIL_DEBUG(DBG_INFO, "content = %d\n",inBasicSrvGroup->content);
	}
	return SvcClass;
}

int SrvGroupToATClass(BasicSrvGroup_t* inBasicSrvGroup)
{
    //**FixMe** We should really skip the intermediate conversion to old SS api types
    // and just do the conversion here.  For now, just do the intermediate conversion
    // (copied from ss_api_old.c).
    SS_SvcCls_t ssClass = GetSsClassFromSrvGrp(inBasicSrvGroup);
    return SvcClassToATClass(ssClass);
}

//******************************************************************************
//
// Function Name:	Util_GetATClassFromSvcGrp
//
// Description: This is an internal function to convert from basicsrvgroup as per 
//				CAPI format to the 3GPP 27.007 format to be displayed on the AT console
//

//******************************************************************************
ATC_CLASS_t Util_GetATClassFromSvcGrp(BasicSrvGroup_t inputSrvGrp)
{

	ATC_CLASS_t atcClass = ATC_NOT_SPECIFIED;

	KRIL_DEBUG(DBG_INFO, "srvGrpType=%x, srvGrpcontent=%x\n",inputSrvGrp.type, inputSrvGrp.content);
	
	if(inputSrvGrp.type == BASIC_SERVICE_TYPE_BEARER_SERVICES )
	{
		switch(inputSrvGrp.content)
		{
			case BEARER_SRV_ALL_BEARER_SERVICES:
				atcClass = ATC_DATA;
				break;
				
			case BEARER_SRV_ALL_DATA_CDA_SERVICES:
			case BEARER_SRV_DATA_CDA_300_BPS:
			case BEARER_SRV_DATA_CDA_1200_BPS:
			case BEARER_SRV_DATA_CDA_1200_75_BPS:
			case BEARER_SRV_DATA_CDA_2400_BPS:
			case BEARER_SRV_DATA_CDA_4800_BPS:
			case BEARER_SRV_DATA_CDA_9600_BPS:
			case BEARER_SRV_ALL_DATA_CIRCUIT_ASYNCH:
			case BEARER_SRV_ALL_ALTER_SPEECH_DATA_CDA:
				atcClass = ATC_DATA_CIRCUIT_ASYNC;
				break;
				
			case BEARER_SRV_ALL_DATA_CDS_SERVICES:
			case BEARER_SRV_DATA_CDS_1200_BPS:
			case BEARER_SRV_DATA_CDS_2400_BPS:
			case BEARER_SRV_DATA_CDS_4800_BPS:
			case BEARER_SRV_DATA_CDS_9600_BPS:
			case BEARER_SRV_ALL_ALTER_SPEECH_DATA_CDS:
				atcClass = ATC_DATA_CIRCUIT_SYNC;
				break;
				
			case BEARER_SRV_ALL_PAD_ACCESS_CA_SERVICES:
			case BEARER_SRV_PAD_ACCESS_CA_300_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_1200_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_1200_75_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_2400_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_4800_BPS:
			case BEARER_SRV_PAD_ACCESS_CA_9600_BPS:
				atcClass = ATC_DPAD;
				break;

			case BEARER_SRV_ALL_DATA_PDS_SERVICES:
			case BEARER_SRV_DATA_PDS_2400_BPS:
			case BEARER_SRV_DATA_PDS_4800_BPS:
			case BEARER_SRV_DATA_PDS_9600_BPS:
				atcClass = ATC_DPA;
				break;

			case BEARER_SRV_ALL_SPEECH_FOLLOWED_DATA_CDA:
				break;

			case BEARER_SRV_ALL_SPEECH_FOLLOWED_DATA_CDS:
				break; 
				
			case BEARER_SRV_ALL_DATA_CIRCUIT_SYNCH:
				atcClass = ATC_DATA_CIRCUIT_SYNC;
				break;

			case BEARER_SRV_ALL_ASYNCHRONOUS_SERVICES:
				atcClass = ATC_DATA;
				break;
				
			case BEARER_SRV_ALL_SYNCHRONOUS_SERVICES:
				atcClass = ATC_DATA;
				break;

			case BEARER_SRV_ALL_PLMN_SPECIFIC_BASIC_SRV:			
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_1:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_2:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_3: 
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_4: 
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_5:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_6: 
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_7: 
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_8:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_9:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_A:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_B:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_C:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_D:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_E:
			case BEARER_SRV_PLMN_SPECIFIC_BASIC_SRV_F:
				// Need to investigate if we this maps somewhere else.
				break;
				
			default:
				break;
		}
	}
	else if(inputSrvGrp.type == BASIC_SERVICE_TYPE_TELE_SERVICES)
	{
		switch(inputSrvGrp.content)
		{
			case TELE_SRV_ALL_TELE_SERVICES:
				atcClass = ATC_VOICE;
				break;
	
			case TELE_SRV_ALL_SPEECH_TRANSMISSION_SERVICES:
			case TELE_SRV_TELEPHONY:
			case TELE_SRV_EMERGENCY_CALLS:
				atcClass = ATC_VOICE;
				break;

			case TELE_SRV_ALL_SHORT_MESSAGE_SERVICES:
			case TELE_SRV_SHORT_MESSAGE_MT_PP:
			case TELE_SRV_SHORT_MESSAGE_MO_PP:
				atcClass = ATC_SMS;
				break;

			case TELE_SRV_ALL_AUXILIARY_TELEPHONY: 
				atcClass = ATC_VOICE;
				break;

			case TELE_SRV_ALL_FACIMILE_TRANSMISSION_SERVICES:
			case TELE_SRV_FACIMILE_GROUP_3_ALTER_SPEECH:
			case TELE_SRV_AUTOMATIC_FACIMILE_GROUP_3:
			case TELE_SRV_FACIMILE_GROUP_4:
				atcClass = ATC_FAX;
				break;

			case TELE_SRV_ALL_DATA_TELE_SERVICES:
				atcClass = ATC_DATA;
				break;

			case TELE_SRV_ALL_TELE_SRV_EXCEPT_SMS:
				atcClass = ATC_VOICE;
				break;

			case TELE_SRV_TELEPHONY_AND_ALL_SYNC_SERVICES:
				atcClass = ATC_DATA;	// ?? need to verify
				break;

			case TELE_SRV_VOICE_GROUP_CALL_SERVICE:
				// need to investigate
				break;
			case TELE_SRV_VOICE_BROADCAST_SERVICE:
				// need to investigate
				break;

			case TELE_SRV_ALL_PLMN_SPECIFIC_SERVICES:
			case TELE_SRV_PLMN_SPECIFIC_1:
			case TELE_SRV_PLMN_SPECIFIC_2:
			case TELE_SRV_PLMN_SPECIFIC_3:
			case TELE_SRV_PLMN_SPECIFIC_4:
			case TELE_SRV_PLMN_SPECIFIC_5:
			case TELE_SRV_PLMN_SPECIFIC_6:
			case TELE_SRV_PLMN_SPECIFIC_7:
			case TELE_SRV_PLMN_SPECIFIC_8:
			case TELE_SRV_PLMN_SPECIFIC_9:
			case TELE_SRV_PLMN_SPECIFIC_A:
			case TELE_SRV_PLMN_SPECIFIC_B:
			case TELE_SRV_PLMN_SPECIFIC_C:
			case TELE_SRV_PLMN_SPECIFIC_D:
			case TELE_SRV_PLMN_SPECIFIC_E:
			case TELE_SRV_PLMN_SPECIFIC_F:
				break;
				
			default:
				break;
		}
	}
	if(atcClass == ATC_NOT_SPECIFIED)
	{
		KRIL_DEBUG(DBG_ERROR, "SSGetSsClassfromSvcGrp(): Error could not map the input Service Grp type=%x, content=%x\n",inputSrvGrp.type, inputSrvGrp.content);
	}
	KRIL_DEBUG(DBG_INFO, "atcClass=%x\n",atcClass);
	return atcClass;	
}

//******************************************************************************
// Function Name:	SS_GetCallfwdReasonfromCode
//
// Description:		This function derives the call forward reason from the input SS code
//					The SS_CallFwdReason_t is used in the old SS and is needed to map
//					the new SS responses back so that it can be sent to ATC
//******************************************************************************
static SS_CallFwdReason_t SS_GetCallfwdReasonfromCode(SS_Code_t inputCode)
{
	SS_CallFwdReason_t reason = SS_CALLFWD_REASON_NOTSPECIFIED;

	switch(inputCode)
	{
		case SS_CODE_ALL_FORWARDING:
			reason = SS_CALLFWD_REASON_ALL_CF;
			break;

		case SS_CODE_CFU:
			reason = SS_CALLFWD_REASON_UNCONDITIONAL;
			break;
		case SS_CODE_ACF:
			reason = SS_CALLFWD_REASON_ALL_CONDITIONAL;
			break;
		case SS_CODE_CFB:
			reason = SS_CALLFWD_REASON_BUSY;
			break;
		case SS_CODE_CFNRY:
			reason = SS_CALLFWD_REASON_NO_REPLY;
			break;
		case SS_CODE_CFNRC:
			reason = SS_CALLFWD_REASON_NOT_REACHABLE;
			break;
		default:
			KRIL_DEBUG(DBG_INFO, "SSGetCallfwdReasonFromCode(): Error could not map the inputcode = %d\n",inputCode);
			break;
	}
	return reason;
}

#ifndef isdigit
#define isdigit(_c)  ((_c >= '0' && _c <= '9') ? 1:0)
#endif

//******************************************************************************
// Function Name:	UtilApi_DialStr2PartyAdd
//
// Description:		See GSM 02.30, Section 2.3 for encoding rules
//******************************************************************************
void UtilApi_DialStr2PartyAdd(PartyAddress_t* outPartyAddPtr, const UInt8* inStrPtr)
{
    int		length;
    UInt8*	numberPtr = outPartyAddPtr->number;

    outPartyAddPtr->ton = TON_UNKNOWN;
    outPartyAddPtr->npi = NPI_UNKNOWN;

    if (*inStrPtr == INTERNATIONAL_CODE)
    {
        outPartyAddPtr->ton = TON_INTERNATIONAL_NUMBER;
        inStrPtr++;
    }

    outPartyAddPtr->length = sizeof(outPartyAddPtr->number);

    strncpy(outPartyAddPtr->number, inStrPtr, outPartyAddPtr->length);
    outPartyAddPtr->number[outPartyAddPtr->length-1] = '\0';

    length = strlen((char*)outPartyAddPtr->number);

    if (length < outPartyAddPtr->length)
    {
        outPartyAddPtr->length = (UInt8)length;
    }

    if (*numberPtr != 0x00)
    {
        length = outPartyAddPtr->length;

        outPartyAddPtr->npi = NPI_ISDN_TELEPHONY_NUMBERING_PLAN;

        while (length)
        {
            if (!isdigit(numberPtr[length - 1]))
            {
                outPartyAddPtr->npi = NPI_UNKNOWN;
                break;
            }

            length--;
        }
    }
}

void KRIL_SendUSSDHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilSendUSSDInfo_t *tdata = (KrilSendUSSDInfo_t *)pdata->ril_cmd->data;

            if(tdata->StringSize <= 0)
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            KRIL_DEBUG(DBG_INFO, "gUssdID[%d]:%d m_USSDString:%s used_size:%d\n", pdata->ril_cmd->SimId, gUssdID[pdata->ril_cmd->SimId], tdata->USSDString, tdata->StringSize);

            if(gUssdID[pdata->ril_cmd->SimId] == CALLINDEX_INVALID) //new USSD request
            {
                SsApi_UssdSrvReq_t* ussdSrvReq;
                ClientInfo_t clientInfo;

                gUssdID[pdata->ril_cmd->SimId] = 1;

                CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                clientInfo.simId = pdata->ril_cmd->SimId;
                ussdSrvReq = kmalloc(sizeof(SsApi_UssdSrvReq_t), GFP_KERNEL);
                if(!ussdSrvReq) {
                    KRIL_DEBUG(DBG_ERROR, "unable to allocate pdata->bcm_ril_rsp buf\n");
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;                
                }
                else
                {
                    memset(ussdSrvReq, 0, sizeof(SsApi_UssdSrvReq_t));    
                    pdata->cmdContext = ussdSrvReq;
                    ussdSrvReq->fdnCheck = CONFIG_MODE_INVOKE;
                    ussdSrvReq->stkCheck = CONFIG_MODE_INVOKE; // Refer to CP: SsApi_UssdSrvReq(.) of ss_api.c and ATCmd_CUSD_Handler(.) of at_ss.c
                    ussdSrvReq->operation = SS_OPERATION_CODE_PROCESS_UNSTRUCTURED_SS_REQ;
                    ussdSrvReq->ussdInfo.dcs = 0xF;    // UTF-8
                    ussdSrvReq->ussdInfo.length = tdata->StringSize;
                    memcpy( ussdSrvReq->ussdInfo.data,
                            tdata->USSDString,
                            tdata->StringSize);
    
                    CAPI2_SsApi_UssdSrvReq(&clientInfo, ussdSrvReq);
                    gDialogID = clientInfo.dialogId;
                    KRIL_DEBUG(DBG_INFO, "New USSD request: gDialogID=%x\n", (int)gDialogID);
                    pdata->handler_state = BCM_RESPCAPI2Cmd;
                }
            }
            else
            {
                SsApi_DataReq_t apiDataReq={0};
                ClientInfo_t clientInfo;

                KRIL_DEBUG(DBG_INFO, "Existing USSD session: gDialogID: %ld\n", gDialogID);
                apiDataReq.operation = SS_OPERATION_CODE_UNSTRUCTURED_SS_REQEST;
                apiDataReq.param.ussdInfo.dcs = 0xF;    // UTF-8
                apiDataReq.param.ussdInfo.length = tdata->StringSize;
                memcpy(apiDataReq.param.ussdInfo.data,
                        tdata->USSDString,
                        tdata->StringSize);

                CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                clientInfo.simId = pdata->ril_cmd->SimId;
                clientInfo.dialogId = gDialogID;
                CAPI2_SsApi_DataReq(&clientInfo, &apiDataReq);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_INFO, "BCM_RESPCAPI2Cmd:: capi2_rsp->result=%d !=RESULT_OK\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
            {
                StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
                // The USSD session is released. Reset the session related global variables.
                gDialogID = 0;
                gUssdID[pdata->ril_cmd->SimId] = CALLINDEX_INVALID;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                 if ((capi2_rsp->msgType == MSG_SSAPI_USSDSRVREQ_RSP) ||
                     (capi2_rsp->msgType == MSG_SSAPI_DATAREQ_RSP ))
                 {
                    // Do nothing. MSG_CLIENT_SS_SRV_{RSP|REL|IND} will trigger the sending of RIL_UNSOL_ON_USSD
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                 }
                 else
                 {
                    // The USSD session is released. Reset the session related global variables.
                    gDialogID = 0;
                    gUssdID[pdata->ril_cmd->SimId] = CALLINDEX_INVALID;
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                 }
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

void KRIL_CancelUSSDHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KRIL_DEBUG(DBG_INFO, "gUssdID[%d]:%d\n", pdata->ril_cmd->SimId, gUssdID[pdata->ril_cmd->SimId]);
            if(gUssdID[pdata->ril_cmd->SimId] != CALLINDEX_INVALID)
            {
                ClientInfo_t    clientInfo;
                SsApi_SrvReq_t  apiReq = {0};

                CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                clientInfo.simId = pdata->ril_cmd->SimId;
                clientInfo.dialogId = gDialogID;
                //**FixMe** No CAPI2 api for MNSS_CompMnssDialogIdFromCallindex?
                //from the callindex compute the dialog id and use that.
                //if(( MNSS_CompMnssDialogIdFromCallindex(&clientInfo.dialogId, ussd_id)) == TRUE)

                CAPI2_SsApi_SsReleaseReq(&clientInfo, &apiReq);
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
            if (capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_ERROR, "result:%d\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
            {
                StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
                // The USSD session is released. Reset the session related global variables.
                gDialogID = 0;
                gUssdID[pdata->ril_cmd->SimId] = CALLINDEX_INVALID;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
               if (capi2_rsp->msgType == MSG_SSAPI_SSRELEASEREQ_RSP)
               {
                  gDialogID = 0;
                  gUssdID[pdata->ril_cmd->SimId] = CALLINDEX_INVALID;
                  KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSRELEASEREQ_RSP\n");
                  pdata->handler_state = BCM_FinishCAPI2Cmd;
               }
                else
                {
                    // The USSD session is released. Reset the session related global variables.
                    gDialogID = 0;
                    gUssdID[pdata->ril_cmd->SimId] = CALLINDEX_INVALID;
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
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


void KRIL_GetCLIRHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;

            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilCLIRInfo_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                pdata->rsp_len = sizeof(KrilCLIRInfo_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);    
                CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                clientInfo.simId = pdata->ril_cmd->SimId;
                ssSrvReqPtr->operation = SS_OPERATION_CODE_INTERROGATE;
                ssSrvReqPtr->basicSrv.type = BASIC_SERVICE_TYPE_UNSPECIFIED;
                ssSrvReqPtr->ssCode = SS_CODE_CLIR;    
                KRIL_SetSsSrvReqTID(GetTID());  // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
                CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);    
                pdata->handler_state = BCM_SS_GetElement;
            }
        }
        break;

        case BCM_SS_GetElement:
        {
            KrilCLIRInfo_t *rdata = (KrilCLIRInfo_t *) pdata->bcm_ril_rsp;

            if (capi2_rsp->result != RESULT_OK)
            {
                rdata->value2 = SS_SERVICE_STATUS_UKNOWN;
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else
            {
                if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
                {
                    SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                    ClientInfo_t clientInfo;

                    KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? callIndex:%d component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n",
                        srvRel_rsp->callIndex, srvRel_rsp->component,
                        srvRel_rsp->operation, srvRel_rsp->ssCode,
                        srvRel_rsp->type, srvRel_rsp->param.ussdInfo.dcs,
                        srvRel_rsp->param.ussdInfo.length,
                        srvRel_rsp->param.ussdInfo.data);

                    if (srvRel_rsp->type == SS_SRV_TYPE_GENERIC_SRV_INFORMATION)
                    {
                        // CLI restriction option:
                        //  0 CLIR not provisioned
                        //  1 CLIR provisioned in permanent mode
                        //  2 unknown (e.g. no network, etc.)
                        //  3 CLIR temporary mode presentation restricted
                        //  4 CLIR temporary mode presentation allowed
                        //
                        // SS_ClirOption_t srvRel_rsp->param.genSrvInfo.clir
                        // only has the fields:
                        //  SS_CLI_RESTRICTION_PERMENENT			= 0x00,	///< Permenent
                        //  SS_CLI_RESTRICTION_TEMPORARY_RESTRICTED	= 0x01,	///< Temporary (Default Restricted)
                        //  SS_CLI_RESTRICTION_TEMPORARY_ALLOWED	= 0x02	///< Temporary (Default Allowed)
                        if ((srvRel_rsp->param.genSrvInfo.ssStatus & SS_STATUS_MASK_PROVISIONED) == 0)
                        {
                            // Not provisioned
                            rdata->value2 = 0;
                        }
                        else if ((srvRel_rsp->param.genSrvInfo.include & 0x1) == 0x0)
                        {
                           // Structure : Generic Service Information
                           // include	          8   7   6   5   4   3   2   1
                           //                 |   |   |   |   |   |   |   |   |
                           //                   ^   ^   ^   ^   ^   ^   ^   ^
                           // clir                                           X     CLI Restriction Option
                            rdata->value2 = 2; // unknown
                        }
                        else if (srvRel_rsp->param.genSrvInfo.clir == SS_CLI_RESTRICTION_PERMENENT)
                        {
                            rdata->value2 = 1;
                        }
                        else if (srvRel_rsp->param.genSrvInfo.clir == SS_CLI_RESTRICTION_TEMPORARY_RESTRICTED)
                        {
                            rdata->value2 = 3;
                        }
                        else if (srvRel_rsp->param.genSrvInfo.clir == SS_CLI_RESTRICTION_TEMPORARY_ALLOWED)
                        {
                            rdata->value2 = 4;
                        }
                        else
                        {
                            rdata->value2 = 2;
                        }
                        KRIL_DEBUG(DBG_INFO, "srvRel_rsp->param.genSrvInfo: ssStatus:%d clir:%d include:%d maxEntPrio:%d defaultPrio:%d\n",
                            srvRel_rsp->param.genSrvInfo.ssStatus,
                            srvRel_rsp->param.genSrvInfo.clir,
                            srvRel_rsp->param.genSrvInfo.include,
                            srvRel_rsp->param.genSrvInfo.maxEntPrio,
                            srvRel_rsp->param.genSrvInfo.defaultPrio);
                        KRIL_DEBUG(DBG_INFO, "rdata->value2:%d\n", rdata->value2);
                    }
                    else
                    {
                        KRIL_DEBUG(DBG_ERROR, "unknown response type, expecting SS_SRV_TYPE_GENERIC_SRV_INFORMATION\n");
                    }
                    //KRIL_DEBUG(DBG_INFO, "provision_status:%d serviceStatus:%d netCause:%d\n", rsp->provision_status, rsp->serviceStatus, rsp->netCause);
                    KRIL_DEBUG(DBG_INFO, "srvRel_rsp->causeIe.codStandard:%d location:%d, recommendation:%d cause:%d diagnostic:%d facIeLength:%d\n",
                        srvRel_rsp->causeIe.codStandard,
                        srvRel_rsp->causeIe.location,
                        srvRel_rsp->causeIe.recommendation,
                        srvRel_rsp->causeIe.cause,
                        srvRel_rsp->causeIe.diagnostic,
                        srvRel_rsp->facIeLength);

                    // Get the CLIR value from CAPI (parameter sets the adjustment for outgoing calls; local setting applicable to temporary mode CLIR?)
                    CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                    clientInfo.simId = pdata->ril_cmd->SimId;
                    CAPI2_MsDbApi_GetElement(&clientInfo, MS_LOCAL_SS_ELEM_CLIR);
                    pdata->handler_state = BCM_RESPCAPI2Cmd;
                }
                else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
                {
                   KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                   pdata->handler_state = BCM_SS_GetElement;
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR, "unhandled msg type = 0x%x\n", capi2_rsp->msgType);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            CAPI2_MS_Element_t *rsp = (CAPI2_MS_Element_t *) capi2_rsp->dataBuf;
            KrilCLIRInfo_t *rdata = (KrilCLIRInfo_t *) pdata->bcm_ril_rsp;

            rdata->value1 = rsp->data_u.u8Data;
            KRIL_DEBUG(DBG_INFO, "CLIR:%d\n", rdata->value1);
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


void KRIL_SetCLIRHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;
            KrilCLIRValue_t *tdata = (KrilCLIRValue_t *)pdata->ril_cmd->data;

            KRIL_DEBUG(DBG_INFO, "value:%d\n", tdata->value);

            CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
            clientInfo.simId = pdata->ril_cmd->SimId;

            if (tdata->value == 1)  //CLIRMODE_INVOKED
            {
                ssSrvReqPtr->operation = SS_OPERATION_CODE_ACTIVATE;
            }
            else if(tdata->value == 2)  //CLIRMODE_SUPPRESSED
            {
                ssSrvReqPtr->operation = SS_OPERATION_CODE_DEACTIVATE;
            }
            else if(tdata->value == 0)  //CLIRMODE_DEFAULT
            {
                ssSrvReqPtr->operation = SS_OPERATION_CODE_ERASE;
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                break;
            }

            ssSrvReqPtr->basicSrv.type = BASIC_SERVICE_TYPE_UNSPECIFIED;
            ssSrvReqPtr->ssCode = SS_CODE_CLIR;

            // In this specific case, TID should be stored before calling CAPI2. Otherwise, MSG_MS_LOCAL_ELEM_NOTIFY_IND will be returned with tid 0 (due to OS scheduling).
            KRIL_SetSsSrvReqTID(GetTID()); // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
            CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);

            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if(capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_INFO, "capi2_rsp->result:%d\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_MS_LOCAL_ELEM_NOTIFY_IND)
            {
                // Refer to SS_LocalSettingProcedure(.) and SsApi_SsSrvReq(.) of ss_api.c
                KRIL_DEBUG(DBG_INFO, "MSG_MS_LOCAL_ELEM_NOTIFY_IND\n");
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
            {
                KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                pdata->handler_state = BCM_RESPCAPI2Cmd;
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

void KRIL_QueryCallForwardStatusHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            KRIL_SetServiceClassValue(0);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilCallForwardStatus_t *tdata = (KrilCallForwardStatus_t *)pdata->ril_cmd->data;
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;

            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilCallForwardinfo_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                pdata->rsp_len = sizeof(KrilCallForwardinfo_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
    
                CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                clientInfo.simId = pdata->ril_cmd->SimId;
                ssSrvReqPtr->operation = SS_OPERATION_CODE_INTERROGATE;
                ssSrvReqPtr->ssCode = Kril_FwdReasonCode[tdata->reason];
                SS_SvcCls2BasicSrvGroup(tdata->ss_class, &(ssSrvReqPtr->basicSrv));
    
                KRIL_DEBUG(DBG_INFO, "Kril_FwdReasonCode[%d]:%d ss_class:%d\n", tdata->reason, Kril_FwdReasonCode[tdata->reason], GetServiceClass(tdata->ss_class));
                KRIL_DEBUG(DBG_INFO, "GetServiceClass(%d): type:%d content:%d\n", tdata->ss_class, ssSrvReqPtr->basicSrv.type, ssSrvReqPtr->basicSrv.content);
                KRIL_DEBUG(DBG_INFO, "ssCode:%d\n", ssSrvReqPtr->ssCode);
    
                KRIL_SetSsSrvReqTID(GetTID()); // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
                CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);
                KRIL_SetServiceClassValue(tdata->ss_class);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KRIL_DEBUG(DBG_INFO, "msgType:0x%x\n", capi2_rsp->msgType);

            pdata->handler_state = BCM_FinishCAPI2Cmd;

            if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
            {
                SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                KrilCallForwardinfo_t *rdata = (KrilCallForwardinfo_t *)pdata->bcm_ril_rsp;
                KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? callIndex:%d component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n", srvRel_rsp->callIndex, srvRel_rsp->component, srvRel_rsp->operation, srvRel_rsp->ssCode, srvRel_rsp->type, srvRel_rsp->param.ussdInfo.dcs, srvRel_rsp->param.ussdInfo.length, srvRel_rsp->param.ussdInfo.data);

                switch (srvRel_rsp->type)
                {
                    case SS_SRV_TYPE_FORWARDING_INFORMATION:
                    {
                        SS_FwdFeature_t* fwdInfoPtr = srvRel_rsp->param.forwardInfo.fwdFeatureList;
                        UInt8            index;

                        rdata->class_size = srvRel_rsp->param.forwardInfo.listSize;
                        if( SS_CALLFWD_REASON_NOTSPECIFIED == SS_GetCallfwdReasonfromCode(srvRel_rsp->ssCode) )
                        {
                            KrilCallForwardStatus_t *tdata = (KrilCallForwardStatus_t *)pdata->ril_cmd->data;
                            rdata->reason = tdata->reason;
                        }
                        else
                        {
                            /* restore Kril_FwdReason to Framework flavor */                        
                            rdata->reason = SS_GetCallfwdReasonfromCode(srvRel_rsp->ssCode) - 1;
                        }
                        
                        KRIL_DEBUG(DBG_INFO, "rdata->reason:%d\n", rdata->reason);
                        for (index = 0; index < rdata->class_size; index++)
                        {
                            KRIL_DEBUG(DBG_INFO, "[index:%d/size:%d]\n", index, rdata->class_size);

                            //based on what paramters are included in the input structure create the output struct
                            if( fwdInfoPtr->include & 0x01 ) // basic service group included
                            {
                                rdata->call_forward_class_info_list[index].ss_class = SrvGroupToATClass(&(fwdInfoPtr->basicSrv));
                                KRIL_DEBUG(DBG_INFO, "ss_class = %d\n", rdata->call_forward_class_info_list[index].ss_class);
                            }
                            else
                            {
                                //invalidate the ssclass
                                rdata->call_forward_class_info_list[index].ss_class = ATC_NOT_SPECIFIED;
                                KRIL_DEBUG(DBG_INFO, "SS_SVCCLS_NOTSPECIFIED = %d\n", rdata->call_forward_class_info_list[index].ss_class);
                            }

                            //update the ss status if present. by default this is FALSE due to the memset to 0 above.
                            if(fwdInfoPtr->include & 0x02)
                            {
                                rdata->call_forward_class_info_list[index].activated = (fwdInfoPtr->ssStatus & SS_STATUS_MASK_ACTIVE)? TRUE:FALSE;
                            }
                            else
                            {
                                rdata->call_forward_class_info_list[index].activated = TRUE;
                            }

                            KRIL_DEBUG(DBG_INFO, "activated = %d\n", rdata->call_forward_class_info_list[index].activated);

                            if(fwdInfoPtr->include & 0x04)
                            {
                                //need to ensure that the TypeOfNumber_t is in sync with the  gsm_TON_t defined
                                rdata->call_forward_class_info_list[index].ton = (gsm_TON_t)fwdInfoPtr->partyAdd.ton;

                                //need to ensure that the NumberPlanId_t is in sync with the gsm_NPI_t
                                rdata->call_forward_class_info_list[index].npi = (gsm_NPI_t)fwdInfoPtr->partyAdd.npi;

                                KRIL_DEBUG(DBG_INFO, "fwdInfoPtr->partyAdd.number = %s (len:%d)\n", fwdInfoPtr->partyAdd.number, fwdInfoPtr->partyAdd.length);

                                memcpy(	rdata->call_forward_class_info_list[index].number,
                                        fwdInfoPtr->partyAdd.number,
                                        fwdInfoPtr->partyAdd.length);
                            }

                            if(fwdInfoPtr->include & 0x20)
                            {
                                //extract the reply time
                                rdata->call_forward_class_info_list[index].noReplyTime = fwdInfoPtr->noReplyTime;
                            }

                            if(fwdInfoPtr->include & 0x40)
                            {
                                //need to ensure that the TypeOfNumber_t is in sync with the  gsm_TON_t defined
                                rdata->call_forward_class_info_list[index].ton = (gsm_TON_t)fwdInfoPtr->LongPartyAdd.ton;

                                //need to ensure that the NumberPlanId_t is in sync with the gsm_NPI_t
                                rdata->call_forward_class_info_list[index].npi = (gsm_NPI_t)fwdInfoPtr->LongPartyAdd.npi;

                                KRIL_DEBUG(DBG_INFO, "fwdInfoPtr->LongPartyAdd.number = %s (len:%d)\n", fwdInfoPtr->LongPartyAdd.number, fwdInfoPtr->LongPartyAdd.length);

                                memcpy(	rdata->call_forward_class_info_list[index].number,
                                        fwdInfoPtr->LongPartyAdd.number,
                                        fwdInfoPtr->LongPartyAdd.length);
                            }

                            fwdInfoPtr++;
                        }
                        /* if ss_class is '0' when service is deactivated, set the ss_class for all services to display properly */
                        if( (rdata->call_forward_class_info_list[0].activated == FALSE) 
                             && (rdata->call_forward_class_info_list[0].ss_class == ATC_NOT_SPECIFIED) )
                        {
                            if( KRIL_GetServiceClassValue() != 0 )
                                rdata->call_forward_class_info_list[0].ss_class = KRIL_GetServiceClassValue();
                            else                           
                                rdata->call_forward_class_info_list[0].ss_class = 0xff;  // all services
                        }
                        
                        KRIL_SetServiceClassValue(0);            
                        break;
                    }
                    case SS_SRV_TYPE_SS_STATUS:
                    {
                        rdata->class_size = 1;
                        if( SS_CALLFWD_REASON_NOTSPECIFIED == SS_GetCallfwdReasonfromCode(srvRel_rsp->ssCode) )
                        {
                            KrilCallForwardStatus_t *tdata = (KrilCallForwardStatus_t *)pdata->ril_cmd->data;
                            rdata->reason = tdata->reason;
                        }
                        else
                        {
                            /* restore Kril_FwdReason to Framework flavor */                        
                            rdata->reason = SS_GetCallfwdReasonfromCode(srvRel_rsp->ssCode) - 1;
                        }
                        KRIL_DEBUG(DBG_INFO, "rdata->reason:%d\n", rdata->reason);
                        rdata->call_forward_class_info_list[0].activated = 
                           (srvRel_rsp->param.ssStatus & SS_STATUS_MASK_PROVISIONED) && (srvRel_rsp->param.ssStatus & SS_STATUS_MASK_ACTIVE);

                        /* if ss_class is '0' when service is deactivated, set the ss_class for all services to display properly */
                        if( (rdata->call_forward_class_info_list[0].activated == 0) 
                             && (rdata->call_forward_class_info_list[0].ss_class == 0) )
                        {
                            if( KRIL_GetServiceClassValue() != 0 )
                                rdata->call_forward_class_info_list[0].ss_class = KRIL_GetServiceClassValue();
                            else
                                rdata->call_forward_class_info_list[0].ss_class = 0xff;  // all services
                        }
                         
                        KRIL_DEBUG(DBG_INFO, "rdata->call_forward_class_info_list[0].activated = %d\n", rdata->call_forward_class_info_list[0].activated);
                        KRIL_SetServiceClassValue(0);            
                        break;
                    }
                    // Errors:
                    case SS_SRV_TYPE_NONE:
                    {
                        KRIL_DEBUG(DBG_INFO, "Error: %d\n", srvRel_rsp->type);
                        KRIL_SetServiceClassValue(0);            
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_COMPONENT_TYPE_RETURN_ERROR:
                    {
                        KRIL_DEBUG(DBG_INFO, "RETURN_ERROR: SS_ErrorCode_t:%d\n", srvRel_rsp->param.returnError.errorCode);
                        KRIL_SetServiceClassValue(0);            
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_COMPONENT_TYPE_REJECT:
                    {
                        KRIL_DEBUG(DBG_INFO, "REJECT: problemType:%d content:%d\n", srvRel_rsp->param.reject.problemType, srvRel_rsp->param.reject.content);
                        KRIL_SetServiceClassValue(0);            
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_SRV_TYPE_LOCAL_RESULT:
                    {
                        KRIL_DEBUG(DBG_INFO, "localResult::%d\n", srvRel_rsp->param.localResult);
                        KRIL_SetServiceClassValue(0);            
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_SRV_TYPE_CAUSE_IE:
                    {
                        KRIL_DEBUG(DBG_INFO, "CAUSE_IE: codStandard:%d location:%d recommendation:%d cause:%d diagnostic:%d cause:%d\n", srvRel_rsp->causeIe.codStandard, srvRel_rsp->causeIe.location, srvRel_rsp->causeIe.recommendation, srvRel_rsp->causeIe.cause, srvRel_rsp->causeIe.diagnostic, srvRel_rsp->causeIe.cause);
                        KRIL_SetServiceClassValue(0);            
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    default:
                    {
                        KRIL_DEBUG(DBG_INFO, "Unhandled type: %d\n", srvRel_rsp->type);
                        KRIL_SetServiceClassValue(0);            
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                }
            }
            else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
            {
                KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_SS_CALL_FORWARD_RSP)
            {
                NetworkCause_t *rsp = (NetworkCause_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_ERROR, "MSG_SS_CALL_FORWARD_RSP::netCause:%d\n", *rsp);
                if(*rsp != GSMCAUSE_SUCCESS)
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                KRIL_SetServiceClassValue(0);            
            }
            else if (capi2_rsp->msgType == MSG_SS_CALL_FORWARD_STATUS_RSP)
            {
                CallForwardStatus_t *rsp = (CallForwardStatus_t *) capi2_rsp->dataBuf;
                KrilCallForwardinfo_t *rdata = (KrilCallForwardinfo_t *)pdata->bcm_ril_rsp;
                KRIL_DEBUG(DBG_INFO, "netCause:%d reason:%d\n", rsp->netCause, rsp->reason);
                if(rsp->netCause == GSMCAUSE_SUCCESS)
                {
                    int i;
                    rdata->class_size = rsp->class_size;
                    rdata->reason = rsp->reason;
                    KRIL_DEBUG(DBG_INFO, "class_size:%d\n", rsp->class_size);
                    for(i = 0 ; i < rsp->class_size ; i++)
                    {
                        rdata->call_forward_class_info_list[i].activated = rsp->call_forward_class_info_list[i].activated;
                        rdata->call_forward_class_info_list[i].ss_class = SvcClassToATClass(rsp->call_forward_class_info_list[i].ss_class);
                        rdata->call_forward_class_info_list[i].ton = rsp->call_forward_class_info_list[i].forwarded_to_number.ton;
                        rdata->call_forward_class_info_list[i].npi = rsp->call_forward_class_info_list[i].forwarded_to_number.npi;
                        strcpy(rdata->call_forward_class_info_list[i].number, rsp->call_forward_class_info_list[i].forwarded_to_number.number);
                        rdata->call_forward_class_info_list[i].noReplyTime = rsp->call_forward_class_info_list[i].noReplyTime;
                        KRIL_DEBUG(DBG_INFO, "activated:%d ss_class:%d noReplyTime:%d\n", rsp->call_forward_class_info_list[i].activated, rsp->call_forward_class_info_list[i].ss_class, rsp->call_forward_class_info_list[i].noReplyTime);
                        KRIL_DEBUG(DBG_INFO, "ton:%d npi:%d number:%s\n", rsp->call_forward_class_info_list[i].forwarded_to_number.ton, rsp->call_forward_class_info_list[i].forwarded_to_number.npi, rsp->call_forward_class_info_list[i].forwarded_to_number.number);
                    }
                }
                else
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                KRIL_SetServiceClassValue(0);            
            }
        }
        break;

        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            KRIL_SetServiceClassValue(0);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}



void KRIL_SetCallForwardStatusHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilCallForwardStatus_t *tdata = (KrilCallForwardStatus_t *)pdata->ril_cmd->data;
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;

            CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
            clientInfo.simId = pdata->ril_cmd->SimId;
            ssSrvReqPtr->ssCode = Kril_FwdReasonCode[tdata->reason];
            SS_SvcCls2BasicSrvGroup(tdata->ss_class, &(ssSrvReqPtr->basicSrv));
            ssSrvReqPtr->operation = Kril_FwdModeToOp[tdata->mode];


            ssSrvReqPtr->param.cfwInfo.include |= 0x01; /* SS_CallForwardInfo_t of ss_def.h */
            UtilApi_DialStr2PartyAdd(&ssSrvReqPtr->param.cfwInfo.partyAdd, tdata->number);            

            if (tdata->timeSeconds)
            {
                ssSrvReqPtr->param.cfwInfo.include |= 0x04; /* SS_CallForwardInfo_t of ss_def.h */
                ssSrvReqPtr->param.cfwInfo.noReplyTime = tdata->timeSeconds;
            }

            KRIL_DEBUG(DBG_INFO, "GetServiceClass(%d): type:%d content:%d\n", tdata->ss_class, ssSrvReqPtr->basicSrv.type, ssSrvReqPtr->basicSrv.content);
            KRIL_DEBUG(DBG_INFO, "Kril_FwdModeToOp[%d]:%d Kril_FwdReasonCode[%d]:%d ss_class:%d BasicSrv.type:%d BasicSrv.content:%d timeSeconds:%d number:%s\n", tdata->mode, Kril_FwdModeToOp[tdata->mode], tdata->reason, Kril_FwdReasonCode[tdata->reason], tdata->ss_class, ssSrvReqPtr->basicSrv.type, ssSrvReqPtr->basicSrv.content, tdata->timeSeconds, tdata->number);
            KRIL_DEBUG(DBG_INFO, "cfwInfo.include:%d \n", ssSrvReqPtr->param.cfwInfo.include);
            KRIL_DEBUG(DBG_INFO, "cfwInfo.partyAdd: ton:%d npi:%d num:%s len:%d\n", ssSrvReqPtr->param.cfwInfo.partyAdd.ton, ssSrvReqPtr->param.cfwInfo.partyAdd.npi, ssSrvReqPtr->param.cfwInfo.partyAdd.number, ssSrvReqPtr->param.cfwInfo.partyAdd.length);

            KRIL_SetSsSrvReqTID(GetTID()); // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
            CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KRIL_DEBUG(DBG_INFO, "msgType:0x%x\n", capi2_rsp->msgType);
            if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
            {
                SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n", srvRel_rsp->component, srvRel_rsp->operation, srvRel_rsp->ssCode, srvRel_rsp->type, srvRel_rsp->param.ussdInfo.dcs, srvRel_rsp->param.ussdInfo.length, srvRel_rsp->param.ussdInfo.data);
                if (srvRel_rsp->type == SS_SRV_TYPE_RETURN_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_REJECT ||
                    srvRel_rsp->type == SS_SRV_TYPE_LOCAL_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_CAUSE_IE)
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    if (srvRel_rsp->type == SS_SRV_TYPE_RETURN_ERROR)
                    {
                        KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? SS_ErrorCode_t:errorCode:%d\n", srvRel_rsp->param.returnError.errorCode);
                    }
                    else if (srvRel_rsp->type == SS_SRV_TYPE_LOCAL_ERROR)
                    {
                        KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? localResult:%d\n", srvRel_rsp->param.localResult);
                    }
                    else if (srvRel_rsp->type == SS_SRV_TYPE_CAUSE_IE)
                    {
                        KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? causeIe.cause:%d\n", srvRel_rsp->causeIe.cause);
                    }
                }
                else
                {
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
            {
                KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
            {
                StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR, "Receive error MsgType:0x%x...!\n", capi2_rsp->msgType);
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

void KRIL_QueryCallWaitingHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilCallWaitingInfo_t *tdata = (KrilCallWaitingInfo_t *)pdata->ril_cmd->data;
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;

            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilCallWaitingClass_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {    
                pdata->rsp_len = sizeof(KrilCallWaitingClass_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
    
                CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                clientInfo.simId = pdata->ril_cmd->SimId;
                ssSrvReqPtr->operation = SS_OPERATION_CODE_INTERROGATE;
                ssSrvReqPtr->ssCode = SS_CODE_CW;
    
                SS_SvcCls2BasicSrvGroup(tdata->ss_class, &(ssSrvReqPtr->basicSrv));
    
                KRIL_DEBUG(DBG_INFO, "GetServiceClass(%d): type:%d content:%d\n", tdata->ss_class, ssSrvReqPtr->basicSrv.type, ssSrvReqPtr->basicSrv.content);
                KRIL_SetSsSrvReqTID(GetTID()); // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
                CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);
    
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KRIL_DEBUG(DBG_INFO, "msgType:0x%x\n", capi2_rsp->msgType);
            if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
            {
                SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                KrilCallWaitingInfo_t *tdata = (KrilCallWaitingInfo_t *)pdata->ril_cmd->data;
                KrilCallWaitingClass_t *rdata = (KrilCallWaitingClass_t *)pdata->bcm_ril_rsp;
                int useful_listSize = 0;
                SsApi_SrvReq_t  ssApiSrvReq = {0};
                SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;

                SS_SvcCls2BasicSrvGroup(tdata->ss_class, &(ssSrvReqPtr->basicSrv));

                KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? callIndex:%d component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n",
                    srvRel_rsp->callIndex, srvRel_rsp->component,
                    srvRel_rsp->operation, srvRel_rsp->ssCode,
                    srvRel_rsp->type, srvRel_rsp->param.ussdInfo.dcs,
                    srvRel_rsp->param.ussdInfo.length,
                    srvRel_rsp->param.ussdInfo.data);

                pdata->handler_state = BCM_FinishCAPI2Cmd;
                switch (srvRel_rsp->type)
                {
                    case SS_SRV_TYPE_SS_STATUS:
                    {
                        KRIL_DEBUG(DBG_INFO, "ssStatus:%d\n", srvRel_rsp->param.ssStatus);
                        if ((srvRel_rsp->param.ssStatus & SS_STATUS_MASK_ACTIVE) != 0)
                        {
                            rdata->activated = 1;
                        }
                        else
                        {
                            rdata->activated = 0;
                        }
                        rdata->ss_class = StdClassToATClass(tdata->ss_class);
                        KRIL_DEBUG(DBG_INFO, "activated:%d ss_class:%d\n", rdata->activated, rdata->ss_class);
                        break;
                    }
                    case SS_SRV_TYPE_BASIC_SRV_INFORMATION:
                    {
                        // if a list is present and the operation is register or
                        // interrogate then the status should be assumed as
                        // active even if the netkwork doesn't send it.

                        if (srvRel_rsp->param.basicSrvInfo.listSize > 0)
                        {
                            int i = 0;
                            for (i = 0 ; i < srvRel_rsp->param.basicSrvInfo.listSize ; i++)
                            {
                                KRIL_DEBUG(DBG_INFO, "basicSrvGroupList[%d] --> type:%d content:0x%x\n", i, srvRel_rsp->param.basicSrvInfo.basicSrvGroupList[i].type, srvRel_rsp->param.basicSrvInfo.basicSrvGroupList[i].content);
                                if (srvRel_rsp->param.basicSrvInfo.basicSrvGroupList[i].type == ssSrvReqPtr->basicSrv.type ||
                                    ssSrvReqPtr->basicSrv.type == BASIC_SERVICE_TYPE_UNSPECIFIED)
                                {
                                    useful_listSize++;
                                rdata->ss_class |= SvcClassToATClass(GetSsClassFromSrvGrp(&srvRel_rsp->param.basicSrvInfo.basicSrvGroupList[i]));
                            }
                        }
                        }
                        else
                        {
                            rdata->ss_class = ATC_NOT_SPECIFIED;
                        }
                        rdata->activated = (useful_listSize > 0);
                        KRIL_DEBUG(DBG_INFO, "activated:%d ss_class:%d\n", rdata->activated, rdata->ss_class);
                        break;
                    }
                    case SS_SRV_TYPE_NONE:
                    {
                        KRIL_DEBUG(DBG_INFO, "Error: %d\n", srvRel_rsp->type);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_COMPONENT_TYPE_RETURN_ERROR:
                    {
                        KRIL_DEBUG(DBG_INFO, "RETURN_ERROR: SS_ErrorCode_t:%d\n",
                            srvRel_rsp->param.returnError.errorCode);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_COMPONENT_TYPE_REJECT:
                    {
                        KRIL_DEBUG(DBG_INFO, "REJECT: problemType:%d content:%d\n",
                            srvRel_rsp->param.reject.problemType,
                            srvRel_rsp->param.reject.content);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_SRV_TYPE_LOCAL_RESULT:
                    {
                        KRIL_DEBUG(DBG_INFO, "localResult::%d\n",
                            srvRel_rsp->param.localResult);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    case SS_SRV_TYPE_CAUSE_IE:
                    {
                        KRIL_DEBUG(DBG_INFO, "CAUSE_IE: codStandard:%d location:%d recommendation:%d cause:%d diagnostic:%d\n",
                            srvRel_rsp->causeIe.codStandard,
                            srvRel_rsp->causeIe.location,
                            srvRel_rsp->causeIe.recommendation,
                            srvRel_rsp->causeIe.cause,
                            srvRel_rsp->causeIe.diagnostic);
                        KRIL_DEBUG(DBG_ERROR, "CAUSE_IE: cause:%d\n", srvRel_rsp->causeIe.cause);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                    default:
                    {
                        KRIL_DEBUG(DBG_INFO, "Unhandled type: %d\n", srvRel_rsp->type);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        break;
                    }
                }
            }
            else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
            {
                KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
            {
                StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR, "msgType:%d\n", capi2_rsp->msgType);
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


void KRIL_SetCallWaitingHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilCallWaitingInfo_t *tdata = (KrilCallWaitingInfo_t *)pdata->ril_cmd->data;
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;

            CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
            clientInfo.simId = pdata->ril_cmd->SimId;
            ssSrvReqPtr->ssCode = SS_CODE_CW;
            SS_SvcCls2BasicSrvGroup(tdata->ss_class, &(ssSrvReqPtr->basicSrv));

            if (1 == tdata->state)
                ssSrvReqPtr->operation = SS_OPERATION_CODE_ACTIVATE;
            else
                ssSrvReqPtr->operation = SS_OPERATION_CODE_DEACTIVATE;

            KRIL_DEBUG(DBG_INFO, "GetServiceClass(%d): type:%d content:%d\n", tdata->ss_class, ssSrvReqPtr->basicSrv.type, ssSrvReqPtr->basicSrv.content);

            KRIL_SetSsSrvReqTID(GetTID()); // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
            CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);

            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KRIL_DEBUG(DBG_INFO, "msgType:0x%x\n", capi2_rsp->msgType);

            pdata->handler_state = BCM_FinishCAPI2Cmd;
            if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
            {
                SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n",
                    srvRel_rsp->component,
                    srvRel_rsp->operation, srvRel_rsp->ssCode,
                    srvRel_rsp->type, srvRel_rsp->param.ussdInfo.dcs,
                    srvRel_rsp->param.ussdInfo.length,
                    srvRel_rsp->param.ussdInfo.data);
                if (srvRel_rsp->type == SS_SRV_TYPE_RETURN_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_REJECT ||
                    srvRel_rsp->type == SS_SRV_TYPE_LOCAL_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_LOCAL_RESULT ||
                    srvRel_rsp->type == SS_SRV_TYPE_CAUSE_IE)
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
            }
            else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
            {
                KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                pdata->handler_state = BCM_RESPCAPI2Cmd;
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


void KRIL_ChangeBarringPasswordHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilCallBarringPasswd_t *tdata = (KrilCallBarringPasswd_t *)pdata->ril_cmd->data;
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;
            SS_Password_t*	ssPwdPtr = &ssSrvReqPtr->param.ssPassword;

            KRIL_DEBUG(DBG_INFO, "fac_id:%d OldPasswd:%s NewPasswd:%s NewPasswdConfirm:%s\n", 
                  tdata->fac_id, tdata->OldPasswd, tdata->NewPasswd, tdata->NewPassConfirm);
            
            CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
            clientInfo.simId = pdata->ril_cmd->SimId;
            ssSrvReqPtr->operation = SS_OPERATION_CODE_REGISTER_PASSWORD;
            ssSrvReqPtr->ssCode = ssBarringCodes[tdata->fac_id];
            ssSrvReqPtr->basicSrv.type = BASIC_SERVICE_TYPE_UNSPECIFIED;

            if (tdata->OldPasswd)
            {
                memcpy((void*)ssPwdPtr->currentPwd, (void*)tdata->OldPasswd, SS_PASSWORD_LENGTH);
            }

            if (tdata->NewPasswd)
            {
                memcpy((void*)ssPwdPtr->newPwd, (void*)tdata->NewPasswd, SS_PASSWORD_LENGTH);
                memcpy((void*)ssPwdPtr->reNewPwd, (void*)tdata->NewPassConfirm, SS_PASSWORD_LENGTH);
            }

            KRIL_SetSsSrvReqTID(GetTID());  // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
            CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);

            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KRIL_DEBUG(DBG_INFO, "msgType:0x%x\n", capi2_rsp->msgType);
            if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
            {
                SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n",
                    srvRel_rsp->component,
                    srvRel_rsp->operation, srvRel_rsp->ssCode,
                    srvRel_rsp->type, srvRel_rsp->param.ussdInfo.dcs,
                    srvRel_rsp->param.ussdInfo.length,
                    srvRel_rsp->param.ussdInfo.data);
                if (srvRel_rsp->operation == SS_OPERATION_CODE_REGISTER_PASSWORD &&
                    srvRel_rsp->ssCode == SS_CODE_ALL_CALL_RESTRICTION &&
                    srvRel_rsp->type == SS_SRV_TYPE_RETURN_ERROR)
                {
                    KRIL_DEBUG(DBG_INFO, "errorCode:%d", srvRel_rsp->param.returnError.errorCode);
                    pdata->result = BCM_E_PASSWORD_INCORRECT;
                }
                if (srvRel_rsp->type == SS_SRV_TYPE_RETURN_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_REJECT ||
                    srvRel_rsp->type == SS_SRV_TYPE_LOCAL_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_LOCAL_RESULT ||
                    srvRel_rsp->type == SS_SRV_TYPE_CAUSE_IE)
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
            {
                KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
            {
                StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR, "Receive error MsgType:0x%x...!\n", capi2_rsp->msgType);
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


void KRIL_QueryCLIPHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            ClientInfo_t    clientInfo;
            SsApi_SrvReq_t  ssApiSrvReq = {0};
            SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;

            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilCLIPInfo_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                pdata->rsp_len = sizeof(KrilCLIPInfo_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
    
                CAPI2_InitClientInfo(&clientInfo, GetNewTID(), GetClientID());
                clientInfo.simId = pdata->ril_cmd->SimId;
                ssSrvReqPtr->operation = SS_OPERATION_CODE_INTERROGATE;
                ssSrvReqPtr->basicSrv.type = BASIC_SERVICE_TYPE_UNSPECIFIED;
                ssSrvReqPtr->ssCode = SS_CODE_CLIP;
                KRIL_DEBUG(DBG_INFO, "Query CLIP\n");
    
                KRIL_SetSsSrvReqTID(GetTID());  // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0
                CAPI2_SsApi_SsSrvReq(&clientInfo, &ssApiSrvReq);
    
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KrilCLIPInfo_t *rdata = (KrilCLIPInfo_t *)pdata->bcm_ril_rsp;
            if(capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_INFO, "MSG_MNSS_CLIENT_SS_SRV_REL capi2_rsp->result:%d\n", capi2_rsp->result);
                rdata->value = SS_SERVICE_STATUS_UKNOWN;
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else
            {
                if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
                {
                    SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;

                    KRIL_DEBUG(DBG_INFO, "MSG_MNSS_CLIENT_SS_SRV_REL callIndex:%d component:%d operation:%d ssCode:%d type:%d ssStatus:%d\n",
                        srvRel_rsp->callIndex, srvRel_rsp->component,
                        srvRel_rsp->operation, srvRel_rsp->ssCode,
                        srvRel_rsp->type,
                        srvRel_rsp->param.ssStatus);

                    if (srvRel_rsp->type == SS_SRV_TYPE_GENERIC_SRV_INFORMATION)
                    {
                        KRIL_DEBUG(DBG_ERROR, "SS_SRV_TYPE_GENERIC_SRV_INFORMATION\n");

                        // CLI restriction option:
                        //  0 CLIP not provisioned
                        //  1 CLIP provisioned
                        //  2 unknown (e.g. no network, etc.)
                        if ((srvRel_rsp->param.genSrvInfo.ssStatus & SS_STATUS_MASK_PROVISIONED) ||
                            (srvRel_rsp->param.genSrvInfo.ssStatus & SS_STATUS_MASK_ACTIVE))
                        {
                            rdata->value = 1;
                        }
                        else
                        {
                            // Not provisioned
                            rdata->value = 0;
                        }
                        KRIL_DEBUG(DBG_INFO, "srvRel_rsp->param.genSrvInfo: ssStatus:%d\n",
                            srvRel_rsp->param.genSrvInfo.ssStatus);
                        KRIL_DEBUG(DBG_INFO, "rdata->value:%d\n", rdata->value);
                    }
                    else if (srvRel_rsp->type == SS_SRV_TYPE_SS_STATUS)
                    {
                        KRIL_DEBUG(DBG_ERROR, "SS_SRV_TYPE_SS_STATUS\n");

                        // CLI restriction option:
                        //  0 CLIP not provisioned
                        //  1 CLIP provisioned
                        //  2 unknown (e.g. no network, etc.)
                        if ((srvRel_rsp->param.ssStatus & SS_STATUS_MASK_PROVISIONED) ||
                            (srvRel_rsp->param.ssStatus & SS_STATUS_MASK_ACTIVE))
                        {
                            rdata->value = 1;
                        }
                        else
                        {
                            // Not provisioned
                            rdata->value = 0;
                        }
                        KRIL_DEBUG(DBG_INFO, "srvRel_rsp->param.ssStatus:%d\n",
                            srvRel_rsp->param.ssStatus);
                        KRIL_DEBUG(DBG_INFO, "rdata->value:%d\n", rdata->value);
                    }
                    else
                    {
                        KRIL_DEBUG(DBG_ERROR, "unknown response type, expecting SS_SRV_TYPE_GENERIC_SRV_INFORMATION or SS_SRV_TYPE_SS_STATUS\n");
                    }
                    KRIL_DEBUG(DBG_INFO, "srvRel_rsp->causeIe.codStandard:%d location:%d, recommendation:%d cause:%d diagnostic:%d facIeLength:%d\n",
                        srvRel_rsp->causeIe.codStandard,
                        srvRel_rsp->causeIe.location,
                        srvRel_rsp->causeIe.recommendation,
                        srvRel_rsp->causeIe.cause,
                        srvRel_rsp->causeIe.diagnostic,
                        srvRel_rsp->facIeLength);

                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
                else if (capi2_rsp->msgType == MSG_SSAPI_SSSRVREQ_RSP)
                {
                    KRIL_DEBUG(DBG_INFO, "MSG_SSAPI_SSSRVREQ_RSP\n");
                    pdata->handler_state = BCM_RESPCAPI2Cmd;
                }
                else 
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
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

void KRIL_SetSuppSvcNotificationHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
        // **FIXME** MAG - MS_LOCAL_SS_ELEM_NOTIFICATION_SWITCH not currently included in CIB; need to be integrated from 2157?
#ifndef CONFIG_BRCM_FUSE_RIL_CIB
        case BCM_SendCAPI2Cmd:
        {
            int *iEnable = (int *)(pdata->ril_cmd->data);
            CAPI2_MS_Element_t data;
            memset((UInt8*)&data, 0, sizeof(CAPI2_MS_Element_t));
            data.inElemType = MS_LOCAL_SS_ELEM_NOTIFICATION_SWITCH;
            data.data_u.u8Data = *iEnable;
            KRIL_DEBUG(DBG_INFO, "iEnable:%d\n", *iEnable);
            CAPI2_MsDbApi_SetElement(InitClientInfo(pdata->ril_cmd->SimId), &data);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }

        case BCM_RESPCAPI2Cmd:
        {
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }
#endif
        default:
        {
            KRIL_DEBUG(DBG_ERROR, "handler_state:%lu error...!\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
        }
    }
}
