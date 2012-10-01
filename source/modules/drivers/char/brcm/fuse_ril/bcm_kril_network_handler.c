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

#include "capi2_pch_api.h"
#include "capi2_stk_ds.h"
#include "capi2_pch_msg.h"
#include "capi2_gen_msg.h"
#include "capi2_reqrep.h"
#include "capi2_gen_api.h"
#include "plmn_table_samsung.h"		/* Irine_JSHAN_SunnyVale */

#define FILTER_GSM_SIGNAL_LEVEL 16

extern MSRegInfo_t  gRegInfo[DUAL_SIM_SIZE];
extern MSUe3gStatusInd_t  gUE3GInfo[DUAL_SIM_SIZE];
extern KrilDataCallResponse_t pdp_resp[DUAL_SIM_SIZE][BCM_NET_MAX_RIL_PDP_CNTXS];

bool block_operator_name = false;
/*+20111110 HKPARK if NO SIM status, send Error Message*/
// bool v_NoSimCheck2 = false;
// bool v_NoSimCheck1 = false;
/*-20111110 HKPARK if NO SIM status, send Error Message*/

UInt8 FindDunPdpCid(SimNumber_t SimId);
UInt8 ReleaseDunPdpContext(SimNumber_t SimId, UInt8 cid);
void Kpdp_pdp_deactivate_ind(int cid);


// The cause of register denied
static const UInt8 g_DeniedCause[] =
{
    -1,
    23,
    2,
    3,
    6,
    11,
    12,
    13,
    15,
    17
};
#define NUM_DENIEDCAUSE (sizeof(g_DeniedCause) / sizeof(int))

// Support Band Mode for System
BandSelect_t g_systemband = BAND_NULL;
//WCDMA-IMT-2000 in Android's context means WCDMA 2100Mhz
static UInt32 g_bandMode[] =
{
    BAND_AUTO, // "selected by baseband automatically"
    BAND_NULL, //BAND_GSM900_ONLY|BAND_DCS1800_ONLY|BAND_UMTS2100_ONLY, // "EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
    BAND_NULL, //BAND_GSM850_ONLY|BAND_PCS1900_ONLY|BAND_UMTS850_ONLY|BAND_UMTS1900_ONLY, //"US band"
    BAND_NULL, //BAND_UMTS2100_ONLY, // "JPN band" (WCDMA-800 / WCDMA-IMT-2000)
    BAND_NULL, //BAND_GSM900_ONLY|BAND_DCS1800_ONLY|BAND_UMTS850_ONLY|BAND_UMTS2100_ONLY,  // "AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
    BAND_NULL, //BAND_GSM900_ONLY|BAND_DCS1800_ONLY|BAND_UMTS850_ONLY, //"AUS band 2"
    BAND_NULL,  // "Cellular (800-MHz Band)"
    BAND_NULL, // "PCS (1900-MHz Band)" // Android MMI does not support
//band as below is CDMA mode
    BAND_NULL, // "Band Class 3 (JTACS Band)"
    BAND_NULL, // "Band Class 4 (Korean PCS Band)"(Tx = 1750-1780MHz)
    BAND_NULL, // "Band Class 5 (450-MHz Band)"
    BAND_NULL, // "Band Class 6 (2-GMHz IMT2000 Band)"
    BAND_NULL, // "Band Class 7 (Upper 700-MHz Band)"
    BAND_NULL, // "Band Class 8 (1800-MHz Band)"
    BAND_NULL, // "Band Class 9 (900-MHz Band)"
    BAND_NULL, // "Band Class 10 (Secondary 800-MHz Band)"
    BAND_NULL, // "Band Class 11 (400-MHz European PAMR Band)"
    BAND_NULL, // "Band Class 15 (AWS Band)"
    BAND_NULL, // "Band Class 16 (US 2.5-GHz Band)"
};
#define NUM_BANDMODE (sizeof(g_bandMode) / sizeof(UInt32))


static const UInt32 g_selbandMode[] =
{
	BAND_NULL,									//= 0,			///< 0
	BAND_AUTO,									//= 0x0001,		///< 0 0000 0001
	BAND_GSM900_ONLY,							//= 0x0002,		///< 0 0000 0010
	BAND_DCS1800_ONLY,							//= 0x0004,		///< 0 0000 0100
	BAND_GSM900_DCS1800,						//	= 0x0006,		///< 0 0000 0110
	BAND_PCS1900_ONLY,							//= 0x0008,		///< 0 0000 1000
	BAND_GSM850_ONLY,							//= 0x0010,		///< 0 0001 0000
	BAND_PCS1900_GSM850,						//	= 0x0018,		///< 0 0001 1000
	BAND_ALL_GSM,								//= 0x001E,		///< 0 0001 1110 All GSM band(900/1800/850/1900)
	BAND_UMTS2100_ONLY,							//= 0x0020,		///< 0 0010 0000
	BAND_GSM900_DCS1800_UMTS2100,				//= 0x0026,		///< 0 0010 0110
	BAND_UMTS1900_ONLY	,						//= 0x0040,		///< 0 0100 0000
	BAND_UMTS850_ONLY,							//= 0x0080,		///< 0 1000 0000
	BAND_UMTS850_UMTS1900,						//= 0x00C0,		///< 0 1100 0000 BAND II and BAND V 
	BAND_UMTS1700_ONLY	,						//= 0x0100,		///< 1 0000 0000
	BAND_PCS1900_GSM850_UMTS1900_UMTS850,		//= 0x01D8,		///< 1 1101 1000 Add 1700 to this group temporarily
 	BAND_UMTS900_ONLY ,         					//= 0x0200,       ///< 10 0000 0000 - GNATS TR 13152 - Band 8
 	BAND_UMTS900_UMTS2100,						//= 0x0220,		///< 10 0010 0000 - BAND I and BAND VIII
 	BAND_UMTS1800_ONLY  ,           				//= 0x0400,       ///< 100 0000 0000 - GNATS TR 13152 - Band 3
 	BAND_ALL_UMTS ,             					//= 0x07E0,		///< 111 1110 0000 All UMTS band(2100/1900/850/1700/900/1800)
 	BAND_ALL ,                  					//= 0x07FF	    ///< 111 1111 1111
};


/* START : Irine_JSHAN_SunnyVale */
const char* PLMN_GetCountryByMcc(UInt16 mcc)
{
	UInt16 i;

	for (i = 1; i < MSAP_COUNTRY_LIST_LENGTH; i++)
	{
		if (Plmn_Country_List[i].mcc == mcc)
		{
			return Plmn_Country_List[i].country_name;
		}
	}

	return PLMN_UNKNOWN_COUNTRY;
}


void GetSamsungPLMNname(
					Boolean ucs2,
					UInt16	plmn_mcc,
					UInt16	plmn_mnc,
					char *long_name, 
					char *short_name,
					const char	**country_name,
					Boolean deci  )
{

	typedef enum  {

		SEARCH_POSITION=0,

		SEARCH_BACKWARD,

		SEARCH_FORWARD

	}mnc_search;



	UInt16 i;

	UInt16 start=0;

	UInt16 end= NUMBER_OF_PLMN_ENTRY_SAMSUNG -1;

	UInt16 middle;

	UInt16 plmn_mcc_deci;  // AHJ_090817_PLMN table_modifiy

	UInt16 plmn_mnc_deci; // AHJ_090817_PLMN table_modifiy

	UInt16 name_size = 0;


	mnc_search mnc_search_flow;

	Boolean            found = FALSE;

	Boolean            continue_search = TRUE ;

	// ++JSHAN
	if(deci)
	{
		plmn_mcc_deci = plmn_mcc ; 
		plmn_mnc_deci = plmn_mnc ;
	}
	else 
	{
	// --JSHAN
	if( (plmn_mcc < 0x1) || (plmn_mcc > 0x901 ) || ((plmn_mcc == 0x1)&&(plmn_mnc < 0x1)) || ((plmn_mcc == 0x901)&& (plmn_mnc > 0x18)))
	{
		KRIL_DEBUG(DBG_INFO, "No PLMN name in PLMN table ::  MCC=%x, MNC=%x \n", plmn_mcc, plmn_mnc);
		return found;
	}
	plmn_mcc_deci = ((plmn_mcc & 0x0F00) >> 8) *100 + ((plmn_mcc & 0x00F0) >> 4)*10 + (plmn_mcc & 0x000F) ;
	plmn_mnc_deci = ((plmn_mnc & 0x0F00) >> 8) *100 + ((plmn_mnc & 0x00F0) >> 4)*10 + (plmn_mnc & 0x000F) ;
	KRIL_DEBUG(DBG_ERROR, "GetSamsungPLMNname ::  hex MCC=%x, MNC=%x \n", plmn_mcc, plmn_mnc);
		
	// ++JSHAN
	}
	// --JSHAN
	
	KRIL_DEBUG(DBG_ERROR, "GetSamsungPLMNname ::  deci MCC=%d, MNC=%d \n", plmn_mcc_deci, plmn_mnc_deci);

	if(ucs2)
	{	
		for (i = 0; i < NUMBER_OF_UCS2_PLMN; i++)
		{
			if( (Plmn_Ucs2_List[i].mnc == plmn_mnc) &&
				(Plmn_Country_List[Plmn_Ucs2_List[i].country_index].mcc == plmn_mcc) )
			{
				if(long_name != NULL)
				{
					//long_name->coding = (ALPHA_CODING_t)Plmn_Ucs2_List[i].ucs2_long_name[0];
					name_size = Plmn_Ucs2_List[i].long_name_size - 1;
					memcpy(long_name, Plmn_Ucs2_List[i].ucs2_long_name + 1, name_size);
				}

				if(short_name != NULL)
				{
					//short_name->coding = (ALPHA_CODING_t)Plmn_Ucs2_List[i].ucs2_short_name[0];
					name_size = Plmn_Ucs2_List[i].short_name_size - 1;
					memcpy(short_name, Plmn_Ucs2_List[i].ucs2_short_name + 1, name_size);
				}

				if (country_name != NULL)
				{
					*country_name = Plmn_Country_List[Plmn_Ucs2_List[i].country_index].country_name;
				}

				return TRUE;

			}

		}

	}

	while((start <= end ) && ( continue_search == TRUE ))
 	{
		middle = (start+end)/2;

  		if (plmn_mcc_deci == Plmn_Table_Samsung[ middle ].mcc)
		{
			mnc_search_flow = SEARCH_POSITION;

			while ( plmn_mcc_deci == Plmn_Table_Samsung[ middle ].mcc)
  			{

 				if (plmn_mnc_deci == Plmn_Table_Samsung[ middle ].mnc)
				{

					if (long_name != NULL)
					{
						//long_name->coding = ALPHA_CODING_GSM_ALPHABET;
						name_size = strlen(Plmn_Table_Samsung[middle].full_name_ptr);
						memcpy(long_name, Plmn_Table_Samsung[middle].full_name_ptr, name_size);
					}

					if (short_name != NULL)
					{
						//short_name->coding = ALPHA_CODING_GSM_ALPHABET;
						name_size = strlen(Plmn_Table_Samsung[middle].short_name_ptr);
						memcpy(short_name, Plmn_Table_Samsung[middle].short_name_ptr, name_size);
					}

			
					found = TRUE;

					break;		
				}

  				else if (plmn_mnc_deci < Plmn_Table_Samsung[ middle ].mnc) 
  				{
  					if (mnc_search_flow == SEARCH_FORWARD)
						break;

  					middle--;

  					mnc_search_flow= SEARCH_BACKWARD;
  				}

  				else if (plmn_mnc_deci > Plmn_Table_Samsung[ middle ].mnc)
  				{
  					if (mnc_search_flow == SEARCH_BACKWARD)
						break;

					middle++;

  					mnc_search_flow= SEARCH_FORWARD;
    				}

  			}

			continue_search  = FALSE;

		}

		else if (plmn_mcc_deci < Plmn_Table_Samsung[ middle ].mcc)
		{
			end = middle -1;

		}

		else if (plmn_mcc_deci > Plmn_Table_Samsung[ middle ].mcc)
		{
			start = middle +1;

		}

	}


	if ( found == TRUE )
	{

		if (country_name != NULL)
		{
			*country_name = PLMN_GetCountryByMcc(plmn_mcc);
		}
		KRIL_DEBUG(DBG_INFO, "PLMN is found in Samsung table\n");
	}

	else
	{
		KRIL_DEBUG(DBG_INFO, "PLMN is not found in Samsung table\n");
	}

	return found;


}

/* END : Irine_JSHAN_SunnyVale */

//******************************************************************************
// Function Name: ConvertNetworkType
//
// Description: Convert RIL network type value to equivalent RATSelect_t
//
//******************************************************************************
RATSelect_t ConvertNetworkType(int type)
{
    if (0 == type || 0xFF == type) // 0xFF is default value, so also use DUAL_MODE_UMTS_PREF for default
    {
        return DUAL_MODE_UMTS_PREF;
    }
    else if (1 == type)
    {
        return GSM_ONLY;
    }
    else if (2 == type)
    {
        return UMTS_ONLY;
    }
    else
    {
        return INVALID_RAT;
    }
}

//******************************************************************************
// Function Name: ConvertRATType
//
// Description: Convert the RAT type(RATSelect_t) value to equivalent set of Android values
//
//******************************************************************************
int ConvertRATType(RATSelect_t type)
{
    if(type == GSM_ONLY)
    {
        return 1;
    }
    else  if(type == UMTS_ONLY)
    {
        return 2;
    }
    else  if(type == DUAL_MODE_GSM_PREF || type == DUAL_MODE_UMTS_PREF)
    {
        return 0;
    }
    else
    {
        return 0xFF;
    }
}

//******************************************************************************
// Function Name: ConvertBandMode
//
// Description: Convert RIL band mode value to equivalent set of BandSelect_t values
//
//******************************************************************************
BandSelect_t ConvertBandMode(int mode)
{
    if(mode==BAND_NULL)
    {
        if (mode >= 0 &&
            mode < NUM_BANDMODE)
        {
            return (g_bandMode[mode] & g_systemband);
        }
        else
        {
            return BAND_NULL;
        }
    }
    else
    {
        return g_selbandMode[mode];
    }
}

//******************************************************************************
// Function Name: MS_PlmnConvertRawMcc	
//
// Description: This function converts the MCC values: 
// For example: from 0x1300 to 0x310 for Pacific Bell.
//******************************************************************************
UInt16 MS_PlmnConvertRawMcc(UInt16 mcc)
{
    return ( (mcc & 0x0F00) | ((mcc & 0xF000) >> 8) | (mcc & 0x000F) );
}

//******************************************************************************
// Function Name: MS_PlmnConvertRawMnc	
//
// Description: This function converts the MNC values. 
// The third digit of MNC may be stored in the third nibble of the passed MCC.
// For example: the passed MCC is 0x1300 and passed MNC is 0x0071 for Pacific Bell.
// This function returns the converted MNC value as 0x170
//******************************************************************************
UInt16 MS_PlmnConvertRawMnc(UInt16 mcc, UInt16 mnc)
{
    UInt16 converted_mcc;

    if( (mcc & 0x00F0) == 0x00F0 )
    {
        converted_mcc = MS_PlmnConvertRawMcc(mcc);

        if( (converted_mcc == 0x302) ||  ((converted_mcc >= 0x0310) && (converted_mcc <= 0x0316)) )
        {
            /* This is a North America PCS network, the third MNC digit is 0: see Annex A of GSM 03.22.
             * Canadian PCS networks (whose MCC is 0x302) also use 3-digit MNC, even though GSM 03.22 does
             * not specify it.
             */
            return ( (mnc & 0x00F0) | ((mnc & 0x000F) << 8) );
        }
        else
        {
            /* Two digit MNC */
            return ( ((mnc & 0x00F0) >> 4) | ((mnc & 0x000F) << 4) );
        }
    }
    else
    {
        /* Three digit MNC */
        return ( (mnc & 0x00F0) | ((mnc & 0x000F) << 8) | ((mcc & 0x00F0) >> 4) );
    }
}

//******************************************************************************
// Function Name: MS_ConvertRawPlmnToHer	
//
// Description: 
//******************************************************************************
void MS_ConvertRawPlmnToHer(UInt16 mcc, UInt16 mnc, UInt8 *plmn_str, UInt8 plmn_str_sz)
{
    unsigned short mcc_code = MS_PlmnConvertRawMcc( mcc );
    unsigned short mnc_code = MS_PlmnConvertRawMnc( mcc, mnc );

    if( (mcc & 0x00F0) != 0x00F0 ) {
        sprintf((char *) plmn_str, "%03X%03X", mcc_code, mnc_code);
    }
    else {
        sprintf((char *) plmn_str, "%03X%02X", mcc_code, mnc_code);
    }
}

void BCMSetSupportedRATandBand(KRIL_CmdList_t *pdata)
{
    KrilSetPreferredNetworkType_t *tdata = (KrilSetPreferredNetworkType_t *)pdata->ril_cmd->data;
    KRIL_DEBUG(DBG_INFO, "SimId:%d networktype1:%d networktype2:%d band:%d\n", pdata->ril_cmd->SimId, tdata->networktype, tdata->networktypeEx, tdata->band);
    KRIL_SetPreferredNetworkType(SIM_DUAL_FIRST, tdata->networktype);
    KRIL_SetPreferredNetworkType(SIM_DUAL_SECOND, tdata->networktypeEx);
    CAPI2_NetRegApi_SetSupportedRATandBand(InitClientInfo(pdata->ril_cmd->SimId), ConvertNetworkType(tdata->networktype), ConvertBandMode(tdata->band), ConvertNetworkType(tdata->networktypeEx), ConvertBandMode(tdata->band));

	KRIL_SetHandling2GOnlyRequest(pdata->ril_cmd->SimId, TRUE ); 

    pdata->handler_state = BCM_RESPCAPI2Cmd;
}

void KRIL_SignalStrengthHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSignalStrength_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pdata->rsp_len = sizeof(KrilSignalStrength_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                ((KrilSignalStrength_t*)pdata->bcm_ril_rsp)->RAT = gRegInfo[pdata->ril_cmd->SimId].netInfo.rat;
                KRIL_DEBUG(DBG_INFO, "SignalStrengthHandler::presult:%d\n", ((KrilSignalStrength_t*)pdata->bcm_ril_rsp)->RAT);
                CAPI2_PhoneCtrlApi_GetRxSignalInfo(InitClientInfo(pdata->ril_cmd->SimId));
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KRIL_DEBUG(DBG_INFO, "result:0x%x\n", capi2_rsp->result);
            if(RESULT_OK != capi2_rsp->result)
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if(NULL != capi2_rsp->dataBuf)
            {
                MsRxLevelData_t* presult = (MsRxLevelData_t*) capi2_rsp->dataBuf;
                KrilSignalStrength_t *rdata = (KrilSignalStrength_t *)pdata->bcm_ril_rsp;

                rdata->RxLev = presult->RxLev;
                rdata->RxQual = presult->RxQual;
                KRIL_DEBUG(DBG_INFO, "rdata->RAT:%d RxLev:%d RxQual:%d\n", rdata->RAT, rdata->RxLev, rdata->RxQual);
                pdata->handler_state = BCM_FinishCAPI2Cmd;
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


void KRIL_RegistationStateHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilRegState_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pdata->rsp_len = sizeof(KrilRegState_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);    
                KRIL_DEBUG(DBG_TRACE, "handler state:%lu\n", pdata->handler_state);
                CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_NETWORK_ELEM_REGSTATE_INFO);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if(capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_ERROR, "Error result:0x%x\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if(NULL != capi2_rsp->dataBuf)
            {
                KrilRegState_t *rdata = (KrilRegState_t *)pdata->bcm_ril_rsp;
                MSRegStateInfo_t* presult = NULL; 
                CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*)capi2_rsp->dataBuf;
                presult = (MSRegStateInfo_t*)(&(rsp->data_u));

                rdata->gsm_reg_state = presult->gsm_reg_state;

                /* GCF TC 26.7.4.3.4 Step 81, MO call attempt blocked by Android UI
                 * Refer to Android framework: GsmServiceStateTracker.handlePollStateResult(.) and GsmServiceStateTracker.regCodeToServiceState(.)
                 */
                if (REG_STATE_LIMITED_SERVICE == rdata->gsm_reg_state  &&
                    GSMCAUSE_REG_NO_ACCESS_MAX_ATTEMPT == gRegInfo[pdata->ril_cmd->SimId].cause)
                {
                    rdata->gsm_reg_state = REG_STATE_NORMAL_SERVICE;
                }

                rdata->gprs_reg_state = presult->gprs_reg_state;
                rdata->mcc = presult->mcc;
                rdata->mnc = presult->mnc;
                rdata->band = presult->band;
                rdata->lac = presult->lac;
                rdata->cell_id = presult->cell_id;
                gRegInfo[pdata->ril_cmd->SimId].netInfo.rat = presult->rat;

                if (presult->gsm_reg_state != REG_STATE_NORMAL_SERVICE
                    && presult->gsm_reg_state != REG_STATE_ROAMING_SERVICE
                    && presult->gsm_reg_state != REG_STATE_LIMITED_SERVICE
                    && presult->gprs_reg_state != REG_STATE_NORMAL_SERVICE
                    && presult->gprs_reg_state != REG_STATE_ROAMING_SERVICE
                    && presult->gprs_reg_state != REG_STATE_LIMITED_SERVICE)
                {
                    rdata->network_type = 0; //Unknown
                }
                else if (presult->rat == RAT_UMTS)
                {
                    if (SUPPORTED == gRegInfo[pdata->ril_cmd->SimId].netInfo.hsdpa_supported ||
                        TRUE == gUE3GInfo[pdata->ril_cmd->SimId].in_uas_conn_info.hsdpa_ch_allocated)
                    {
                        rdata->network_type = 9; //HSDPA
                    }
                    else if (SUPPORTED == gRegInfo[pdata->ril_cmd->SimId].netInfo.hsupa_supported)
                    {
                        rdata->network_type = 10; //HSUPA
                    }
                    else
                    {
                        rdata->network_type = 3; //UMTS
                    }
                }
                else if (presult->rat == RAT_GSM)
                {
                    if (SUPPORTED == gRegInfo[pdata->ril_cmd->SimId].netInfo.egprs_supported)
                    {
                        rdata->network_type = 2; //EDGE
                    }
                    else
                    {
                        rdata->network_type = 1; //GPRS
                    }
                }
                else
                {
                    rdata->network_type = 0; //Unknown
                }

                if(REG_STATE_LIMITED_SERVICE == presult->gsm_reg_state) // set denied cause
                {
                    UInt8 i;
                    rdata->cause = 0;
                    for(i = 0 ; i < NUM_DENIEDCAUSE ; i++)
                    {
                        if(g_DeniedCause[i] == gRegInfo[pdata->ril_cmd->SimId].netCause)
                        {
                            rdata->cause = i;
                            break;
                        }
                    }
                }
                KRIL_DEBUG(DBG_INFO, "regstate:%d gprs_reg_state:%d mcc:ox%x mnc:0x%x rat:%d lac:%d cell_id:%d network_type:%d band:%d cause:%d\n", rdata->gsm_reg_state, rdata->gprs_reg_state, rdata->mcc, rdata->mnc, presult->rat, rdata->lac, rdata->cell_id, rdata->network_type, rdata->band, rdata->cause);
                
				if (presult->rat == RAT_UMTS)
				{
                    if (TRUE == presult->uasConnInfo.ue_out_of_service &&
                       (TRUE == gRegInfo[pdata->ril_cmd->SimId].netInfo.hsdpa_supported || TRUE == gRegInfo[pdata->ril_cmd->SimId].netInfo.hsupa_supported)) // if UAS in out of services, MMI need to display the no_services.
                    {
                        rdata->gsm_reg_state = REG_STATE_NO_SERVICE;
                        rdata->gprs_reg_state = REG_STATE_NO_SERVICE;
                        rdata->network_type = 0; //Unknown
                    }
					KRIL_DEBUG(DBG_ERROR, "handler state:BCM_MS_GetRNC\n");
					CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_NETWORK_ELEM_RNC);
					pdata->handler_state = BCM_MS_GetRNC;
				}
				else
				{
					pdata->handler_state = BCM_FinishCAPI2Cmd;
				}
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

		case BCM_MS_GetRNC:
		{
			if(capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_ERROR, "Error result:0x%x\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if(NULL != capi2_rsp->dataBuf)
            {
                KrilRegState_t *rdata = (KrilRegState_t *)pdata->bcm_ril_rsp;
				UInt16* pRncValue = NULL;
                
                CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*)capi2_rsp->dataBuf;
                pRncValue = (UInt16*)(&(rsp->data_u));
                rdata->cell_id += ((*pRncValue) << 16);

				KRIL_DEBUG(DBG_ERROR, "rdata->cell_id:0x%x 0d%d RNC=0x%x\n", rdata->cell_id, rdata->cell_id, *pRncValue);
                pdata->handler_state = BCM_FinishCAPI2Cmd;
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


void KRIL_OperatorHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    /* Irine_JSHAN_SunnyVale */
    static UInt16		v_current_mcc = 0;
    static UInt16		v_current_mnc = 0;	

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
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilOperatorInfo_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pdata->rsp_len = sizeof(KrilOperatorInfo_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_NETWORK_ELEM_REGSTATE_INFO);
                pdata->handler_state = BCM_MS_GetRegistrationInfo;
            }
        }
        break;

        case BCM_MS_GetRegistrationInfo:
        {
            if(RESULT_OK != capi2_rsp->result)
            {
                KRIL_DEBUG(DBG_INFO, "result:0x%x\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            else
            {
                CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*)capi2_rsp->dataBuf;
                MSRegStateInfo_t* presult = NULL; 
                if ( rsp && (rsp->inElemType == MS_NETWORK_ELEM_REGSTATE_INFO) )
                {
                    presult = (MSRegStateInfo_t*)(&(rsp->data_u));
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR,"unexpected response retrieving CAPI2_MS_GetElement !!\n");
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    break;
                }
                if ((presult->gsm_reg_state != REG_STATE_NORMAL_SERVICE && presult->gsm_reg_state != REG_STATE_ROAMING_SERVICE) ||
                    (TRUE == presult->uasConnInfo.ue_out_of_service && (TRUE == gRegInfo[pdata->ril_cmd->SimId].netInfo.hsdpa_supported || TRUE == gRegInfo[pdata->ril_cmd->SimId].netInfo.hsupa_supported)))
                {
                    pdata->result = BCM_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    KrilOperatorInfo_t *rdata = (KrilOperatorInfo_t *)pdata->bcm_ril_rsp;
                    unsigned char oper_str[7];
                    KRIL_DEBUG(DBG_INFO, "mcc:%d mnc:%d lac:%d\n",presult->mcc, presult->mnc, presult->lac);
                    MS_ConvertRawPlmnToHer(presult->mcc, presult->mnc, oper_str, 7);
					
			 /* START :  Irine_JSHAN_SunnyVale */
			 v_current_mcc = MS_PlmnConvertRawMcc(presult->mcc);
			 v_current_mnc = MS_PlmnConvertRawMnc(presult->mcc , presult->mnc);
			 KRIL_DEBUG(DBG_INFO, "**v_current_mcc:%d v_current_mnc:%d \n",v_current_mcc, v_current_mnc);
			 /* END :  Irine_JSHAN_SunnyVale */
			 
                    strcpy(rdata->numeric, oper_str);
                    CAPI2_NetRegApi_GetPLMNNameByCode(InitClientInfo(pdata->ril_cmd->SimId), presult->mcc, presult->mnc, presult->lac, FALSE);
                    pdata->handler_state = BCM_RESPCAPI2Cmd;
                }

				if ((presult->gsm_reg_state != REG_STATE_NORMAL_SERVICE) 
					   && (presult->gsm_reg_state != REG_STATE_ROAMING_SERVICE))
				{
					block_operator_name = TRUE;
				}
				else
				{
					block_operator_name = FALSE;
				}
/*+20111110 HKPARK if NO SIM status, send Error Message*/				
//		  if((v_NoSimCheck2 ==true)&&(v_NoSimCheck1 ==true))
//		  {
//                    pdata->result = BCM_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
//                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
//		  }
/*-20111110 HKPARK if NO SIM status, send Error Message*/		 
				
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if ( MSG_MS_PLMN_NAME_RSP == capi2_rsp->msgType )
            {
                CAPI2_NetRegApi_GetPLMNNameByCode_Rsp_t* nameResult = (CAPI2_NetRegApi_GetPLMNNameByCode_Rsp_t*)capi2_rsp->dataBuf;
                if ( capi2_rsp->dataLength == sizeof(CAPI2_NetRegApi_GetPLMNNameByCode_Rsp_t) )
                {
                	    /* START : Irine_JSHAN_SunnyVale */
                    //if ( nameResult->val )
                    //{
			     /* END :  Irine_JSHAN_SunnyVale */
                        KrilOperatorInfo_t *rdata = (KrilOperatorInfo_t *)pdata->bcm_ril_rsp;
			     /* START : Irine_JSHAN_SunnyVale */
			     Boolean		coding = FALSE;
			     /* END :  Irine_JSHAN_SunnyVale */
                        KRIL_DEBUG(DBG_INFO, "longName:%s size %d shortName:%s %d\n", nameResult->long_name.name,nameResult->long_name.name_size, nameResult->short_name.name,nameResult->short_name.name_size);

                        /* START : Irine_JSHAN_SunnyVale */
                        /*
                        memcpy(rdata->longname, nameResult->long_name.name, (nameResult->long_name.name_size < PLMN_LONG_NAME)?nameResult->long_name.name_size:PLMN_LONG_NAME );
                        memcpy(rdata->shortname, nameResult->short_name.name, (nameResult->short_name.name_size < PLMN_SHORT_NAME)?nameResult->short_name.name_size:PLMN_SHORT_NAME);
                        */
                        if( (nameResult->long_name.nameType == PLMN_NAME_TYPE_LKUP_TABLE)  || (nameResult->long_name.nameType == PLMN_NAME_TYPE_INVALID) )
			     {
			     		coding = (nameResult->long_name.coding==0x00)?FALSE:TRUE;
					GetSamsungPLMNname(coding, v_current_mcc, v_current_mnc, rdata->longname, NULL, NULL,FALSE);
			     }
			     else
                        {
                        		memcpy(rdata->longname, nameResult->long_name.name, (nameResult->long_name.name_size < PLMN_LONG_NAME)?nameResult->long_name.name_size:PLMN_LONG_NAME );
			     }

			     if( (nameResult->short_name.nameType == PLMN_NAME_TYPE_LKUP_TABLE) || (nameResult->short_name.nameType == PLMN_NAME_TYPE_INVALID) )
                        {
                        		coding = (nameResult->short_name.coding==0x00)?FALSE:TRUE;              
					GetSamsungPLMNname(coding, v_current_mcc, v_current_mnc, NULL,  rdata->shortname, NULL,FALSE);
                        }			   	
			     else
                        {
                        		memcpy(rdata->shortname, nameResult->short_name.name, (nameResult->short_name.name_size < PLMN_SHORT_NAME)?nameResult->short_name.name_size:PLMN_SHORT_NAME);
                        }
                        /* END :  Irine_JSHAN_SunnyVale */
                    /* START : Irine_JSHAN_SunnyVale */
                    //}
                    //else
                    if (0 == strlen(rdata->longname))
                    {
                    /* END :  Irine_JSHAN_SunnyVale */
                        // lookup failed, but don't return an error; if rdata->longname is empty, URIL
                        // will pass numeric string back to Android RIL as longname
                        KRIL_DEBUG(DBG_INFO, "CAPI2_MS_GetPLMNNameByCode result FALSE, just returning numeric...\n");
                    }

		    if(block_operator_name == TRUE)
		    {
			rdata->longname[0] = '\0';
			rdata->shortname[0] = '\0';
		    }
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR, "** MSG_MS_PLMN_NAME_RSP size mismatch got %ld expected %d\n", capi2_rsp->dataLength, sizeof(MsPlmnName_t) );
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR, "Unexpected response message for CAPI2_MS_GetPLMNNameByCode 0x%x tid %ld\n",capi2_rsp->msgType, capi2_rsp->tid);
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

void KRIL_QueryNetworkSelectionModeHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSelectionMode_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pdata->rsp_len = sizeof(KrilSelectionMode_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_LOCAL_PHCTRL_ELEM_PLMN_MODE);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KrilSelectionMode_t *rdata = (KrilSelectionMode_t *)pdata->bcm_ril_rsp;
            CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*) capi2_rsp->dataBuf;
            PlmnSelectMode_t presult;
            memcpy(&presult, &(rsp->data_u), sizeof(PlmnSelectMode_t));
            // Android only "understands" manual (1) or auto (0) so map PlmnSelectMode_t to this
            if (PLMN_SELECT_AUTO == presult ||
               PLMN_SELECT_MANUAL_FORCE_AUTO == presult)
            {
                //automatic selection
                rdata->selection_mode = 0;
            }
            else if (PLMN_SELECT_MANUAL == presult)
            {
                //manual selection
                rdata->selection_mode = 1;
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR, "Error PLMN mode:%d...!\n", presult);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            KRIL_DEBUG(DBG_INFO, "selection_mode:%d PlmnSelectMode:%d\n", rdata->selection_mode, presult);
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


void KRIL_SetNetworkSelectionAutomaticHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            // we're not re-entrant, so indicate to command thread that we're already handling 
            // a network select request 
            KRIL_SetInNetworkSelectHandler( TRUE );
            KRIL_DEBUG(DBG_INFO, "Calling CAPI2_NetRegApi_PlmnSelect\n");
            CAPI2_NetRegApi_PlmnSelect(InitClientInfo(pdata->ril_cmd->SimId), FALSE, PLMN_SELECT_AUTO, PLMN_FORMAT_LONG, "");
            KRIL_SetNetworkSelectTID( GetTID() );
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if ( MSG_PLMN_SELECT_RSP == capi2_rsp->msgType )
            {
                if ( RESULT_OK != capi2_rsp->result )
                {
                    KRIL_DEBUG(DBG_ERROR, "CAPI2_NetRegApi_PlmnSelect error result:%d, exiting handler\n", capi2_rsp->result);
                    pdata->result = RILErrorResult(capi2_rsp->result);
                    KRIL_SetNetworkSelectTID( 0 );
                    KRIL_SetInNetworkSelectHandler( FALSE );
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    // payload is a boolean indicating whether or not request was sent to the network; if not sent, then
                    // we've successfully completed the request; if it was sent, we need to wait for the MSG_PLMN_SELECT_CNF
                    // response...
                    Boolean bSent = *((Boolean*) capi2_rsp->dataBuf);
                    if ( !bSent )
                    {
                        KRIL_DEBUG(DBG_INFO, "CAPI2_NetRegApi_PlmnSelect successfully completed (not sent)\n");
                        KRIL_SetNetworkSelectTID( 0 );
                        KRIL_SetInNetworkSelectHandler( FALSE );
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
                    }
                    else
                    {
                        // if we get to here, things are OK and we should just have to wait for the MSG_PLMN_SELECT_CNF msg
                        KRIL_DEBUG( DBG_INFO, "CAPI2_NetRegApi_PlmnSelect no error, waiting for MSG_PLMN_SELECT_CNF\n" );
                    }
                }
            }
            else if ( MSG_PLMN_SELECT_CNF ==  capi2_rsp->msgType )
            {
                UInt16 presult = *((UInt16*) capi2_rsp->dataBuf);
                KRIL_DEBUG(DBG_INFO, "MSG_PLMN_SELECT_CNF presult:%d\n", presult);
                if(OPERATION_SUCCEED == presult || NO_REJECTION == presult)
                {
                    KRIL_DEBUG(DBG_INFO, "Completed OK presult: %d\n", presult );
                    KRIL_SetInNetworkSelectHandler( FALSE );
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR, "error result:%d, calling CAPI2_NetRegApi_AbortPlmnSelect...\n", presult);
                    CAPI2_NetRegApi_AbortPlmnSelect(InitClientInfo(pdata->ril_cmd->SimId));
                    pdata->handler_state = BCM_MS_AbortPlmnSelect;
                }
            }
            else
            {
                KRIL_DEBUG( DBG_ERROR, "CAPI2_NetRegApi_PlmnSelect unexpected message 0x%x\n", capi2_rsp->msgType );
                KRIL_SetNetworkSelectTID( 0 );
                KRIL_SetInNetworkSelectHandler( FALSE );
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        case BCM_MS_AbortPlmnSelect:
        {
            KRIL_DEBUG(DBG_INFO, "CAPI2_NetRegApi_AbortPlmnSelect result %d\n", capi2_rsp->result);
            KRIL_SetInNetworkSelectHandler( FALSE );
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
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


void KRIL_SetNetworkSelectionManualHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            KrilManualSelectInfo_t *tdata = (KrilManualSelectInfo_t *)pdata->ril_cmd->data;
            // we're not re-entrant, so indicate to command thread that we're already handling 
            // a network select request 
            KRIL_SetInNetworkSelectHandler( TRUE );
            KRIL_DEBUG(DBG_INFO, "network_info:%s\n", tdata->networkInfo);
            CAPI2_NetRegApi_PlmnSelect(InitClientInfo(pdata->ril_cmd->SimId), FALSE, PLMN_SELECT_MANUAL, PLMN_FORMAT_NUMERIC, (char *)tdata->networkInfo);
            KRIL_SetNetworkSelectTID( GetTID() );
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            // we should first get a MSG_PLMN_SELECT_RSP response indicating that the request has been 
            // sent to the network, followed by a MSG_PLMN_SELECT_CNF message with the result from the network
            if ( MSG_PLMN_SELECT_RSP == capi2_rsp->msgType )
            {
                if ( RESULT_OK != capi2_rsp->result )
                {
                    // request failed before it was issued to network, so we shouldn't need to call abort here...
                    KRIL_DEBUG(DBG_ERROR, "CAPI2_NetRegApi_PlmnSelect capi2 error result:%d\n", capi2_rsp->result);
                    pdata->result = RILErrorResult(capi2_rsp->result);
                    KRIL_SetNetworkSelectTID( 0 );
                    KRIL_SetInNetworkSelectHandler( FALSE );
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
                else
                {
                    // payload is a boolean indicating whether or not request was sent to the network; if not sent, then
                    // we've successfully completed the request; if it was sent, we need to wait for the MSG_PLMN_SELECT_CNF
                    // response...
                    Boolean bSent = *((Boolean*) capi2_rsp->dataBuf);
                    if ( !bSent )
                    {
                        KRIL_DEBUG(DBG_INFO, "CAPI2_NetRegApi_PlmnSelect successfully completed (not sent)\n");
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
                        KRIL_SetNetworkSelectTID( 0 );
                        KRIL_SetInNetworkSelectHandler( FALSE );
                    }
                    else
                    {
                        // if we get to here, things are OK and we should just have to wait for the MSG_PLMN_SELECT_CNF msg
                        KRIL_DEBUG( DBG_INFO, "CAPI2_NetRegApi_PlmnSelect no error, waiting for MSG_PLMN_SELECT_CNF\n" );
                    }
                }
            }
            else if ( MSG_PLMN_SELECT_CNF ==  capi2_rsp->msgType )
            {
                UInt16 presult = *((UInt16*) capi2_rsp->dataBuf);
                KRIL_DEBUG(DBG_INFO, "MSG_PLMN_SELECT_CNF presult:%d\n", presult);
                if(OPERATION_SUCCEED == presult || NO_REJECTION == presult)
                {
                    KRIL_DEBUG(DBG_INFO, "Completed OK presult: %d\n", presult);
                    KRIL_SetInNetworkSelectHandler( FALSE );
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR, "error result:%d, calling CAPI2_NetRegApi_AbortPlmnSelect...\n", presult);
                    KRIL_SetNetworkSelectTID( 0 );
                    KRIL_SetInNetworkSelectHandler( FALSE );
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                }
            }
            else
            {
                // unexpected message
                KRIL_DEBUG(DBG_ERROR, "unexpected message 0x%x\n", capi2_rsp->msgType);
                KRIL_SetNetworkSelectTID( 0 );
                KRIL_SetInNetworkSelectHandler( FALSE );
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


void KRIL_QueryAvailableNetworksHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    /* Irine_JSHAN_SunnyVale */
    UInt16		v_current_mcc = 0;
    UInt16		v_current_mnc = 0;
    unsigned char oper_str[7];

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
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilNetworkList_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pdata->rsp_len = sizeof(KrilNetworkList_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                CAPI2_NetRegApi_SearchAvailablePLMN(InitClientInfo(pdata->ril_cmd->SimId));
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            SEARCHED_PLMN_LIST_t *rsp = (SEARCHED_PLMN_LIST_t *)capi2_rsp->dataBuf;
            UInt8 i, j;
            Boolean match = FALSE;
            KrilNetworkList_t *rdata = (KrilNetworkList_t *)pdata->bcm_ril_rsp;
            rdata->num_of_plmn = 0;

	      /* START : Irine_JSHAN_SunnyVale */
	      Boolean		coding = FALSE;
	      /* END :  Irine_JSHAN_SunnyVale */

            KRIL_DEBUG(DBG_INFO, "BCM_RESPCAPI2Cmd::num_of_plmn:%d plmn_cause:%d\n", rsp->num_of_plmn, rsp->plmn_cause);
            if (rsp->plmn_cause != PLMNCAUSE_SUCCESS)
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            for(i = 0 ; i < rsp->num_of_plmn ; i++)
            {
                for (j = 0 ; j < i ; j++)
                {
                     KRIL_DEBUG(DBG_INFO, "BCM_RESPCAPI2Cmd::mcc[%d]:%d mcc[%d]:%d\n", i, rsp->searched_plmn[i].mcc, j, rsp->searched_plmn[j].mcc);
                     KRIL_DEBUG(DBG_INFO, "BCM_RESPCAPI2Cmd::mnc[%d]:%d mnc[%d]:%d\n", i, rsp->searched_plmn[i].mnc, j, rsp->searched_plmn[j].mnc);
                     if ((rsp->searched_plmn[i].mcc == rsp->searched_plmn[j].mcc) &&
                         (rsp->searched_plmn[i].mnc == rsp->searched_plmn[j].mnc))
                     {
                         match = TRUE;
                         break;
                     }
                     match = FALSE;
                }
                if (TRUE == match)
                {
                    match = FALSE;
                    continue;
                }
                rdata->available_plmn[rdata->num_of_plmn].mcc = rsp->searched_plmn[i].mcc;
                rdata->available_plmn[rdata->num_of_plmn].mnc = rsp->searched_plmn[i].mnc;
                rdata->available_plmn[rdata->num_of_plmn].network_type = rsp->searched_plmn[i].network_type;
                rdata->available_plmn[rdata->num_of_plmn].rat = rsp->searched_plmn[i].rat;

		   /* START : Irine_JSHAN_SunnyVale */
                   /*
                strncpy(rdata->available_plmn[rdata->num_of_plmn].longname, rsp->searched_plmn[i].nonUcs2Name.longName.name, rsp->searched_plmn[i].nonUcs2Name.longName.name_size);
                strncpy(rdata->available_plmn[rdata->num_of_plmn].shortname, rsp->searched_plmn[i].nonUcs2Name.shortName.name, rsp->searched_plmn[i].nonUcs2Name.shortName.name_size);
                   */
		   v_current_mcc = MS_PlmnConvertRawMcc(rsp->searched_plmn[i].mcc);
		   v_current_mnc = MS_PlmnConvertRawMnc(rsp->searched_plmn[i].mcc, rsp->searched_plmn[i].mnc);
	
		   if( (rsp->searched_plmn[i].nonUcs2Name.longName.nameType== PLMN_NAME_TYPE_LKUP_TABLE) || (rsp->searched_plmn[i].nonUcs2Name.longName.nameType== PLMN_NAME_TYPE_INVALID) )
		   {
		   		coding = (rsp->searched_plmn[i].nonUcs2Name.longName.coding==0x00)?FALSE:TRUE;
				GetSamsungPLMNname(coding, v_current_mcc, v_current_mnc, rdata->available_plmn[rdata->num_of_plmn].longname, NULL, NULL,FALSE);
		   }
		   else
		   {		
                		strncpy(rdata->available_plmn[rdata->num_of_plmn].longname, rsp->searched_plmn[i].nonUcs2Name.longName.name, rsp->searched_plmn[i].nonUcs2Name.longName.name_size);
		   }
		   if( (rsp->searched_plmn[i].nonUcs2Name.shortName.nameType == PLMN_NAME_TYPE_LKUP_TABLE) || (rsp->searched_plmn[i].nonUcs2Name.shortName.nameType == PLMN_NAME_TYPE_INVALID) )
                {
                   		coding = (rsp->searched_plmn[i].nonUcs2Name.shortName.coding==0x00)?FALSE:TRUE;  
				GetSamsungPLMNname(coding, v_current_mcc, v_current_mnc, NULL,  rdata->available_plmn[rdata->num_of_plmn].shortname, NULL,FALSE);
                }			   	
		   else
                {
                		strncpy(rdata->available_plmn[rdata->num_of_plmn].shortname, rsp->searched_plmn[i].nonUcs2Name.shortName.name, rsp->searched_plmn[i].nonUcs2Name.shortName.name_size);
		   }
		   /* END :  Irine_JSHAN_SunnyVale */
		   
                rdata->num_of_plmn++;
                KRIL_DEBUG(DBG_INFO, "BCM_RESPCAPI2Cmd::num_of_plmn:%d network_type:%d longname:%s shortname:%s\n", rdata->num_of_plmn, rdata->available_plmn[rdata->num_of_plmn].network_type, rdata->available_plmn[rdata->num_of_plmn].longname, rdata->available_plmn[rdata->num_of_plmn].shortname);
            }
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


void KRIL_SetBandModeHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            KrilBandModeInfo_t *tdata = (KrilBandModeInfo_t *)pdata->ril_cmd->data;
            
            if (TRUE == KRIL_GetInNetworkSelectHandler())
            {
                CAPI2_NetRegApi_AbortPlmnSelect(InitClientInfo(pdata->ril_cmd->SimId));
                return;
            }

            if (tdata->bandmode >= NUM_BANDMODE || BAND_NULL == g_bandMode[tdata->bandmode])
            {
                KRIL_DEBUG(DBG_ERROR, "CP not support the bandmode:%d\n", tdata->bandmode);
                pdata->result = BCM_E_MODE_NOT_SUPPORTED;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_INFO, "g_bandMode:%ld g_systemband:%d combine:%d\n", g_bandMode[tdata->bandmode], g_systemband, ConvertBandMode(tdata->bandmode));
                CAPI2_NetRegApi_SelectBand(InitClientInfo(pdata->ril_cmd->SimId), ConvertBandMode(tdata->bandmode));
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;
        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_ERROR, "result :%d error...!\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
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


void KRIL_QueryAvailableBandModeHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilAvailableBandMode_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pdata->rsp_len = sizeof(KrilAvailableBandMode_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_LOCAL_NETREG_ELEM_SYSTEM_BAND);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->result != RESULT_OK)
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                KrilAvailableBandMode_t *rdata = (KrilAvailableBandMode_t *)pdata->bcm_ril_rsp;
                int index, index1 = 0;
                int tempband[18];
                CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*) capi2_rsp->dataBuf;
                BandSelect_t SystemBand;
                memcpy(&SystemBand, &(rsp->data_u), sizeof(BandSelect_t));

                SystemBand = SystemBand | BAND_AUTO;
                g_systemband = SystemBand;
                for (index = 0 ; index < NUM_BANDMODE ; index++)
                {
                    if(g_bandMode[index] & SystemBand)
                    {
                        tempband[index1] = index;
                        KRIL_DEBUG(DBG_INFO, "g_bandMode[%d]:%ld tempband[%d]:%d\n", index, g_bandMode[index], index1, tempband[index1]);
                        index1++;
                    }
                }
                rdata->band_mode[0] = (index1+1);
                KRIL_DEBUG(DBG_INFO, "band_mode length:%d\n", rdata->band_mode[0]);
                for (index = 0 ; (index < rdata->band_mode[0]-1) ; index++)
                {
                    rdata->band_mode[index+1] = tempband[index];
                    KRIL_DEBUG(DBG_INFO, "rdata->band_mode[%d]:%d\n",index+1, rdata->band_mode[index+1]);
                }
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


void KRIL_SetPreferredNetworkTypeHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::msgType:%d result:%d\n", pdata->handler_state, capi2_rsp->msgType, capi2_rsp->result);
        if(capi2_rsp->result != RESULT_OK)
        {
            pdata->result = RILErrorResult(capi2_rsp->result);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            KRIL_SetInSetPrferredNetworkTypeHandler(FALSE);
            return;
        }
    }

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            KrilSetPreferredNetworkType_t *tdata = (KrilSetPreferredNetworkType_t *)pdata->ril_cmd->data;
            KrilSetPreferNWInfo_t *context = (KrilSetPreferNWInfo_t *)pdata->cmdContext;
            Boolean isDualSim;
            context->Index = 0;
            KRIL_SetInSetPrferredNetworkTypeHandler(TRUE);

            if (TRUE == KRIL_GetInNetworkSelectHandler())
            {
                CAPI2_NetRegApi_AbortPlmnSelect(InitClientInfo(pdata->ril_cmd->SimId));
                return;
            }

            if (SIM_SINGLE == pdata->ril_cmd->SimId)
                isDualSim = FALSE;
            else
                isDualSim = TRUE;

            if (0xFF == KRIL_GetPreferredNetworkType(pdata->ril_cmd->SimId))
            {
                CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_LOCAL_NETREG_ELEM_SUPPORTED_RAT);
                pdata->handler_state = BCM_NET_QueryCurrentRAT;
            }
            else if (((!isDualSim) &&
                      tdata->networktype == KRIL_GetPreferredNetworkType(SIM_SINGLE)) ||
                     (isDualSim &&
                      tdata->networktype == KRIL_GetPreferredNetworkType(SIM_DUAL_FIRST) &&
                      tdata->networktypeEx == KRIL_GetPreferredNetworkType(SIM_DUAL_SECOND)))
            {
                KRIL_SetInSetPrferredNetworkTypeHandler(FALSE);
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else
            {
                BCMSetSupportedRATandBand(ril_cmd);
            }
        }
        break;

        case BCM_NET_QueryCurrentRAT:
        {
            CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*)capi2_rsp->dataBuf;
            KrilSetPreferredNetworkType_t *tdata = (KrilSetPreferredNetworkType_t *)pdata->ril_cmd->data;
            KrilSetPreferNWInfo_t *context = (KrilSetPreferNWInfo_t *)pdata->cmdContext;
            RATSelect_t data;
            int networktype;

            memcpy(&data, &(rsp->data_u), sizeof(RATSelect_t));
            if (SIM_DUAL_SECOND == pdata->ril_cmd->SimId)
                networktype = tdata->networktypeEx;
            else
                networktype = tdata->networktype;

            if (networktype == ConvertRATType(data))
            {
                KRIL_SetInSetPrferredNetworkTypeHandler(FALSE);
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else
            {
                UInt8 cid;
                for(context->SimId = SIM_DUAL_FIRST; context->SimId <= SIM_DUAL_SECOND; context->SimId++)
                {
                    cid = FindPdpCid(context->SimId);
                    if (cid != 0 && cid > BCM_NET_MAX_DUN_PDP_CNTXS && cid <= BCM_NET_MAX_PDP_CNTXS) // GPRS cid is 8-10
                    {
                        CAPI2_PchExApi_SendPDPDeactivateReq(InitClientInfo(context->SimId), cid);
                        pdata->handler_state = BCM_PDP_SendPDPDeactivateReq;
                        return;
                    }
                }
                BCMSetSupportedRATandBand(ril_cmd);
            }
        }
        break;

        case BCM_PDP_SendPDPDeactivateReq:
        {
            PDP_SendPDPDeactivateReq_Rsp_t *rsp = (PDP_SendPDPDeactivateReq_Rsp_t *)capi2_rsp->dataBuf;
            KrilSetPreferNWInfo_t *context = (KrilSetPreferNWInfo_t *)pdata->cmdContext;
            KRIL_DEBUG(DBG_INFO, "SimId:%d response:%d\n", context->SimId, rsp->response);

            if (rsp->response == PCH_REQ_ACCEPTED)
            {
                UInt8 cid;
                SimNumber_t sim;
                ReleasePdpContext(context->SimId, FindPdpCid(context->SimId));
                for(sim = context->SimId; sim <= SIM_DUAL_SECOND; sim++)
                {
                    cid = FindPdpCid(sim);
                    if (cid != 0 && cid > BCM_NET_MAX_DUN_PDP_CNTXS && cid <= BCM_NET_MAX_PDP_CNTXS) // GPRS cid is 8-10
                    {
                        CAPI2_PchExApi_SendPDPDeactivateReq(InitClientInfo(sim), cid);
                        context->SimId = sim;
                        pdata->handler_state = BCM_PDP_SendPDPDeactivateReq;
                        return;
                    }
                }

                //RIL CID finish and then find DUN cid
                for(context->SimId = SIM_DUAL_FIRST; context->SimId <= SIM_DUAL_SECOND; context->SimId++)
                {
                    cid = FindDunPdpCid(context->SimId);
                    if (cid != 0 && cid > 0 && cid <= BCM_NET_MAX_DUN_PDP_CNTXS) // GPRS cid is 8-10
                    {
                        CAPI2_PchExApi_SendPDPDeactivateReq(InitClientInfo(context->SimId), cid);
                        pdata->handler_state = BCM_PDP_SendDunPDPDeactivateReq;
                        return;
                    }
                }

                BCMSetSupportedRATandBand(ril_cmd);
            }
            else // PDP deactive again if return fail
            {
                CAPI2_PchExApi_SendPDPDeactivateReq(InitClientInfo(context->SimId), FindPdpCid(context->SimId));
            }
        }
        break;

        case BCM_PDP_SendDunPDPDeactivateReq:
        {
            PDP_SendPDPDeactivateReq_Rsp_t *rsp = (PDP_SendPDPDeactivateReq_Rsp_t *)capi2_rsp->dataBuf;
            KrilSetPreferNWInfo_t *context = (KrilSetPreferNWInfo_t *)pdata->cmdContext;
            KRIL_DEBUG(DBG_ERROR, "SimId:%d response:%d\n", context->SimId, rsp->response);

            if (rsp->response == PCH_REQ_ACCEPTED)
            {
                UInt8 cid;
                SimNumber_t sim;                
                
                cid = FindDunPdpCid(context->SimId);
                ReleaseDunPdpContext(context->SimId, cid);                
                Kpdp_pdp_deactivate_ind(cid);
                
                
                for(sim = context->SimId; sim <= SIM_DUAL_SECOND; sim++)
                {
                    cid = FindDunPdpCid(sim);
                    if (cid != 0 && cid > 0 && cid <= BCM_NET_MAX_DUN_PDP_CNTXS) // GPRS cid is 1-7
                    {
                        CAPI2_PchExApi_SendPDPDeactivateReq(InitClientInfo(sim), cid);
                        context->SimId = sim;
                        pdata->handler_state = BCM_PDP_SendDunPDPDeactivateReq;
                        return;
            }
                }

                BCMSetSupportedRATandBand(ril_cmd);
            }
            else // PDP deactive again if return fail
            {
                CAPI2_PchExApi_SendPDPDeactivateReq(InitClientInfo(context->SimId), FindDunPdpCid(context->SimId));
            }
            }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->result != RESULT_OK)
            {
                KRIL_DEBUG(DBG_ERROR, "handler result:%d error...!\n", capi2_rsp->result);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            KRIL_SetInSetPrferredNetworkTypeHandler(FALSE);
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


void KRIL_GetPreferredNetworkTypeHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            pdata->bcm_ril_rsp = kmalloc(sizeof(krilGetPreferredNetworkType_t), GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;             
            }
            else
            {
                pdata->rsp_len = sizeof(krilGetPreferredNetworkType_t);
                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                CAPI2_MsDbApi_GetElement(InitClientInfo(pdata->ril_cmd->SimId), MS_LOCAL_NETREG_ELEM_SUPPORTED_RAT);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->result != RESULT_OK)
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                krilGetPreferredNetworkType_t *rdata = (krilGetPreferredNetworkType_t *)pdata->bcm_ril_rsp;
                CAPI2_MS_Element_t* rsp = (CAPI2_MS_Element_t*) capi2_rsp->dataBuf;
                RATSelect_t RAT;
                memcpy(&RAT, &(rsp->data_u), sizeof(RATSelect_t));

                if (0xFF == ConvertRATType(RAT))
                {
                    KRIL_DEBUG(DBG_ERROR, "RAT:%d\n", RAT);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
                }
                rdata->network_type = ConvertRATType(RAT);
                KRIL_DEBUG(DBG_INFO, "BCM_RESPCAPI2Cmd:: network_type:%d RAT:%d\n", rdata->network_type, RAT);
                KRIL_SetPreferredNetworkType(pdata->ril_cmd->SimId, rdata->network_type);
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


void KRIL_GetNeighboringCellIDsHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    if (capi2_rsp != NULL)
        KRIL_DEBUG(DBG_INFO, "handler_state:0x%lX::msgType:%d result:%d\n", pdata->handler_state, capi2_rsp->msgType, capi2_rsp->result);

    switch(pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            if ((gRegInfo[pdata->ril_cmd->SimId].regState != REG_STATE_NORMAL_SERVICE) &&
                (gRegInfo[pdata->ril_cmd->SimId].regState != REG_STATE_ROAMING_SERVICE))
            {
                pdata->result = BCM_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            // can only handle one of these requests at a time...
            KRIL_SetInNeighborCellHandler( TRUE );
            CAPI2_MsDbApi_SYS_EnableCellInfoMsg(InitClientInfo(pdata->ril_cmd->SimId), TRUE);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->result != RESULT_OK)
            {
                CAPI2_MsDbApi_SYS_EnableCellInfoMsg(InitClientInfo(pdata->ril_cmd->SimId), FALSE);
                pdata->handler_state = BCM_SYS_EnableCellInfoMsg;
            }
            else
            {
                if (MSG_SERVING_CELL_INFO_IND == capi2_rsp->msgType)
                {
                    if (NULL == capi2_rsp->dataBuf)
                    {
                        KRIL_DEBUG(DBG_ERROR, "dataBuf is NULL\n");
                        KRIL_ASSERT(1);
                        goto DisableCellInfo;
                    }
                    ServingCellInfo_t *rsp = (ServingCellInfo_t *)capi2_rsp->dataBuf;
                    krilGetNeighborCell_t *rdata;
                    UInt8 loop = 0;
                    UInt32 count = 0, alloc_count=0;
                    UInt8 *temp_rsp = NULL ;
                    KRIL_DEBUG(DBG_ERROR, "mRAT:%d address:0x%p\n", rsp->mRAT, &rsp->mRAT);
                    if (RAT_UMTS == rsp->mRAT)
                    {
                        // 3G Received Signal Code Power Range: -121 to -25
                        const Int8 rscpBottom = -121;
                        const Int8 rscpTop = -25;

                        KRIL_DEBUG(DBG_INFO, "num_umts_freq:%d \n", rsp->mLbsParams.lbs_umts_params.num_umts_freq);
                        if (rsp->mLbsParams.lbs_umts_params.num_umts_freq > LBS_MAX_NUM_UMTS_FREQ)
                        {
                            KRIL_ASSERT(1);
                            goto DisableCellInfo;
                        }
                        for (loop = 0 ; loop < rsp->mLbsParams.lbs_umts_params.num_umts_freq ; loop++)
                        {
                            UInt8 loop1 = 0;
                            KRIL_DEBUG(DBG_INFO, "num_cell:%d\n", rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].num_cell);
                            if (rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].num_cell > LBS_MAX_NUM_CELL_PER_FREQ)
                            {
                                KRIL_DEBUG(DBG_ERROR, "loop:%d num_cell:%d\n", loop, rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].num_cell);
                                KRIL_ASSERT(1);
                                goto DisableCellInfo;
                            }
                            for (loop1 = 0 ; loop1 < rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].num_cell ; loop1++)
                            {
                                if (rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].psc != 0 &&
                                    rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].rscp >= rscpBottom &&
                                    rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].rscp <= rscpTop)
                                {
                                    alloc_count++;
                                }
                            }
                        }
                        KRIL_DEBUG(DBG_INFO, "count:%ld \n", alloc_count);
                        temp_rsp = kmalloc(sizeof(krilGetNeighborCell_t)*alloc_count, GFP_KERNEL);
                        if(!temp_rsp) {
                            KRIL_DEBUG(DBG_ERROR, "unable to allocate temp_rsp buf\n");                
                            KRIL_ASSERT(1);
                            goto DisableCellInfo;
                        }
                        rdata = (krilGetNeighborCell_t *)temp_rsp;
                        memset(temp_rsp, 0,sizeof(krilGetNeighborCell_t)*alloc_count);
                        count = 0;
                        for (loop = 0 ; loop < rsp->mLbsParams.lbs_umts_params.num_umts_freq ; loop++)
                        {
                            UInt8 loop1 = 0;
                            KRIL_DEBUG(DBG_INFO, "num_cell:%d\n", rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].num_cell);
                            for (loop1 = 0 ; loop1 < rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].num_cell ; loop1++)
                            {
                                if (rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].psc != 0 &&
                                    rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].rscp >= rscpBottom &&
                                    rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].rscp <= rscpTop)
                                {
                                    KRIL_DEBUG(DBG_INFO, "psc:0x%x rscp:%d\n", rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].psc, rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].rscp);
                                    if(count >= alloc_count || count >= LBS_MAX_NUM_UMTS_FREQ*LBS_MAX_NUM_CELL_PER_FREQ)
                                    {
                                        kfree(temp_rsp);
                                        pdata->rsp_len = 0;
                                        KRIL_DEBUG(DBG_ERROR, "count:%ld alloc_count:%ld\n", count, alloc_count);
                                        KRIL_ASSERT(1);
                                        goto DisableCellInfo;
                                    }
                                    rdata[count].cid = rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].psc;
                                    rdata[count].rssi = rsp->mLbsParams.lbs_umts_params.umts_freqs[loop].cells[loop1].rscp;
                                    KRIL_DEBUG(DBG_INFO, "loop:%d cid:0x%x rssi:%d\n", loop, rdata[count].cid, rdata[count].rssi);
                                    count++;

                                }
                            }
                        }
                        pdata->rsp_len = sizeof(krilGetNeighborCell_t)*count;
                        pdata->bcm_ril_rsp = kmalloc(pdata->rsp_len, GFP_KERNEL);
                        if(!pdata->bcm_ril_rsp) {
                            KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                        }
                        else
                        {
                            memcpy(pdata->bcm_ril_rsp, temp_rsp, pdata->rsp_len);
                        }
                            if (temp_rsp != NULL)
                            kfree(temp_rsp);
                        }
                    else if (RAT_GSM == rsp->mRAT)
                    {
                        KRIL_DEBUG(DBG_INFO, "num_gsm_ncells:%d \n", rsp->mLbsParams.lbs_gsm_params.num_gsm_ncells);
                        if(rsp->mLbsParams.lbs_gsm_params.num_gsm_ncells > LBS_MAX_NUM_GSM_NCELL)
                        {
                            KRIL_ASSERT(1);
                            goto DisableCellInfo;
                        }
                        temp_rsp = kmalloc(sizeof(krilGetNeighborCell_t)*rsp->mLbsParams.lbs_gsm_params.num_gsm_ncells, GFP_KERNEL);
                        if(!temp_rsp) {
                            KRIL_DEBUG(DBG_ERROR, "unable to allocate temp_rsp buf\n");                
                            KRIL_ASSERT(1);
                            goto DisableCellInfo;
                        }
                        KRIL_DEBUG(DBG_INFO, "num_gsm_ncells:%d \n", rsp->mLbsParams.lbs_gsm_params.num_gsm_ncells);
                        rdata = (krilGetNeighborCell_t *)temp_rsp;
                        memset(temp_rsp, 0,sizeof(krilGetNeighborCell_t)*rsp->mLbsParams.lbs_gsm_params.num_gsm_ncells);
                        count = 0;
                        for (loop = 0 ; loop < rsp->mLbsParams.lbs_gsm_params.num_gsm_ncells ; loop++)
                        {
                            if (rsp->mLbsParams.lbs_mm_params.lac != 0 &&
                                rsp->mLbsParams.lbs_gsm_params.gsm_ncells[loop].cell_id != 0 &&
                                rsp->mLbsParams.lbs_gsm_params.gsm_ncells[loop].rxlev >= FILTER_GSM_SIGNAL_LEVEL &&
                                rsp->mLbsParams.lbs_gsm_params.gsm_ncells[loop].rxlev <= 63) //2G receive power level Valid values: 0 to 63, and also filter lower signal level
                            {
                                KRIL_DEBUG(DBG_INFO, "lac:0x%x cell_id:0x%x rxlev:%d\n", rsp->mLbsParams.lbs_mm_params.lac, rsp->mLbsParams.lbs_gsm_params.gsm_ncells[loop].cell_id, rsp->mLbsParams.lbs_gsm_params.gsm_ncells[loop].rxlev);
                                rdata[count].cid = (rsp->mLbsParams.lbs_mm_params.lac << 16) | rsp->mLbsParams.lbs_gsm_params.gsm_ncells[loop].cell_id;
                                rdata[count].rssi = rsp->mLbsParams.lbs_gsm_params.gsm_ncells[loop].rxlev;
                                KRIL_DEBUG(DBG_INFO, "loop:%d count:%ld cid:0x%x rssi:%d\n", loop, count, rdata[count].cid, rdata[count].rssi);
                                count++;
                                if(count > LBS_MAX_NUM_GSM_NCELL || count > rsp->mLbsParams.lbs_gsm_params.num_gsm_ncells)
                                {
                                    kfree(temp_rsp);
                                    KRIL_ASSERT(1);
                                    goto DisableCellInfo;
                                }
                            }
                        }
                        pdata->rsp_len = sizeof(krilGetNeighborCell_t)*count;
                        pdata->bcm_ril_rsp = kmalloc(pdata->rsp_len, GFP_KERNEL);
                        if(!pdata->bcm_ril_rsp) {
                            KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                        }
                        else
                        {    
                            memcpy(pdata->bcm_ril_rsp, temp_rsp, pdata->rsp_len);
                        }
                            if (temp_rsp != NULL)
                            kfree(temp_rsp);
                        }
                    else
                    {
                        KRIL_DEBUG(DBG_ERROR, "KRIL_GetNeighboringCellIDsHandler:: no RAT\n");
                        pdata->result = BCM_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW;
                    }
DisableCellInfo:
                    CAPI2_MsDbApi_SYS_EnableCellInfoMsg(InitClientInfo(pdata->ril_cmd->SimId), FALSE);
                    pdata->handler_state = BCM_SYS_EnableCellInfoMsg;
                }
                else
                {
                    KRIL_SetServingCellTID(capi2_rsp->tid);
                }
            }
        }
        break;

        case BCM_SYS_EnableCellInfoMsg:
        {
            if (BCM_E_OP_NOT_ALLOWED_BEFORE_REG_TO_NW == pdata->result)
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            else
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            KRIL_SetInNeighborCellHandler( FALSE );
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


void KRIL_SetLocationUpdatesHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
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
            KrilLocationUpdates_t *tdate = (KrilLocationUpdates_t *)pdata->ril_cmd->data;
            KRIL_DEBUG(DBG_INFO, "location_updates:%d\n", tdate->location_updates);
            KRIL_SetLocationUpdateStatus(pdata->ril_cmd->SimId, tdate->location_updates);
            pdata->handler_state = BCM_FinishCAPI2Cmd;
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            if (capi2_rsp->result != RESULT_OK)
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
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


void KRIL_SetBPMModeHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t *)ril_cmd;
    if (capi2_rsp && capi2_rsp->result != RESULT_OK)
    {
        KRIL_DEBUG(DBG_ERROR,"CAPI2 response failed:%d\n", capi2_rsp->result);
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
        {
            char* rawdata = (char*)pdata->ril_cmd->data;
            KRIL_DEBUG(DBG_INFO, "status:%d\n", rawdata[5]);
            if (0 == rawdata[5] || 1 == rawdata[5])
            {
                CAPI2_PhoneCtrlApi_SetPagingStatus(InitClientInfo(pdata->ril_cmd->SimId), (UInt8)rawdata[5]);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR, "not support status:%d\n", rawdata[5]);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            break;
        }

        case BCM_RESPCAPI2Cmd:
        {
            UInt8 *resp = NULL;
            pdata->bcm_ril_rsp = kmalloc(sizeof(UInt8)*5, GFP_KERNEL);
            if(!pdata->bcm_ril_rsp) {
                KRIL_DEBUG(DBG_ERROR, "unable to allocate bcm_ril_rsp buf\n");                
                pdata->handler_state = BCM_ErrorCAPI2Cmd;                                         
            }
            else
            {
                pdata->rsp_len = sizeof(UInt8)*5 ;
                resp = (UInt8*)pdata->bcm_ril_rsp;
                resp[0] = (UInt8)'B';
                resp[1] = (UInt8)'R';
                resp[2] = (UInt8)'C';
                resp[3] = (UInt8)'M';
                resp[4] = (UInt8)BRIL_HOOK_SET_BPM_MODE;
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
