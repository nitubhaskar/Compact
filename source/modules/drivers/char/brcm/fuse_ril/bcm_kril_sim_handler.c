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
#include "bcm_kril_simlockfun.h"

#define  APDUFILEID_EF_DIR            0x2F00
#define  APDUFILEID_DF_GRAPHICS       0x5F50
#define  APDUFILEID_EF_MML            0x4F47
#define  APDUFILEID_EF_MMDF           0x4F48
#define  APDUFILEID_DF_MULTIMEDIA     0X5F3B
#define  APDUFILEID_EF_PBK_ADN        0x4F3A
#define  APDUFILEID_EF_PBK_EMAIL      0x4F50
#define  APDUFILEID_EF_PBK_GRP1       0x4F25
#define  APDUFILEID_EF_PBK_EXT1       0x4F4A
#define  APDUFILEID_EF_PBK_AAS        0x4F4B
#define  APDUFILEID_EF_PBK_GAS        0x4F4C
#define  APDUFILEID_EF_PBK_PBC        0x4F09
#define  APDUFILEID_EF_PBK_ANRA       0x4F11
#define  APDUFILEID_EF_PBK_ANRB       0x4F13
#define  APDUFILEID_EF_PBK_SNE        0x4F19
#define  APDUFILEID_EF_PBK_UID        0x4F21
#define  APDUFILEID_EF_PBK_GRP        0x4F26
#define  APDUFILEID_EF_PBK_ANRC       0x4F15
#define  APDUFILEID_EF_PBK_ADN1       0x4F3B
#define  APDUFILEID_EF_PBK_PBC1       0x4F0A
#define  APDUFILEID_EF_PBK_ANRA1      0x4F12
#define  APDUFILEID_EF_PBK_ANRB1      0x4F14
#define  APDUFILEID_EF_PBK_EMAIL1     0x4F51
#define  APDUFILEID_EF_PBK_SNE1       0x4F1A
#define  APDUFILEID_EF_PBK_ANRC1      0x4F16
#define  APDUFILEID_DF_SOLSA          0X5F70

#define  SIMPBK_NAME      40

#define  FDN_MANDATORY_BYTE_NUM       14
#define  isdigit(c)   ((c) >= '0' && (c) <= '9')

#define CONVERT_USIM_SELECT_RESPONSE_FORMAT_TO_SIM
#define ANDROID_SIMIO_PATH_AVAILABLE
// Enable the define to update the FDN record using CAPI2 PBK API, not SIM IO command.
#define UPDATE_FDN_USE_CAPI2_PBK_API

#define BCM_OEM_HOOK_HEADER_LENGTH 5

#ifdef BCM_RIL_FOR_EAP_SIM  // BCM_EAP_SIM
#define MAXBUFF  512
#endif

const UInt16 Sim_Df_Telecom_Path[1] =        {APDUFILEID_MF};
const UInt16 Sim_Df_Gsm_Path[1] =            {APDUFILEID_MF};
const UInt16 Sim_Df_ADFusim_Path[1] =        {APDUFILEID_MF};
const UInt16 Sim_Df_Global_PhoneBK_Path[2] = {APDUFILEID_MF, APDUFILEID_DF_TELECOM};
const UInt16 Sim_Df_Local_PhoneBK_Path[2]  = {APDUFILEID_MF, APDUFILEID_USIM_ADF};
const UInt16 Sim_Df_Graphics_Path[2] =       {APDUFILEID_MF, APDUFILEID_DF_TELECOM};
const UInt16 Sim_Df_Acenet_Path[2] =         {APDUFILEID_MF, APDUFILEID_DF_CINGULAR};
const UInt16 Sim_Df_Cingular_Path[1] =       {APDUFILEID_MF};
const UInt16 Sim_Df_Multimedia_Path[1] =     {APDUFILEID_MF};
const UInt16 Sim_Df_SoLSA_Path[2] =          {APDUFILEID_MF, APDUFILEID_DF_GSM};
const UInt16 Sim_Df_SoLSA_Usim_Path[2] =     {APDUFILEID_MF, APDUFILEID_USIM_ADF};

Int16         gImageInstCount = 0;
APDUFileID_t  gImageInstance[30];
static SIM_PIN_Status_t sSimPinstatus[DUAL_SIM_SIZE] = {NO_SIM_PIN_STATUS};
static SIMLOCK_SIM_DATA_t* sSimdata[DUAL_SIM_SIZE] = {NULL,NULL,NULL};

// For FDN
static UInt16 sFDNfilesize[DUAL_SIM_SIZE] = {0};
static UInt8  sFDNrecordlength[DUAL_SIM_SIZE] = {0};

void SetSimPinStatusHandle(SimNumber_t SimId, SIM_PIN_Status_t SimPinState);
SIM_PIN_Status_t GetSimPinStatusHandle(SimNumber_t SimId);

extern SS_CallBarType_t ssBarringTypes[];
extern SS_Code_t ssBarringCodes[];
extern SS_SvcCls_t GetServiceClass (int InfoClass);
extern int SvcClassToATClass (SS_SvcCls_t InfoClass);
extern void SS_SvcCls2BasicSrvGroup(int inInfoClass, BasicSrvGroup_t* inBasicSrvPtr);
/*+20110822 HKPARK FOR DebugScreen Slave*/
extern bool v_SimCheck_Slave;
extern bool v_SimCheck_MASTER;
/*-20110822 HKPARK FOR DebugScreen Slave*/

/*+20111110 HKPARK if NO SIM status, send Error Message*/
// extern bool v_NoSimCheck2;
// extern bool v_NoSimCheck1;
/*-20111110 HKPARK if NO SIM status, send Error Message*/

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

extern ATC_CLASS_t Util_GetATClassFromSvcGrp(BasicSrvGroup_t inputSrvGrp);

Boolean SimIOGetDedicatedFileId(APDUFileID_t efile_id, 
                             SIM_APPL_TYPE_t simAppType, 
                             APDUFileID_t *dfile_id
                             )
{
    Boolean result = FALSE;
    UInt8 Identifier;
    
    switch (efile_id)
    {
        case APDUFILEID_EF_ADN    : 
        case APDUFILEID_EF_FDN    :
        case APDUFILEID_EF_SMS    :
        case APDUFILEID_EF_CCP    :
        case APDUFILEID_EF_MSISDN :
        case APDUFILEID_EF_SMSP   :
        case APDUFILEID_EF_SMSS   :
        case APDUFILEID_EF_LND    :
        case APDUFILEID_EF_SMSR   :
        case APDUFILEID_EF_SDN    :
        case APDUFILEID_EF_EXT1   :
        case APDUFILEID_EF_EXT2   :
        case APDUFILEID_EF_EXT3   :
        case APDUFILEID_EF_BDN    :
        case APDUFILEID_EF_EXT4_3G  :
        case APDUFILEID_EF_EXT5_3G  :
        case APDUFILEID_EF_ECCP   :   
        case APDUFILEID_EF_CMI    :
        case APDUFILEID_DF_GRAPHICS :
        {
            *dfile_id = APDUFILEID_DF_TELECOM;
            break;
        }
        
        case APDUFILEID_DF_PHONEBK:
        case APDUFILEID_DF_MULTIMEDIA:
        {    
            if(SIM_APPL_2G == simAppType)
                goto Error;
            
            *dfile_id = APDUFILEID_DF_TELECOM; 
            break;
        }
        
        case APDUFILEID_EF_LP     :
        case APDUFILEID_EF_IMSI   :
        case APDUFILEID_EF_KC     :
        case APDUFILEID_EF_PLMNSEL  :
        case APDUFILEID_EF_HPLMN  : 
        case APDUFILEID_EF_ACMMAX :
        case APDUFILEID_EF_SST    : //2G:APDUFILEID_EF_SST 3G:APDUFILEID_EF_UST_3G
        case APDUFILEID_EF_ACM    :
        case APDUFILEID_EF_GID1   :
        case APDUFILEID_EF_GID2   :
        case APDUFILEID_EF_PUCT   :
        case APDUFILEID_EF_CBMI   :
        case APDUFILEID_EF_SPN    :
        case APDUFILEID_EF_CBMID  : 
        case APDUFILEID_EF_BCCH   :
        case APDUFILEID_EF_ACC    :
        case APDUFILEID_EF_FPLMN  : 
        case APDUFILEID_EF_LOCI   :
        case APDUFILEID_EF_AD     :
        case APDUFILEID_EF_PHASE  : 
        case APDUFILEID_EF_VGCS   :     
        case APDUFILEID_EF_VGCSS  : 
        case APDUFILEID_EF_VBS    :
        case APDUFILEID_EF_VBSS   :
        case APDUFILEID_EF_EMLPP  : 
        case APDUFILEID_EF_AAEM   :
        case APDUFILEID_EF_ECC    :
        case APDUFILEID_EF_CBMIR  :
        case APDUFILEID_EF_DCK    :   
        case APDUFILEID_EF_CNL    :
        case APDUFILEID_EF_NIA    :
        case APDUFILEID_EF_SUME   : 
        case APDUFILEID_EF_PLMN_WACT:   
        case APDUFILEID_EF_OPLMN_WACT:  
        case APDUFILEID_EF_HPLMN_WACT:  
        case APDUFILEID_EF_CPBCCH : 
        case APDUFILEID_EF_INV_SCAN : 
        case APDUFILEID_EF_RPLMN_ACT:   
        case APDUFILEID_EF_PNN    :
        case APDUFILEID_EF_OPL    : 
        case APDUFILEID_EF_MBDN   : 
        case APDUFILEID_EF_EXT6   : 
        case APDUFILEID_EF_MBI    :
        case APDUFILEID_EF_MWIS   : 
        case APDUFILEID_EF_CFIS   :
        case APDUFILEID_EF_EXT7   :
        case APDUFILEID_EF_SPDI   :
        case APDUFILEID_EF_MMSN   : 
        case APDUFILEID_EF_EXT8   :
        case APDUFILEID_EF_MMSICP : 
        case APDUFILEID_EF_MMSUP  :
        {
            if (SIM_APPL_3G == simAppType)
            {
                *dfile_id = APDUFILEID_USIM_ADF;
            }
            else if (SIM_APPL_2G == simAppType)
            {
                *dfile_id = APDUFILEID_DF_GSM;
            }
            else
            {
                goto Error;
            }
            break;
        }

        case APDUFILEID_EF_KC_GPRS  : 
        case APDUFILEID_EF_LOCI_GPRS:
        // The following EF are defined in Common PCN Handset 
        // Sepcification (CPHS): under DF GSM. They are not defined in GSM 11.11.
        case APDUFILEID_EF_MWI_CPHS :
        case APDUFILEID_EF_CFL_CPHS :   
        case APDUFILEID_EF_ONS_CPHS :   
        case APDUFILEID_EF_CSP_CPHS :   
        case APDUFILEID_EF_INFO_CPHS:   
        case APDUFILEID_EF_MBN_CPHS :   
        case APDUFILEID_EF_ONSS_CPHS:   
        case APDUFILEID_EF_IN_CPHS  :
        {
            *dfile_id = APDUFILEID_DF_GSM;    
            break;
        }
                  
        case APDUFILEID_EF_EST_3G: /* EF-EST, exists only in USIM */
        case APDUFILEID_EF_ACL_3G: /* EF-ACL, exists only in USIM */
        {
            if(SIM_APPL_2G == simAppType)
                goto Error;
            
            *dfile_id = APDUFILEID_USIM_ADF;
            break;
        }
        
        case APDUFILEID_EF_ACTHPLMN: 
        {
            *dfile_id = APDUFILEID_DF_ACTNET;
            break;
        }
        
        case APDUFILEID_EF_CINGULAR_TST:
        {
            *dfile_id = APDUFILEID_DF_CINGULAR;
            break;
        }
        
        case APDUFILEID_EF_IMG:
        {
            *dfile_id = (APDUFileID_t)APDUFILEID_DF_GRAPHICS;
            break;
        }
        
        case APDUFILEID_EF_PSC:
        case APDUFILEID_EF_CC:
        case APDUFILEID_EF_PUID:
        case APDUFILEID_EF_PBR:
        case APDUFILEID_EF_PBK_ADN:
        case APDUFILEID_EF_PBK_EMAIL:
        case APDUFILEID_EF_PBK_GRP1:
        case APDUFILEID_EF_PBK_EXT1:  
        case APDUFILEID_EF_PBK_AAS:   
        case APDUFILEID_EF_PBK_GAS:   
        case APDUFILEID_EF_PBK_PBC:   
        case APDUFILEID_EF_PBK_ANRA:  
        case APDUFILEID_EF_PBK_ANRB:  
        case APDUFILEID_EF_PBK_SNE:   
        case APDUFILEID_EF_PBK_UID:   
        case APDUFILEID_EF_PBK_GRP:   
        case APDUFILEID_EF_PBK_ANRC:  
        case APDUFILEID_EF_PBK_ADN1:  
        case APDUFILEID_EF_PBK_PBC1:  
        case APDUFILEID_EF_PBK_ANRA1: 
        case APDUFILEID_EF_PBK_ANRB1: 
        case APDUFILEID_EF_PBK_EMAIL1:
        case APDUFILEID_EF_PBK_SNE1:  
        case APDUFILEID_EF_PBK_ANRC1: 
        {
            if (SIM_APPL_2G == simAppType)
                goto Error;
            
            *dfile_id = APDUFILEID_DF_PHONEBK;
            break;
        }
        
        case APDUFILEID_EF_MML:
        case APDUFILEID_EF_MMDF:
        {
            if (SIM_APPL_2G == simAppType)
                goto Error;
            
            *dfile_id = (APDUFileID_t)APDUFILEID_DF_MULTIMEDIA;
            break;
        }
            
        case APDUFILEID_EF_ICCID:
        case APDUFILEID_EF_PL:
        case APDUFILEID_EF_DIR: //The presence of EFDIR on a SIM is optional
        case APDUFILEID_MF:
        case APDUFILEID_DF_TELECOM:
        case APDUFILEID_DF_GSM:
        case APDUFILEID_USIM_ADF:
        {
            *dfile_id = APDUFILEID_MF;
            break;
        }
        
        case APDUFILEID_EF_ARR_UNDER_MF:
        {
            if (SIM_APPL_2G == simAppType)
                goto Error;
            
            *dfile_id = APDUFILEID_MF;
            break;
        }
        
        default:          
            break;    
    }
    
    if (APDUFILEID_INVALID_FILE == *dfile_id)
    {
        //Some special EF ID need to be handled.
        Identifier = (UInt8)(((UInt16)efile_id & 0xFF00) >> 8);
        
        if (0x4F == Identifier)
        {
            UInt16 i;
            
            for (i = 0 ; i < gImageInstCount ; i++)
            {
                if (gImageInstance[i] == efile_id)
                {
                    KRIL_DEBUG(DBG_ERROR,"The EF:0x%X belongs to Image Instance\n",efile_id);
                    *dfile_id = (APDUFileID_t)APDUFILEID_DF_GRAPHICS;
                    break;
                }
            }
        }
    }
    
    if (APDUFILEID_INVALID_FILE == *dfile_id)
        goto Error;
        
    result = TRUE;

Error:
    if (FALSE == result)
        KRIL_DEBUG(DBG_ERROR,"Unknow or not support EF ID:0x%X\n",efile_id);
    
    return result;
}


Boolean SimIOGetDedicatedFilePath(APDUFileID_t        dfile_id, 
                                  SIMServiceStatus_t  localPBKStatus, 
                                  KrilSimDedFilePath_t *path_info)
{
    Boolean result = FALSE;
    
    switch (dfile_id)
    {
        case APDUFILEID_DF_TELECOM:
            
            path_info->select_path = Sim_Df_Telecom_Path;
            path_info->path_len = sizeof(Sim_Df_Telecom_Path) / sizeof(UInt16);
            break;
        
        case APDUFILEID_DF_GSM:
            
            path_info->select_path = Sim_Df_Gsm_Path;
            path_info->path_len = sizeof(Sim_Df_Gsm_Path) / sizeof(UInt16);
            break;
        
        case APDUFILEID_USIM_ADF:
            
            path_info->select_path = Sim_Df_ADFusim_Path;
            path_info->path_len = sizeof(Sim_Df_ADFusim_Path) / sizeof(UInt16);
            break;
        
        case APDUFILEID_DF_PHONEBK:
            
            if (SIMSERVICESTATUS_ACTIVATED == localPBKStatus)
            {
                path_info->select_path = Sim_Df_Local_PhoneBK_Path;
                path_info->path_len = sizeof(Sim_Df_Local_PhoneBK_Path) / sizeof(UInt16);
            }
            else
            {
                path_info->select_path = Sim_Df_Global_PhoneBK_Path;
                path_info->path_len = sizeof(Sim_Df_Global_PhoneBK_Path) / sizeof(UInt16);
            }
            break;
        
        case APDUFILEID_DF_GRAPHICS:
            
            path_info->select_path = Sim_Df_Graphics_Path;
            path_info->path_len = sizeof(Sim_Df_Graphics_Path) / sizeof(UInt16);
            break;
        
        case APDUFILEID_DF_ACTNET:
            
            path_info->select_path = Sim_Df_Acenet_Path;
            path_info->path_len = sizeof(Sim_Df_Acenet_Path) / sizeof(UInt16);
            break;
        
        case APDUFILEID_DF_CINGULAR:
            
            path_info->select_path = Sim_Df_Cingular_Path;
            path_info->path_len = sizeof(Sim_Df_Cingular_Path) / sizeof(UInt16);
            break;
        
        case APDUFILEID_DF_MULTIMEDIA:
            
            path_info->select_path = Sim_Df_Multimedia_Path;
            path_info->path_len = sizeof(Sim_Df_Multimedia_Path) / sizeof(UInt16);
            break;
        
        case APDUFILEID_DF_SOLSA:
            
            path_info->select_path = Sim_Df_SoLSA_Path;
            path_info->path_len = sizeof(Sim_Df_SoLSA_Path) / sizeof(UInt16);
            break;
            
        case APDUFILEID_MF:
            
            path_info->select_path = NULL; 
            path_info->path_len = 0;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Unknow or not support DF ID:0x%X\n",dfile_id);
            goto Error;
    }
    result = TRUE;

Error:
    return result;  
}


void SimIOSubmitRestrictedAccess(KRIL_CmdList_t     *pdata, 
                                 APDUFileID_t       efile_id, 
                                 APDUFileID_t       dfile_id, 
                                 KrilSimDedFilePath_t *path_info)
{
    KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
    Int16 i;
    
    if (path_info->path_len > 0)
    {
        for(i = 0 ; i < path_info->path_len ; i++ )
            KRIL_DEBUG(DBG_ERROR,"select_path[%d]:0x%X\n", i, path_info->select_path[i]);
    }
    else
    {
        KRIL_DEBUG(DBG_ERROR,"select_path is NULL\n");
    }
    
    if (cmd_data->data[0] != '\0')
    {
        CAPI2_SimApi_SubmitRestrictedAccessReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID,
            (APDUCmd_t)cmd_data->command, efile_id, dfile_id, (UInt8)cmd_data->p1, (UInt8)cmd_data->p2,
            (UInt8)cmd_data->p3, path_info->path_len, path_info->select_path, (UInt8*)cmd_data->data);
    }
    else
    {
        CAPI2_SimApi_SubmitRestrictedAccessReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID,
            (APDUCmd_t)cmd_data->command, efile_id, dfile_id, (UInt8)cmd_data->p1, (UInt8)cmd_data->p2,
            (UInt8)cmd_data->p3, path_info->path_len, path_info->select_path, NULL);
    }    
}


Boolean SimIOGetImageInstfileID(const UInt8 *sim_data)
{
    Boolean result = FALSE;
    Int16 CurInstance, CurDataOffset;
    UInt8* pbData;
    
    if (NULL == sim_data)
    {
        KRIL_DEBUG(DBG_ERROR,"sim_data is NULL Error!!\n");
        goto Error;
    }
        
    gImageInstCount = sim_data[0];
    
    if (1 > gImageInstCount || 30 < gImageInstCount)
    {
        KRIL_DEBUG(DBG_ERROR,"InstanceCount is invalid: %d\n",gImageInstCount);
        goto Error;
    }
    
    //retrieve the image instance file ID from sim data
    for (CurInstance = 0 ; CurInstance < gImageInstCount ; CurInstance++)
    {
        CurDataOffset = SIM_IMAGE_INSTANCE_COUNT_SIZE + (CurInstance * SIM_IMAGE_INSTANCE_SIZE);
        pbData = (UInt8*)(sim_data + CurDataOffset);
        
        gImageInstance[CurInstance] = (APDUFileID_t)((UInt16)pbData[3] << 8 | (UInt16)pbData[4]);
        KRIL_DEBUG(DBG_ERROR,"SIM IMAGE INSTANCE: gImageInstance[%d]: 0x%X\n", CurInstance, gImageInstance[CurInstance]);
    }

    result = TRUE;
        
Error:
    return  result;  
}


Boolean SimIOEFileUndefFirstUSIMADF(APDUFileID_t efile_id, SIM_APPL_TYPE_t SimAppType)
{
    Boolean result = FALSE;
    
    if (SIM_APPL_2G == SimAppType)
        return result;
    
    switch (efile_id)
    {
        case APDUFILEID_EF_LP     :     
        case APDUFILEID_EF_IMSI   :   
        case APDUFILEID_EF_KC     :     
        case APDUFILEID_EF_PLMNSEL: 
        case APDUFILEID_EF_HPLMN  :   
        case APDUFILEID_EF_ACMMAX :   
        case APDUFILEID_EF_SST    :   
        case APDUFILEID_EF_ACM    :   
        case APDUFILEID_EF_GID1   :   
        case APDUFILEID_EF_GID2   :   
        case APDUFILEID_EF_PUCT   :   
        case APDUFILEID_EF_CBMI   :   
        case APDUFILEID_EF_SPN    :   
        case APDUFILEID_EF_CBMID  :   
        case APDUFILEID_EF_BCCH   :   
        case APDUFILEID_EF_ACC    :   
        case APDUFILEID_EF_FPLMN  :   
        case APDUFILEID_EF_LOCI   :   
        case APDUFILEID_EF_AD     :     
        case APDUFILEID_EF_PHASE  :   
        case APDUFILEID_EF_VGCS   :   
        case APDUFILEID_EF_VGCSS  :   
        case APDUFILEID_EF_VBS    :   
        case APDUFILEID_EF_VBSS   :   
        case APDUFILEID_EF_EMLPP  :   
        case APDUFILEID_EF_AAEM   :   
        case APDUFILEID_EF_ECC    :   
        case APDUFILEID_EF_CBMIR  :   
        case APDUFILEID_EF_DCK    :   
        case APDUFILEID_EF_CNL    :   
        case APDUFILEID_EF_NIA    :   
        case APDUFILEID_EF_SUME   :   
        case APDUFILEID_EF_PLMN_WACT : 
        case APDUFILEID_EF_OPLMN_WACT:
        case APDUFILEID_EF_HPLMN_WACT:
        case APDUFILEID_EF_CPBCCH :   
        case APDUFILEID_EF_INV_SCAN  : 
        case APDUFILEID_EF_RPLMN_ACT : 
        case APDUFILEID_EF_PNN    :   
        case APDUFILEID_EF_OPL    :   
        case APDUFILEID_EF_MBDN   :   
        case APDUFILEID_EF_EXT6   :   
        case APDUFILEID_EF_MBI    :   
        case APDUFILEID_EF_MWIS   :   
        case APDUFILEID_EF_CFIS   :   
        case APDUFILEID_EF_EXT7   :   
        case APDUFILEID_EF_SPDI   :   
        case APDUFILEID_EF_MMSN   :   
        case APDUFILEID_EF_EXT8   :   
        case APDUFILEID_EF_MMSICP :   
        case APDUFILEID_EF_MMSUP  :   
            
            result = TRUE;
            break;
        
        default:
            result = FALSE;
            break;
    }   

    return  result;
}


Boolean SimIODetermineResponseError(UInt8 dwSW1, UInt8 dwSW2)
{
    Boolean hr = FALSE;
    
    switch (dwSW1)
    {
        case 0x90:
        case 0x91:
        case 0x9F:
            // This is good
            hr = TRUE;
            break;
        
        case 0x92:
            if ((dwSW2 & 0xf0) == 0x00)
            {
                // The command completed OK, but the SIM had to retry the command internally
                // a few times, but what do I care if the SIM is whining?
                hr = TRUE;
            }
            else
            {
                // Memory problem
                hr = FALSE;
            }
            break;
        
        case 0x94:
            // SW2 = 0x00, 0x02, or 0x08
            // These all means that there was a problem with the file (e.g. invalid address, or
            // they specified the type of file incorrectly)
            hr = FALSE;
            break;
        
        case 0x98:
            // Security and/or authentication problems
            hr = FALSE;
            break;
        
        case 0x6F:
            // Internal SIM error, no further information given
            hr = FALSE;
            break;
        
        case 0x67:
        case 0x6B:
        case 0x6D:
        case 0x6E:
            // I really shouldn't be getting these -- it means that one of the parameters I sent
            // down to the SIM was invalid, but I already error checked for those -- so I guess
            // it's best to assert and let a tester yell at me.  In other words, FALL THROUGH!
        default:
            // I don't recognize this command!
            hr = FALSE;
            break;
    }

    return hr;
}


void ParseSimBoolData(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    Boolean* rsp = (Boolean*)capi2_rsp->dataBuf;
    KrilFacLock_t *lock_status;
    
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
    pdata->bcm_ril_rsp = kmalloc(sizeof(KrilFacLock_t), GFP_KERNEL);
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }

    lock_status = pdata->bcm_ril_rsp;
    memset(lock_status, 0, sizeof(KrilFacLock_t));
    pdata->rsp_len = sizeof(KrilFacLock_t);
    
    if (capi2_rsp->result != RESULT_OK)
    {
        KRIL_DEBUG(DBG_ERROR,"Get Lock status failed!! result:%d\n", capi2_rsp->result);
        lock_status->result = BCM_E_GENERIC_FAILURE;
        lock_status->lock = 0;
    }
    else
    {
        lock_status->result = BCM_E_SUCCESS;
        lock_status->lock = (*rsp ? 1 : 0);
        KRIL_DEBUG(DBG_INFO,"Get Lock status:%d\n", *rsp);
    }     
    
    pdata->handler_state = BCM_FinishCAPI2Cmd;
}


void ParseGetCallBarringData(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{   
    if (NULL == capi2_rsp->dataBuf)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
    }
    else
    {
        if (MSG_SS_CALL_BARRING_STATUS_RSP == capi2_rsp->msgType)
        {
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilFacLock_t), GFP_KERNEL);
            if (NULL == pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                CallBarringStatus_t* rsp = (CallBarringStatus_t*) capi2_rsp->dataBuf;
                KrilFacLock_t *lock_status;
                lock_status = pdata->bcm_ril_rsp;
                memset(lock_status, 0, sizeof(KrilFacLock_t));
                pdata->rsp_len = sizeof(KrilFacLock_t);
                KRIL_DEBUG(DBG_ERROR,"netCause:%d call_barring_type:%d class_size:%d\n", rsp->netCause, rsp->call_barring_type, rsp->class_size);
                if (GSMCAUSE_SUCCESS == rsp->netCause)
                {
                    int i;
                    lock_status->lock = 0;
                    for (i = 0 ; i < rsp->class_size ; i++)
                    {
                        KRIL_DEBUG(DBG_ERROR,"activate[%d]:%d ss_class:%d lock:%d\n", i, rsp->ss_activation_class_info[i].activated, rsp->ss_activation_class_info[i].ss_class, lock_status->lock);
                        if (TRUE == rsp->ss_activation_class_info[i].activated)
                            lock_status->lock |= SvcClassToATClass(rsp->ss_activation_class_info[i].ss_class);
                    }
                    lock_status->result = BCM_E_SUCCESS;
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR,"Get Lock status failed!! netCause:%d\n", rsp->netCause);
                    lock_status->result = BCM_E_GENERIC_FAILURE;
                    lock_status->lock = 0;
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                 }
            }
        }
        else
        {
            KRIL_DEBUG(DBG_ERROR,"Receive error MsgType:0x%x...!\n", capi2_rsp->msgType);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
        }
    }
}


int ParseSetCallBarringData(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{   

    KrilSimPinResult_t *SimPinResult;
    
    pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimPinResult_t), GFP_KERNEL);
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
        return 0;
    }

    SimPinResult = pdata->bcm_ril_rsp;
    memset(SimPinResult, 0, sizeof(KrilSimPinResult_t));
    pdata->rsp_len = sizeof(KrilSimPinResult_t);
    SimPinResult->result = BCM_E_SUCCESS;
    SimPinResult->remain_attempt = 0;
    
    if (capi2_rsp->msgType == MSG_SS_CALL_BARRING_RSP)
    {
        NetworkCause_t* presult = (NetworkCause_t*) capi2_rsp->dataBuf;
        KRIL_DEBUG(DBG_ERROR,"netCause:%d\n", *presult);
        if(*presult != GSMCAUSE_SUCCESS)
        {
            SimPinResult->result = BCM_E_GENERIC_FAILURE;
            return FALSE;
        }
    }
    else if (capi2_rsp->msgType == MSG_SS_CALL_BARRING_STATUS_RSP)
    {
        CallBarringStatus_t* presult = (CallBarringStatus_t*) capi2_rsp->dataBuf;
        KRIL_DEBUG(DBG_ERROR,"netCause:%d call_barring_type:%d class_size:%d\n", presult->netCause, presult->call_barring_type, presult->class_size);

        if (GSMCAUSE_SUCCESS == presult->netCause)
        {
            int i;
            for(i = 0 ; i < presult->class_size ; i++)
            {
                KRIL_DEBUG(DBG_ERROR,"activate[%d]:%d ss_class:%d\n", i, presult->ss_activation_class_info[i].activated, presult->ss_activation_class_info[i].ss_class);
            }
        }
        else
        {
            KRIL_DEBUG(DBG_ERROR,"Set Call Barring failed!! netCause:%d\n", presult->netCause);
            SimPinResult->result = BCM_E_GENERIC_FAILURE;
            return FALSE;
         }
    }
    else
    {
        KRIL_DEBUG(DBG_ERROR,"Receive error MsgType:0x%x...!\n", capi2_rsp->msgType);
        SimPinResult->result = BCM_E_GENERIC_FAILURE;
        return FALSE;
    }
    return TRUE;
}


int ParseSimAccessResult(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    SIMAccess_t simAccessType;
    SIMAccess_t* rsp = (SIMAccess_t*)capi2_rsp->dataBuf;

    KrilSimPinResult_t *SimPinResult;
    
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return 0;
    }

    pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimPinResult_t), GFP_KERNEL);
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
        return 0;
    }

    SimPinResult = pdata->bcm_ril_rsp;
    memset(SimPinResult, 0, sizeof(KrilSimPinResult_t));
    pdata->rsp_len = sizeof(KrilSimPinResult_t);
        
    simAccessType = *rsp;

    KRIL_DEBUG(DBG_ERROR,"SIM_ACCESS_RESULT_t:%d\n", simAccessType);
    switch (simAccessType)
    {
        case SIMACCESS_SUCCESS:
            SimPinResult->result = BCM_E_SUCCESS;
           break;
        
        case SIMACCESS_INCORRECT_CHV:
//        case SIMACCESS_BLOCKED_CHV:
            SimPinResult->result = BCM_E_PASSWORD_INCORRECT;
            break;
            
        case SIMACCESS_BLOCKED_CHV:	// pin 2 error
			SimPinResult->result = BCM_E_SIM_PUK2;
			break;
            
        default:
            SimPinResult->result = BCM_E_GENERIC_FAILURE;
            break;
    }
    return 1;
}


int ParseRemainingPinAttempt(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    PIN_ATTEMPT_RESULT_t *rsp = (PIN_ATTEMPT_RESULT_t*)capi2_rsp->dataBuf;

    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return 0;
    }

    if (rsp->result != SIMACCESS_SUCCESS)
    {
        KRIL_DEBUG(DBG_ERROR,"Get remaining PIN attempt failed!! result:%d\n", rsp->result);
        return 0;
    }

    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"pdata->bcm_ril_rsp is NULL, Error!!\n");
        return 0;
    }
    
    KRIL_DEBUG(DBG_ERROR,"Remaining Attempt: pin1:%d pin2:%d puk1:%d puk2:%d\n",
               rsp->pin1_attempt_left,
               rsp->pin2_attempt_left,
               rsp->puk1_attempt_left,
               rsp->puk2_attempt_left
               );

    if (BRCM_RIL_REQUEST_OEM_HOOK_RAW == pdata->ril_cmd->CmdID)
    {
        UInt8* resp = (UInt8*)pdata->bcm_ril_rsp;
        
        resp[5] = rsp->pin1_attempt_left;
        resp[6] = rsp->pin2_attempt_left;
        resp[7] = rsp->puk1_attempt_left;
        resp[8] = rsp->puk2_attempt_left;
    }
    else
    {
        KrilSimPinResult_t *SimPinResult = NULL;
        SimPinResult = (KrilSimPinResult_t*)pdata->bcm_ril_rsp;
            
        switch (pdata->ril_cmd->CmdID)
        {
            case BRCM_RIL_REQUEST_ENTER_SIM_PIN:
                SimPinResult->remain_attempt = rsp->pin1_attempt_left;
                break;
            
            case BRCM_RIL_REQUEST_ENTER_SIM_PIN2:
                SimPinResult->remain_attempt = rsp->pin2_attempt_left;
                break;
            
            case BRCM_RIL_REQUEST_ENTER_SIM_PUK:
                SimPinResult->remain_attempt = rsp->puk1_attempt_left;
                break;
            
            case BRCM_RIL_REQUEST_ENTER_SIM_PUK2:
                SimPinResult->remain_attempt = rsp->puk2_attempt_left;
                break;
            
            case BRCM_RIL_REQUEST_CHANGE_SIM_PIN:
                SimPinResult->remain_attempt = rsp->pin1_attempt_left;
                break;
                
            case BRCM_RIL_REQUEST_CHANGE_SIM_PIN2:
                SimPinResult->remain_attempt = rsp->pin2_attempt_left;
                break;
                
            case BRCM_RIL_REQUEST_SET_FACILITY_LOCK:
            {
                KrilSetFacLock_t *cmd_data = (KrilSetFacLock_t*)pdata->ril_cmd->data;
                
                switch (cmd_data->fac_id)
                {
                    case FAC_SC:
                        SimPinResult->remain_attempt = rsp->pin1_attempt_left;
                        break;
                    
                    case FAC_FD:
                        SimPinResult->remain_attempt = rsp->pin2_attempt_left;
                        break;
                    
                    default:
                        KRIL_DEBUG(DBG_ERROR,"Facility ID:%d Not Supported Error!!\n", cmd_data->fac_id);
                        return 0;
                }
                break;
            }
            
            default:
                KRIL_DEBUG(DBG_ERROR,"CmdID:%lu Not Supported Error!!\n", pdata->ril_cmd->CmdID);
                return 0;
        }
        KRIL_DEBUG(DBG_ERROR,"CmdID:%lu remain_attempt:%d\n", pdata->ril_cmd->CmdID, SimPinResult->remain_attempt);
    }
    
    return 1;
}


int ParseSimPinResult(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    SIMAccess_t result;         
    SIM_PIN_Status_t pin_status;    
    SIM_PIN_Status_t* rsp = (SIM_PIN_Status_t*)capi2_rsp->dataBuf;

    KrilSimStatusResult_t *p_sim_status;
    
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return 0;
    }
 
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"bcm_ril_rsp is NULL. Error!!!\n");
        return 0;
    }

    result = ((capi2_rsp->result == RESULT_OK) ? SIMACCESS_SUCCESS : SIMACCESS_INVALID);          
    pin_status = *rsp;    

    p_sim_status = (KrilSimStatusResult_t *)pdata->bcm_ril_rsp;
    
     
    if (result != SIMACCESS_SUCCESS)
    {
        KRIL_DEBUG(DBG_ERROR,"Get PIN status failed!! result:%d\n", result);
        p_sim_status->pin_status = SIM_NOT_READY;
    }
    else
    {
        SetSimPinStatusHandle(pdata->ril_cmd->SimId, pin_status);
        
        switch (pin_status)
        {
            case PIN_READY_STATUS:
                p_sim_status->pin_status = SIM_READY;
/*+20110822 HKPARK FOR DebugScreen Slave*/				
                if(pdata->ril_cmd->SimId == 2)
                {
                   v_SimCheck_Slave = true;
		   }	
		  else if(pdata->ril_cmd->SimId == 1)
		  {
 		     v_SimCheck_MASTER = true;
		  }
/*-20110822 HKPARK FOR DebugScreen Slave*/	   
                break;
            
            case SIM_PIN_STATUS:
                p_sim_status->pin_status = SIM_PIN;
                break;
                
            case SIM_PIN2_STATUS:
                p_sim_status->pin_status = SIM_PIN2;
                break;
                
            case PH_SIM_PIN_STATUS:
                p_sim_status->pin_status = SIM_SIM;
                break;
                
            case PH_NET_PIN_STATUS:
                p_sim_status->pin_status = SIM_NETWORK;
                break;
                
            case PH_NETSUB_PIN_STATUS:
                p_sim_status->pin_status = SIM_NETWORK_SUBSET;
                break;
                
            case PH_SP_PIN_STATUS:
                p_sim_status->pin_status = SIM_SERVICE_PROVIDER;
                break;
                
            case PH_CORP_PIN_STATUS:
                p_sim_status->pin_status = SIM_CORPORATE;
                break;
            
            case SIM_PUK_STATUS:
                p_sim_status->pin_status = SIM_PUK;
                break;
                
            case SIM_PUK2_STATUS:
                p_sim_status->pin_status = SIM_PUK2;
                break;
                
            case PH_SIM_PUK_STATUS:
                p_sim_status->pin_status = SIM_SIM_PUK;
                break;
            
            case PH_NET_PUK_STATUS:
                p_sim_status->pin_status = SIM_NETWORK_PUK;
                break;
                
            case PH_NETSUB_PUK_STATUS:
                p_sim_status->pin_status = SIM_NETWORK_SUBSET_PUK;
                break;
                
            case PH_SP_PUK_STATUS:
                p_sim_status->pin_status = SIM_SERVICE_PROVIDER_PUK;
                break;
                
            case PH_CORP_PUK_STATUS:
                p_sim_status->pin_status = SIM_CORPORATE_PUK;
                break;
                
            case SIM_PUK_BLOCK_STATUS:
                p_sim_status->pin_status = SIM_PUK_BLOCK;
                break;
                
            case SIM_PUK2_BLOCK_STATUS:
                p_sim_status->pin_status = SIM_PUK2_BLOCK;
                break;
            
            case SIM_ABSENT_STATUS:
                p_sim_status->pin_status = SIM_ABSENT;
/*+20111110 HKPARK if NO SIM status, send Error Message*/
//                if(pdata->ril_cmd->SimId == 2)
//                {
//                   v_NoSimCheck2 = true;
//                 }
//		  else if(pdata->ril_cmd->SimId == 1)
//		  {
//		    v_NoSimCheck1 =true;
//		  }
/*-20111110 HKPARK if NO SIM status, send Error Message*/				
                break;
                
            case NO_SIM_PIN_STATUS:
                p_sim_status->pin_status = SIM_NOT_READY;
                break;
                
            case SIM_ERROR_STATUS:
            default:
                p_sim_status->pin_status = SIM_ERROR;
/*+20111110 HKPARK if NO SIM status, send Error Message*/
//                if(pdata->ril_cmd->SimId == 2)
//                {
//                   v_NoSimCheck2 = true;
//                 }
//		  else if(pdata->ril_cmd->SimId == 1)
//		  {
//		    v_NoSimCheck1 =true;
//		  }
/*-20111110 HKPARK if NO SIM status, send Error Message*/						
                break;
        }
    }

    switch (KRIL_GetSimAppType(pdata->ril_cmd->SimId))
    {
        case SIM_APPL_2G:
           p_sim_status->app_type = BCM_APPTYPE_SIM;
           break;

        case SIM_APPL_3G:
           p_sim_status->app_type = BCM_APPTYPE_USIM;
           break;

        default:
           p_sim_status->app_type = BCM_APPTYPE_UNKNOWN;
           break;
    }
    
    return 1;
}


int ParseIsPUK2Blocked(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    Boolean* rsp = (Boolean*)capi2_rsp->dataBuf;
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return 0;
    }
    
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"bcm_ril_rsp is NULL. Error!!!\n");
        return 0;
    }
        
    if (1 == *rsp)
    {
        KrilSimStatusResult_t *p_sim_status = (KrilSimStatusResult_t *)pdata->bcm_ril_rsp;
        
        KRIL_DEBUG(DBG_ERROR,"PUK2 is blocked\n");
        p_sim_status->pin2_status = SIM_PUK2_BLOCK;
    }
    
    return 1;
}


int ParseIsPIN2Blocked(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    Boolean* rsp = (Boolean*)capi2_rsp->dataBuf;
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return 0;
    }
    
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"bcm_ril_rsp is NULL. Error!!!\n");
        return 0;
    }
        
    if (1 == *rsp)
    {
        KrilSimStatusResult_t *p_sim_status = (KrilSimStatusResult_t *)pdata->bcm_ril_rsp;
        
        KRIL_DEBUG(DBG_ERROR,"PIN2 is blocked\n");
        p_sim_status->pin2_status = SIM_PUK2;
    }

    return 1;
}


int ParseIsPIN1Enable(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    Boolean* rsp = (Boolean*)capi2_rsp->dataBuf;
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return 0;
    }
    
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"bcm_ril_rsp is NULL. Error!!!\n");
        return 0;
    }

    {
        KrilSimStatusResult_t *p_sim_status = (KrilSimStatusResult_t *)pdata->bcm_ril_rsp;
        
        if (1 == *rsp)
        {
            KRIL_DEBUG(DBG_ERROR,"PIN1 is enable\n");
            p_sim_status->pin1_enable = 1;
        }
        else
        {
            KRIL_DEBUG(DBG_ERROR,"PIN1 is disable\n");
            p_sim_status->pin1_enable = 0;
        }
    }
                
    return 1;    
}


int ParseIsPIN2Enable(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    Boolean* rsp = (Boolean*)capi2_rsp->dataBuf;
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return 0;
    }
    
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"bcm_ril_rsp is NULL. Error!!!\n");
        return 0;
    }

    {
        KrilSimStatusResult_t *p_sim_status = (KrilSimStatusResult_t *)pdata->bcm_ril_rsp;
        
        if (1 == *rsp)
        {
            KRIL_DEBUG(DBG_ERROR,"PIN2 is enable\n");
            p_sim_status->pin2_enable = 1;
        }
        else
        {
            KRIL_DEBUG(DBG_ERROR,"PIN2 is disable\n");
            p_sim_status->pin2_enable = 0;
        }
    }
    
    return 1;    
}


Boolean ParseSimServiceStatusResult(Kril_CAPI2Info_t* capi2_rsp, 
                                    SIMServiceStatus_t* localPBKStatus)
{
    SIMServiceStatus_t* rsp = (SIMServiceStatus_t*)capi2_rsp->dataBuf;
    
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        return FALSE;
    }
    
    if (capi2_rsp->result != RESULT_OK)
    {
        KRIL_DEBUG(DBG_ERROR,"CAPI2_SimApi_GetServiceStatus result failed!! result:%d\n", capi2_rsp->result);
        return FALSE;
    }

    *localPBKStatus = *rsp;
    KRIL_DEBUG(DBG_ERROR,"Local PHONE BOOK Status:%s\n",
        SIMSERVICESTATUS_ACTIVATED == *localPBKStatus ? "ACTIVATED": "not ACTIVATED or not ALLOCATED");
    return TRUE;
}

#ifdef CONVERT_USIM_SELECT_RESPONSE_FORMAT_TO_SIM
// Refer to TS 51.011 section 9.2.1
void ConvertUSIMSelectRspformatToSIM(KRIL_CmdList_t *pcmd,
                                     SIM_RESTRICTED_ACCESS_DATA_t *rsp, 
                                     UInt8 *data, 
                                     UInt16 *data_len
                                     )
{
    KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pcmd->ril_cmd->data;
    int fileid = cmd_data->fileid;
    UInt8 *pdata = rsp->data;
    UInt16 pdata_len = rsp->data_len;
    UInt16 i;
    
    // Fill Byte 1-2: RFU
    data[0] = 0;
    data[1] = 0;
    
    // Fill Byte 3-4: File Size
    for (i = 0 ; i < (pdata_len-1) ; i++)
    {
        if (0x80 == pdata[i] && 0x02 == pdata[i+1])
        {
            data[2] = pdata[i+2];
            data[3] = pdata[i+3];
            break;
        }
    }
    
    if (i == (pdata_len-1))
    {
        KRIL_DEBUG(DBG_ERROR,"Get File Size failed!!\n");
        data[2] = 0x00;
        data[3] = 0x00;
    }
    
    // Fill Byte 5-6: File ID
#if 0
    for (i = 0 ; i < (pdata_len-1) ; i++)
    {
        if (0x83 == pdata[i] && 0x02 == pdata[i+1])
        {
            data[4] = pdata[i+2];
            data[5] = pdata[i+3];
            break;
        } 
    }
    
    if (i == (pdata_len-1))
    {
        KRIL_DEBUG(DBG_ERROR,"Get File ID failed!!\n");
        data[4] = 0x00;
        data[5] = 0x00;
    }
#endif

    data[4] = (fileid>>8)&0xFF;
    data[5] = fileid&0xFF;
    
    // Fill Byte 7: Type of file
    data[6] = 0x04;
    
    // Fill Byte 8:
    data[7] = 0x00;
    
    // Fill Byte 9-11: Access conditions
    if (APDUFILEID_EF_ADN == fileid || 
        APDUFILEID_EF_FDN == fileid ||
        APDUFILEID_EF_MSISDN == fileid)
    {
        data[8]  = 0x11;
        data[9]  = 0x00;
        data[10] = 0x22;
    }
    else if (APDUFILEID_EF_SMS == fileid)
    {
        data[8]  = 0x11;
        data[9]  = 0x00;
        data[10] = 0x55;
    }
    else if ((APDUFILEID_EF_IMG == fileid) || (APDUFILEID_EF_PBR == fileid))
    {
        data[8]  = 0x14;
        data[9]  = 0x00;
        data[10] = 0x44;
    }
    else if (APDUFILEID_EF_CFIS == fileid)
    {
        data[8]  = 0x11;
        data[9]  = 0x00;
        data[10] = 0x44;
    }
    else
    {
        /* EF_EMAIL, EF_ADN are of the same access conditions. However, their IDs are not fixed in USIM (actual ID is indicated in EF_PBR)
         * Temp workaround: Assume read/update are allowed in default case. If it is not so, we hope CP can catch the error if some wrong command is issued.
         * The current Android framework code does not access this field.
         */
        data[8]  = 0x11;
        data[9]  = 0x00;
        data[10] = 0x22;
    }
 
    // Fill Byte 12: File status
    data[11] = 0x01;
    
    // Fill Byte 13: Length of following data
    data[12] = 0x02;


    /* The file descriptor byte can be decoded from 'File Descriptor' (TLV = 0x82 0x02|0x05 ...) */
    for (i = 0 ; i < (pdata_len-1) ; i++)
    {
        if (0x82 == pdata[i] && (0x02 == pdata[i+1] || 0x05 == pdata[i+1]))
        {
            /*
              [ETSI 102 221 (V9.1.0) Table 11.5]
              -----001 Transparent structure
              -----010 Linear fixed structure
              -----110 Cyclic structure

              [3GPP 51.011 9.3 Definitions and Coding]
              Structure of file
              '00' : transparent;
              '01' : linear fixed;
              '03' : cyclic.
            */

            // Fill Byte 14: Structure of EF
            data[13] = (pdata[i+2] & 0x07) >> 1;

            /* '0x82' '0x05' ... are for linear fixed and cyclic files
             * ETSI 102 201 11.1.1.4.3 File Descriptor
             * "Record length", and "Number of records" are conditional fields.
             * C: These bytes are mandatory for linear fixed and cyclic files, otherwise they are not applicable. 
             */
            
            // Fill Byte 15: Length of a record
            data[14] = (0x05 == pdata[i+1])? pdata[i+5]: 0x00;

            break;
        }
    }

    if (i == (pdata_len-1))
    {
        KRIL_DEBUG(DBG_ERROR,"Get Length of a record failed!!\n");
        data[13] = 0x00;
        data[14] = 0x00;
    }
    
    // Fill the length once (constant 15)
    *data_len = 15;
}
#endif // CONVERT_USIM_SELECT_RESPONSE_FORMAT_TO_SIM


void ParseSimRestrictedAccessData(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    SIM_RESTRICTED_ACCESS_DATA_t *rsp = (SIM_RESTRICTED_ACCESS_DATA_t*)capi2_rsp->dataBuf;
    KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
    KrilSimIOResponse_t *simioresult;
    
	
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"bcm_ril_rsp is NULL. Error!!!\n");
        return;
    }

    simioresult = pdata->bcm_ril_rsp;
    
    if (!rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
        simioresult->result = BCM_E_GENERIC_FAILURE;
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
    if (rsp->result != SIMACCESS_SUCCESS)
    {
        KRIL_DEBUG(DBG_ERROR,"SIM IO RSP: SIM access failed!! result:%d\n", rsp->result);
        simioresult->result = BCM_E_GENERIC_FAILURE;
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
    KRIL_DEBUG(DBG_ERROR,"SIM IO: command:%d fileid:0x%X SW1:0x%X SW2:0x%X DataLen:%d\n",
        cmd_data->command, cmd_data->fileid, rsp->sw1, rsp->sw2, rsp->data_len);
            
    // For some specificed EF, if response is error, try again.
#ifndef ANDROID_SIMIO_PATH_AVAILABLE
    if (SimIODetermineResponseError(rsp->sw1, rsp->sw2) == FALSE)
    {
        if (simioresult->searchcount < 1 && 
            SimIOEFileUndefFirstUSIMADF(cmd_data->fileid, KRIL_GetSimAppType(pdata->ril_cmd->SimId)))
        {
            KrilSimDedFilePath_t path_info;
            
            simioresult->searchcount++;
            
            // Get Dedicated file path inform
            SimIOGetDedicatedFilePath(APDUFILEID_DF_GSM, SIMSERVICESTATUS_NOT_ALLOCATED1, &path_info);
            
            KRIL_DEBUG(DBG_ERROR,"DF ID:0x%X path_len:%d searchcount:%d\n", APDUFILEID_DF_GSM,
                path_info.path_len, simioresult->searchcount);
            
            // Start running CAPI2_SimApi_SubmitRestrictedAccessReq
            SimIOSubmitRestrictedAccess(pdata, cmd_data->fileid, APDUFILEID_DF_GSM, &path_info);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            return;               
        }
    }
#endif //ANDROID_SIMIO_PATH_AVAILABLE
    
    if (rsp->data_len > 0)
    {
        //print the SIM data
        //RawDataPrintfun(rsp->data, rsp->data_len, "SIM IO RSP");

        if (rsp->data_len > 256)
        {
            // If this issue happen, modify the buffer length of 
            // simResponse[] in KrilSimIOResponse_t
            KRIL_DEBUG(DBG_ERROR,"SIMIO RSP: SIM response length is too long:%d\n", rsp->data_len);
            simioresult->result = BCM_E_GENERIC_FAILURE;
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            return;
        }
        
        // Get image instance file identifier from the record data of EF IMG
        if ((APDUCmd_t)cmd_data->command == APDUCMD_READ_RECORD && 
            (APDUFileID_t)cmd_data->fileid == APDUFILEID_EF_IMG)
        {
            SimIOGetImageInstfileID(rsp->data);
        }
        
#ifdef CONVERT_USIM_SELECT_RESPONSE_FORMAT_TO_SIM
        // Convert USIM SELECT response format to SIM format.
       if (SIM_APPL_3G == KRIL_GetSimAppType(pdata->ril_cmd->SimId) &&
           APDUCMD_GET_RESPONSE == cmd_data->command) 
        {
            UInt8 data[256];
            UInt16 data_len = 0;
            
            ConvertUSIMSelectRspformatToSIM(pdata, rsp, data, &data_len);
            //RawDataPrintfun(data, data_len, "CONVERT SIMIO");
            HexDataToHexStr(simioresult->simResponse, data, data_len);
        }
        else
#endif // CONVERT_USIM_SELECT_RESPONSE_FORMAT_TO_SIM
        {
            HexDataToHexStr(simioresult->simResponse, rsp->data, rsp->data_len);
        }

    }        
    
    simioresult->sw1 = rsp->sw1;
    simioresult->sw2 = rsp->sw2;    
    
    // Android RIL framework just check the RIL command result (BRIL_Errno), but ignore
    // the SIM IO command response (SW1, SW2). Need fill the RIL command result with 
    // correct value by parsing SIM IO command response.
    if (SimIODetermineResponseError(rsp->sw1, rsp->sw2) == TRUE)
    {
        simioresult->result = BCM_E_SUCCESS;
    }
    else
    {
        simioresult->result = BCM_E_GENERIC_FAILURE;
    } 
    
    pdata->handler_state = BCM_FinishCAPI2Cmd;
}


//*******************************************************************************
/**
	Function to parse the FDN information.

	@param		pcmd(in) pointer to the URIL command data.
	@param		capi2_rsp(in) pointer to the CAPI2 command response.
	@return   void.
	@note
	This function can get the FDN file size and FDN record length.

********************************************************************************/
void ParseFDNInfo(KRIL_CmdList_t *pcmd, Kril_CAPI2Info_t *capi2_rsp)
{
    SIM_RESTRICTED_ACCESS_DATA_t *rsp = (SIM_RESTRICTED_ACCESS_DATA_t*)capi2_rsp->dataBuf;
    UInt8 *pdata = rsp->data;
    UInt16 pdata_len = rsp->data_len;
    UInt16 i;
    
    if (SIM_APPL_3G == KRIL_GetSimAppType(pcmd->ril_cmd->SimId))
    {
        for (i = 0 ; i < (pdata_len-1) ; i++)
        {
            if (0x80 == pdata[i] && 0x02 == pdata[i+1])
            {
                sFDNfilesize[pcmd->ril_cmd->SimId] = (pdata[i+2] << 8) + pdata[i+3];
                break;
            }
        }
        
        for (i = 0 ; i < (pdata_len-1) ; i++)
        {
            if (0x82 == pdata[i] && (0x02 == pdata[i+1] || 0x05 == pdata[i+1]))
            {
                sFDNrecordlength[pcmd->ril_cmd->SimId] = (0x05 == pdata[i+1]) ? pdata[i+5]: 0x00;
                break;
            }
        }
    }
    else if (SIM_APPL_2G == KRIL_GetSimAppType(pcmd->ril_cmd->SimId))
    {
        sFDNfilesize[pcmd->ril_cmd->SimId] = (pdata[2] << 8) + pdata[3];
        sFDNrecordlength[pcmd->ril_cmd->SimId] = pdata[14];        
    }
    
    KRIL_DEBUG(DBG_ERROR,"FDN Info: SimId:%d sFDNfilesize:%d sFDNrecordlength:%d\n", pcmd->ril_cmd->SimId,
        sFDNfilesize[pcmd->ril_cmd->SimId], sFDNrecordlength[pcmd->ril_cmd->SimId]);    
}


//*******************************************************************************
/**
	Function to parse the result of updating FDN record.

	@param		pdata(in) pointer to the URIL command data.
	@param		capi2_rsp(in) pointer to the CAPI2 command response.
	@return   void.
	@note

********************************************************************************/
void ParseWriteFDNResult(KRIL_CmdList_t *pdata, Kril_CAPI2Info_t *capi2_rsp)
{
    PBK_WRITE_ENTRY_RSP_t *rsp = (PBK_WRITE_ENTRY_RSP_t*)capi2_rsp->dataBuf;
    KrilSimIOResponse_t *simioresult;
    
    simioresult = pdata->bcm_ril_rsp;
    
    KRIL_DEBUG(DBG_INFO,"write_result:%d\n", rsp->write_result);
    
    switch (rsp->write_result)
    {
        case PBK_WRITE_SUCCESS:
            simioresult->sw1 = 0x90;
            simioresult->sw2 = 0x00;
            simioresult->result = BCM_E_SUCCESS;
            break;
        
        default:
            simioresult->sw1 = 0x69;
            simioresult->sw2 = 0x82;
            simioresult->result = BCM_E_GENERIC_FAILURE;
            break;
        
    }

    pdata->handler_state = BCM_FinishCAPI2Cmd;
}


//*******************************************************************************
/**
	Function to convert the BCD number to ASCII string.

	@param		p_num(out) the phone number in ASCII format.
	@param		bcdnum(in) BCD number.
	@param		bcd_len(in) BCD number length.
	@return   void.
	@note

********************************************************************************/
void ConvertBCDnumToString(UInt8 *p_num, const UInt8* bcdnum, UInt8 bcd_len)
{
    Boolean   first_digit;
    UInt8     bcd_digit;
    UInt8     i;
    
    first_digit = TRUE;
    
    for (i = 0 ; i < bcd_len ;)
    {
        if (TRUE == first_digit)
        {
            bcd_digit = bcdnum[i] & 0x0F;
            first_digit = FALSE;
        }
        else
        {
            bcd_digit = (bcdnum[i] & 0xF0) >> 4;
            first_digit = TRUE;
            i++;
        }
        
        if (bcd_digit < 10)
        {
            *p_num = bcd_digit + '0';
        }
        else
        {
            switch (bcd_digit)
            {
                case 0x0A:
                  *p_num = '*';
                  break;
                case 0x0B:
                  *p_num = '#';
                  break;
                case 0x0C:
                  *p_num = 'p';
                  break;
                case 0x0D:
                  *p_num = '?';
                  break;
                case 0x0E:
                  *p_num = 'b';
                  break;
                case 0x0F:
                  *p_num = 0x00;
                  break;                
            }
        }
        p_num++;
    }
    
    *p_num = 0x00;    
}


//*******************************************************************************
/**
	Function to get the SS string offset.

	@param		p_num(in) the phone number in ASCII format.
	@return   SS string length.
	@note
	 Compre the phone number with the available SS number.

********************************************************************************/
UInt8 GetOffsetOfSsString(const char* p_num)
{
    UInt8 i;
    UInt8 str_len;
    
    const char *SS_Serv_Code[] = 
    {
      "**21*",  /* Register Call Forward Unconditional */
      "**67*",  /* Register Call Forward When Busy */
      "**61*",  /* Register Call Forward When No Reply */
      "**62*",  /* Register Call Forward When Not Reachable */
      "**002*", /* Register All Call Forward */
      "**004*", /* Register All Call Forward Conditional */
    
      "*21*",   /* Register Call Forward Unconditional */
      "*67*",   /* Register Call Forward When Busy */
      "*61*",   /* Register Call Forward When No Reply */
      "*62*",   /* Register Call Forward When Not Reachable */
      "*002*",  /* Register All Call Forward */
      "*004*",  /* Register All Call Forward Conditional */
    
      "*30#",   /* Activate CLIP For a Number */
      "#30#",   /* Deactivate CLIP For a Number */
      "*31#",   /* Suppress CLIR For Single Call */
      "#31#"    /* Invoke CLIR For Single Call */
    };
    
    for (i = 0; i < (sizeof(SS_Serv_Code) / sizeof(char*)); i++)
    {
        str_len = strlen(SS_Serv_Code[i]);
        
        if (memcmp(p_num, SS_Serv_Code[i], str_len) == 0)
        {
            return (((p_num[str_len] == INTERNATIONAL_CODE) || isdigit(p_num[str_len])) ? str_len : 0);
        }
    }
    
    return 0;
}


//*******************************************************************************
/**
	Function to update the FDN record.

	@param		pdata(in) pointer to the URIL command data.
	@return   void.
	@note
	First parse the SIM IO command and get the number, number type, Alpha 
	coding, Alpha string and Alpha string length. Then use the CAPI2 PBK
	API to update the FDN record.

********************************************************************************/
void UpdateFDNRecordRequest(KRIL_CmdList_t *pdata)
{
    KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
    UInt8 *cmddata = (UInt8 *)cmd_data->data;
    UInt8 alpha_id_num;
    UInt8 type_of_number;
    PBK_Digits_t  number;
    UInt8 *p_num = number;
    UInt8 *pp_num = p_num;
    gsm_TON_t  ton;
    gsm_NPI_t  npi;
    UInt8      bcd_len;
    UInt8      ssOffset;
    ALPHA_CODING_t alpha_coding;
    UInt8          alpha[(SIMPBK_NAME)*2+1];
    UInt8          alpha_size;
    UInt8         i;
    
    for (i = 0 ; i < sFDNrecordlength[pdata->ril_cmd->SimId] ; i++)
    {
        if (0xFF != cmddata[i])
            break;
    }
    
    if (i == sFDNrecordlength[pdata->ril_cmd->SimId])
    {
        // All data are 0xFF, MMI want to clear this record
        KRIL_DEBUG(DBG_INFO,"Clean FDN record:%d\n",cmd_data->p1);
        CAPI2_PbkApi_SendWriteEntryReq(InitClientInfo(pdata->ril_cmd->SimId), PB_FDN, FALSE, (cmd_data->p1-1), UNKNOWN_TON_ISDN_NPI, 
            NULL, ALPHA_CODING_GSM_ALPHABET, 0, NULL);        
        pdata->handler_state = BCM_SIM_UpdateFDNRsult;
        return;       
    }
    
    alpha_id_num = sFDNrecordlength[pdata->ril_cmd->SimId] - FDN_MANDATORY_BYTE_NUM;
    type_of_number = cmddata[alpha_id_num + 1];
    
    ton = (cmddata[alpha_id_num + 1] & 0x70) >> 4;
    npi = cmddata[alpha_id_num + 1] & 0x0F;
    
    if (InternationalTON == ton)
    {
        *p_num++ = '+';
    }
    
    bcd_len = cmddata[alpha_id_num] - 1;
    
    // Convert the BCD number to ASCII.
    ConvertBCDnumToString(p_num, &cmddata[alpha_id_num+2], bcd_len);
    
    // Check if there is a phone number in the SS dialling string
    ssOffset = GetOffsetOfSsString(p_num);
    
    if (ssOffset)
    {
        if (InternationalTON == ton)
        {
            memcpy(pp_num, p_num, ssOffset);
            pp_num[ssOffset] = INTERNATIONAL_CODE;
        }
    }
    
    alpha_coding = (cmddata[0] == ALPHA_CODING_UCS2_80 ? ALPHA_CODING_UCS2_80 : ALPHA_CODING_GSM_ALPHABET);
    
    i = 0;
    
    switch (alpha_coding)
    {
        case ALPHA_CODING_GSM_ALPHABET:
        default:
            while ((cmddata[i] != SIM_RAW_EMPTY_VALUE) && (i < alpha_id_num) && (i < (sizeof(alpha)-1)))
            {
                alpha[i] = cmddata[i];
                i++;
            }
            
            alpha[i+1] = '\0';
            alpha_size = i+1;           
            break;
        
        case ALPHA_CODING_UCS2_80:
            
            while (((cmddata[i+1] != SIM_RAW_EMPTY_VALUE) || (cmddata[i+2] != SIM_RAW_EMPTY_VALUE)) &&
                   ((i + 3) <= alpha_id_num) && 
                   ((i + 2) <= sizeof(alpha))
                   )
            {
                alpha[i] = cmddata[i+1];
                alpha[i+1] = cmddata[i+2];
                i += 2;
            }
            
            alpha[i] = '\0';
            alpha_size = i;           
            break;
    }
    
    KRIL_DEBUG(DBG_INFO,"alpha_id_num:%d type_of_number:0x%02X ton:%d npi:%d bcd_len:%d number:%s\n",
        alpha_id_num,type_of_number,ton,npi,bcd_len,number);
    KRIL_DEBUG(DBG_INFO,"alpha_size:%d alpha_coding:%d alpha:%s\n",alpha_size,alpha_coding,alpha);
    
    CAPI2_PbkApi_SendWriteEntryReq(InitClientInfo(pdata->ril_cmd->SimId), PB_FDN, FALSE, (cmd_data->p1-1), type_of_number, 
        number, alpha_coding, alpha_size, alpha);
    
    pdata->handler_state = BCM_SIM_UpdateFDNRsult;    
}


void SetSimPinStatusHandle(SimNumber_t SimId, SIM_PIN_Status_t SimPinState)
{
    KRIL_DEBUG(DBG_ERROR,"SimId:%d SIM PIN status:%d\n", SimId, SimPinState);
    sSimPinstatus[SimId] = SimPinState;
}


SIM_PIN_Status_t GetSimPinStatusHandle(SimNumber_t SimId)
{
    KRIL_DEBUG(DBG_ERROR,"SimId:%d SIM PIN status:%d\n", SimId, sSimPinstatus[SimId]);
    return sSimPinstatus[SimId];
}

void SetSIMData(SimNumber_t SimId, SIMLOCK_SIM_DATA_t *sim_data)
{
    if (NULL == sSimdata[SimId])
    {
        sSimdata[SimId] = kmalloc(sizeof(SIMLOCK_SIM_DATA_t), GFP_KERNEL);
        if (!sSimdata[SimId])
        {
            KRIL_DEBUG(DBG_ERROR,"Allocate memory for sSimdata Failed!!\n");
            return;
        }
    }
    
    memcpy(sSimdata[SimId], sim_data, sizeof(SIMLOCK_SIM_DATA_t));  
}

SIMLOCK_SIM_DATA_t* GetSIMData(SimNumber_t SimId)
{
    if (!sSimdata[SimId])
    {
        KRIL_DEBUG(DBG_ERROR,"SIM data is NULL\n");
        return NULL;
    }
    return sSimdata[SimId];
}
EXPORT_SYMBOL(GetSIMData);


int SetFacilityLockStateHandler(KRIL_CmdList_t *pdata, SIMLOCK_STATE_t* simlock_state)
{
    SIMLockType_t simlock_type;
    SIMLock_Status_t simlock_status;
    KrilSimPinResult_t *SimPinResult;
    KrilSetFacLock_t *cmd_data = (KrilSetFacLock_t*)pdata->ril_cmd->data;

    pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimPinResult_t), GFP_KERNEL);
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd; 
        return 0;
    }
    
    SimPinResult = pdata->bcm_ril_rsp;
    memset(SimPinResult, 0, sizeof(KrilSimPinResult_t));
    pdata->rsp_len = sizeof(KrilSimPinResult_t); 
        
    switch (cmd_data->fac_id)
    {
        case FAC_PS:
            simlock_type = SIMLOCK_PHONE_LOCK;
            break;
            
        case FAC_PN:
            simlock_type = SIMLOCK_NETWORK_LOCK;
            break;
            
        case FAC_PU:
            simlock_type = SIMLOCK_NET_SUBSET_LOCK;
            break;
            
        case FAC_PP:
            simlock_type = SIMLOCK_PROVIDER_LOCK;
            break;
            
        case FAC_PC:
            simlock_type = SIMLOCK_CORP_LOCK;
            break;

        default:
            KRIL_DEBUG(DBG_ERROR,"Facility ID:%d not support Error!!\n", cmd_data->fac_id);
            return 0;
    }
    
    if (SIMLOCK_PHONE_LOCK == simlock_type)
    {
        SIMLOCK_SIM_DATA_t *sim_data;
        
        // Check if SIM data is available
        sim_data = GetSIMData(pdata->ril_cmd->SimId);
        if (!sim_data)
        {
            return 0;
        }
        
        simlock_status = SIMLockSetLock(pdata->ril_cmd->SimId, cmd_data->lock, FALSE, SIMLOCK_PHONE_LOCK, 
            (UInt8*) cmd_data->password, (UInt8*) sim_data->imsi_string, NULL, NULL); 
    }
    else
    {
        simlock_status = SIMLockSetLock(pdata->ril_cmd->SimId, cmd_data->lock, FALSE, simlock_type, 
            (UInt8*) cmd_data->password, NULL, NULL, NULL);
    }
    
    switch (simlock_status)
    {
        case SIMLOCK_SUCCESS:
            SimPinResult->result = BCM_E_SUCCESS;
            break;
        
        case SIMLOCK_WRONG_KEY:
        case SIMLOCK_PERMANENTLY_LOCKED:
            SimPinResult->result = BCM_E_PASSWORD_INCORRECT;
            break;
        
        default:
            SimPinResult->result = BCM_E_GENERIC_FAILURE;
            break;
    }
    
    SIMLockGetSIMLockState(pdata->ril_cmd->SimId, simlock_state);    
    SimPinResult->remain_attempt = (int)SIMLockGetRemainAttempt(pdata->ril_cmd->SimId);
    return 1;
}


int QueryFacilityLockStateHandler(KRIL_CmdList_t *pdata)
{
    SIMLockType_t simlock_type;
    KrilFacLock_t *lock_status;
    KrilGetFacLock_t *cmd_data = (KrilGetFacLock_t*)pdata->ril_cmd->data;
    
    pdata->bcm_ril_rsp = kmalloc(sizeof(KrilFacLock_t), GFP_KERNEL);
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd; 
        return 0;
    }

    lock_status = pdata->bcm_ril_rsp;
    memset(lock_status, 0, sizeof(KrilFacLock_t));
    pdata->rsp_len = sizeof(KrilFacLock_t);

    switch (cmd_data->fac_id)
    {
        case FAC_PS:
            simlock_type = SIMLOCK_PHONE_LOCK;
            break;
            
        case FAC_PN:
            simlock_type = SIMLOCK_NETWORK_LOCK;
            break;
            
        case FAC_PU:
            simlock_type = SIMLOCK_NET_SUBSET_LOCK;
            break;
            
        case FAC_PP:
            simlock_type = SIMLOCK_PROVIDER_LOCK;
            break;
            
        case FAC_PC:
            simlock_type = SIMLOCK_CORP_LOCK;
            break;

        default:
            KRIL_DEBUG(DBG_ERROR,"Facility ID:%d not support Error!!\n", cmd_data->fac_id);
            return 0;
    }
        
    lock_status->lock = (SIMLockIsLockOn(simlock_type, NULL) == TRUE ? 1 : 0);
    lock_status->result = BCM_E_SUCCESS;
    return 1;    
}


int ChangeFacilityLockstateHandler(KRIL_CmdList_t *pdata, SIMLOCK_STATE_t* simlock_state)
{
    SIMLockType_t simlock_type;
    SIMLock_Status_t simlock_status;
    KrilSimPinResult_t *SimPinResult;
    KrilSimPinNum_t *pin_word = pdata->ril_cmd->data;
    
    pdata->bcm_ril_rsp = kmalloc(sizeof(KrilFacLock_t), GFP_KERNEL);
    if (!pdata->bcm_ril_rsp)
    {
        KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd; 
        return 0;
    }

    SimPinResult = pdata->bcm_ril_rsp;
    memset(SimPinResult, 0, sizeof(KrilSimPinResult_t));
    pdata->rsp_len = sizeof(KrilSimPinResult_t);
            
    switch (GetSimPinStatusHandle(pdata->ril_cmd->SimId))
    {
        case PH_SIM_PIN_STATUS:
            simlock_type = SIMLOCK_PHONE_LOCK;
            break;
            
        case PH_NET_PIN_STATUS:
            simlock_type = SIMLOCK_NETWORK_LOCK;
            break;
            
        case PH_NETSUB_PIN_STATUS:
            simlock_type = SIMLOCK_NET_SUBSET_LOCK;
            break;
            
        case PH_SP_PIN_STATUS:
            simlock_type = SIMLOCK_PROVIDER_LOCK;
            break;
            
        case PH_CORP_PIN_STATUS:
            simlock_type = SIMLOCK_CORP_LOCK;
            break;

        default:
            KRIL_DEBUG(DBG_ERROR,"SimId:%d Error SIM PIN status:%d\n", 
            pdata->ril_cmd->SimId, GetSimPinStatusHandle(pdata->ril_cmd->SimId));
            return 0;
    }
    
    if (SIMLOCK_PHONE_LOCK == simlock_type)
    {
        /* PH-SIM lock is a bit different from the other locks:
         * after the password is verified to be correct, the PH-SIM lock
         * is unlocked, but the PH-SIM lock setting is still on and now
         * it is locked to the new SIM card. So we can not use SIMLockUnlockSIM
         * function to unlock PH-SIM lock. We need to first obtain the IMSI
         * of the new SIM card before trying to unlock the PH-SIM lock.
         */
        SIMLOCK_SIM_DATA_t *sim_data;
        
        // Check if SIM data is available
        sim_data = GetSIMData(pdata->ril_cmd->SimId);
        if (!sim_data)
        {
            return 0;
        }
        
        simlock_status = SIMLockSetLock(pdata->ril_cmd->SimId, 1, FALSE, SIMLOCK_PHONE_LOCK, 
            (UInt8*) pin_word->password, (UInt8*) sim_data->imsi_string, NULL, NULL); 
    }
    else
    {
        simlock_status = SIMLockUnlockSIM(pdata->ril_cmd->SimId, simlock_type, (UInt8*)pin_word->password);
    }

    
    switch (simlock_status)
    {
        case SIMLOCK_SUCCESS:
            SimPinResult->result = BCM_E_SUCCESS;
            break;
        
        case SIMLOCK_WRONG_KEY:
        case SIMLOCK_PERMANENTLY_LOCKED:
            SimPinResult->result = BCM_E_PASSWORD_INCORRECT;
            break;
        
        default:
            SimPinResult->result = BCM_E_GENERIC_FAILURE;
            break;
    }
    
    SIMLockGetSIMLockState(pdata->ril_cmd->SimId, simlock_state);    
    SimPinResult->remain_attempt = (int)SIMLockGetRemainAttempt(pdata->ril_cmd->SimId);
    return 1;
}


void KRIL_GetSimStatusHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_ERROR,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
    if (capi2_rsp && capi2_rsp->result != RESULT_OK)
    {
        KRIL_DEBUG(DBG_ERROR,"CAPI2 response failed:%d\n", capi2_rsp->result);
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
    switch (pdata->handler_state)
    {
        case BCM_SendCAPI2Cmd:
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimStatusResult_t), GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            memset(pdata->bcm_ril_rsp, 0, sizeof(KrilSimStatusResult_t));
            pdata->rsp_len = sizeof(KrilSimStatusResult_t);
            
            CAPI2_SimApi_GetPinStatus(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_IS_PIN1_ENABLE;
            break;
        
        case BCM_IS_PIN1_ENABLE:
            if (!ParseSimPinResult(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            // Check if PIN1 is enable
            CAPI2_SimApi_IsPINRequired(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_IS_PUK2_BLOCK;
            break;
            
                    
        case BCM_IS_PUK2_BLOCK:
            if (!ParseIsPIN1Enable(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            // Check if PUK2 is blocked
            CAPI2_SimApi_IsPUKBlocked(InitClientInfo(pdata->ril_cmd->SimId), CHV2);
            pdata->handler_state = BCM_IS_PIN2_BLOCK;
            break;
            
        case BCM_IS_PIN2_BLOCK:
            if (!ParseIsPUK2Blocked(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            // Check if PIN2 is blocked
            CAPI2_SimApi_IsPINBlocked(InitClientInfo(pdata->ril_cmd->SimId), CHV2);
            pdata->handler_state = BCM_IS_PIN2_ENABLE;
            break;
        
        case BCM_IS_PIN2_ENABLE:
            if (!ParseIsPIN2Blocked(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            // Check if PIN2 is enable
            CAPI2_SimApi_IsOperationRestricted(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        
        case BCM_RESPCAPI2Cmd:
            if (!ParseIsPIN2Enable(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
    }
}


void KRIL_EnterSimPinHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_INFO,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
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
            KrilSimPinNum_t *pin_word = pdata->ril_cmd->data;
            
            KRIL_DEBUG(DBG_ERROR,"PIN NUM:%s\n", pin_word->password);
            CAPI2_SimApi_SendVerifyChvReq(InitClientInfo(pdata->ril_cmd->SimId), 
                (pdata->ril_cmd->CmdID == BRCM_RIL_REQUEST_ENTER_SIM_PIN ? CHV1 : CHV2), 
                (UInt8*)(pin_word->password));
            pdata->handler_state = BCM_SIM_GetRemainingPinAttempt;
            break;
        }
        
        case BCM_SIM_GetRemainingPinAttempt:

            if (!ParseSimAccessResult(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            CAPI2_SimApi_SendRemainingPinAttemptReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
            
        case BCM_RESPCAPI2Cmd:
            
            if (!ParseRemainingPinAttempt(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;

        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
    }
}


void KRIL_EnterSimPukHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_INFO,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
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
            KrilSimPinNum_t *pin_word = pdata->ril_cmd->data;
            
            KRIL_DEBUG(DBG_ERROR,"PUK NUM:%s\n", pin_word->password);
            KRIL_DEBUG(DBG_ERROR,"New PIN NUM:%s\n", pin_word->newpassword);
            CAPI2_SimApi_SendUnblockChvReq(InitClientInfo(pdata->ril_cmd->SimId), 
                (pdata->ril_cmd->CmdID == BRCM_RIL_REQUEST_ENTER_SIM_PUK ? CHV1 : CHV2), 
                (UInt8*)(pin_word->password), (UInt8*)(pin_word->newpassword));
            
            pdata->handler_state = BCM_SIM_GetRemainingPinAttempt;
            break;
        }

        case BCM_SIM_GetRemainingPinAttempt:

            if (!ParseSimAccessResult(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            CAPI2_SimApi_SendRemainingPinAttemptReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
            
        case BCM_RESPCAPI2Cmd:
            
            if (!ParseRemainingPinAttempt(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
    }
}


void KRIL_EnterNetworkDepersonHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_ERROR,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
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
            SIMLOCK_STATE_t simlock_state;
            
            if (!ChangeFacilityLockstateHandler(pdata, &simlock_state))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            CAPI2_SIMLOCKApi_SetStatus(InitClientInfo(pdata->ril_cmd->SimId), &simlock_state);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }
        
        case BCM_RESPCAPI2Cmd:
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;        
    }    
}


void KRIL_ChangeSimPinHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_INFO,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
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
            KrilSimPinNum_t *pin_word = pdata->ril_cmd->data;
            
            KRIL_DEBUG(DBG_ERROR,"Old PIN NUM:%s\n", pin_word->password);
            KRIL_DEBUG(DBG_ERROR,"New PIN NUM:%s\n", pin_word->newpassword);
            CAPI2_SimApi_SendChangeChvReq(InitClientInfo(pdata->ril_cmd->SimId), (pdata->ril_cmd->CmdID == BRCM_RIL_REQUEST_CHANGE_SIM_PIN ? CHV1 : CHV2),
                (UInt8*)(pin_word->password), (UInt8*)(pin_word->newpassword));
            pdata->handler_state = BCM_SIM_GetRemainingPinAttempt;
            break;
        }

        case BCM_SIM_GetRemainingPinAttempt:

            if (!ParseSimAccessResult(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            CAPI2_SimApi_SendRemainingPinAttemptReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
            
        case BCM_RESPCAPI2Cmd:
            
            if (!ParseRemainingPinAttempt(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;

        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
    }
}


void KRIL_QueryFacilityLockHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_INFO,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
    
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
            KrilGetFacLock_t *cmd_data = (KrilGetFacLock_t*)pdata->ril_cmd->data;

            KRIL_DEBUG(DBG_ERROR,"Facility ID:%d\n", cmd_data->fac_id);
            switch (cmd_data->fac_id)
            {
                case FAC_SC:
                    CAPI2_SimApi_IsPINRequired(InitClientInfo(pdata->ril_cmd->SimId));
                    pdata->handler_state = BCM_RESPCAPI2Cmd;
                    break;
                
                case FAC_FD:
                    CAPI2_SimApi_IsOperationRestricted(InitClientInfo(pdata->ril_cmd->SimId));
                    pdata->handler_state = BCM_RESPCAPI2Cmd;
                    break;

                case FAC_AO:
                case FAC_OI:
                case FAC_OX:
                case FAC_AI:
                case FAC_IR:
                case FAC_AB:
                case FAC_AG:
                case FAC_AC:

                    {
                        SsApi_SrvReq_t  ssApiSrvReq = {0};
                        SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;
                        SS_Password_t*	ssPwdPtr = &ssSrvReqPtr->param.ssPassword;

                        KRIL_DEBUG(DBG_ERROR,"fac_id:%d password:%s service:%d\n", cmd_data->fac_id, cmd_data->password, cmd_data->service);

                        ssSrvReqPtr->ssCode = ssBarringCodes[cmd_data->fac_id];
                        ssSrvReqPtr->basicSrv.type = BASIC_SERVICE_TYPE_UNSPECIFIED; // refer to AT+CLCK implementation in CP
                        ssSrvReqPtr->operation = SS_OPERATION_CODE_INTERROGATE;
                        memcpy((void*)ssPwdPtr->currentPwd, (void*)cmd_data->password, SS_PASSWORD_LENGTH);

                        CAPI2_SsApi_SsSrvReq(InitClientInfo(pdata->ril_cmd->SimId), &ssApiSrvReq);
                        KRIL_SetSsSrvReqTID(GetTID()); // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0

                        pdata->handler_state = BCM_SS_SendCallBarringReq;
                    }

                    break;

                case FAC_PS:
                case FAC_PN:
                case FAC_PU:
                case FAC_PP:
                case FAC_PC:
                    if (QueryFacilityLockStateHandler(pdata))
                    {
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
                    }
                    else
                    {
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    }    
                    break;
                    
                default:
                    KRIL_DEBUG(DBG_ERROR,"Facility ID:%d Not Supported Error!!\n", cmd_data->fac_id);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    break;
            }
            break;
        }
        
        case BCM_RESPCAPI2Cmd:
        {
            KrilGetFacLock_t *cmd_data = (KrilGetFacLock_t*)pdata->ril_cmd->data;

            switch (cmd_data->fac_id)
            {
                case FAC_SC:
                case FAC_FD:
                    ParseSimBoolData(pdata, capi2_rsp);
                    break;

                case FAC_AO:
                case FAC_OI:
                case FAC_OX:
                case FAC_AI:
                case FAC_IR:
                case FAC_AB:
                case FAC_AG:
                case FAC_AC:
                    ParseGetCallBarringData(pdata, capi2_rsp);
                    break;

                default:
                    KRIL_DEBUG(DBG_ERROR,"Facility ID:%d Not Supported Error!!\n", cmd_data->fac_id);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
            }
            
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }

        case BCM_SS_SendCallBarringReq:
        {
            if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
            {
                SS_SrvRel_t       *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                KrilFacLock_t     *lock_status;

                pdata->bcm_ril_rsp = kmalloc(sizeof(KrilFacLock_t), GFP_KERNEL);
                if(!pdata->bcm_ril_rsp) {
                    KRIL_DEBUG(DBG_ERROR, "unable to allocate pdata->bcm_ril_rsp buf\n");
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
                }
                pdata->rsp_len = sizeof(KrilFacLock_t);

                if (NULL == pdata->bcm_ril_rsp)
                {
                    KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
                }

                memset(pdata->bcm_ril_rsp, 0, pdata->rsp_len);
                lock_status         = pdata->bcm_ril_rsp;
                lock_status->result = BCM_E_GENERIC_FAILURE;
                lock_status->lock   = 0;

                switch (srvRel_rsp->type)
                {
                    case SS_SRV_TYPE_CALL_BARRING_INFORMATION:
                    {
                        SS_CallBarInfo_t      barInfo = srvRel_rsp->param.barringInfo;
                        SS_CallBarFeature_t   inClassInfo;
                        ATC_CLASS_t           theAtcClass = ATC_NOT_SPECIFIED;
                        Boolean               isStatusActive;
                        int                   index;

                        // Follow at_ss.c implementation. It looks like network can compress the response like following example.
                        // srv_group1:status1, srv_group2, srv_group3, srv_group4:status2, srv_group5; and it means the following:
                        // status1: srv_group1, srv_group2, srv_group3
                        // status2: srv_group4, srv_group5
                        isStatusActive = TRUE;

                        for(index = 0; index < barInfo.listSize; index++)
                        {
                            inClassInfo = barInfo.callBarFeatureList[index];

                            KRIL_DEBUG(DBG_INFO, "barInfo.callBarFeatureList[%d]: include=%x, basicSrv.type=%x, basicSrv.content=%x, ssStatus=%x\n", index, inClassInfo.include, inClassInfo.basicSrv.type, inClassInfo.basicSrv.content, inClassInfo.ssStatus);

                            if(inClassInfo.include & 0x01) // svc group is included
                            {
                                theAtcClass = Util_GetATClassFromSvcGrp(inClassInfo.basicSrv);
                            }

                            if(inClassInfo.include & 0x02) // status included
                            {
                                isStatusActive  = (inClassInfo.ssStatus & SS_STATUS_MASK_ACTIVE)? TRUE:FALSE;
                            }

                            if(isStatusActive)
                            {
                                lock_status->lock |= theAtcClass;
                            }
                        }

                        KRIL_DEBUG(DBG_INFO, "lock = %x\n", lock_status->lock);
                        lock_status->result = BCM_E_SUCCESS;
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
                        break;
                    }

                    case SS_SRV_TYPE_BASIC_SRV_INFORMATION:
                    {
                        SS_BasicSrvInfo_t	  bscSrvInfo = srvRel_rsp->param.basicSrvInfo;
                        int                   index;

                        for(index = 0; index < bscSrvInfo.listSize; index++)
                        {
                           KRIL_DEBUG(DBG_INFO, "bscSrvInfo[%d]: basicSrv.type=%x, basicSrv.content=%x\n", index, bscSrvInfo.basicSrvGroupList[index].type, bscSrvInfo.basicSrvGroupList[index].content);
                           lock_status->lock |= Util_GetATClassFromSvcGrp(bscSrvInfo.basicSrvGroupList[index]);;
                        }

                        KRIL_DEBUG(DBG_INFO, "lock = %x\n", lock_status->lock);
                        lock_status->result = BCM_E_SUCCESS;
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
                        break;
                    }

                    case SS_SRV_TYPE_SS_STATUS:
                    {
                        Boolean isProvisioned = FALSE;

                        if( (srvRel_rsp->param.ssStatus & SS_STATUS_MASK_PROVISIONED)&& 
                            (srvRel_rsp->param.ssStatus & SS_STATUS_MASK_ACTIVE))
                        {
                            isProvisioned = TRUE;
                        }

                        KRIL_DEBUG(DBG_INFO, "ssStatus=%x\n", srvRel_rsp->param.ssStatus);

                        if (isProvisioned)
                        {
                           lock_status->lock |= (ATC_VOICE|ATC_DATA);
                        }

                        KRIL_DEBUG(DBG_INFO, "lock = %x\n", lock_status->lock);
                        lock_status->result = BCM_E_SUCCESS;
                        pdata->handler_state = BCM_FinishCAPI2Cmd;
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
                pdata->handler_state = BCM_SS_SendCallBarringReq;
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR,"Receive error MsgType:0x%x...!\n", capi2_rsp->msgType);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
    }
}


void KRIL_SetFacilityLockHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_ERROR,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
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
            KrilSetFacLock_t *cmd_data = (KrilSetFacLock_t*)pdata->ril_cmd->data;
            
            KRIL_DEBUG(DBG_ERROR,"Facility ID:%d password:%s service:%d\n", cmd_data->fac_id, cmd_data->password, cmd_data->service);
            switch (cmd_data->fac_id)
            {
                case FAC_SC:
                    CAPI2_SimApi_SendSetChv1OnOffReq(InitClientInfo(pdata->ril_cmd->SimId), (UInt8*)cmd_data->password, (Boolean)cmd_data->lock);
                    pdata->handler_state = BCM_SIM_GetRemainingPinAttempt;
                    break;
                
                case FAC_FD:
                    CAPI2_SimApi_SendSetOperStateReq(InitClientInfo(pdata->ril_cmd->SimId), 
                        (cmd_data->lock == 1) ? SIMOPERSTATE_RESTRICTED_OPERATION : SIMOPERSTATE_UNRESTRICTED_OPERATION, 
                        (UInt8*)cmd_data->password);
                    pdata->handler_state = BCM_SIM_GetRemainingPinAttempt;
                    break;

                case FAC_AO:
                case FAC_OI:
                case FAC_OX:
                case FAC_AI:
                case FAC_IR:
                case FAC_AB:
                case FAC_AG:
                case FAC_AC:

                    if ((1 == cmd_data->lock) && (FAC_AB == cmd_data->fac_id || FAC_AG == cmd_data->fac_id || FAC_AC == cmd_data->fac_id))
                    {
                        // 3GPP TS 27.007 7.4
                        KRIL_DEBUG(DBG_INFO, "operation not allowed, lock=%d, fac_id=%d\n", cmd_data->lock, cmd_data->fac_id);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    }
                    else
                    {
                        SsApi_SrvReq_t  ssApiSrvReq = {0};
                        SS_SrvReq_t*    ssSrvReqPtr = &ssApiSrvReq.ssSrvReq;
                        SS_Password_t*	ssPwdPtr = &ssSrvReqPtr->param.ssPassword;

                        ssSrvReqPtr->ssCode = ssBarringCodes[cmd_data->fac_id];
                        SS_SvcCls2BasicSrvGroup(cmd_data->service, &(ssSrvReqPtr->basicSrv));

                        if (1 == cmd_data->lock)
                            ssSrvReqPtr->operation = SS_OPERATION_CODE_ACTIVATE;
                        else
                            ssSrvReqPtr->operation = SS_OPERATION_CODE_DEACTIVATE;

                        memcpy((void*)ssPwdPtr->currentPwd, (void*)cmd_data->password, SS_PASSWORD_LENGTH);

                        KRIL_DEBUG(DBG_INFO, "GetServiceClass(%d): type:%d content:%d\n", cmd_data->service, ssSrvReqPtr->basicSrv.type, ssSrvReqPtr->basicSrv.content);

                        CAPI2_SsApi_SsSrvReq(InitClientInfo(pdata->ril_cmd->SimId), &ssApiSrvReq);
                        KRIL_SetSsSrvReqTID(GetTID()); // MSG_MNSS_CLIENT_SS_SRV_REL is an asynchronous response. TID=0

                        pdata->handler_state = BCM_SS_SendCallBarringReq;
                    }
                    break;

                case FAC_PS:
                case FAC_PN:
                case FAC_PU:
                case FAC_PP:
                case FAC_PC:
                {
                    SIMLOCK_STATE_t simlock_state;
                    
                    if (!SetFacilityLockStateHandler(pdata, &simlock_state))
                    {
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        return;
                    }
                    
                    CAPI2_SIMLOCKApi_SetStatus(InitClientInfo(pdata->ril_cmd->SimId), &simlock_state);
                    pdata->handler_state = BCM_RESPCAPI2Cmd;
                    break;
                }
                    
                default:
                    KRIL_DEBUG(DBG_ERROR,"Facility ID:%d Not Supported Error!!\n", cmd_data->fac_id);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
            }
            break;
        }

        case BCM_SIM_GetRemainingPinAttempt:
            if (!ParseSimAccessResult(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            CAPI2_SimApi_SendRemainingPinAttemptReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;

        case BCM_SS_SendCallBarringReq:
        {
            KrilSimPinResult_t *SimPinResult;

            KRIL_DEBUG(DBG_INFO, "msgType:0x%x\n", capi2_rsp->msgType);

            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimPinResult_t), GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
               KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
               pdata->handler_state = BCM_ErrorCAPI2Cmd;
               return;
            }

            SimPinResult = pdata->bcm_ril_rsp;
            memset(SimPinResult, 0, sizeof(KrilSimPinResult_t));
            pdata->rsp_len = sizeof(KrilSimPinResult_t);
            SimPinResult->result = BCM_E_SUCCESS;
            SimPinResult->remain_attempt = 0;

            // Simplify the response due to the limitation of RIL_REQUEST_SET_FACILITY_LOCK response interface.
            // RIL_REQUEST_QUERY_FACILITY_LOCK should provide the detailed information about each service group and associated status.
            if (capi2_rsp->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
            {
                SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? component:%d operation:%d ssCode:%d type:%d\n", srvRel_rsp->component, srvRel_rsp->operation, srvRel_rsp->ssCode, srvRel_rsp->type);
                if (srvRel_rsp->type == SS_SRV_TYPE_RETURN_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_REJECT ||
                    srvRel_rsp->type == SS_SRV_TYPE_LOCAL_ERROR ||
                    srvRel_rsp->type == SS_SRV_TYPE_LOCAL_RESULT ||
                    srvRel_rsp->type == SS_SRV_TYPE_CAUSE_IE)
                {
                    SimPinResult->result = BCM_E_GENERIC_FAILURE;
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
                pdata->handler_state = BCM_SS_SendCallBarringReq;
            }
            else if (capi2_rsp->msgType == MSG_STK_CC_SETUPFAIL_IND)
            {
                StkCallSetupFail_t *rsp = (StkCallSetupFail_t *) capi2_rsp->dataBuf;
                KRIL_DEBUG(DBG_ERROR, "STK block the request::failRes:%d\n", rsp->failRes);
                SimPinResult->result = BCM_E_GENERIC_FAILURE;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
            else
            {
                SimPinResult->result = BCM_E_GENERIC_FAILURE;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
            }
        }
        break;

        case BCM_RESPCAPI2Cmd:
        {
            KrilSetFacLock_t *cmd_data = (KrilSetFacLock_t*)pdata->ril_cmd->data;
            
            if (FAC_SC == cmd_data->fac_id || FAC_FD == cmd_data->fac_id)
            {
                if (!ParseRemainingPinAttempt(pdata, capi2_rsp))
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
                }
            }
            
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
    }
}


#ifdef ANDROID_SIMIO_PATH_AVAILABLE

void KRIL_SimIOHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_ERROR,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
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
            KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
            APDUFileID_t efile_id = APDUFILEID_INVALID_FILE;
            APDUFileID_t dfile_id = APDUFILEID_INVALID_FILE;
            KrilSimIOResponse_t *simioresult; 
            
            KRIL_DEBUG(DBG_ERROR,"SIM IO: command:%d fileid:0x%X DFid:0x%X p1:%d p2:%d p3:%d pin2:%s\n",
                cmd_data->command, cmd_data->fileid, cmd_data->dfid, cmd_data->p1, cmd_data->p2, cmd_data->p3,
                cmd_data->pin2);            
            RawDataPrintfun((UInt8*)cmd_data->path, cmd_data->pathlen*2, "SIM IO: PATH");
            
            // For some EF in USIM, they are under USIM ADF. But Android RIL framework 
            // give wrong path. And this cause reading SIM EF error. Modify the path 
            // in KRIL_SimIOHandler (refer to 3GPP TS 31.102 section 4.2).
            if (SIM_APPL_3G == KRIL_GetSimAppType(pdata->ril_cmd->SimId) &&
                (APDUFILEID_EF_FDN           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MSISDN        ==  cmd_data->fileid   ||
                 APDUFILEID_EF_LP            ==  cmd_data->fileid   ||
                 APDUFILEID_EF_IMSI          ==  cmd_data->fileid   ||
                 APDUFILEID_ISIM_EF_P_CSCF   ==  cmd_data->fileid   ||
                 APDUFILEID_EF_PLMN_WACT     ==  cmd_data->fileid   ||
                 APDUFILEID_EF_HPLMN         ==  cmd_data->fileid   ||
                 APDUFILEID_EF_ACMMAX        ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SST           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_ACM           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_GID1          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_GID2          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SPN           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_PUCT          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_CBMI          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_ACC           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_FPLMN         ==  cmd_data->fileid   ||
                 APDUFILEID_EF_LOCI          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_AD            ==  cmd_data->fileid   ||
                 APDUFILEID_EF_CBMID         ==  cmd_data->fileid   ||
                 APDUFILEID_EF_ECC           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_CBMIR         ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SMS           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SMSP          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SMSS          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SDN           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EXT2          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EXT3          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SMSR          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EXT4_3G       ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EXT5_3G       ==  cmd_data->fileid   ||
                 APDUFILEID_EF_ECCP          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EMLPP         ==  cmd_data->fileid   ||
                 APDUFILEID_EF_AAEM          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_HIDDEN_KEY_3G ==  cmd_data->fileid   ||
                 APDUFILEID_EF_BDN           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_CMI           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EST_3G        ==  cmd_data->fileid   ||
                 APDUFILEID_EF_ACL_3G        ==  cmd_data->fileid   ||
                 APDUFILEID_EF_DCK           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_CNL           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_OPLMN_WACT    ==  cmd_data->fileid   ||
                 APDUFILEID_EF_HPLMN_WACT    ==  cmd_data->fileid   ||
                 APDUFILEID_EF_ARR_UNDER_DF  ==  cmd_data->fileid   ||
                 APDUFILEID_EF_PNN           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_OPL           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MBDN          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EXT6          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MBI           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MWIS          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_CFIS          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EXT7          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_SPDI          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MMSN          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_EXT8          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MMSICP        ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MMSUP         ==  cmd_data->fileid   ||
                 APDUFILEID_EF_MMSUCP        ==  cmd_data->fileid   ||
                 APDUFILEID_EF_VGCS          ==  cmd_data->fileid   ||
                 APDUFILEID_EF_VGCSS         ==  cmd_data->fileid   ||
                 APDUFILEID_EF_VBS           ==  cmd_data->fileid   ||
                 APDUFILEID_EF_VBSS          ==  cmd_data->fileid   ||
                 APDUFILEID_ISIM_EF_P_GBAP   ==  cmd_data->fileid   ||
                 APDUFILEID_ISIM_EF_P_GBANL  ==  cmd_data->fileid
                 )
                )
            {
                cmd_data->dfid = APDUFILEID_USIM_ADF;
                cmd_data->path[0] = APDUFILEID_MF;
                cmd_data->pathlen = 1;
            }

            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimIOResponse_t), GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            simioresult = pdata->bcm_ril_rsp;
            memset(simioresult, 0, sizeof(KrilSimIOResponse_t));
            pdata->rsp_len = sizeof(KrilSimIOResponse_t);
            simioresult->command = cmd_data->command;
            simioresult->fileid = cmd_data->fileid;
            simioresult->searchcount = 0;
            
            efile_id = (APDUFileID_t)cmd_data->fileid;     
            dfile_id = (APDUFileID_t)cmd_data->dfid; 
            
            // If update the FDN record using SIM IO command, the FDN list cache 
            // in PBK module isn't updated. So the PBK module still prevent the 
            // outgoing call whose number is added by SIM IO command. The solution 
            // is replacing the SIM IO command with CAPI2 PBK API.
            if (APDUFILEID_EF_FDN == cmd_data->fileid && APDUCMD_UPDATE_RECORD == cmd_data->command)
            {
                if (NULL != cmd_data->pin2)
                {
                    // Verify the PIN2 before update the FDN record.
                    CAPI2_SimApi_SendVerifyChvReq(InitClientInfo(pdata->ril_cmd->SimId), CHV2, (UInt8*)cmd_data->pin2);
#ifdef UPDATE_FDN_USE_CAPI2_PBK_API
                    pdata->handler_state = BCM_SIM_PBK_UpdateFDNRecord;
#else
                    pdata->handler_state = BCM_SIM_SIMIO_UpdateFDNRecord;
#endif //UPDATE_FDN_USE_CAPI2_PBK_API
                }
                else
                {
                    KRIL_DEBUG(DBG_ERROR,"Update FDN need PIN2. Error!!!\n");
                    simioresult->result = BCM_E_GENERIC_FAILURE;
                    pdata->handler_state = BCM_FinishCAPI2Cmd;
                }
            }
            else
            {
                CAPI2_SimApi_SubmitRestrictedAccessReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID,
                      (APDUCmd_t)cmd_data->command, efile_id, dfile_id, (UInt8)cmd_data->p1, (UInt8)cmd_data->p2,
                      (UInt8)cmd_data->p3, cmd_data->pathlen, cmd_data->path, (UInt8*)cmd_data->data);
                
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }                
            break;
        }
        
        case BCM_SIM_SIMIO_UpdateFDNRecord:
        {
            KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
            APDUFileID_t efile_id = (APDUFileID_t)cmd_data->fileid;
            APDUFileID_t dfile_id = (APDUFileID_t)cmd_data->dfid;
            SIMAccess_t simAccessType;
#ifdef CONFIG_BRCM_FUSE_RIL_CIB
            SIMAccess_t* rsp = (SIMAccess_t*)capi2_rsp->dataBuf;
            simAccessType = *rsp;
#else
            SIM_ACCESS_RESULT_t *rsp = (SIM_ACCESS_RESULT_t*)capi2_rsp->dataBuf;
            simAccessType = rsp->result;
#endif
            if (SIMACCESS_SUCCESS != simAccessType)
            {
                KrilSimIOResponse_t *simioresult = pdata->bcm_ril_rsp;
                
                KRIL_DEBUG(DBG_ERROR,"simAccessType:%d PIN2 Error!!!\n", simAccessType);
                simioresult->result = BCM_E_PASSWORD_INCORRECT;
                pdata->handler_state = BCM_FinishCAPI2Cmd;
                return;
            }
            
            CAPI2_SimApi_SubmitRestrictedAccessReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID,
                  (APDUCmd_t)cmd_data->command, efile_id, dfile_id, (UInt8)cmd_data->p1, (UInt8)cmd_data->p2,
                  (UInt8)cmd_data->p3, cmd_data->pathlen, cmd_data->path, (UInt8*)cmd_data->data);
            
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }

        
#ifdef UPDATE_FDN_USE_CAPI2_PBK_API
        case BCM_SIM_PBK_UpdateFDNRecord:
        {    
            SIMAccess_t simAccessType;
#ifdef CONFIG_BRCM_FUSE_RIL_CIB
            SIMAccess_t* rsp = (SIMAccess_t*)capi2_rsp->dataBuf;
            simAccessType = *rsp;
#else
            SIM_ACCESS_RESULT_t *rsp = (SIM_ACCESS_RESULT_t*)capi2_rsp->dataBuf;
            simAccessType = rsp->result;
#endif
            if (SIMACCESS_SUCCESS != simAccessType)
            {
                KrilSimIOResponse_t *simioresult = pdata->bcm_ril_rsp;
                
                KRIL_DEBUG(DBG_ERROR,"simAccessType:%d PIN2 Error!!!\n", simAccessType);
                simioresult->result = BCM_E_PASSWORD_INCORRECT;
                pdata->handler_state = BCM_FinishCAPI2Cmd;
                return;
            }   
            
            if (0 == sFDNrecordlength[pdata->ril_cmd->SimId])
            {
                KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
                APDUFileID_t efile_id = (APDUFileID_t)cmd_data->fileid;
                APDUFileID_t dfile_id = (APDUFileID_t)cmd_data->dfid;

                // If don't have the FDN record length, query FDN information.
                CAPI2_SimApi_SubmitRestrictedAccessReq(InitClientInfo(pdata->ril_cmd->SimId), USIM_BASIC_CHANNEL_SOCKET_ID,
                      APDUCMD_GET_RESPONSE, efile_id, dfile_id, 0, 0, 15, cmd_data->pathlen, cmd_data->path, (UInt8*)NULL);
                                      
                pdata->handler_state = BCM_SIM_GetFDNInfo;
                return;
            }
            
            UpdateFDNRecordRequest(pdata);
            break;
        }
        
        case BCM_SIM_GetFDNInfo:
        {
            ParseFDNInfo(pdata, capi2_rsp);
            
            if (0 == sFDNrecordlength[pdata->ril_cmd->SimId])
            {
                SIM_RESTRICTED_ACCESS_DATA_t *rsp = (SIM_RESTRICTED_ACCESS_DATA_t*)capi2_rsp->dataBuf;
                KrilSimIOResponse_t *simioresult = pdata->bcm_ril_rsp;
                
                KRIL_DEBUG(DBG_ERROR,"FDN record length is 0. Failed!!!\n");
                
                if (rsp->data_len > 0)
                {
                    RawDataPrintfun(rsp->data, rsp->data_len, "SIM IO(FDN) RSP");
                }
                
                simioresult->result = BCM_E_GENERIC_FAILURE;
                pdata->handler_state = BCM_FinishCAPI2Cmd;
                return;
            }
            
            UpdateFDNRecordRequest(pdata);
            break;
        }

        case BCM_SIM_UpdateFDNRsult:
        {
            ParseWriteFDNResult(pdata, capi2_rsp);
            break;
        }
#endif //UPDATE_FDN_USE_CAPI2_PBK_API

        case BCM_RESPCAPI2Cmd:
        {
            ParseSimRestrictedAccessData(pdata, capi2_rsp);
            break;
        }
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;        
    }
}

#else //ANDROID_SIMIO_PATH_AVAILABLE

void KRIL_SimIOHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_ERROR,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
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
            KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
            SIM_APPL_TYPE_t simAppType;
            KrilSimDedFilePath_t path_info;
            APDUFileID_t efile_id = APDUFILEID_INVALID_FILE;
            APDUFileID_t dfile_id = APDUFILEID_INVALID_FILE;
            KrilSimIOResponse_t *simioresult;        
            
            KRIL_DEBUG(DBG_ERROR,"SIM IO: command:%d fileid:0x%X DFid:0x%X p1:%d p2:%d p3:%d pin2:%s\n",
                cmd_data->command, cmd_data->fileid, cmd_data->dfid, cmd_data->p1, cmd_data->p2, cmd_data->p3,
                cmd_data->pin2);            
            RawDataPrintfun((UInt8*)cmd_data->path, cmd_data->pathlen*2, "SIM IO: PATH");
                        
            pdata->bcm_ril_rsp = kmalloc(sizeof(KrilSimIOResponse_t), GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            simioresult = pdata->bcm_ril_rsp;
            memset(simioresult, 0, sizeof(KrilSimIOResponse_t));
            pdata->rsp_len = sizeof(KrilSimIOResponse_t);
            simioresult->command = cmd_data->command;
            simioresult->fileid = cmd_data->fileid;
            simioresult->searchcount = 0;
                    
            simAppType = KRIL_GetSimAppType(pdata->ril_cmd->SimId);
            
            if (SIM_APPL_INVALID == simAppType)
            {
                KRIL_DEBUG(DBG_ERROR,"SIM APP TYPE is Invalid Error!!\n");
                simioresult->result = BCM_E_GENERIC_FAILURE;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            efile_id = (APDUFileID_t)cmd_data->fileid;
            
            if (APDUFILEID_EF_ADN == efile_id && SIM_APPL_3G == simAppType)
            {
                efile_id = APDUFILEID_EF_PBK_ADN;
            }

            if (SIM_APPL_3G == simAppType && !strcmp(cmd_data->path, "3F007F105F3A"))
            {
                /* Special handling for 3G phonebook related EF whose ID is decided by EF_PBR (i.e. not fixed) 
                 * Trust Android and let CP catch error if any 
                 */
                dfile_id = APDUFILEID_DF_PHONEBK;
            }       
            else if (FALSE == SimIOGetDedicatedFileId(efile_id, simAppType, &dfile_id))
            {
                KRIL_DEBUG(DBG_ERROR,"SIM IO: EF ID:0x%X Get DF ID failed!!\n",efile_id);
                simioresult->result = BCM_E_GENERIC_FAILURE;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if (APDUFILEID_DF_PHONEBK == dfile_id && SIM_APPL_3G == simAppType)
            {
                //Need to check if support the local phonebook
                CAPI2_SimApi_GetServiceStatus(InitClientInfo(pdata->ril_cmd->SimId), SIMSERVICE_LOCAL_PHONEBK);
                pdata->handler_state = BCM_SIM_GetServiceStatus;
                break;
            }
            else
            {
                //Get Dedicated file path inform
                if (FALSE == SimIOGetDedicatedFilePath(dfile_id, SIMSERVICESTATUS_NOT_ALLOCATED1, &path_info))
                {
                    KRIL_DEBUG(DBG_ERROR,"DF ID:0x%X Get DF PATH failed!!\n",dfile_id);
                    simioresult->result = BCM_E_GENERIC_FAILURE;
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
                }
                
                KRIL_DEBUG(DBG_ERROR,"DF ID:0x%X path_len:%d\n", dfile_id, path_info.path_len);
                
                //Start running CAPI2_SimApi_SubmitRestrictedAccessReq
                SimIOSubmitRestrictedAccess(pdata, efile_id, dfile_id, &path_info);
                pdata->handler_state = BCM_RESPCAPI2Cmd;
            }            
            break;
        }
        
        case BCM_SIM_GetServiceStatus:
        {
            SIMServiceStatus_t localPBKStatus = SIMSERVICESTATUS_NOT_ALLOCATED1;
            KrilSimIOCmd_t *cmd_data = (KrilSimIOCmd_t*)pdata->ril_cmd->data;
            KrilSimDedFilePath_t path_info;
            APDUFileID_t efile_id,dfile_id;
            KrilSimIOResponse_t *simioresult;
            SIM_APPL_TYPE_t simAppType;
            
            simioresult = pdata->bcm_ril_rsp;
            
            if (FALSE == ParseSimServiceStatusResult(capi2_rsp, &localPBKStatus))
            {
                simioresult->result = BCM_E_GENERIC_FAILURE;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            efile_id = (APDUFileID_t)cmd_data->fileid;
            simAppType = KRIL_GetSimAppType(pdata->ril_cmd->SimId);
            
            if (APDUFILEID_EF_ADN == efile_id && SIM_APPL_3G == simAppType)
            {
                efile_id = APDUFILEID_EF_PBK_ADN;
            }
            
            dfile_id = APDUFILEID_DF_PHONEBK;
            
            //Get Dedicated file path inform
            if (FALSE == SimIOGetDedicatedFilePath(dfile_id, localPBKStatus, &path_info))
            {
                KRIL_DEBUG(DBG_ERROR,"DF ID:0x%X Get DF PATH failed!!\n",dfile_id);
                simioresult->result = BCM_E_GENERIC_FAILURE;
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            KRIL_DEBUG(DBG_ERROR,"DF ID:0x%X path_len:%d\n", dfile_id, path_info.path_len);

            //Start running CAPI2_SimApi_SubmitRestrictedAccessReq
            SimIOSubmitRestrictedAccess(pdata, efile_id, dfile_id, &path_info);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }
        
        case BCM_RESPCAPI2Cmd:
        {
            ParseSimRestrictedAccessData(pdata, capi2_rsp);
            break;
        }
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;
    }
}
#endif //ANDROID_SIMIO_PATH_AVAILABLE


void KRIL_QuerySimPinRemainingHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;

    KRIL_DEBUG(DBG_ERROR,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    
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
            CAPI2_SimApi_SendRemainingPinAttemptReq(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }

        case BCM_RESPCAPI2Cmd:
        {
            UInt8 *resp = NULL;
            pdata->bcm_ril_rsp = kmalloc(sizeof(UInt8)*6, GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            pdata->rsp_len = sizeof(UInt8)*9 ;
            
            resp = (UInt8*)pdata->bcm_ril_rsp;
            resp[0] = (UInt8)'B';
            resp[1] = (UInt8)'R';
            resp[2] = (UInt8)'C';
            resp[3] = (UInt8)'M';
            resp[4] = (UInt8)BRIL_HOOK_QUERY_SIM_PIN_REMAINING;
            
            if (!ParseRemainingPinAttempt(pdata, capi2_rsp))
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;        
    }
}

//BCM_EAP_SIM 
#ifdef BCM_RIL_FOR_EAP_SIM 
void KRIL_GenericSimAccessHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;
    KRIL_DEBUG(DBG_INFO,"pdata->handler_state:0x%lX\n", pdata->handler_state);
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
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
            //Bytes 0~3="BRCM";Byte 4:Message ID;Byte 5~6:UInt16 APDU length;Byte 7~ : APDU data
            UInt8 *cmd_data = (UInt8*)pdata->ril_cmd->data;
            UInt16 cmd_lenth = (((UInt16)cmd_data[5])|((UInt16)cmd_data[6])<<8);
            CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),&cmd_data[7], cmd_lenth);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }
        case BCM_RESPCAPI2Cmd:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
            UInt8 *resp = NULL;
            UInt32 rsp_len = 0;
       
            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            //APDU data length + 2bytes for APDU response data length info 
            //+"BRCM"(4 bytes)+ Messgae ID(1 byte)
            rsp_len = rsp->len + sizeof(UInt16)+ BCM_OEM_HOOK_HEADER_LENGTH;

            if(rsp->resultCode != SIM_GENERIC_APDU_RES_SUCCESS) 
            {
                KRIL_DEBUG(DBG_ERROR,"Do Generic Sim Access is not success!! result:%d\n", rsp->resultCode);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
       
            pdata->bcm_ril_rsp = kmalloc(rsp_len, GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
       
            pdata->rsp_len = rsp_len;
            //Bytes 0~3="BRCM";Byte 4:Message ID;Byte 5~6:UInt16 APDU length;Byte 7~ : APDU data
            resp = (UInt8*)pdata->bcm_ril_rsp;
            resp[0] = (UInt8)'B';
            resp[1] = (UInt8)'R';
            resp[2] = (UInt8)'C';
            resp[3] = (UInt8)'M';
            resp[4] = (UInt8)BRIL_HOOK_GENERIC_SIM_ACCESS;
            resp[5] = (UInt8)(rsp->len & 0x00FF);      //Low Byte
            resp[6] = (UInt8)((rsp->len & 0xFF00)>>8); //High Byte      
            memcpy(&resp[7], rsp->data,rsp->len );
            //RawDataPrintfun(&resp[7], rsp->len, "Sim Access:Rsp ");
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }
            
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;      
    }
}
//EAP-SIM
void KRIL_GsmSimAuthenticationHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;
    KRIL_DEBUG(DBG_INFO,"pdata->handler_state:0x%lX\n", pdata->handler_state);

    KRIL_DEBUG(DBG_ERROR,"pdata->handler_state:0x%lX\n", pdata->handler_state);  	// TY_del	

    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
	
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
            CAPI2_SimApi_GetPinStatus(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_SIM_SelectMF;
        }
        break;
        case BCM_SIM_SelectMF:
        {
            SIM_PIN_Status_t SimStatus;	
            SIM_PIN_Status_t* rsp = (SIM_PIN_Status_t*)capi2_rsp->dataBuf;
            SIM_APPL_TYPE_t SimType = SIM_APPL_INVALID;

	    KRIL_DEBUG(DBG_ERROR,"BCM_SIM_SelectMF \n"); 			// TY_del

            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

	    SimStatus = *rsp;
	    KRIL_DEBUG(DBG_ERROR,"Sim pin state = %d\n",SimStatus); 			// TY_del
		 
            if(SimStatus != PIN_READY_STATUS)
            {
                KRIL_DEBUG(DBG_INFO,"Sim pin state = %d\n",SimStatus);
                switch(SimStatus)
                {
                    case SIM_ABSENT_STATUS:
                        pdata->result = BCM_E_SIM_ABSENT;
                    break;	
                    default:
                        pdata->result = BCM_E_GENERIC_FAILURE;
                    break;	
                }		      
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            //check sim type is USIM or SIM.
            SimType = KRIL_GetSimAppType(pdata->ril_cmd->SimId);
            if(SimType == SIM_APPL_3G)
            {//For Usim MF select
                //SELECT MF USIM "00a4000C023f00"
                UInt8 SELECT_MF_USIM_APDU[]={0x00,0xa4,0x00,0x0C,0x02,0x3f,0x00};
                CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),SELECT_MF_USIM_APDU,sizeof(SELECT_MF_USIM_APDU));
            }
            else if(SimType == SIM_APPL_2G)
            {//For sim MF select
                //SELECT SIM Master File "A0A40000023F00"
                UInt8 SELECT_MF_SIM_APDU[] ={0xa0,0xa4,0x00,0x00,0x02,0x3f,0x00};			
            CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),SELECT_MF_SIM_APDU,sizeof(SELECT_MF_SIM_APDU));
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR,"SimType is INVALID!!SimType(%d)\n",SimType);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }            
            pdata->handler_state = BCM_SIM_SelectDedicatedFile;
        }
        break;
        case BCM_SIM_SelectDedicatedFile:
        {
             SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
             SIM_APPL_TYPE_t SimType = SIM_APPL_INVALID;

             if (!rsp)
             {
                 KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                 pdata->handler_state = BCM_ErrorCAPI2Cmd;
                 return;
             }
             if(rsp->resultCode == SIM_GENERIC_APDU_RES_SUCCESS)
             {
                 if (SimIODetermineResponseError(rsp->data[0], rsp->data[1]) == FALSE)
                 {
                     pdata->handler_state = BCM_ErrorCAPI2Cmd;
                     KRIL_DEBUG(DBG_ERROR,"Select Master File RSP Error!!len=%d,SW1=0x%x SW2=0x%x\n",rsp->len,rsp->data[0],rsp->data[1]);
                     return;				
                 }
             }
             else
             {
                 pdata->handler_state = BCM_ErrorCAPI2Cmd;
                 KRIL_DEBUG(DBG_ERROR,"Select Master File Error!!resultCode=%d\n",rsp->resultCode);
                 return;		
             }			  

             //check sim type is USIM or SIM.
             SimType = KRIL_GetSimAppType(pdata->ril_cmd->SimId);
             if(SimType == SIM_APPL_3G)
             {//For Usim ADF select
                 //SELECT DF ADF "00a4000C027FFF"
                 UInt8 SELECT_DF_ADF_APDU[] ={0x00,0xa4,0x00,0x0C,0x02,0x7F,0xFF};
                 CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),SELECT_DF_ADF_APDU, sizeof(SELECT_DF_ADF_APDU));			 
             }
             else if(SimType == SIM_APPL_2G)
             {//For sim DF_GSM select
                 //SELECT DF GSM	"A0A40000027F20"
                 UInt8 SELECT_DF_GSM_APDU[] ={0xa0,0xa4,0x00,0x00,0x02,0x7f,0x20};		 
             CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),SELECT_DF_GSM_APDU, sizeof(SELECT_DF_GSM_APDU));
             }
             else
             {
                 KRIL_DEBUG(DBG_ERROR,"SimType is INVALID!!!!SimType(%d)\n",SimType);
                 pdata->handler_state = BCM_ErrorCAPI2Cmd;
                 return;
             }             
             pdata->handler_state = BCM_SIM_DoGSMAuthentication;
        }
        break;
        case BCM_SIM_DoGSMAuthentication:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
            //cmd_data Bytes 0~3="BRCM";Byte 4:Message type;Byte 5:Rand length;Byte 6~ : Rand data
            UInt8 *cmd_data = (UInt8*)pdata->ril_cmd->data;
            SIM_APPL_TYPE_t SimType = SIM_APPL_INVALID;
            UInt8 Rand_lenth = cmd_data[5];
            UInt8  APDU[MAXBUFF] = {0};
            UInt16 APDUlen = 0;
            UInt8 Headerlen = 0;  

            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            if(rsp->resultCode == SIM_GENERIC_APDU_RES_SUCCESS)
            {
                if (SimIODetermineResponseError(rsp->data[0], rsp->data[1]) == FALSE)
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    KRIL_DEBUG(DBG_ERROR,"Select DF_Gsm RSP Error!!len=%d,SW1=0x%x SW2=0x%x\n",rsp->len,rsp->data[0],rsp->data[1]);
                    return;				
                }
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                KRIL_DEBUG(DBG_ERROR,"Select DF Error!!resultCode=%d\n",rsp->resultCode);
                return;		
            }						
            //RawDataPrintfun(cmd_data, pdata->ril_cmd->datalen, "GSM Ath cmd form uril");
            //check sim type is USIM or SIM.
            SimType = KRIL_GetSimAppType(pdata->ril_cmd->SimId);
            if(SimType == SIM_APPL_3G)
            {
                //RUN 2G AUTHENTICATE  "008800801110"
                UInt8 RUN_2G_AUTH_HEADER[] ={0x00,0x88,0x00,0x80,0x11,0x10};	   
                Headerlen = sizeof(RUN_2G_AUTH_HEADER);

                memcpy(&APDU[0],RUN_2G_AUTH_HEADER,Headerlen);		   
                //rand data
                memcpy(&APDU[Headerlen],&cmd_data[6] ,Rand_lenth);
                //le
                APDU[Headerlen + Rand_lenth]=0x00;			   
                //6 Bytes for 008800801110 + 1 byte for le
                APDUlen = Headerlen + Rand_lenth + 1;
                if(APDUlen > MAXBUFF)
                {
                    KRIL_DEBUG(DBG_ERROR,"The Data buffer length is over Maxbuff! size:%d\n", APDUlen);
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;					
                }
            }
            else if(SimType == SIM_APPL_2G)
            {			
                //RUN GSM APDU header "A088000010"
                UInt8 RUN_GSM_AUTH_HEADER[]={0xA0,0x88,0x00,0x00,0x10}; 
                Headerlen = sizeof(RUN_GSM_AUTH_HEADER);
    
            memcpy(&APDU[0],RUN_GSM_AUTH_HEADER,Headerlen);
            APDUlen = Rand_lenth + Headerlen;
            if(APDUlen > MAXBUFF)
            {
                KRIL_DEBUG(DBG_ERROR,"The Data buffer length is over Maxbuff! size:%d\n", APDUlen);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            memcpy(&APDU[Headerlen] ,&cmd_data[6],Rand_lenth);
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR,"SimType is INVALID!!!!SimType(%d)\n",SimType);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }	
            //RawDataPrintfun(APDU, APDUlen, "GSM Ath CMD to CP");			
            CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),APDU, APDUlen);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }
        case BCM_RESPCAPI2Cmd:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
            UInt8 *resp = NULL;
            UInt32 rsp_len;
			
            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if(rsp->len==2)
            {
                switch (rsp->data[0]) 
                {
                    case 0x61:
                    case 0x9f:
                    {
                        UInt8  APDU[MAXBUFF];
                        UInt16 APDUlen = 0;	 
                        if (rsp->data[1]==0) 
                        {
                            KRIL_DEBUG(DBG_ERROR,"Exception:SW2 is 0 while SW1 is 0x%x!!Warning\n",rsp->data[0]);
                            break;
                        }
                        APDU[0]=0xa0;//CLA
                        APDU[1]=0xc0;//INS
                        APDU[2]=0x00;//P1
                        APDU[3]=0x00;//P2

                        if(KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_3G)
                        {
                            APDU[0] = 0x00; // USIM CLA
                        }
				
                        APDU[4] = rsp->data[1];
                        APDUlen = 5;
                        CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),APDU, APDUlen);
                        pdata->handler_state = BCM_SIM_GetAdditionDataReq;
					
                        return;
                    }
                    break; 
                    default:
                        KRIL_DEBUG(DBG_ERROR,"Exception:SW1 is 0x%x,SW2 is 0x%x !! Warning\n",rsp->data[0],rsp->data[1]);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        return;
                    break;
                }
            }
			        
            rsp_len = sizeof(SIM_GENERIC_APDU_XFER_RSP_t) + BCM_OEM_HOOK_HEADER_LENGTH;	

            pdata->bcm_ril_rsp = kmalloc(rsp_len, GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
			
            pdata->rsp_len = rsp_len;
            //Bytes 0~3="BRCM";Byte4:Message type;Byte5:Result;Byte6:Response length;Byte7~:Rsp data
            resp = (UInt8*)pdata->bcm_ril_rsp;
            resp[0] = (UInt8)'B';
            resp[1] = (UInt8)'R';
            resp[2] = (UInt8)'C';
            resp[3] = (UInt8)'M';
            resp[4] = (UInt8)BRIL_HOOK_EAP_SIM_AUTHENTICATION;

            memcpy(&resp[5], rsp,sizeof(SIM_GENERIC_APDU_XFER_RSP_t));

            //RawDataPrintfun(&resp[5], rsp->len, "GSM CP RSP");
                
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
		    }
        case BCM_SIM_GetAdditionDataReq:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
            UInt8 *resp = NULL;
            UInt32 rsp_len;

            //APDU data length + 2bytes for APDU response data length info 
            //+"BRCM"(4 bytes)+ MessgaeType(1 byte)
            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"GetAddition:capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            if(rsp->len == 2)
            {
                KRIL_DEBUG(DBG_ERROR,"Exception:Addition SW1 is 0x%x,SW2 is 0x%x !! Warning\n",rsp->data[0],rsp->data[1]);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            rsp_len = sizeof(SIM_GENERIC_APDU_XFER_RSP_t)+ BCM_OEM_HOOK_HEADER_LENGTH; 

            pdata->bcm_ril_rsp = kmalloc(rsp_len, GFP_KERNEL);
            if (!pdata->bcm_ril_rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"GetAddition:Allocate bcm_ril_rsp memory failed!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            pdata->rsp_len = rsp_len;
            //Bytes 0~3="BRCM";Byte4:Message type;Byte5:Result;Byte6:Response length;Byte7~:Rsp data
            resp = (UInt8*)pdata->bcm_ril_rsp;
            resp[0] = (UInt8)'B';
            resp[1] = (UInt8)'R';
            resp[2] = (UInt8)'C';
            resp[3] = (UInt8)'M';
            resp[4] = (UInt8)BRIL_HOOK_EAP_SIM_AUTHENTICATION;

            memcpy(&resp[5], rsp, sizeof(SIM_GENERIC_APDU_XFER_RSP_t));

            //RawDataPrintfun(&resp[5], rsp->len, "GSM Addition CP RSP");
			
            pdata->handler_state = BCM_FinishCAPI2Cmd;
            break;
        }		
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;		  
    }    
}

void KRIL_USimAuthenticationHandler(void *ril_cmd, Kril_CAPI2Info_t *capi2_rsp)
{
    KRIL_CmdList_t *pdata = (KRIL_CmdList_t*)ril_cmd;
    KRIL_DEBUG(DBG_INFO,"pdata->handler_state:0x%lX\n", pdata->handler_state);
	
    if((BCM_SendCAPI2Cmd != pdata->handler_state)&&(NULL == capi2_rsp))
    {
        KRIL_DEBUG(DBG_ERROR,"capi2_rsp is NULL\n");
        pdata->handler_state = BCM_ErrorCAPI2Cmd;
        return;
    }
		
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
            CAPI2_SimApi_GetPinStatus(InitClientInfo(pdata->ril_cmd->SimId));
            pdata->handler_state = BCM_SIM_SelectMFUsim;
        }
        break;
        case BCM_SIM_SelectMFUsim:
        {            
            SIM_PIN_Status_t SimStatus;	
            SIM_PIN_Status_t* rsp = (SIM_PIN_Status_t*)capi2_rsp->dataBuf;
            //SELECT MF USIM "00a4000C023f00"
            UInt8 SELECT_MF_USIM_APDU[]={0x00,0xa4,0x00,0x0C,0x02,0x3f,0x00};
            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            SimStatus = *rsp;
 
            if(SimStatus != PIN_READY_STATUS)
            {
                KRIL_DEBUG(DBG_INFO,"USim Pin Status =%d \n",SimStatus);
                switch(SimStatus)
                {
                    case SIM_ABSENT_STATUS:
                        pdata->result = BCM_E_SIM_ABSENT;
                    break;	
                    default:
                        pdata->result = BCM_E_GENERIC_FAILURE;
                    break;	
                }		      
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }		   
            CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),SELECT_MF_USIM_APDU, sizeof(SELECT_MF_USIM_APDU));
            pdata->handler_state = BCM_SIM_SelectDedicatedFileOfADF;
        }
        break;
        case BCM_SIM_SelectDedicatedFileOfADF:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
            //SELECT DF ADF "00a4000C027FFF"
            UInt8 SELECT_DF_ADF_APDU[] ={0x00,0xa4,0x00,0x0C,0x02,0x7F,0xFF};
            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            if(rsp->resultCode == SIM_GENERIC_APDU_RES_SUCCESS)
            {
                if (SimIODetermineResponseError(rsp->data[0], rsp->data[1]) == FALSE)
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    KRIL_DEBUG(DBG_ERROR,"Select Usim Master File RSP Error!!len=%d,SW1=0x%x SW2=0x%x\n",rsp->len,rsp->data[0],rsp->data[1]);
                    return;
                }
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                KRIL_DEBUG(DBG_ERROR,"Select Usim Master File Error!!resultCode=%d\n",rsp->resultCode);
                return;
            }			

             CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),SELECT_DF_ADF_APDU,sizeof(SELECT_DF_ADF_APDU));
             pdata->handler_state = BCM_SIM_DoUSimAuthentication;
        }
        break;
        case BCM_SIM_DoUSimAuthentication:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;					
            //OEM hook cmd_data Bytes 0~3="BRCM";Byte4:Message type;Byte5:auth_cnt
            //6:Rand length 7:autn length
            //8~(8+Rand length-1)                          : Rand data
            //(8+Rand length)~(8+Rand length+autn_length-1): autn data
            UInt8  *cmd_data = (UInt8*)pdata->ril_cmd->data;
            UInt8  auth_cnt    = cmd_data[5];
            UInt8  Rand_length = cmd_data[6];
            UInt8  Autn_length = cmd_data[7];
            UInt8  APDU[MAXBUFF] = {0};
            UInt16 APDUlen = 0;
            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            if(rsp->resultCode == SIM_GENERIC_APDU_RES_SUCCESS)
            {   //Success :SW1=0x90 SW2=0x00  
                if (SimIODetermineResponseError(rsp->data[0], rsp->data[1]) == FALSE)
                {
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    KRIL_DEBUG(DBG_ERROR,"Select DF ADF RSP Error!!len=%d,SW1=0x%x SW2=0x%x\n",rsp->len,rsp->data[0],rsp->data[1]);
                    return;
                }
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                KRIL_DEBUG(DBG_ERROR,"Select DF ADF Error!!resultCode=%d\n",rsp->resultCode);
                return;
            }		 
            //RawDataPrintfun(cmd_data, pdata->ril_cmd->datalen, "Usim Ath cmd form Uril");			
            if(auth_cnt == 0)
            {
                //RUN 2G AUTHENTICATE  "008800801110"
                UInt8 RUN_2G_AUTH_HEADER[] ={0x00,0x88,0x00,0x80,0x11,0x10};	   
                UInt8 Headerlen = sizeof(RUN_2G_AUTH_HEADER);

                memcpy(&APDU[0],RUN_2G_AUTH_HEADER,Headerlen);		   
                //rand data
                memcpy(&APDU[Headerlen],&cmd_data[8] ,Rand_length);
                //le
                APDU[Headerlen + Rand_length]=0x00;			   
                //6 Bytes for 008800801110 + 1 byte for le
                APDUlen = Headerlen + Rand_length + 1;
            }
            else if((Rand_length > 0) && (Autn_length > 0))
            {
                UInt8 total_len = Rand_length + Autn_length + 2; // 1 byte for rand len + 1 byte for autn len
                //RUN USIM AUTHENTICATE "00880081"	
                UInt8 RUN_AUTH_HEADER[]	 ={0x00,0x88,0x00,0x81};							 
                UInt8 Headerlen=sizeof(RUN_AUTH_HEADER);
                memcpy(&APDU[0],RUN_AUTH_HEADER,Headerlen);
                //Lc  
                APDU[Headerlen] = total_len;
                //rand length
                APDU[Headerlen+1] = Rand_length;
                //Rand data 
                memcpy(&APDU[Headerlen+2],&cmd_data[8],Rand_length);
                //auth length
                APDU[Headerlen+2+Rand_length] = Autn_length;
                //Auth data
                memcpy(&APDU[Headerlen+2 + Rand_length + 1],&cmd_data[8 + Rand_length],Autn_length);
                //Le
                APDU[Headerlen+2+Rand_length+1+Autn_length] = 0x00;
                //4 bytes for 00880081 + 1 byte for total len + 1 byte rand len + 1 byte auth len +1 byte for le                           
                APDUlen = Headerlen + 1 + 1 + Rand_length + 1 + Autn_length + 1;
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR,"command failed:auth_cnt = %d; Rand_length = %d;Autn_length = %d\n"
                                     ,auth_cnt,Rand_length,Autn_length);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }
            //RawDataPrintfun(APDU, APDUlen, "Usim Ath cmd to CP");
            CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),APDU, APDUlen);
            pdata->handler_state = BCM_RESPCAPI2Cmd;
            break;
        }
        case BCM_RESPCAPI2Cmd:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
            UInt8 *cmd_data = (UInt8*)pdata->ril_cmd->data;
            UInt8 *resp = NULL;
            UInt32 rsp_len  = 0; 
            UInt8  auth_cnt = cmd_data[5];

            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if(rsp->len==2)
            {
                switch (rsp->data[0]) 
                {
                    case 0x61:
                    case 0x9f:
                    {
                        UInt8  APDU[MAXBUFF];
                        UInt16 APDUlen = 0;	 
                        if (rsp->data[1]==0) 
                        {
                            break;
                        }
                        APDU[0]=0xa0;//CLA
                        APDU[1]=0xc0;//INS
                        APDU[2]=0x00;//P1
                        APDU[3]=0x00;//P2

                        if(KRIL_GetSimAppType(pdata->ril_cmd->SimId) == SIM_APPL_3G)
                        {
                            APDU[0] = 0x00; // USIM CLA
                        }
				
                        APDU[4] = rsp->data[1];
                        APDUlen = 5;					
                        CAPI2_SimApi_SendGenericApduCmd(InitClientInfo(pdata->ril_cmd->SimId),APDU, APDUlen);
                        pdata->handler_state = BCM_SIM_GetAdditionDataReq;					
                        return;
                    }
                    default:
                        KRIL_DEBUG(DBG_ERROR,"Exception:SW1 is 0x%x,SW2 is 0x%x !! Warning\n",rsp->data[0],rsp->data[1]);
                        pdata->handler_state = BCM_ErrorCAPI2Cmd;
                        return;
                    break;					
                }
            }
			        
            if(((auth_cnt == 0) && (rsp->len == 16)) || (auth_cnt != 0))
            {			
                rsp_len = sizeof(SIM_GENERIC_APDU_XFER_RSP_t)+ BCM_OEM_HOOK_HEADER_LENGTH;	
			
                pdata->bcm_ril_rsp = kmalloc(rsp_len, GFP_KERNEL);
                if (!pdata->bcm_ril_rsp)
                {
                    KRIL_DEBUG(DBG_ERROR,"Allocate bcm_ril_rsp memory failed!!\n");
                    pdata->handler_state = BCM_ErrorCAPI2Cmd;
                    return;
                }
			
                pdata->rsp_len = rsp_len;
                //Bytes 0~3="BRCM";Byte4:Message type;Byte5:Resultcode;Byte6:Response data length;Byte7~ : Response data
                resp = (UInt8*)pdata->bcm_ril_rsp;
                resp[0] = (UInt8)'B';
                resp[1] = (UInt8)'R';
                resp[2] = (UInt8)'C';
                resp[3] = (UInt8)'M';
                resp[4] = (UInt8)BRIL_HOOK_EAP_AKA_AUTHENTICATION;
                memcpy(&resp[5], rsp, sizeof(SIM_GENERIC_APDU_XFER_RSP_t));
                //RawDataPrintfun(&resp[5], rsp->len, "USIM CP RSP");
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                KRIL_DEBUG(DBG_ERROR,"Response data Error!! auth_cnt = %d,Rsp length=%d\n",auth_cnt,rsp->len);
            }
	    break;
        }
        case BCM_SIM_GetAdditionDataReq:
        {
            SIM_GENERIC_APDU_XFER_RSP_t *rsp = (SIM_GENERIC_APDU_XFER_RSP_t*)capi2_rsp->dataBuf;
            UInt8 *cmd_data = (UInt8*)pdata->ril_cmd->data;
            UInt8 *resp = NULL;
            //APDU data length + 2bytes for APDU response data length info 
            //+"BRCM"(4 bytes)+ MessgaeType(1 byte)
            UInt32 rsp_len  = 0; 
            UInt8  auth_cnt = cmd_data[5];

            if (!rsp)
            {
                KRIL_DEBUG(DBG_ERROR,"GetAddition:capi2_rsp->dataBuf is NULL, Error!!\n");
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }

            if(rsp->len == 2)
            {
                KRIL_DEBUG(DBG_ERROR,"Exception:Addition SW1 is 0x%x,SW2 is 0x%x !! Warning\n",rsp->data[0],rsp->data[1]);
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                return;
            }			
            if(((auth_cnt == 0) && (rsp->len == 16)) || (auth_cnt != 0))
            {			
                rsp_len = sizeof(SIM_GENERIC_APDU_XFER_RSP_t)+ BCM_OEM_HOOK_HEADER_LENGTH;	
                pdata->bcm_ril_rsp = kmalloc(rsp_len, GFP_KERNEL);
                if (!pdata->bcm_ril_rsp)
                {
                   KRIL_DEBUG(DBG_ERROR,"GetAddition:Allocate bcm_ril_rsp memory failed!!\n");
                   pdata->handler_state = BCM_ErrorCAPI2Cmd;
                   return;
                }
					
                pdata->rsp_len = rsp_len;
                //Bytes 0~3="BRCM";Byte4:Message type;Byte5:Resultcode;Byte6:Response data length;Byte7~ : Response data
                resp = (UInt8*)pdata->bcm_ril_rsp;
                resp[0] = (UInt8)'B';
                resp[1] = (UInt8)'R';
                resp[2] = (UInt8)'C';
                resp[3] = (UInt8)'M';
                resp[4] = (UInt8)BRIL_HOOK_EAP_AKA_AUTHENTICATION;
                memcpy(&resp[5], rsp, sizeof(SIM_GENERIC_APDU_XFER_RSP_t));				
                //RawDataPrintfun(&resp[5], rsp->len, "USIM Addition CP RSP");			
                pdata->handler_state = BCM_FinishCAPI2Cmd;
            }
            else
            {
                pdata->handler_state = BCM_ErrorCAPI2Cmd;
                KRIL_DEBUG(DBG_ERROR,"GetAddition:Response data Error!! auth_cnt = %d,Rsp length=%d\n",auth_cnt,rsp->len);
            }
            break;
        }    
        default:
            KRIL_DEBUG(DBG_ERROR,"Error handler_state:0x%lX\n", pdata->handler_state);
            pdata->handler_state = BCM_ErrorCAPI2Cmd;
            break;		  
    }
}
#endif
