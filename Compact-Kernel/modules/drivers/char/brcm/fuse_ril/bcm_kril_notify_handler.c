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
#include "bcm_kril_cmd_handler.h"
#include "bcm_kril_ioctl.h"

#include "capi2_stk_ds.h"
#include "capi2_pch_msg.h"
#include "capi2_gen_msg.h"
#include "capi2_reqrep.h"
#include <linux/time.h>
#include <linux/rtc.h>

#ifdef CONFIG_CPU_FREQ_GOV_BCM21553
#include <mach/bcm21553_cpufreq_gov.h>
static struct cpufreq_client_desc* sRIL_dvfs_client = NULL;
static Boolean sHspaChnlAlloc = FALSE;
#endif

Boolean StkCall= FALSE;  // gearn setup call 
Boolean StkIcon= FALSE;  // gearn not support icon TR
Boolean Msisdnck= FALSE;  // solokou check MSISDN
Boolean Msisdnck_1= FALSE;  // solokou check MSISDN

#ifdef BRCM_AGPS_CONTROL_PLANE_ENABLE
#include "capi2_lcs_cplane_api.h"
#endif

// For Network
static MSRegState_t  sCreg_state[DUAL_SIM_SIZE] = {REG_STATE_NO_SERVICE, REG_STATE_NO_SERVICE, REG_STATE_NO_SERVICE};
static MSRegState_t  sCgreg_state[DUAL_SIM_SIZE] = {REG_STATE_NO_SERVICE, REG_STATE_NO_SERVICE, REG_STATE_NO_SERVICE};
MSRegInfo_t  gRegInfo[DUAL_SIM_SIZE];
MSUe3gStatusInd_t  gUE3GInfo[DUAL_SIM_SIZE];
extern int gdataprefer; // SIM1:0 SIM2:1

extern int gpowerOffcard;

// For STK
/* Macro to determine if the passed coding type is ALPHAID encoding */
#define IS_ALPHAID_CODING(code_type) ( ((code_type) == UNICODE_80) || ((code_type) == UNICODE_81) || ((code_type) == UNICODE_82))

#define MAX_SIMPLE_TLV_DATA_LENGTH  256
#define MAX_TLV_DATA_LENGTH         (MAX_SIMPLE_TLV_DATA_LENGTH + 3)
#define MAX_TLV_STRING_LENGTH       (MAX_TLV_DATA_LENGTH * 2 + 1)

#define	CODE_PAGE_MTI_AT_SIGN	0x80		///< Used to mark '@' sign for CodePageMTI
#define	DEF_GSM_AT_SIGN			0x00		///< Used to mark '@' sign for Default GSM

CallIndex_t  gUssdID[DUAL_SIM_SIZE] = {CALLINDEX_INVALID, CALLINDEX_INVALID, CALLINDEX_INVALID};;
CallIndex_t  gPreviousUssdID[DUAL_SIM_SIZE] = {CALLINDEX_INVALID, CALLINDEX_INVALID, CALLINDEX_INVALID};;

UInt32  gDialogID;

extern bcm_kril_dev_result_t bcm_dev_results[TOTAL_BCMDEVICE_NUM];

extern Boolean gIsFlightModeOnBoot;

extern Boolean gIsStkRefreshReset;
extern Boolean gIsStkRefreshResetSTK2;  // gearn STK2 SIM refresh reset

//[LTN_SW3_PROTOCOL] sj0212.park 2011.11.09 initial merge from Luisa
#if defined (CONFIG_LTN_COMMON)  //[latin_protocol] skh STK Issue
extern Boolean radio_on_setupmenu;
#endif

extern struct timezone sys_tz;

typedef enum
{
  KRIL_CHARACTER_SET_GSM_7_BIT_DEFAULT,     ///< GSM 7 bit default alphabet
  KRIL_CHARACTER_SET_8_BIT_DATA,          ///< 8 bit data
  KRIL_CHARACTER_SET_UCS2_16_BIT,         ///< UCS2 16 bit
  KRIL_CHARACTER_SET_RESERVED           ///< Reserved
} KRIL_CharacterSet_t;                ///< Character Set

/// Enum : Coding group : "Data Coding Scheme GSM 3.38, section 5"
typedef enum
{
  KRIL_CODING_GROUP_0_LANGUAGE_GSM_7_BIT_ALPHABET,      ///< German, English, Italian ....
  KRIL_CODING_GROUP_1_MESSAGE_PROTECTED_BY_LANGUAGE_IND,  ///< GSM 7 Bit Default
  KRIL_CODING_GROUP_2_LANGUAGE_GSM_7_BIT_ALPHABET,      ///< Czech, Hebrew, Arabic, ....
  KRIL_CODING_GROUP_3_LANGUAGE_GSM_7_BIT_ALPHABET,      ///< Languages using the GSM 7 bit default alphabet
  KRIL_CODING_GROUP_4_GENERAL_TEXT_UNCOMPRESSED = 0x04, ///< General, Text Uncompressed
  KRIL_CODING_GROUP_5_GENERAL_TEXT_UNCOMPRESSED,      ///< General, Text Uncompressed, have message class meaning
  KRIL_CODING_GROUP_6_GENERAL_TEXT_COMPRESSED,        ///< General, Text Compressed
  KRIL_CODING_GROUP_7_GENERAL_TEXT_COMPRESSED,        ///< General, Text Compressed, have message class meaning
  KRIL_CODING_GROUP_9_MSG_WITH_USER_DATA_HEADER = 0x09, ///< Message with User Data Header (UDH)
  KRIL_CODING_GROUP_E_WAP_RELATED = 0x0E,         ///< Refer to "Wireless Datagram Protocol Specification", Wireless Application Protocol Forum Ltd.
  KRIL_CODING_GROUP_F_DATA_CODING_MSG_HANDLING        ///< Data Coding/Message Handling
} KRIL_CodingGroup_t;                   ///< SS Data Coding Scheme Type

//Irine_22June_airplanemode
char satk_setup_menu_tlv_data_string[MAX_TLV_STRING_LENGTH];
UInt16 satk_setup_menu_tlv_length = 0;

char satk_setup_menu_tlv_data_string_STK2[MAX_TLV_STRING_LENGTH];    // gearn airplane mode control for SIMAT 
UInt16 satk_setup_menu_tlv_length_STK2 = 0;


#ifdef CONFIG_CPU_FREQ_GOV_BCM21553
//******************************************************************************
//
// Function Name: KRIL_InitDVFSHandler
//
// Description:   Register KRIL as client of kernel DVFS manager
//
// Notes:
//
//******************************************************************************
void KRIL_InitDVFSHandler( void )
{
	sRIL_dvfs_client = cpufreq_bcm_client_get("kril");
	if ( !sRIL_dvfs_client )
	{
        KRIL_DEBUG(DBG_ERROR,"cpufreq_bcm_client_get failed\n");
	}
	else
	{
        KRIL_DEBUG(DBG_INFO,"cpufreq_bcm_client_get OK sRIL_dvfs_client: 0x%x \n", (int)sRIL_dvfs_client );
	}

}

//******************************************************************************
//
// Function Name: KRIL_UnregDVFSHandler
//
// Description:   Unregister KRIL as client of kernel DVFS manager
//
// Notes:
//
//******************************************************************************
void KRIL_UnregDVFSHandler( void )
{
    KRIL_DEBUG(DBG_INFO,"unregistering as DVFS client\n" );
    if ( sRIL_dvfs_client )
    {
        cpufreq_bcm_client_put( sRIL_dvfs_client );
        sRIL_dvfs_client = NULL;
    }
}
#endif

//******************************************************************************
//
// Function Name: KRIL_BCD_ConvertString2PBCD
//
// Description:   This function converts an ASCII string to Packed BCD number.
//          Packed BCD numbers is defined as two BCD digits per byte.
//          Note: if the return size is an odd number, the extra
//          nibble will be filled with 0xF and the size will be rounded
//          up to the next even number.
//
// Notes:
//
//******************************************************************************

UInt16 KRIL_BCD_ConvertString2PBCD(   // returns the number of digits converted
  UInt8 *bcd,         // BCD value to store converted value
                  // (packed format, two BCD numbers per byte) 
  UInt8 *str          // ASCII string to convert (NULL terminated)
  )
{
    Boolean first_nibble;
    UInt8 bcd_digit;
    UInt16 bcd_size;
    Boolean is_skipped;
    
    first_nibble = TRUE;
    
    for (bcd_size = 0; *str != 0x00; str++)
    {
        is_skipped = FALSE;
        
        if ( *str >= '0' && *str <= '9' )
        {
            bcd_digit = *str - '0';
        }
        else
        {
            switch ( *str )
            {
                case '*':
                    bcd_digit = 10;
                    break;
                case '#':
                    bcd_digit = 11;
                    break;
                case 'p':
                case 'P':
                    bcd_digit = 12;
                    break;
                case '?':
                    bcd_digit = 13;
                    break;
                case 'b':
                    bcd_digit = 14;
                    break;
                // The INTERNATIONAL_CODE ('+'), VOICE_CALL_CODE (';'), 'w', and 'W' are supported
                // in BCD number, '+' is in the TON part of GSM telephone number
                // while 'w' and 'W' has no support in BCD
                case 0x80: //SYS_UNKNOWN_TON_UNKNOWN_NPI
                case 0x81://SYS_UNKNOWN_TON_ISDN_NPI
                case 0x91://SYS_INTERNA_TON_ISDN_NPI
                case '+'://SYS_INTERNATIONAL_CODE
                case ';'://SYS_VOICE_CALL_CODE
                case 'w':
                case 'W':
                    is_skipped = TRUE;
                    break;
                
                default:
                    // problems, can't convert to BCD format, not
                    // an ASCII representation of BCD number. 
                    // assert( FALSE );  ==> There's no need to assert, just skip the invalid digit.
                    is_skipped = TRUE;
                    break;
            }
        }
        
        if ( is_skipped == FALSE )
        {
            if ( first_nibble == TRUE )
            {
                *bcd = bcd_digit;
                first_nibble = FALSE;
            }
            else
            {
                *bcd |= (bcd_digit << 4);
                first_nibble = TRUE;
                bcd++;
            }
            bcd_size++;
        }
    }
    
    if ( (bcd_size & 0x01) == 0x01 )
    {   // check for odd size, and fill with 0xF0
        *bcd |= 0xF0;
        bcd_size++;
    }
    
    return( bcd_size );
}


//******************************************************************************
//
// Function Name: KRIL_USSDOctet2Septet
//
// Description:   Converts unpacked octets to packed Septets
//
// Notes:
//
//******************************************************************************
UInt16 KRIL_USSDOctet2Septet(
  UInt8 *p_src,
  UInt8 *p_dest,
  UInt16 num_of_octets
  )
{
    UInt16    num_of_bytes;
    UInt8     septet_mod;
    UInt8     c;
    
    // CODE_PAGE_MTI_AT_SIGN -> 0x00
    // Conversion from CodePageMTI_t to Default GSM Alphabet for
    // '@' character is done here
    num_of_bytes = num_of_octets - (num_of_octets >> 3);  // divide by 8
    septet_mod = 0;
    for (; num_of_octets > 0; num_of_octets-- )
    {
        c = *p_src++;
        if ( c == 0x80 )
        {
            c = 0x00;
        }
        
        switch ( septet_mod )
        {
            case 0:
                p_dest[0] = (c & 0x7F);
                break;
            case 1:
                p_dest[0] |= ((c & 0x01) << 7);
                p_dest[1]  = ((c & 0x7E) >> 1);
                break;
            case 2:
                p_dest[1] |= ((c & 0x03) << 6);
                p_dest[2]  = ((c & 0x7C) >> 2);
                break;
            case 3:
                p_dest[2] |= ((c & 0x07) << 5);
                p_dest[3]  = ((c & 0x78) >> 3);
                break;
            case 4:
                p_dest[3] |= ((c & 0x0F) << 4);
                p_dest[4]  = ((c & 0x70) >> 4);
                break;
            case 5:
               p_dest[4] |= ((c & 0x1F) << 3);
               p_dest[5]  = ((c & 0x60) >> 5);
               break;
            case 6:
               p_dest[5] |= ((c & 0x3F) << 2);
               p_dest[6]  = ((c & 0x40) >> 6);
               break;
            case 7:
               p_dest[6] |= ((c & 0x7F) << 1);
               p_dest = &p_dest[7];
               break;
        }
        septet_mod = ((septet_mod + 1) & 0x07);
    }
    // remainder of any septet will be filled by the
    // ANDing and shifting of the second byte in the above section of code
    
    // per GSM03.38, if the left-over is 7bits, we have to fill it with <CR> to 
    // avoid confusion with the "0x00" being "@" in 7bit default character set.
    if (septet_mod == 7)
    {
        KRIL_DEBUG(DBG_INFO,"7bit boundary matches 8bit boundary..\n");
        p_dest[6] |= (0x0D << 1);
    }
    
    KRIL_DEBUG(DBG_INFO,"num bytes %d\n", num_of_bytes);
    
    return( num_of_bytes );
}


//******************************************************************************
//
// Function Name:	KRIL_USSDSeptet2Octet
//
// Description:	This function converts Septet formatted data to Octet format
//				in USSD.
//				This function returns the number of bytes that was required
//				storage the convert data.
//
// Notes:
//
//******************************************************************************
UInt16 KRIL_USSDSeptet2Octet(
	UInt8 *p_src,
	UInt8 *p_dest,
	UInt16 num_of_Septets
	)
{
	UInt16			num_of_Octets;
	UInt16			avail_cha_cnt;
	UInt8			septet_mod;
	UInt8			*p_temp;


	num_of_Octets = (num_of_Septets * 8) / 7;
	septet_mod = 0;
	avail_cha_cnt = 0;
	p_temp = p_dest;
	for(; num_of_Octets > 0; num_of_Octets-- )
	{
		switch( septet_mod )
		{
			case 0:
				*p_temp = (p_src[0] & 0x7F);
				break;
			case 1:
				*p_temp = (((p_src[0] & 0x80) >> 7) | ((p_src[1] & 0x3F) << 1));
				break;
			case 2:
				*p_temp = (((p_src[1] & 0xC0) >> 6) | ((p_src[2] & 0x1F) << 2));
				break;
			case 3:
				*p_temp = (((p_src[2] & 0xE0) >> 5) | ((p_src[3] & 0x0F) << 3));
				break;
			case 4:
				*p_temp = (((p_src[3] & 0xF0) >> 4) | ((p_src[4] & 0x07) << 4));
				break;
			case 5:
				*p_temp = (((p_src[4] & 0xF8) >> 3) | ((p_src[5] & 0x03) << 5));
				break;
			case 6:
				*p_temp = (((p_src[5] & 0xFC) >> 2) | ((p_src[6] & 0x01) << 6));
				break;
			case 7:
				*p_temp = ((p_src[6] & 0xFE) >> 1);
				break;
		}
		p_temp++;
		avail_cha_cnt++;
		if(septet_mod == 7)	p_src = &p_src[7];
		septet_mod = ((septet_mod + 1) & 0x07);
	}

	// per GSM03.38, if the last 7bit char is on 8bit boundary and it is 
	// a <CR> char, it will need to be removed. 
	if( (septet_mod == 0) && (*(p_temp-1) == 0x0D) ){
		KRIL_DEBUG(DBG_INFO, "USSD: 7bit boundary matches 8bit boundary, extra <CR> removed..");
		p_temp--;
		avail_cha_cnt--;
	}

	// reuse the variable 'num_of_septets'
	num_of_Octets = avail_cha_cnt;
	// now convert all Default GSM Alphabet '@' to MTI version
	// 0x00 -> CODE_PAGE_MTI_AT_SIGN
	//for(p_temp = p_dest; num_of_Octets > 0; num_of_Octets--, p_temp++ )
	//{
	//	if( *p_temp == DEF_GSM_AT_SIGN )
	//	{
	//		*p_temp = CODE_PAGE_MTI_AT_SIGN;
	//	}
	//}

	// NULL terminate the string
	*p_temp = 0x00;
	return( avail_cha_cnt );
}


//******************************************************************************
//
// Function Name: KRIL_GetCharacterSet
//
// Description:   Gets character set from DCS
//
// Notes:
//
//******************************************************************************
UInt8 KRIL_GetCharacterSet(UInt8 inDcs)
{
    KRIL_CharacterSet_t charSet = KRIL_CHARACTER_SET_8_BIT_DATA;
    
    switch (inDcs >> 4) //Coding Group, refere to CodingGroup_t
    {
        case KRIL_CODING_GROUP_0_LANGUAGE_GSM_7_BIT_ALPHABET:
        case KRIL_CODING_GROUP_2_LANGUAGE_GSM_7_BIT_ALPHABET:
        case KRIL_CODING_GROUP_3_LANGUAGE_GSM_7_BIT_ALPHABET:
            charSet = KRIL_CHARACTER_SET_GSM_7_BIT_DEFAULT;
            break;
        
        case KRIL_CODING_GROUP_1_MESSAGE_PROTECTED_BY_LANGUAGE_IND:
            if (inDcs & 0x01) //UCS2
            {
              charSet = KRIL_CHARACTER_SET_UCS2_16_BIT;
            }
            else //GSM 7 bit default alphabet
            {
              charSet = KRIL_CHARACTER_SET_GSM_7_BIT_DEFAULT;
            }
            break;
        
        case KRIL_CODING_GROUP_4_GENERAL_TEXT_UNCOMPRESSED:
        case KRIL_CODING_GROUP_5_GENERAL_TEXT_UNCOMPRESSED:
        case KRIL_CODING_GROUP_9_MSG_WITH_USER_DATA_HEADER:
            charSet = (KRIL_CharacterSet_t)((inDcs >> 2) & 0x03);
            break;
        
        case KRIL_CODING_GROUP_6_GENERAL_TEXT_COMPRESSED:
        case KRIL_CODING_GROUP_7_GENERAL_TEXT_COMPRESSED:
            break;
        
        case KRIL_CODING_GROUP_E_WAP_RELATED:
            break;
        
        case KRIL_CODING_GROUP_F_DATA_CODING_MSG_HANDLING:
            if (!((inDcs >> 2) & 0x01))
            {
              charSet = KRIL_CHARACTER_SET_GSM_7_BIT_DEFAULT;
            }
            break;
        
        default:
            charSet = KRIL_CHARACTER_SET_RESERVED;
            break;
    }
    
    return (charSet);
}

//******************************************************************************
//
// Function Name: PoressDeviceNotifyCallbackFun
//
// Description:   Implement the Notify call back function of device client.
//
// Notes:
//
//******************************************************************************
void PoressDeviceNotifyCallbackFun(int SimId, UInt32 msgType, void* dataBuf, UInt32 dataLength)
{
    int i,j;
    
    for(i = 0 ; i < TOTAL_BCMDEVICE_NUM ; i++)
    {
        if(bcm_dev_results[i].notifyid_list != NULL && bcm_dev_results[i].notifyid_list_len != 0)
        {
            for(j = 0 ; j < bcm_dev_results[i].notifyid_list_len ; j++)
            {
                if(bcm_dev_results[i].notifyid_list[j] == msgType)
                {
                    if(bcm_dev_results[i].notifyCb != NULL)
                    {
                        KRIL_DEBUG(DBG_INFO,"Call Notify Callback of ClientID:%d\n",i+1);
                        bcm_dev_results[i].notifyCb(SimId, msgType, 1, dataBuf, dataLength);
                    }
                    else
                    {
                        KRIL_DEBUG(DBG_ERROR,"Notify Callback of ClientID:%d is NULL !!!\n",i+1);
                    }
                    break;
                }
            }
        }
    }    
}


//******************************************************************************
//
// Function Name: DisplayStkTextString
//
// Description:   Display STK text string
//
// Notes:
//
//******************************************************************************
void DisplayStkTextString(SATKString_t pSTKstr)
{
    Unicode_t codeType;
    UInt16    len;
    char strout[300];
    
    codeType = pSTKstr.unicode_type;

    len = pSTKstr.len > (sizeof(strout)-1) ? (sizeof(strout)-1) : pSTKstr.len;
     
    if ((len > 0) && (codeType == UNICODE_GSM || codeType == UNICODE_UCS1))
    {
        memcpy(strout, pSTKstr.string, len);
        strout[len] = '\0';
        
        KRIL_DEBUG(DBG_INFO,"string: %s\n",strout);
    }
}


//******************************************************************************
//
// Function Name: ParseSetupMenuCommandQualifier
//
// Description:   Parse Setup Menu Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseSetupMenuCommandQualifier(UInt8 *cmdQualifierData, SetupMenu_t *pSetupMenu)
{
    if (pSetupMenu->isHelpAvailable)
        *cmdQualifierData |= 0x80;
    else
        *cmdQualifierData &= ~0x80;    
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);    
}


//******************************************************************************
//
// Function Name: ParseSelectItemCommandQualifier
//
// Description:   Parse Select Item Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseSelectItemCommandQualifier(UInt8 *cmdQualifierData, SelectItem_t *pSelectItem)
{
    if (pSelectItem->isHelpAvailable)
        *cmdQualifierData |= 0x80;
    else
        *cmdQualifierData &= ~0x80;    
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);
}


//******************************************************************************
//
// Function Name: ParseGetInputCommandQualifier
//
// Description:   Parse Get Input Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseGetInputCommandQualifier(UInt8 *cmdQualifierData, GetInput_t *pGetInput)
{
    switch (pGetInput->inPutType)
    {
        case S_IKT_DIGIT:
            *cmdQualifierData &= ~0x03;
            break;
        
        case S_IKT_SMSDEFAULTSET:
            *cmdQualifierData |= 0x01;
            *cmdQualifierData &= ~0x02;
            break;
        
        case S_IKT_UCS2:
            *cmdQualifierData |= 0x01;
            *cmdQualifierData |= 0x02;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Not Support inPutType:%d\n",pGetInput->inPutType);
            break;
    }
    
    if (pGetInput->isEcho)
        *cmdQualifierData &= ~0x04;
    else
        *cmdQualifierData |= 0x04;
    
    // Because CP STK module can handle SMS packed/unpacked internally, RIL always
    // ignore the isPacked flag.
    //if (pGetInput->isPacked)
    //    *cmdQualifierData |= 0x08;
    //else
    //    *cmdQualifierData &= ~0x08;
        
    if (pGetInput->isHelpAvailable)
        *cmdQualifierData |= 0x80;
    else
        *cmdQualifierData &= ~0x80;
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);
}


//******************************************************************************
//
// Function Name: ParseGetInkeyCommandQualifier
//
// Description:   Parse Get Inkey Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseGetInkeyCommandQualifier(UInt8 *cmdQualifierData, GetInkey_t *pGetInkey)
{
    switch (pGetInkey->inKeyType)
    {
        case S_IKT_YESNO:
            *cmdQualifierData &= ~0x03;
            *cmdQualifierData |= 0x04;
            break;
        
        case S_IKT_DIGIT:
            *cmdQualifierData &= ~0x07;
            break;
        
        case S_IKT_SMSDEFAULTSET:
            *cmdQualifierData |= 0x01;
            *cmdQualifierData &= ~0x02;
            break;
        
        case S_IKT_UCS2:
            *cmdQualifierData |= 0x01;
            *cmdQualifierData |= 0x02;
            break;        
    }

    if (pGetInkey->isHelpAvailable)
        *cmdQualifierData |= 0x80;
    else
        *cmdQualifierData &= ~0x80;
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);
}


//******************************************************************************
//
// Function Name: ParseDisplayTextCommandQualifier
//
// Description:   Parse Display Text Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseDisplayTextCommandQualifier(UInt8 *cmdQualifierData, DisplayText_t *pDisplayText)
{
    if (pDisplayText->isHighPrio)
        *cmdQualifierData |= 0x01;
    else
        *cmdQualifierData &= ~0x01;
    
    if (pDisplayText->isDelay)
        *cmdQualifierData &= ~0x80;
    else
        *cmdQualifierData |= 0x80;
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);
}


//******************************************************************************
//
// Function Name: ParseSendMOSMSCommandQualifier
//
// Description:   Parse Send MO SMS Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseSendMOSMSCommandQualifier(UInt8 *cmdQualifierData, SendMOSMS_t *pSendMOSMS)
{
    *cmdQualifierData = pSendMOSMS->packingReq;
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);
}


//******************************************************************************
//
// Function Name: ParseSetupCallCommandQualifier
//
// Description:   Parse Setup Call Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseSetupCallCommandQualifier(UInt8 *cmdQualifierData, SetupCall_t *pSetupCall)
{
    *cmdQualifierData = 0x00;
    
    switch (pSetupCall->callType)
    {
        case S_CT_ONIDLE:
            *cmdQualifierData = 0x00;
            break;
        
        case S_CT_ONIDLE_REDIALABLE:
            *cmdQualifierData = 0x01;
            break;
        
        case S_CT_HOLDABLE:
            *cmdQualifierData = 0x02;
            break;
        
        case S_CT_HOLDABLE_REDIALABLE:
            *cmdQualifierData = 0x03;
            break;
        
        case S_CT_DISCONNECTABLE:
            *cmdQualifierData = 0x04;
            break;
        
        case S_CT_DISCONNECTABLE_REDIALABLE:
            *cmdQualifierData = 0x05;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Unknow callType:0x%02X\n",pSetupCall->callType);
            break;
    }
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);
}


//******************************************************************************
//
// Function Name: ParseRefreshCommandQualifier
//
// Description:   Parse Refresh Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseRefreshCommandQualifier(UInt8 *cmdQualifierData, Refresh_t *pRefresh)
{
    *cmdQualifierData = 0x00;
    
    switch (pRefresh->refreshType)
    {
        case SMRT_INIT_FULLFILE_CHANGED:
            *cmdQualifierData = 0x00;
            break;
        
        case SMRT_FILE_CHANGED:
            *cmdQualifierData = 0x01;
            break;
        
        case SMRT_INIT_FILE_CHANGED:
            *cmdQualifierData = 0x02;
            break;
        
        case SMRT_INIT:
            *cmdQualifierData = 0x03;
            break;
        
        case SMRT_RESET:
            *cmdQualifierData = 0x04;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Unknow refreshType:%d\n",pRefresh->refreshType);
            break;
    }
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);
}


//******************************************************************************
//
// Function Name: ParseLaunchBrowserCommandQualifier
//
// Description:   Parse Launch Browser Command Qualifier data
//
// Notes:
//
//******************************************************************************
void ParseLaunchBrowserCommandQualifier(UInt8 *cmdQualifierData, LaunchBrowserReq_t *pLaunchBrowserReq)
{
    *cmdQualifierData = 0x00;
    
    switch (pLaunchBrowserReq->browser_action)
    {
        case LAUNCH_BROWSER_NO_CONNECT:
            *cmdQualifierData = 0x00;
            break;
        
        case LAUNCH_BROWSER_CONNECT:
            *cmdQualifierData = 0x01;
            break;
        
        case LAUNCH_BROWSER_USE_EXIST:
            *cmdQualifierData = 0x02;
            break;
        
        case LAUNCH_BROWSER_CLOSE_EXIST_NORMAL:
            *cmdQualifierData = 0x03;
            break;
        
        case LAUNCH_BROWSER_CLOSE_EXIST_SECURE:
            *cmdQualifierData = 0x04;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Unknow browser_action:%d\n",pLaunchBrowserReq->browser_action);
            break;
    }
    
    KRIL_DEBUG(DBG_INFO,"*cmdQualifierData:0x%02X\n",*cmdQualifierData);    
}


//******************************************************************************
//
// Function Name: ParseCommandDetails
//
// Description:   Parse Command Details(refer to 11.14 section 12.6)
//
// Notes:
//
//******************************************************************************
UInt16 ParseCommandDetails(UInt8 *simple_tlv_ptr, void *pSATKdata, UInt8 cmdType)
{
    UInt8 command_detail_data[5] = {0x81, 0x03, 0x01, 0x00, 0x00};
    UInt16 command_detail_length = 0;
    
    // Fill command type
    command_detail_data[3] = cmdType;
    
    // Parse Command Qualifier
    switch (cmdType)
    {
        case STK_SETUPMENU:
        {
            SetupMenu_t *pSetupMenu = (SetupMenu_t*)pSATKdata;
            ParseSetupMenuCommandQualifier(&command_detail_data[4], pSetupMenu);
            break;
        }
        
        case STK_SELECTITEM:
        {
            SelectItem_t *pSelectItem = (SelectItem_t*)pSATKdata;
            ParseSelectItemCommandQualifier(&command_detail_data[4], pSelectItem);
            break;
        }
            
        case STK_GETINPUT:
        {
            GetInput_t *pGetInput = (GetInput_t*)pSATKdata;
            ParseGetInputCommandQualifier(&command_detail_data[4], pGetInput);
            break;
        }
        
        case STK_GETINKEY:
        {
            GetInkey_t *pGetInkey = (GetInkey_t*)pSATKdata;
            ParseGetInkeyCommandQualifier(&command_detail_data[4], pGetInkey);
            break;
        }
        
        case STK_DISPLAYTEXT:
        {
            DisplayText_t *pDisplayText = (DisplayText_t*)pSATKdata;
            ParseDisplayTextCommandQualifier(&command_detail_data[4], pDisplayText);
            break;
        }
        
        case STK_SENDSMS:
        {
            SendMOSMS_t *pSendMOSMS = (SendMOSMS_t*)pSATKdata;
            ParseSendMOSMSCommandQualifier(&command_detail_data[4], pSendMOSMS);
            break;
        }
        
        case STK_SETUPCALL:
        {
            SetupCall_t *pSetupCall = (SetupCall_t*)pSATKdata;
            ParseSetupCallCommandQualifier(&command_detail_data[4], pSetupCall);
            break;
        }
        
        case STK_REFRESH:
        {
            Refresh_t *pRefresh = (Refresh_t*)pSATKdata;
            ParseRefreshCommandQualifier(&command_detail_data[4], pRefresh);
            break;
        }
        
        case STK_LAUNCHBROWSER:
        {
            LaunchBrowserReq_t *pLaunchBrowserReq = (LaunchBrowserReq_t*)pSATKdata;
            ParseLaunchBrowserCommandQualifier(&command_detail_data[4], pLaunchBrowserReq);
            break;
        }
        
        case STK_LANGUAGENOTIFICATION:
		{
			StkLangNotification_t* pLaungageNotification = (StkLangNotification_t*)pSATKdata;
            
            if (pLaungageNotification->language[0]=='\0')
            {
				command_detail_data[4] = 0x00;
            }
            else
            {
				command_detail_data[4] = 0x01;
			}
			break;
        }
        
        case STK_SENDSS:
        case STK_SENDUSSD:
        case STK_EVENTLIST:
        case STK_PLAYTONE:
        case STK_SENDDTMF:
        case STK_SETUPIDLEMODETEXT:   
            // The command qualifier is RFU, there is nothing to do
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Not Support Command type:0x%02X\n",cmdType);
            return 0;
    }

    command_detail_length = sizeof(command_detail_data)/sizeof(UInt8);
    memcpy(simple_tlv_ptr, command_detail_data, command_detail_length);    
    return command_detail_length;
}


//******************************************************************************
//
// Function Name: ParseDeviceIdentities
//
// Description:   Parse Device Identities(refer to 11.14 section 12.7)
//
// Notes:
//
//******************************************************************************
UInt16 ParseDeviceIdentities(UInt8 *simple_tlv_ptr, UInt8 cmdType)
{
    UInt8 device_identities_data[4] = {0x82, 0x02, 0x81, 0x82};
    UInt16 device_identities_length = 0;
    
    switch (cmdType)
    {
        case STK_SETUPCALL:
        case STK_SENDSS:
        case STK_SENDUSSD:
        case STK_SENDSMS:
        case STK_SENDDTMF:
            device_identities_data[3] = 0x83;
            break;
        
        default:
            device_identities_data[3] = 0x82;
            break;
    }

    device_identities_length = sizeof(device_identities_data)/sizeof(UInt8);
    memcpy(simple_tlv_ptr, device_identities_data, device_identities_length);  
    return device_identities_length;
}


//******************************************************************************
//
// Function Name: ParseAlphaIdentifier
//
// Description:   Parse Alpha Identifier(refer to 11.14 section 12.2)
//
// Notes:
//
//******************************************************************************
UInt16 ParseAlphaIdentifier(UInt8 *simple_tlv_ptr, SATKString_t title)
{
    UInt16 total_byte  = 0;
    UInt16 alphaid_byte = 0;
    UInt16 alpha_length = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill Alpha identifier tag
    tlv_ptr[0] = 0x85;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Length
    if ((title.len == 0) || (title.string == NULL))
    {
        KRIL_DEBUG(DBG_INFO,"The text string length is 0\n");
        tlv_ptr[0] = 0x00;
        tlv_ptr += 1;
        total_byte += 1;
        return total_byte;
    }
    
    if (IS_ALPHAID_CODING(title.unicode_type))
        alphaid_byte = 1;

    alpha_length = alphaid_byte + title.len;
    
    if (alpha_length > 127)
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = alpha_length;
        tlv_ptr += 2;
        total_byte += 2;
    }
    else
    {
        tlv_ptr[0] = alpha_length;
        tlv_ptr += 1;
        total_byte += 1;
    }
    
    // Fill Alpha identifier
    if (IS_ALPHAID_CODING(title.unicode_type))
    {
        tlv_ptr[0] = title.unicode_type;
        tlv_ptr += 1;
        total_byte += 1;
    }

    memcpy(tlv_ptr, title.string, title.len);
    total_byte += title.len;
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "AlphaId");
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseItemData
//
// Description:   Parse Item Data Object(refer to 11.14 section 12.9)
//
// Notes:
//
//******************************************************************************
UInt16 ParseItemData(UInt8 *simple_tlv_ptr, UInt8 numItems, UInt8 *pItemIdList, SATKString_t *pItemList)
{
    UInt8 i;
    UInt16 total_byte   = 0;
    UInt16 alphaid_byte = 0;
    UInt16 itemid_byte  = 1;
    UInt16 item_length  = 0;
    UInt16 total_sum    = 0;
    UInt8  *tlv_ptr = NULL;
    
    // If the "item data object for item 1" is a null data object(i.e. length = "00"
    // and no value part), this is an indication to the ME to remove the existing menu.
    if ((0 == numItems) || (0 == pItemIdList[0]))
    {
        tlv_ptr = simple_tlv_ptr;
        
        // Fill item tag
        tlv_ptr[0] = 0x8F;
        
        // Fill Length
        tlv_ptr[1] = 0;
        
        total_sum = 2;
    }
    else if (NULL != pItemList)
    {
        for (i = 0 ; i < numItems ; i++)
        {
            tlv_ptr = simple_tlv_ptr;
            total_byte = 0;
            
            if (IS_ALPHAID_CODING(pItemList[i].unicode_type))
                alphaid_byte = 1;
            else
                alphaid_byte = 0;
            
            // Fill item tag
            tlv_ptr[0] = 0x8F;
            tlv_ptr += 1;
            total_byte += 1;
            
            // Fill Length
            item_length = itemid_byte + alphaid_byte + pItemList[i].len;
            if (item_length > 127)
            {
                tlv_ptr[0] = 0x81;
                tlv_ptr[1] = item_length;
                tlv_ptr += 2;
                total_byte += 2;
            }
            else
            {
                tlv_ptr[0] = item_length;
                tlv_ptr += 1;
                total_byte += 1;
            }
            
            KRIL_DEBUG(DBG_ERROR,"pItemList[%d].len = %d\n",i,pItemList[i].len);	
            KRIL_DEBUG(DBG_ERROR,"pItemList[%d].string = %02X\n",i,pItemList[i].string);	
			
            // Fill Identifier od item
            tlv_ptr[0] = pItemIdList[i];
            tlv_ptr += 1;
            total_byte += 1;
            
            // Fill Text string of item
            if (IS_ALPHAID_CODING(pItemList[i].unicode_type))
            {
                tlv_ptr[0] = pItemList[i].unicode_type;
                tlv_ptr += 1;
                total_byte += 1;
            }        
            
            memcpy(tlv_ptr, pItemList[i].string, pItemList[i].len);
            total_byte += pItemList[i].len;
            KRIL_DEBUG(DBG_INFO,"i:%d itemID:%d total_byte:%d\n",i,pItemIdList[i],total_byte);
            DisplayStkTextString(pItemList[i]);
            
            //RawDataPrintfun(simple_tlv_ptr, total_byte, "item");
            simple_tlv_ptr += total_byte;
            total_sum += total_byte;
        }
    }
    
    KRIL_DEBUG(DBG_INFO,"total_sum:%d\n",total_sum);
    return total_sum;
}


//******************************************************************************
//
// Function Name: ParseTextString
//
// Description:   Parse TextString(refer to 11.14 section 12.15)
//
// Notes:
//
//******************************************************************************
UInt16 ParseTextString(UInt8 *simple_tlv_ptr, SATKString_t textStr)
{
    UInt16 total_byte  = 0;
    UInt16 codingscheme_byte = 1;
    UInt16 text_length = 0;
    UInt16 septet_len = 0;
    char   septet_string[255];
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill Text string tag
    tlv_ptr[0] = 0x8D;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Length
    if (textStr.len == 0)
    {
        KRIL_DEBUG(DBG_INFO,"The text string length is 0\n");
        tlv_ptr[0] = 0x00;
        tlv_ptr += 1;
        total_byte += 1;
        return total_byte;  // gearn text length 0 fixed  for setup idle mode text
    }

    // If the coding scheme is UNICODE_GSM, we should convert the unpacked 8 bit data
    // to packed 7 bit data and modify the string length.
    if (UNICODE_GSM == textStr.unicode_type)
    {
        septet_len = KRIL_USSDOctet2Septet(textStr.string, septet_string, textStr.len);
        text_length = codingscheme_byte + septet_len;
    }
    else
    {
    text_length = codingscheme_byte + textStr.len;
    }    
    
    if (text_length > 127)
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = text_length;
        tlv_ptr += 2;
        total_byte += 2;
    }
    else
    {
        tlv_ptr[0] = text_length;
        tlv_ptr += 1;
        total_byte += 1;
    }
    
    // Fill Data coding scheme
    switch (textStr.unicode_type)
    {
        case UNICODE_GSM:
            tlv_ptr[0] = 0x00;
            break;
            
        case UNICODE_UCS1:
            tlv_ptr[0] = 0x04;
            break;
        
        case UNICODE_80:
            tlv_ptr[0] = 0x08;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Not Supported code type:0x%02X\n",textStr.unicode_type);
            return 0;
    }
    
    tlv_ptr += 1;
    total_byte += 1;
    
    if (UNICODE_GSM == textStr.unicode_type)
    {
        memcpy(tlv_ptr, septet_string, septet_len);
        total_byte += septet_len;
    }
    else
    {
    memcpy(tlv_ptr, textStr.string, textStr.len);
    total_byte += textStr.len;
    }
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "TextStr");    
    
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseDefaultTextString
//
// Description:   Parse Default TextString(refer to 11.14 section 12.23)
//
// Notes:
//
//******************************************************************************
UInt16 ParseDefaultTextString(UInt8 *simple_tlv_ptr, SATKString_t textStr)
{
    UInt16 total_byte  = 0;
    UInt16 codingscheme_byte = 1;
    UInt16 text_length = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill Text string tag
    tlv_ptr[0] = 0x97;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Length
    if (textStr.len == 0)
    {
        KRIL_DEBUG(DBG_INFO,"The text string length is 0\n");
        tlv_ptr[0] = 0x00;
        tlv_ptr += 1;
        total_byte += 1;
        return total_byte;
    }

    text_length = codingscheme_byte + textStr.len;
    
    if (text_length > 127)
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = text_length;
        tlv_ptr += 2;
        total_byte += 2;
    }
    else
    {
        tlv_ptr[0] = text_length;
        tlv_ptr += 1;
        total_byte += 1;
    }
    
    // Fill Data coding scheme
    switch (textStr.unicode_type)
    {
        case UNICODE_GSM:
        case UNICODE_UCS1:
            tlv_ptr[0] = 0x04;
            tlv_ptr += 1;
            total_byte += 1;
            break;
        
        case UNICODE_80:
            tlv_ptr[0] = 0x08;
            tlv_ptr += 1;
            total_byte += 1;
            break;
        
        default:
            KRIL_DEBUG(DBG_ERROR,"Not Supported code type:0x%02X\n",textStr.unicode_type);
            return 0;
    }
    
    memcpy(tlv_ptr, textStr.string, textStr.len);
    total_byte += textStr.len;
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "TextStr");    
    
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseResponseLength
//
// Description:   Parse Response Length(refer to 11.14 section 12.11)
//
// Notes:
//
//******************************************************************************
UInt16 ParseResponseLength(UInt8 *simple_tlv_ptr, UInt8 minLen, UInt8 maxLen)
{
    UInt8 responseLength_data[4] = {0x91, 0x02, 0x00, 0x00};
    UInt16 total_byte  = 0;
    
    // Fill maximum and minimum length od response
    responseLength_data[2] = minLen;
    responseLength_data[3] = maxLen;
    
   if(maxLen == 0)	// gearn Maginal test get input
    return 0;



	
    total_byte = 4;
    memcpy(simple_tlv_ptr, responseLength_data, total_byte);
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseImmediateResponse
//
// Description:   Parse Immediate Response (refer to 11.14 section 12.43)
//
// Notes:
//
//******************************************************************************
void ParseImmediateResponse(UInt8 *simple_tlv_ptr)
{
    UInt8 immediateresponse_data[2] = {0xAB, 0x00};
    
    memcpy(simple_tlv_ptr, immediateresponse_data, 2);
}


//******************************************************************************
//
// Function Name: ParseTone
//
// Description:   Parse Tone (refer to 11.14 section 12.16)
//
// Notes:
//
//******************************************************************************
UInt16 ParseTone(UInt8 *simple_tlv_ptr, SATKToneType_t toneType)
{
    UInt8 tone_data[3] = {0x8E, 0x01, 0x00};
    UInt16 tone_length = 3;
    
    switch (toneType)
    {
        case S_TT_DIALTONE:
            tone_data[2] = 0x01;
            break;
        
        case S_TT_CALLEDUSERBUSY:
            tone_data[2] = 0x02;
            break;
        
        case S_TT_CONGEST:
            tone_data[2] = 0x03;
            break;
        
        case S_TT_RADIOPATHACK:
            tone_data[2] = 0x04;
            break;
        
        case S_TT_RADIOPATHUNAVAILABLE:
            tone_data[2] = 0x05;
            break;
        
        case S_TT_ERROR:
            tone_data[2] = 0x06;
            break;
        
        case S_TT_CALLWAITING:
            tone_data[2] = 0x07;
            break;
        
        case S_TT_RINGING:
            tone_data[2] = 0x08;
            break;
          
        case S_TT_BEEP:
            tone_data[2] = 0x10;
            break;
        
        case S_TT_POSITIVEACK:
            tone_data[2] = 0x11;
            break;
        
        case S_TT_NEGTIVEACK:
            tone_data[2] = 0x12;
            break;
 
        default:
            KRIL_DEBUG(DBG_ERROR,"Unknow or default tone type:0x%02X\n",toneType);
            return 0;
    }
    
    memcpy(simple_tlv_ptr, tone_data, tone_length);
    return tone_length;
}


//******************************************************************************
//
// Function Name: ParseDuration
//
// Description:   Parse Duration (refer to 11.14 section 12.8)
//
// Notes:
//
//******************************************************************************
UInt16 ParseDuration(UInt8 *simple_tlv_ptr, UInt32 duration)
{
    UInt8 duration_data[4] = {0x84, 0x02, 0x00, 0x00};
    UInt16 duration_length = 4;
    UInt8 minutes = 0;
    UInt8 seconds = 0;
    
    if (0 == duration)
    { 
        KRIL_DEBUG(DBG_ERROR,"duration is 0. Error!!!\n");
        return 0;
    }
    
    minutes = duration / 60000;
    seconds = duration / 1000;
    
    if (minutes > 0 && minutes <= 255)
    {
        duration_data[2] = 0x00;
        duration_data[3] = minutes;
    }
    else if (seconds > 0 && seconds <= 255)
    {
        duration_data[2] = 0x01;
        duration_data[3] = seconds;
    }
    else
    {
        KRIL_DEBUG(DBG_ERROR,"duration:%ld is invalid. Error!!!\n", duration);
        return 0;
    }
    
    memcpy(simple_tlv_ptr, duration_data, duration_length);
    return duration_length;
}


//******************************************************************************
//
// Function Name: ParseAddress
//
// Description:   Parse Address (refer to 11.14 section 12.1)
//
// Notes:
//
//******************************************************************************
UInt16 ParseAddress(UInt8 *simple_tlv_ptr, SATKNum_t *num)
{
    UInt16 total_byte  = 0;
    UInt16 address_length = 0;
    UInt16 number_length = 0;
    UInt16 diallingNum_length = 0;
    UInt8  i;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill Address tag
    tlv_ptr[0] = 0x86;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill the length
    number_length = strlen(num->Num);
    diallingNum_length = (number_length + 1)/2;
    address_length = 1 + diallingNum_length;
    
    if (address_length > 127)
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = address_length;
        tlv_ptr += 2;
        total_byte += 2;
    }
    else
    {
        tlv_ptr[0] = address_length;
        tlv_ptr += 1;
        total_byte += 1;
    }
    
    // Fill TON and NPI
    tlv_ptr[0] = num->Npi | (num->Ton << 4) | 0x80;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Dialling number string
    for (i = 0 ; i < diallingNum_length ; i++)
    {
        // reset the value of tlv_ptr[i]
        tlv_ptr[i] = 0;
        
        if ((2*i) < number_length)
        {
            if (num->Num[2*i] == 0x50 || num->Num[2*i] == 0x70)
            {
                tlv_ptr[i] |= 0x0C;
            }
            else
            {
                tlv_ptr[i] |= ((num->Num[2*i] - 0x30)& 0x0F);
            }
        }
        
        if ((2*i+1) < number_length)
        {
            if (num->Num[2*i+1] == 0x50 || num->Num[2*i+1] == 0x70)
            {
                tlv_ptr[i] |= 0xC0;
            }
            else
            {
                tlv_ptr[i] |= (((num->Num[2*i+1] - 0x30)& 0x0F)<<4);
            }            
        }
    }
    
    if ((number_length % 2) != 0)
    {
        tlv_ptr[diallingNum_length - 1] |= 0xF0;
    }
    
    tlv_ptr += diallingNum_length;
    total_byte += diallingNum_length;
    
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Address");
    return total_byte;    
}

//******************************************************************************
//
// Function Name: ParseSmsAddress
//
// Description:   Parse SMS Address (refer to 11.14 section 12.1)
//
// Notes:
//
//******************************************************************************
UInt16 ParseSmsAddress(UInt8 *simple_tlv_ptr, SendMOSMS_t* moSms)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
	
    // Fill Address tag
    tlv_ptr[0] = 0x86;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill the length
    if (moSms->sms_data.sca_len <= sizeof(moSms->sms_data.sca_data))
    {
        tlv_ptr[0] = (moSms->sms_data.sca_len + 1); // one byte for TON and NPI
        tlv_ptr += 1;
        total_byte += 1;
    }
    else
    {
    	  KRIL_DEBUG(DBG_ERROR,"Invalid sms_data.sca_len:%d Error!!!\n", moSms->sms_data.sca_len);
    	  return 0;
    }
    
    // Fill TON and NPI
    tlv_ptr[0] = moSms->sms_data.sca_ton_npi;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Dialling number string
    //memcpy(tlv_ptr, moSms->sms_data.sca_data, moSms->sms_data.sca_len);
    memcpy(tlv_ptr, moSms->sms_data.sca_data, sizeof(moSms->sms_data.sca_data));
    tlv_ptr += moSms->sms_data.sca_len;
    total_byte += moSms->sms_data.sca_len;
    
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "SMS Address");
    return total_byte;    
}


//******************************************************************************
//
// Function Name: ParseFileList
//
// Description:   Parse File List (refer to 11.14 section 12.18)
//
// Notes:
//
//******************************************************************************
UInt16 ParseFileList(UInt8 *simple_tlv_ptr, REFRESH_FILE_LIST_t	*FileIdList)
{
    UInt16 total_byte  = 0;
    UInt16 files_length = 0;
    UInt8  i,j;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    UInt8  *pfile_path = NULL;
    UInt16  file_path_len = 0;
    
    // Fill File List tag
    tlv_ptr[0] = 0x92;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill the length
    files_length += 1;
    
    for (i = 0 ; i < FileIdList->number_of_file ; i++)
    {
        files_length += (FileIdList->changed_file[i].path_len*2);
    }
    
    if (files_length > 127)
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = files_length;
        tlv_ptr += 2;
        total_byte += 2;
    }
    else
    {
        tlv_ptr[0] = files_length;
        tlv_ptr += 1;
        total_byte += 1;
    }
    
    // Fill number of files
    tlv_ptr[0] = FileIdList->number_of_file;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Files
    for (i = 0 ; i < FileIdList->number_of_file ; i++)
    {
        KRIL_DEBUG(DBG_INFO,"changed_file[%d]: path_len:%d\n",i,FileIdList->changed_file[i].path_len);
        pfile_path = (UInt8*)FileIdList->changed_file[i].file_path;
        file_path_len = FileIdList->changed_file[i].path_len*2;
        RawDataPrintfun(pfile_path, file_path_len, "file_path");
        
        for (j = 0 ; j < FileIdList->changed_file[i].path_len ; j++)
        {
            tlv_ptr[2*j] = pfile_path[2*j+1];
            tlv_ptr[2*j+1] = pfile_path[2*j];
        }
        tlv_ptr += file_path_len;
        total_byte += file_path_len;
    }    

    //RawDataPrintfun(simple_tlv_ptr, total_byte, "File List");
    return total_byte;    
}


//******************************************************************************
//
// Function Name: ParseItemsNextActionIndicator
//
// Description:   Parse Items Next Action Indicator (refer to 11.14 section 12.24)
//
// Notes:
//
//******************************************************************************
UInt16 ParseItemsNextActionIndicator(UInt8 *simple_tlv_ptr, UInt8 numItems, STKIconListId_t pNextActIndList)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill Items Next Action Indicator tag
    tlv_ptr[0] = 0x18;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length
    tlv_ptr[0] = numItems;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Items Next Action Indicator list
    memcpy(tlv_ptr, pNextActIndList.Id, numItems);
    tlv_ptr += numItems;
    total_byte += numItems;
    
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Next Action Indicator");
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseItemIdentifier
//
// Description:   Parse Item Identifier (refer to 11.14 section 12.10)
//
// Notes:
//
//******************************************************************************
UInt16 ParseItemIdentifier(UInt8 *simple_tlv_ptr, UInt8 defaultItem)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;    

    // Fill Item Identifier tag
    tlv_ptr[0] = 0x90;
    tlv_ptr += 1;
    total_byte += 1;    

    // Fill length
    tlv_ptr[0] = 0x01;
    tlv_ptr += 1;
    total_byte += 1;

    // Fill Identifier od item chosen
    tlv_ptr[0] = defaultItem;
    tlv_ptr += 1;
    total_byte += 1;

    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Item Identifier");
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseIconIdentifier
//
// Description:   Parse Icon Identifier (refer to 11.14 section 12.31)
//
// Notes:
//
//******************************************************************************
UInt16 ParseIconIdentifier(UInt8 *simple_tlv_ptr, SATKIcon_t icon)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill Icon Identifier tag
    tlv_ptr[0] = 0x9E;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length
    tlv_ptr[0] = 0x02;
    tlv_ptr += 1;
    total_byte += 1;    
        
    // Fill Icon qualifier
    if (icon.IsSelfExplanatory)
    {
        tlv_ptr[0] = 0x00;
    }
    else
    {
        tlv_ptr[0] = 0x01;
    }
    
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Icon identifier
    tlv_ptr[0] = icon.Id;
    tlv_ptr += 1;
    total_byte += 1;
    
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Icon Identifier");
    return total_byte;    
}


//******************************************************************************
//
// Function Name: ParseIconIdentifierList
//
// Description:   Parse Icon Identifier List (refer to 11.14 section 12.32)
//
// Notes:
//
//******************************************************************************
UInt16 ParseIconIdentifierList(UInt8 *simple_tlv_ptr, UInt8 numItems, STKIconListId_t pIconList)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;

    // Fill Icon Identifier tag
    tlv_ptr[0] = 0x9F;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length
    tlv_ptr[0] = numItems+1;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Icon list qualifier
    if (pIconList.IsSelfExplanatory)
    {
        tlv_ptr[0] = 0x00;
    }
    else
    {
        tlv_ptr[0] = 0x01;
    }
    
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Icon identifier list
    memcpy(tlv_ptr, pIconList.Id, numItems);
    tlv_ptr += numItems;
    total_byte += numItems;

    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Icon Identifier List");
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParserBrowserIdentity
//
// Description:   Parse Browser Identity (refer to 11.14 section 12.47)
//
// Notes:
//
//******************************************************************************
UInt16 ParserBrowserIdentity(UInt8 *simple_tlv_ptr, UInt8 browser_id)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;

    // Fill Browser Identity tag
    tlv_ptr[0] = 0x30;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length
    tlv_ptr[0] = 1;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill Browser Identity
    tlv_ptr[0] = browser_id;
    tlv_ptr += 1;
    total_byte += 1;    

    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Browser Identity");
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseURL
//
// Description:   Parse URL (refer to 11.14 section 12.48)
//
// Notes:
//
//******************************************************************************
UInt16 ParseURL(UInt8 *simple_tlv_ptr, char *url)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    UInt8  url_str_len = 0;
    //char*  defaultUrl = "http://www.android.com";

    // Fill URL tag
    tlv_ptr[0] = 0x31;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length and URL
    url_str_len = strlen(url);
    
    if (url_str_len > 0)
    {
        tlv_ptr[0] = url_str_len;
        tlv_ptr += 1;
        total_byte += 1;
        
        memcpy(tlv_ptr, url, url_str_len);
        tlv_ptr += url_str_len;
        total_byte += url_str_len;
    }
    else
    {
        //KRIL_DEBUG(DBG_INFO,"Use default URL\n");
        //url_str_len = strlen(defaultUrl);
        tlv_ptr[0] = 0;
        tlv_ptr += 1;
        total_byte += 1;
        
        //memcpy(tlv_ptr, defaultUrl, url_str_len);
        //tlv_ptr += url_str_len;
        //total_byte += url_str_len;        
    }

    //RawDataPrintfun(simple_tlv_ptr, total_byte, "URL");
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParseBear
//
// Description:   Parse Bear (refer to 11.14 section 12.49)
//
// Notes:
//
//******************************************************************************
UInt16 ParseBear(UInt8 *simple_tlv_ptr, LaunchBrowserReq_t *pLaunchBrowserReq)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    int i;

    // Fill Bear tag
    tlv_ptr[0] = 0x32;
    tlv_ptr += 1;
    total_byte += 1;

    // Fill Length
    tlv_ptr[0] = pLaunchBrowserReq->bearer_length;
    tlv_ptr += 1;
    total_byte += 1;

    // Fill List of bearers in order of priority requested
    for (i = 0 ; i < pLaunchBrowserReq->bearer_length ; i++)
    {
        tlv_ptr[0] = pLaunchBrowserReq->bearer[i];
        tlv_ptr += 1;
        total_byte += 1;
    }

    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Bear");
    return total_byte;
}


//******************************************************************************
//
// Function Name: ParserProvisioning
//
// Description:   Parser Provisioning File Reference (refer to 11.14 section 12.50)
//
// Notes:
//
//******************************************************************************
UInt16 ParserProvisioning(UInt8 *simple_tlv_ptr, LaunchBrowserReq_t *pLaunchBrowserReq)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    int i;

    for (i = 0 ; i < pLaunchBrowserReq->prov_length ; i++)
    {
        // Fill Provisioning File Reference tag
        tlv_ptr[0] = 0x33;
        tlv_ptr += 1;
        total_byte += 1;
        
        // Fill Length
        tlv_ptr[0] = pLaunchBrowserReq->prov_file[i].length;
        tlv_ptr += 1;
        total_byte += 1; 
        
        // Fill path to the provisioning file
        memcpy(tlv_ptr, pLaunchBrowserReq->prov_file[i].prov_file_data, 
        pLaunchBrowserReq->prov_file[i].length);
        tlv_ptr += pLaunchBrowserReq->prov_file[i].length;
        total_byte += pLaunchBrowserReq->prov_file[i].length;
    }
    
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Provisioning");
    return total_byte;
}

//******************************************************************************
//
// Function Name: ParseSsString
//
// Description:   Parse SS String (refer to 11.14 section 12.31)
//
// Notes:
//
//******************************************************************************
UInt16 ParseSsString(UInt8 *simple_tlv_ptr, SATKNum_t* num)
{
    UInt16 total_byte  = 0;
    UInt8* tlv_ptr = simple_tlv_ptr;
    UInt16 ss_str_len = 0;
    UInt8 ss_string[MAX_SIMPLE_TLV_DATA_LENGTH];
    
    // Fill SS String tag
    tlv_ptr[0] = 0x89;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill SS String
    ss_str_len = KRIL_BCD_ConvertString2PBCD(ss_string, num->Num);
    ss_str_len = (ss_str_len & 0x01) ? ((ss_str_len + 1) >> 1) : (ss_str_len >> 1);

    // Fill length
    if(ss_str_len > (127-1))
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = ss_str_len + 1; //1 byte for TON/NPI
        tlv_ptr += 2;
        total_byte += 2;    
    }
    else
    {
        tlv_ptr[0] = ss_str_len + 1; //1 byte for TON/NPI
        tlv_ptr += 1;
        total_byte += 1;    
    }
        
    // Fill Ton/NPI
    tlv_ptr[0] = (num->Npi & 0x0F) | (num->Ton << 4) | 0x80;
    tlv_ptr += 1;
    total_byte += 1;    
    
    // Fill SS String
    memcpy(&tlv_ptr[0], ss_string, ss_str_len);
    tlv_ptr += ss_str_len;
    total_byte += ss_str_len;
    
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "SS String");
    return total_byte;    
}

//******************************************************************************
//
// Function Name: ParseUssdString
//
// Description:   Parse USSD String (refer to 11.14 section 12.31)
//
// Notes:
//
//******************************************************************************
UInt16 ParseUssdString(UInt8 *simple_tlv_ptr, SATKNum_t* num)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    UInt16 septet_len = 0;
    UInt8 septet_string[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt8 total_ussd_len = 0;
    
    // Fill USSD String tag
    tlv_ptr[0] = 0x8A;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Convert from octets to septets for CB default alphabet
    if (KRIL_GetCharacterSet(num->dcs) == KRIL_CHARACTER_SET_GSM_7_BIT_DEFAULT)
    {
        KRIL_DEBUG(DBG_INFO,"pack USSD Octets to Septets\n");
        septet_len = KRIL_USSDOctet2Septet(num->Num, septet_string, num->len);
        total_ussd_len = septet_len + 1; //1 byte for DCS
    }
    else
    {
        total_ussd_len = num->len + 1; //1 byte for DCS
    }

    // Fill length
    if (total_ussd_len > 127)
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = total_ussd_len;
        tlv_ptr += 2;
        total_byte += 2; 
    }
    else
    {
        tlv_ptr[0] = total_ussd_len;
        tlv_ptr += 1;
        total_byte += 1;    
    }
        
    // Fill DCS
    tlv_ptr[0] = num->dcs;
    tlv_ptr += 1;
    total_byte += 1;    
    
    // Fill USSD String
    if (septet_len > 0)
    {
          memcpy(tlv_ptr, septet_string, total_ussd_len - 1);
    }
    else
    {
          memcpy(tlv_ptr, num->Num, total_ussd_len - 1);
    }
    
    tlv_ptr += total_ussd_len - 1;
    total_byte += total_ussd_len - 1;

    //RawDataPrintfun(simple_tlv_ptr, total_byte, "USSD String");
    return total_byte;    
}

//******************************************************************************
//
// Function Name: ParseCcp
//
// Description:   Parse Capability Configuration Parameters (refer to 11.14 section 12.31)
//
// Notes:
//
//******************************************************************************
UInt16 ParseCcp(UInt8 *simple_tlv_ptr, CC_BearerCap_t* bc)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill CCP tag
    tlv_ptr[0] = 0x87;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length
    tlv_ptr[0] = bc->Bc1.val[1] + 2; //2 bytes for BC IEI and IE length
    tlv_ptr += 1;
    total_byte += 1;    
        
    // Fill BC1
    memcpy(tlv_ptr, bc->Bc1.val, bc->Bc1.val[1] + 2);
    tlv_ptr += bc->Bc1.val[1] + 2;
    total_byte += bc->Bc1.val[1] + 2;
        
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Parse CCP");
    return total_byte;    
}

//******************************************************************************
//
// Function Name: ParseSubAddress
//
// Description:   Parse Sub-Address (refer to 11.14 section 12.31)
//
// Notes:
//
//******************************************************************************
UInt16 ParseSubAddress(UInt8 *simple_tlv_ptr, Subaddress_t* subAddr)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill SubAddress tag
    tlv_ptr[0] = 0x88;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length
    tlv_ptr[0] = subAddr->c_subaddr + 1; //tos/odd_even
    tlv_ptr += 1;
    total_byte += 1;    

    //Fill tos/odd_even
    tlv_ptr[0] = 0x80 | ((subAddr->tos & 0x07) << 4) | ((subAddr->odd_even & 0x01) << 3);
    tlv_ptr += 1;
    total_byte += 1;

    // Fill subAddress
    memcpy(tlv_ptr, subAddr->subaddr, subAddr->c_subaddr);
    tlv_ptr += subAddr->c_subaddr;
    total_byte += subAddr->c_subaddr;
        
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Parse Subaddress");
    return total_byte;    
}

//******************************************************************************
//
// Function Name: ParseSmsTpdu
//
// Description:   Parse SMS TPDU (refer to 11.14 section 12.31)
//
// Notes:
//
//******************************************************************************
UInt16 ParseSmsTpdu(UInt8 *simple_tlv_ptr, SendMOSMS_t* pMoSms)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    
    // Fill SMS TPDU tag
    tlv_ptr[0] = 0x8B;
    tlv_ptr += 1;
    total_byte += 1;
    
    // Fill length    
	if (pMoSms->sms_data.pdu_len > 127)
  {
    tlv_ptr[0] = 0x81;
        tlv_ptr[1] = pMoSms->sms_data.pdu_len;
        tlv_ptr += 2;
        total_byte += 2;
  }
    else
    {
        tlv_ptr[0] = pMoSms->sms_data.pdu_len;
        tlv_ptr += 1;
        total_byte += 1;    
    }
        
    // Fill SMS TPDU
	memcpy(tlv_ptr, pMoSms->sms_data.pdu_data, pMoSms->sms_data.pdu_len);

    tlv_ptr += pMoSms->sms_data.pdu_len;
    total_byte += pMoSms->sms_data.pdu_len;
        
    //RawDataPrintfun(simple_tlv_ptr, total_byte, "Parse SMS TPDU");
    return total_byte;    
}


//******************************************************************************
//
// Function Name: PaeseEventList
//
// Description:   Parse Event List (refer to 11.14 section 12.25)
//
// Notes:
//
//******************************************************************************
UInt16 PaeseEventList(UInt8 *simple_tlv_ptr, UInt16* pEventType)
{
    UInt16 total_byte  = 0;
    UInt8  *tlv_ptr = simple_tlv_ptr;
    UInt8  *tlv_len_ptr = NULL;
    
    // Fill SMS TPDU tag
    tlv_ptr[0] = 0x99;
    tlv_ptr += 1;
    total_byte += 1;
    
    if (0 == *pEventType)
    {
        tlv_ptr[0] = 0;
        tlv_ptr += 1;
        total_byte += 1; 
        
        return total_byte;       
    }
    
    // Fill length
    tlv_ptr[0] = 0;
    tlv_len_ptr = &tlv_ptr[0];
    tlv_ptr += 1;
    total_byte += 1;
    
    if (*pEventType & BIT_MASK_MT_CALL_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 0;
        tlv_ptr += 1;
        total_byte += 1;
    }

    if (*pEventType & BIT_MASK_CALL_CONNECTED_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 1;
        tlv_ptr += 1;
        total_byte += 1;
    }

    if (*pEventType & BIT_MASK_CALL_DISCONNECTED_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 2;
        tlv_ptr += 1;
        total_byte += 1;
    }

    if (*pEventType & BIT_MASK_LOCATION_STATUS_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 3;
        tlv_ptr += 1;
        total_byte += 1;
    }    

    if (*pEventType & BIT_MASK_USER_ACTIVITY_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 4;
        tlv_ptr += 1;
        total_byte += 1;
    }   

    if (*pEventType & BIT_MASK_IDLE_SCREEN_AVAIL_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 5;
        tlv_ptr += 1;
        total_byte += 1;
    }  

    if (*pEventType & BIT_MASK_LAUGUAGE_SELECTION_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 7;
        tlv_ptr += 1;
        total_byte += 1;
    }  

    if (*pEventType & BIT_MASK_BROWSER_TERMINATION_EVENT)
    {
        *tlv_len_ptr += 1;
        tlv_ptr[0] = 8;
        tlv_ptr += 1;
        total_byte += 1;
    }
    
    return total_byte;  
}


//******************************************************************************
//
// Function Name: PaeseLanguage
//
// Description:   Parse Event List (refer to 11.14 section 12.25)
//
// Notes:
//
//******************************************************************************
UInt16 ParseLanguage(UInt8 *simple_tlv_ptr, StkLangNotification_t* planguage)
{
    UInt8 language[4] = {0xAD, 0x02, 0x00, 0x00};
    UInt16 language_length = 4;

	if(planguage->language[0] == '\0'){
		return 0;  
	}
	
    language[2]= planguage->language[0];
	language[3]= planguage->language[1];

    memcpy(simple_tlv_ptr, language, language_length);
    return language_length;  
}


//******************************************************************************
//
// Function Name: ParseTlvData
//
// Description:   Parse TLV Data(refer to 11.14 section 13.2 and Annex D)
//
// Notes:
//
//******************************************************************************
UInt16 ParseTlvData(char *tlv_data_string, UInt8 *simple_tlv_objects, UInt16 simple_tlv_length)
{
    UInt8 tlv_object[MAX_TLV_DATA_LENGTH];
    UInt8* tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    
    tlv_ptr = tlv_object;
    
    // Fill the Proactive command tag
    tlv_ptr[0] = 0xD0;
    tlv_ptr += 1;
    tlv_length += 1;
    
    // Fill the Length
    if (simple_tlv_length > MAX_SIMPLE_TLV_DATA_LENGTH)
    {
        KRIL_DEBUG(DBG_ERROR,"simple_tlv_length:%d is too large Error!!\n",simple_tlv_length);
        return 0;
    }
    
    if (simple_tlv_length > 127)
    {
        tlv_ptr[0] = 0x81;
        tlv_ptr[1] = simple_tlv_length;
        tlv_ptr += 2;
        tlv_length += 2;
    }
    else
    {
        tlv_ptr[0] = simple_tlv_length;
        tlv_ptr += 1;
        tlv_length += 1;
    }
    
    // Fill the TLV data objects
    memcpy(tlv_ptr, simple_tlv_objects, simple_tlv_length);
    tlv_length += simple_tlv_length;
    RawDataPrintfun(tlv_object, tlv_length, "TLV");
    
    // Convert the Hex data to Hex string
    HexDataToHexStr(tlv_data_string, tlv_object, tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKSetupMenu
//
// Description:   Parse the SATK Setup Menu(refer to 11.14 section 6.6.7)
//
// Notes:
//
//******************************************************************************
UInt16 ParseSATKSetupMenu(char* tlv_data_string, SetupMenu_t *pSetupMenu)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 itemdata_length = 0;
    UInt16 nextactionindicator_length = 0;
    UInt16 iconidentifier_length = 0;
    UInt16 iconidentifierlist_length = 0;
    
    KRIL_DEBUG(DBG_INFO,"listSize:%d isHelpAvailable:%d title.unicode_type:0x%02X len:%d pNextActIndList.IsExist:%d\n",
        pSetupMenu->listSize,
        pSetupMenu->isHelpAvailable,
        pSetupMenu->title.unicode_type,
        pSetupMenu->title.len,
        pSetupMenu->pNextActIndList.IsExist
        );
    
    KRIL_DEBUG(DBG_INFO,"titleIcon.IsExist:%d Id:%d IsSelfExplanatory:%d pIconList.IsExist:%d Id[0]:%d Id[1]:%d Id[2]:%d IsSelfExplanatory:%d\n",
        pSetupMenu->titleIcon.IsExist,
        pSetupMenu->titleIcon.Id,
        pSetupMenu->titleIcon.IsSelfExplanatory,
        pSetupMenu->pIconList.IsExist,
        pSetupMenu->pIconList.Id[0],
        pSetupMenu->pIconList.Id[1],
        pSetupMenu->pIconList.Id[2],
        pSetupMenu->pIconList.IsSelfExplanatory
        );

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pSetupMenu, STK_SETUPMENU);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;
    
    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SETUPMENU);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;
    
    // Parse Alpha identifier data    
    if (pSetupMenu->title.len > 0)
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pSetupMenu->title);
        if (!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += alphaidentifier_length;
    }
    else
    {
        // Set default SIM Menu title as "SIM MENU".
        UInt8 default_title[] = {0x53, 0x49, 0x4D, 0x20, 0x4D, 0x45, 0x4E, 0x55};
        UInt16 default_title_len = sizeof(default_title)/sizeof(UInt8);
        
        simple_tlv_ptr[0] = 0x85;
        simple_tlv_ptr[1] = default_title_len;
        simple_tlv_ptr += 2;
        
        memcpy(simple_tlv_ptr, default_title, default_title_len);
        simple_tlv_ptr += default_title_len;
        alphaidentifier_length = default_title_len + 2;
    }
    
    // Parse item data object
    itemdata_length = ParseItemData(simple_tlv_ptr, pSetupMenu->listSize, pSetupMenu->pItemIdList,
        pSetupMenu->pItemList);
    if (!itemdata_length)
    {
       KRIL_DEBUG(DBG_ERROR,"ParseItemData() return 0 Error!!\n");
       return 0;
    }
    simple_tlv_ptr += itemdata_length;
    
    // Parse items Next Action Indicator
    if (pSetupMenu->pNextActIndList.IsExist)
    {
        nextactionindicator_length = ParseItemsNextActionIndicator(simple_tlv_ptr, pSetupMenu->listSize, 
            pSetupMenu->pNextActIndList);
        simple_tlv_ptr += nextactionindicator_length;
    }
    
    // Parse Icon Identifier
    if (pSetupMenu->titleIcon.IsExist)
    {
	StkIcon = TRUE; // gearn not support icon TR

	
        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pSetupMenu->titleIcon);
        simple_tlv_ptr += iconidentifier_length;
    }
    
    // Parse Item Icon Identifier List
    if (pSetupMenu->pIconList.IsExist)
    {
        iconidentifierlist_length = ParseIconIdentifierList(simple_tlv_ptr, pSetupMenu->listSize, pSetupMenu->pIconList);
        simple_tlv_ptr += iconidentifierlist_length;
    }
    
    // Decide the Length
    simple_tlv_length = command_detail_length + device_identities_length + alphaidentifier_length 
        + itemdata_length + nextactionindicator_length + iconidentifier_length + iconidentifierlist_length;
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);

    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKSelectItem
//
// Description:   Parse the SATK Select Item(refer to 11.14 section 6.6.8)
//
// Notes:
//
//******************************************************************************
UInt16 ParseSATKSelectItem(char* tlv_data_string, SelectItem_t *pSelectItem)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 itemdata_length = 0;
    UInt16 nextactionindicator_length = 0;
    UInt16 itemidentifier_length = 0;
    UInt16 iconidentifier_length = 0;
    UInt16 iconidentifierlist_length = 0;

    KRIL_DEBUG(DBG_INFO,"listSize:%d isAlphaIdProvided:%d isHelpAvailable:%d defaultItem:%d\n",
        pSelectItem->listSize,
        pSelectItem->isAlphaIdProvided,
        pSelectItem->isHelpAvailable,
        pSelectItem->defaultItem
        );
    
    KRIL_DEBUG(DBG_INFO,"title.unicode_type:0x%02X len:%d titleIcon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
        pSelectItem->title.unicode_type,
        pSelectItem->title.len,
        pSelectItem->titleIcon.IsExist,
        pSelectItem->titleIcon.Id,
        pSelectItem->titleIcon.IsSelfExplanatory
        );

    KRIL_DEBUG(DBG_INFO,"pIconList.IsExist:%d Id[0]:%d Id[1]:%d Id[2]:%d IsSelfExplanatory:%d\n",
        pSelectItem->pIconList.IsExist,
        pSelectItem->pIconList.Id[0],
        pSelectItem->pIconList.Id[1],
        pSelectItem->pIconList.Id[2],
        pSelectItem->pIconList.IsSelfExplanatory
        );
        
    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pSelectItem, STK_SELECTITEM);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SELECTITEM);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Parse Alpha identifier data    
    if (pSelectItem->title.len > 0)
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pSelectItem->title);
        if (!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += alphaidentifier_length;
    }

    // Parse item data object
    itemdata_length = ParseItemData(simple_tlv_ptr, pSelectItem->listSize, pSelectItem->pItemIdList, pSelectItem->pItemList);
    if (!itemdata_length)
    {
       KRIL_DEBUG(DBG_ERROR,"ParseItemData() return 0 Error!!\n");
       return 0;
    }
    simple_tlv_ptr += itemdata_length;
                    
    // Parse items Next Action Indicator
    if (pSelectItem->pNextActIndList.IsExist)
    {
        nextactionindicator_length = ParseItemsNextActionIndicator(simple_tlv_ptr, pSelectItem->listSize, 
            pSelectItem->pNextActIndList);
        simple_tlv_ptr += nextactionindicator_length;
    }
    
    // Parse Item Identifier
    if (pSelectItem->defaultItem)
    {
        itemidentifier_length = ParseItemIdentifier(simple_tlv_ptr, pSelectItem->defaultItem);
        simple_tlv_ptr += itemidentifier_length;
    }
    
    // Parse Icon Identifier
    if (pSelectItem->titleIcon.IsExist)
    {
    	StkIcon = TRUE; // gearn not support icon TR
        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pSelectItem->titleIcon);
        simple_tlv_ptr += iconidentifier_length;
    }
    
    // Parse Item Icon Identifier List
    if (pSelectItem->pIconList.IsExist)
    {
        iconidentifierlist_length = ParseIconIdentifierList(simple_tlv_ptr, pSelectItem->listSize, pSelectItem->pIconList);
        simple_tlv_ptr += iconidentifierlist_length;
    }
                        
    // Decide the Length
    simple_tlv_length = command_detail_length + device_identities_length + alphaidentifier_length 
        + itemdata_length + nextactionindicator_length + itemidentifier_length + iconidentifier_length
        + iconidentifierlist_length;
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKGetInput
//
// Description:   Parse the SATK Get Input(refer to 11.14 section 6.6.3)
//
// Notes:
//
//******************************************************************************
UInt16 ParseSATKGetInput(char* tlv_data_string, GetInput_t *pGetInput)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 text_string_length = 0;
    UInt16 defaultext_string_length = 0;
    UInt16 responselen_length = 0;
    UInt16 iconidentifier_length = 0;

    KRIL_DEBUG(DBG_INFO,"minLen:%d maxLen:%d inPutType:%d isHelpAvailable:%d isEcho:%d isPacked:%d stkStr.unicode_type:0x%02X len:%d\n",
        pGetInput->minLen,
        pGetInput->maxLen,
        pGetInput->inPutType,
        pGetInput->isHelpAvailable,
        pGetInput->isEcho,
        pGetInput->isPacked,
        pGetInput->stkStr.unicode_type,
        pGetInput->stkStr.len
        );
    DisplayStkTextString(pGetInput->stkStr);
    
    KRIL_DEBUG(DBG_INFO,"defaultSATKStr.unicode_type:0x%02X len:%d icon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
        pGetInput->defaultSATKStr.unicode_type,
        pGetInput->defaultSATKStr.len,
        pGetInput->icon.IsExist,
        pGetInput->icon.Id,
        pGetInput->icon.IsSelfExplanatory
        );
    DisplayStkTextString(pGetInput->defaultSATKStr);
    
    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pGetInput, STK_GETINPUT);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_GETINPUT);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Fill Text string
    text_string_length = ParseTextString(simple_tlv_ptr, pGetInput->stkStr);
    if (!text_string_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseTextString() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += text_string_length;
    
    // Fill Response length
    responselen_length = ParseResponseLength(simple_tlv_ptr, pGetInput->minLen, pGetInput->maxLen);
    if (!responselen_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseResponseLength() return 0 Error!!\n");
       //return 0;  gearn get input no presponse length
    }
    simple_tlv_ptr += responselen_length;
    
    // Fill default text
    if (pGetInput->defaultSATKStr.len > 0)
    {
        defaultext_string_length = ParseDefaultTextString(simple_tlv_ptr, pGetInput->defaultSATKStr);
        simple_tlv_ptr += defaultext_string_length;
    }

    // Parse Icon Identifier
    if (pGetInput->icon.IsExist)
    {
    	StkIcon = TRUE; // gearn not support icon TR

        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pGetInput->icon);
        simple_tlv_ptr += iconidentifier_length;
    }
        
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + text_string_length
        + defaultext_string_length + responselen_length + iconidentifier_length; 
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKGetInkey
//
// Description:   Parse the SATK Get Inkey(refer to 11.14 section 6.6.2)
//
// Notes:
//
//******************************************************************************
int ParseSATKGetInkey(char* tlv_data_string, GetInkey_t *pGetInkey)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 text_string_length = 0;
    UInt16 iconidentifier_length = 0;
    
    KRIL_DEBUG(DBG_INFO,"unicode_type:0x%02X len:%d icon.IsExist:%d Id:%d IsSelfExplanatory:%d inKeyType:%d isHelpAvailable:%d\n",
        pGetInkey->stkStr.unicode_type, 
        pGetInkey->stkStr.len,
        pGetInkey->icon.IsExist,
        pGetInkey->icon.Id,
        pGetInkey->icon.IsSelfExplanatory,
        pGetInkey->inKeyType, 
        pGetInkey->isHelpAvailable);
            
    DisplayStkTextString(pGetInkey->stkStr);    

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pGetInkey, STK_GETINKEY);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_GETINKEY);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Fill Text string
    text_string_length = ParseTextString(simple_tlv_ptr, pGetInkey->stkStr);
    if (!text_string_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseTextString() return 0 Error!!\n");
        //return 0; gearn SSTC get inkey issue
    }
    simple_tlv_ptr += text_string_length;

    // Parse Icon Identifier
    if (pGetInkey->icon.IsExist)
    {
    	 StkIcon = TRUE; // gearn not support icon TR
        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pGetInkey->icon);
        simple_tlv_ptr += iconidentifier_length;
    }
    
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + text_string_length 
        + iconidentifier_length;
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
            
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKDisplayText
//
// Description:   Parse the SATK Display Text(refer to 11.14 section 6.6.1)
//
// Notes:
//
//******************************************************************************
int ParseSATKDisplayText(char* tlv_data_string, DisplayText_t *pDisplayText)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 text_string_length = 0;
    UInt16 iconidentifier_length = 0;
    UInt16 immediate_response_length = 0;

    KRIL_DEBUG(DBG_INFO,"isHighPrio:%d isDelay:%d isSustained:%d stkStr.unicode_type:0x%02X len:%d\n",
        pDisplayText->isHighPrio,
        pDisplayText->isDelay,
        pDisplayText->isSustained,
        pDisplayText->stkStr.unicode_type,
        pDisplayText->stkStr.len
        );

    DisplayStkTextString(pDisplayText->stkStr);
    
    KRIL_DEBUG(DBG_INFO,"icon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
        pDisplayText->icon.IsExist,
        pDisplayText->icon.Id,
        pDisplayText->icon.IsSelfExplanatory
        );
        
    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pDisplayText, STK_DISPLAYTEXT);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_DISPLAYTEXT);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Fill Text string
    text_string_length = ParseTextString(simple_tlv_ptr, pDisplayText->stkStr);
    if (!text_string_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseTextString() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += text_string_length;

    // Parse Icon Identifier
    if (pDisplayText->icon.IsExist)
    {
    	 StkIcon = TRUE; // gearn not support icon TR
        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pDisplayText->icon);
        simple_tlv_ptr += iconidentifier_length;
    }
        
    // Fill Immediate response
    if (pDisplayText->isSustained)
    {
        ParseImmediateResponse(simple_tlv_ptr);
        immediate_response_length = 2;
        simple_tlv_ptr += immediate_response_length;
    }
    
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + text_string_length
        + iconidentifier_length + immediate_response_length;
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKSendMOSMS
//
// Description:   Parse the SATK Send MO SMS(refer to 11.14 section 6.6.9)
//
// Notes:
//
//******************************************************************************
int ParseSATKSendMOSMS(char* tlv_data_string, SendMOSMS_t *pSendMOSMS)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 iconidentifier_length = 0;
    UInt16 address_length = 0;
    UInt16 tpdu_len = 0;
    
    KRIL_DEBUG(DBG_INFO,"isAlphaIdProvided:%d text.unicode_type:0x%02X len:%d icon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
        pSendMOSMS->isAlphaIdProvided,
        pSendMOSMS->text.unicode_type,
        pSendMOSMS->text.len,
        pSendMOSMS->icon.IsExist,
        pSendMOSMS->icon.Id,
        pSendMOSMS->icon.IsSelfExplanatory
        );
    
    KRIL_DEBUG(DBG_INFO,"sms_data.sca_ton_npi:%d sca_len:%d pdu_len:%d\n",
        pSendMOSMS->sms_data.sca_ton_npi,
        pSendMOSMS->sms_data.sca_len,
        pSendMOSMS->sms_data.pdu_len
        );

    DisplayStkTextString(pSendMOSMS->text);

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pSendMOSMS, STK_SENDSMS);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SENDSMS);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Parse Alpha identifier data
	// Skip Alpha ID TLV if default is supplied
    if (!((pSendMOSMS->isAlphaIdProvided == FALSE) && (pSendMOSMS->text.len > 0)))
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pSendMOSMS->text);
        if(!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += alphaidentifier_length;
    }    

    // Parse SMS Address
    if (pSendMOSMS->sms_data.sca_len > 0)
    {
        address_length = ParseSmsAddress(simple_tlv_ptr, pSendMOSMS);
        if (!address_length)
        {
          KRIL_DEBUG(DBG_ERROR,"ParseSmsAddress() return 0 Error!!\n");
          return 0;
        }
        
        simple_tlv_ptr += address_length;
    }
    
    //Parse SMS TPDU
    if (pSendMOSMS->sms_data.pdu_len > 0)
    {
        tpdu_len = ParseSmsTpdu(simple_tlv_ptr, pSendMOSMS);
        simple_tlv_ptr += tpdu_len;
    }

    // Parse Icon Identifier
    if (pSendMOSMS->icon.IsExist)
    {
    	 StkIcon = TRUE; // gearn not support icon TR
        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pSendMOSMS->icon);
        simple_tlv_ptr += iconidentifier_length;
    }
        
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + alphaidentifier_length
        + iconidentifier_length + address_length + tpdu_len;
    
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKSendSs
//
// Description:   Parse the SATK Send SS(refer to 11.14 section 6.6.10)
//
// Notes:
//
//******************************************************************************
int ParseSATKSendSs(char* tlv_data_string, SendSs_t *pSendSs)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 iconidentifier_length = 0;
    UInt16 ss_string_length = 0;
    
    KRIL_DEBUG(DBG_INFO,"ssType:%d num.Num:%s Ton:%d Npi:%d dcs:0x%X len:%d\n",
        pSendSs->ssType,
        pSendSs->num.Num, 
        pSendSs->num.Ton, 
        pSendSs->num.Npi, 
        pSendSs->num.dcs,
        pSendSs->num.len
        );
    
    KRIL_DEBUG(DBG_INFO,"isAlphaIdProvided:%d icon.IsExist:%d Id:%d IsSelfExplanatory:%d text.unicode_type:0x%02X len:%d\n",
        pSendSs->isAlphaIdProvided, 
        pSendSs->icon.IsExist,
        pSendSs->icon.Id,
        pSendSs->icon.IsSelfExplanatory, 
        pSendSs->text.unicode_type,
        pSendSs->text.len
        );

    DisplayStkTextString(pSendSs->text);    

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pSendSs, STK_SENDSS);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SENDSS);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Parse Alpha identifier data
	// Skip Alpha ID TLV if default is supplied
    if (!((pSendSs->isAlphaIdProvided == FALSE) && (pSendSs->text.len > 0)))    
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pSendSs->text);
        if(!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += alphaidentifier_length;
    }    

    // Parse Icon Identifier
    if (pSendSs->icon.IsExist)
    {
         StkIcon = TRUE; // gearn not support icon TR

        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pSendSs->icon);
        simple_tlv_ptr += iconidentifier_length;
    }
    
    // Parse SS String
    if (pSendSs->num.len > 0)
    {
        ss_string_length = ParseSsString(simple_tlv_ptr, &pSendSs->num);
        simple_tlv_ptr += ss_string_length;
    }

    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + alphaidentifier_length
        + iconidentifier_length + ss_string_length;
    
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKSendUssd
//
// Description:   Parse the SATK Send USSd(refer to 11.14 section 6.6.11)
//
// Notes:
//
//******************************************************************************
int ParseSATKSendUssd(char* tlv_data_string, SendUssd_t *pSendUssd)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 iconidentifier_length = 0;
    UInt16 ussd_string_length = 0;
    
    KRIL_DEBUG(DBG_INFO,"ssType:%d num.Num:%s Ton:%d Npi:%d dcs:0x%X len:%d\n",
        pSendUssd->ssType, 
        pSendUssd->num.Num, 
        pSendUssd->num.Ton, 
        pSendUssd->num.Npi, 
        pSendUssd->num.dcs, 
        pSendUssd->num.len);
    
    KRIL_DEBUG(DBG_INFO,"isAlphaIdProvided:%d icon.IsExist:%d Id:%d IsSelfExplanatory:%d text.unicode_type:0x%02X len:%d\n",
        pSendUssd->isAlphaIdProvided, 
        pSendUssd->icon.IsExist,
        pSendUssd->icon.Id, 
        pSendUssd->icon.IsSelfExplanatory, 
        pSendUssd->text.unicode_type,
        pSendUssd->text.len
        );

    DisplayStkTextString(pSendUssd->text);

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pSendUssd, STK_SENDUSSD);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SENDUSSD);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Parse Alpha identifier data
    // Skip Alpha ID TLV if default is supplied
    if (!((pSendUssd->isAlphaIdProvided == FALSE) && (pSendUssd->text.len > 0)))        
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pSendUssd->text);
        if (!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += alphaidentifier_length;
    }    

    // Parse Icon Identifier
    if (pSendUssd->icon.IsExist)
    {
    	 StkIcon = TRUE; // gearn not support icon TR
        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pSendUssd->icon);
        simple_tlv_ptr += iconidentifier_length;
    }

    //Parse USSD String
    if (pSendUssd->num.len > 0)
    {
        ussd_string_length = ParseUssdString(simple_tlv_ptr, &pSendUssd->num);
        simple_tlv_ptr += ussd_string_length;
    }

    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + alphaidentifier_length
        + iconidentifier_length + ussd_string_length;
    
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKPlayTone
//
// Description:   Parse the SATK Play Tone(refer to 11.14 section 6.6.5)
//
// Notes:
//
//******************************************************************************
int ParseSATKPlayTone(char* tlv_data_string, PlayTone_t *pPlayTone)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 tone_length = 0;
    UInt16 duration_length = 0;
    UInt16 iconidentifier_length = 0;

    KRIL_DEBUG(DBG_INFO,"defaultStr:%d stkStr.unicode_type:0x%02X len:%d toneType:0x%02X duration:%lu icon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
        pPlayTone->defaultStr,
        pPlayTone->stkStr.unicode_type,
        pPlayTone->stkStr.len,
        pPlayTone->toneType,
        pPlayTone->duration,
        pPlayTone->icon.IsExist,
        pPlayTone->icon.Id,
        pPlayTone->icon.IsSelfExplanatory
        );

    DisplayStkTextString(pPlayTone->stkStr);

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pPlayTone, STK_PLAYTONE);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_PLAYTONE);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Parse Alpha identifier data
    if (FALSE == pPlayTone->defaultStr)
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pPlayTone->stkStr);
        if(!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += alphaidentifier_length;
    }
    
    // Parse Tone
    tone_length = ParseTone(simple_tlv_ptr, pPlayTone->toneType);
    simple_tlv_ptr += tone_length;
    
    // Parse Duration
    if (tone_length > 0 && pPlayTone->duration > 0)
    {
        duration_length = ParseDuration(simple_tlv_ptr, pPlayTone->duration);
        simple_tlv_ptr += duration_length;
    }

    // Parse Icon Identifier
    if (pPlayTone->icon.IsExist)
    {
        StkIcon = TRUE; // gearn not support icon TR

        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pPlayTone->icon);
        simple_tlv_ptr += iconidentifier_length;
    }
        
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + alphaidentifier_length
        + tone_length + duration_length + iconidentifier_length;
    
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKSendStkDtmf
//
// Description:   Parse the SATK Send STK DTMF(refer to 11.14 section 6.6.24)
//
// Notes:
//
//******************************************************************************
int ParseSATKSendStkDtmf(char* tlv_data_string, SendStkDtmf_t *pSendStkDtmf)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 iconidentifier_length = 0;
    
    KRIL_DEBUG(DBG_INFO,"isAlphaIdProvided:%d alphaString.unicode_type:0x%02X len:%d dtmfIcon.IsExist:%d Id:%d IsSelfExplanatory:%d dtmf:%s\n",
        pSendStkDtmf->isAlphaIdProvided,
        pSendStkDtmf->alphaString.unicode_type,
        pSendStkDtmf->alphaString.len,
        pSendStkDtmf->dtmfIcon.IsExist,
        pSendStkDtmf->dtmfIcon.Id,
        pSendStkDtmf->dtmfIcon.IsSelfExplanatory,
        (char*)pSendStkDtmf->dtmf
        );

    DisplayStkTextString(pSendStkDtmf->alphaString);

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pSendStkDtmf, STK_SENDDTMF);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SENDDTMF);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Parse Alpha identifier data
	// Skip Alpha ID TLV if default is supplied
    if (!((pSendStkDtmf->isAlphaIdProvided == FALSE) && (pSendStkDtmf->alphaString.len > 0)))    
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pSendStkDtmf->alphaString);
        if(!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += alphaidentifier_length;
    }

    // Parse Icon Identifier
    if (pSendStkDtmf->dtmfIcon.IsExist)
    {
        StkIcon = TRUE; // gearn not support icon TR

        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pSendStkDtmf->dtmfIcon);
        simple_tlv_ptr += iconidentifier_length;
    }
        
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + alphaidentifier_length
        + iconidentifier_length;
    
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKSetupCall
//
// Description:   Parse the SATK Setup Call(refer to 11.14 section 6.6.12)
//
// Notes:
//
//******************************************************************************
int ParseSATKSetupCall(char* tlv_data_string, SetupCall_t *pSetupCall)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 confirmPhaseStr_length = 0;
    UInt16 setupPhaseStr_length = 0;
    UInt16 address_length = 0;
    UInt16 confirmPhaseIcon_length = 0;
    UInt16 setupPhaseIcon_length = 0;
    UInt16 ccp_length = 0;
    UInt16 subaddress_length = 0;
    UInt16 duration_length = 0;

    KRIL_DEBUG(DBG_INFO,"isEmerCall:%d callType:%d num.Ton:%d Npi:%d Num:%s dcs:%d len:%d duration:%ld IsConfirm:%d IsSetup:%d\n",
        pSetupCall->isEmerCall,
        pSetupCall->callType,
        pSetupCall->num.Ton,
        pSetupCall->num.Npi,
        pSetupCall->num.Num,
        pSetupCall->num.dcs,
        pSetupCall->num.len,
        pSetupCall->duration,
        pSetupCall->IsConfirmAlphaIdProvided,
        pSetupCall->IsSetupAlphaIdProvided
        );
    
    if (pSetupCall->IsConfirmAlphaIdProvided)
    {
        KRIL_DEBUG(DBG_INFO,"ConfirmAlphaIdProvided: confirmPhaseStr.unicode_type:0x%02X len:%d confirmPhaseIcon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
            pSetupCall->confirmPhaseStr.unicode_type,
            pSetupCall->confirmPhaseStr.len,
            pSetupCall->confirmPhaseIcon.IsExist,
            pSetupCall->confirmPhaseIcon.Id,
            pSetupCall->confirmPhaseIcon.IsSelfExplanatory
            );
    
        DisplayStkTextString(pSetupCall->confirmPhaseStr);
    }
    
    if (pSetupCall->IsSetupAlphaIdProvided)
    {
        KRIL_DEBUG(DBG_INFO,"SetupAlphaIdProvided: setupPhaseStr.unicode_type:0x%02X len:%d setupPhaseIcon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
            pSetupCall->setupPhaseStr.unicode_type,
            pSetupCall->setupPhaseStr.len,
            pSetupCall->setupPhaseIcon.IsExist,
            pSetupCall->setupPhaseIcon.Id,
            pSetupCall->setupPhaseIcon.IsSelfExplanatory
            );
    
        DisplayStkTextString(pSetupCall->setupPhaseStr);
    }

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pSetupCall, STK_SETUPCALL);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SETUPCALL);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Confirm Phase Alpha identifier data
    if (pSetupCall->IsConfirmAlphaIdProvided)
    {
        confirmPhaseStr_length = ParseAlphaIdentifier(simple_tlv_ptr, pSetupCall->confirmPhaseStr);
        if(!confirmPhaseStr_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += confirmPhaseStr_length;
    }
    
    // Parse Address
    address_length = ParseAddress(simple_tlv_ptr, &pSetupCall->num);
    if (!address_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseAddress() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += address_length;

    //Parse Cability Configuration Parameters
    if (pSetupCall->bc.Bc1.val[1] > 0)
    {
        ccp_length = ParseCcp(simple_tlv_ptr, &pSetupCall->bc);
        simple_tlv_ptr += ccp_length;
    }

    //Parse Subaddress
    if (pSetupCall->subAddr.c_subaddr > 0)
    {
        subaddress_length = ParseSubAddress(simple_tlv_ptr, &pSetupCall->subAddr);
        simple_tlv_ptr += subaddress_length;
    }

    //Parse Duration
    if (pSetupCall->duration > 0)
    {
        duration_length = ParseDuration(simple_tlv_ptr, pSetupCall->duration);
        simple_tlv_ptr += duration_length;
    }

    // Parse confirm Phase Icon Identifier
    if (pSetupCall->confirmPhaseIcon.IsExist)
    {
        StkIcon = TRUE; // gearn not support icon TR

        confirmPhaseIcon_length = ParseIconIdentifier(simple_tlv_ptr, pSetupCall->confirmPhaseIcon);
        simple_tlv_ptr += confirmPhaseIcon_length;
    }

    // Setup Phase Alpha identifier data
    if (pSetupCall->IsSetupAlphaIdProvided)
    {
        setupPhaseStr_length = ParseAlphaIdentifier(simple_tlv_ptr, pSetupCall->setupPhaseStr);
        if (!setupPhaseStr_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        
        simple_tlv_ptr += setupPhaseStr_length;
    }
    
    // Parse setup Phase Icon Identifier
    if (pSetupCall->setupPhaseIcon.IsExist)
    {
        StkIcon = TRUE; // gearn not support icon TR

        setupPhaseIcon_length = ParseIconIdentifier(simple_tlv_ptr, pSetupCall->setupPhaseIcon);
        simple_tlv_ptr += setupPhaseIcon_length;
    }
            
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + confirmPhaseStr_length
       + address_length + confirmPhaseIcon_length + setupPhaseStr_length + setupPhaseIcon_length
       + ccp_length + duration_length + subaddress_length;
    
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKIdleModeText
//
// Description:   Parse the SATK Idle Mode Text(refer to 11.14 section 6.6.22)
//
// Notes:
//
//******************************************************************************
int ParseSATKIdleModeText(char* tlv_data_string, IdleModeText_t *pIdleModeText)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 text_string_length = 0;
    UInt16 iconidentifier_length = 0;

    KRIL_DEBUG(DBG_INFO,"stkStr.len:%d unicode_type:0x%X icon.IsExist:%d Id:%d IsSelfExplanatory:%d\n",
        pIdleModeText->stkStr.len, 
        pIdleModeText->stkStr.unicode_type, 
        pIdleModeText->icon.IsExist,
        pIdleModeText->icon.Id,
        pIdleModeText->icon.IsSelfExplanatory
        );

    DisplayStkTextString(pIdleModeText->stkStr);

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pIdleModeText, STK_SETUPIDLEMODETEXT);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_SETUPIDLEMODETEXT);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;

    // Fill Text string
    text_string_length = ParseTextString(simple_tlv_ptr, pIdleModeText->stkStr);
    if (!text_string_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseTextString() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += text_string_length;

    // Parse Icon Identifier
    if (pIdleModeText->icon.IsExist)
    {
        StkIcon = TRUE; // gearn not support icon TR

        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pIdleModeText->icon);
        simple_tlv_ptr += iconidentifier_length;
    }
    
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + text_string_length
        + iconidentifier_length;
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKRefresh
//
// Description:   Parse the SATK Refresh(refer to 11.14 section 6.6.13)
//
// Notes:
//
//******************************************************************************
int ParseSATKRefresh(char* tlv_data_string, Refresh_t *pRefresh)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 filelist_length = 0;

    KRIL_DEBUG(DBG_INFO,"refreshType:%d appliType:%d FileIdList.number_of_file:%d\n",
        pRefresh->refreshType,
        pRefresh->appliType,
        pRefresh->FileIdList.number_of_file
        );

    simple_tlv_ptr = simple_tlv_objects;
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pRefresh, STK_REFRESH);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_REFRESH);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;
    
    // Fill File list
    if (pRefresh->FileIdList.number_of_file > 0)
    {
        filelist_length = ParseFileList(simple_tlv_ptr, &pRefresh->FileIdList);
        if (!filelist_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseFileList() return 0 Error!!\n");
            return 0;
        }
        simple_tlv_ptr += filelist_length;
    }

    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + filelist_length;
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}


//******************************************************************************
//
// Function Name: ParseSATKLaunchBrowser
//
// Description:   Parse the SATK Launch Browser(refer to 11.14 section 6.6.26)
//
// Notes:
//
//******************************************************************************
int ParseSATKLaunchBrowser(char* tlv_data_string, LaunchBrowserReq_t *pLaunchBrowserReq)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 browser_id_length = 0;
    UInt16 url_length = 0;
    UInt16 bear_length = 0;
    UInt16 provision_length = 0;
    UInt16 text_string_length = 0;
    UInt16 alphaidentifier_length = 0;
    UInt16 iconidentifier_length = 0;

    KRIL_DEBUG(DBG_INFO,"browser_action:%d browser_id_exist:%d browser_id:%d url:\"%s\"\r\n",
        pLaunchBrowserReq->browser_action,
        pLaunchBrowserReq->browser_id_exist,
        pLaunchBrowserReq->browser_id,
        pLaunchBrowserReq->url
        );

    KRIL_DEBUG(DBG_INFO,"bearer_length:%d bearer[0]:0x%02X prov_length:%d text.len:%d unicode_type:0x%X\r\n",
        pLaunchBrowserReq->bearer_length,
        pLaunchBrowserReq->bearer[0],
        pLaunchBrowserReq->prov_length,
        pLaunchBrowserReq->text.len,
        pLaunchBrowserReq->text.unicode_type
        );

    KRIL_DEBUG(DBG_INFO,"default_alpha_id:%d alpha_id.len:%d unicode_type:0x%X icon_id.IsExist:%d Id:%d IsSelfExplanatory:%d\r\n",
        pLaunchBrowserReq->default_alpha_id,
        pLaunchBrowserReq->alpha_id.len,
        pLaunchBrowserReq->alpha_id.unicode_type,
        pLaunchBrowserReq->icon_id.IsExist,
        pLaunchBrowserReq->icon_id.Id,
        pLaunchBrowserReq->icon_id.IsSelfExplanatory
        );
                
    DisplayStkTextString(pLaunchBrowserReq->text);
    DisplayStkTextString(pLaunchBrowserReq->alpha_id);
    
    simple_tlv_ptr = simple_tlv_objects;    
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, (void*)pLaunchBrowserReq, STK_LAUNCHBROWSER);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_LAUNCHBROWSER);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;
    
    // Fill Browser Identity
    if (pLaunchBrowserReq->browser_id_exist)
    {
        browser_id_length = ParserBrowserIdentity(simple_tlv_ptr, pLaunchBrowserReq->browser_id);
        if (!browser_id_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParserBrowserIdentity() return 0 Error!!\n");
            return 0;
        }
        simple_tlv_ptr += browser_id_length;
    }
    
    // Fill URL
    url_length = ParseURL(simple_tlv_ptr, pLaunchBrowserReq->url);
    if (!url_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseURL() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += url_length;
    
    // Fill Bear
    if (pLaunchBrowserReq->bearer_length > 0)
    {
        bear_length = ParseBear(simple_tlv_ptr, pLaunchBrowserReq);
        if (!bear_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseBear() return 0 Error!!\n");
            return 0;
        }
        simple_tlv_ptr += bear_length;
    }

    // Fill Provisioning File Reference
    if (pLaunchBrowserReq->prov_length > 0)
    {
        provision_length = ParserProvisioning(simple_tlv_ptr, pLaunchBrowserReq);
        if (!provision_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParserProvisioning() return 0 Error!!\n");
            return 0;
        }
        simple_tlv_ptr += provision_length;
    }
    
    // Fill Text string
    if (pLaunchBrowserReq->text.len > 0)
    {
        text_string_length = ParseTextString(simple_tlv_ptr, pLaunchBrowserReq->text);
        if (!text_string_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseTextString() return 0 Error!!\n");
            return 0;
        }
        simple_tlv_ptr += text_string_length;
    }
    
    // Fill Alpha identifier
    if ((pLaunchBrowserReq->alpha_id.len > 0) && (pLaunchBrowserReq->default_alpha_id == FALSE))
    {
        alphaidentifier_length = ParseAlphaIdentifier(simple_tlv_ptr, pLaunchBrowserReq->alpha_id);
        if (!alphaidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseAlphaIdentifier() return 0 Error!!\n");
            return 0;
        }
        simple_tlv_ptr += alphaidentifier_length;        
    }
    
    // Fill Icon identifier
    if (pLaunchBrowserReq->icon_id.IsExist)
    {
        iconidentifier_length = ParseIconIdentifier(simple_tlv_ptr, pLaunchBrowserReq->icon_id);
        if (!iconidentifier_length)
        {
            KRIL_DEBUG(DBG_ERROR,"ParseIconIdentifier() return 0 Error!!\n");
            return 0;
        }        
        simple_tlv_ptr += iconidentifier_length;
    }

    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + browser_id_length +
        + url_length + bear_length + provision_length + text_string_length + alphaidentifier_length +
        + iconidentifier_length;
                 
    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;
}

//******************************************************************************
//
// Function Name: ParseSATKSetupEventList
//
// Description:   Parse the SATK Setup Event List(refer to 11.14 section 6.6.16)
//
// Notes:
//
//******************************************************************************
int ParseSATKSetupEventList(char* tlv_data_string, UInt16* pEventType)
{
    UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
    UInt16 simple_tlv_length = 0;
    UInt8 *simple_tlv_ptr = NULL;
    UInt16 tlv_length = 0;
    UInt16 command_detail_length = 0;
    UInt16 device_identities_length = 0;
    UInt16 event_list_length = 0;
    
    
    KRIL_DEBUG(DBG_ERROR,"EventType: 0x%08X\n", *pEventType);   
    
    simple_tlv_ptr = simple_tlv_objects;    
    
    // Fill Command detail data
    command_detail_length = ParseCommandDetails(simple_tlv_ptr, pEventType, STK_EVENTLIST);
    if (!command_detail_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += command_detail_length;

    // Fill Device identities
    device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_EVENTLIST);
    if (!device_identities_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += device_identities_length;
    
    // Fill Event list
    event_list_length = PaeseEventList(simple_tlv_ptr, pEventType);
    if (!event_list_length)
    {
        KRIL_DEBUG(DBG_ERROR,"PaeseEventList() return 0 Error!!\n");
        return 0;
    }
    simple_tlv_ptr += event_list_length;    
    
    // Decide the total simple TLV Length
    simple_tlv_length = command_detail_length + device_identities_length + event_list_length;

    // Fill the TLV data
    tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);
    
    return tlv_length;       
}


//******************************************************************************
//
// Function Name: ParseSATKLanguageNotification
//
// Description:   Parse the SATK Language Notification(refer to 11.14 section 6.6.25)
//
// Notes:
//
//******************************************************************************
int ParseSATKLanguageNotification(char* tlv_data_string, StkLangNotification_t* plang)
{
	UInt8 simple_tlv_objects[MAX_SIMPLE_TLV_DATA_LENGTH];
	UInt16 simple_tlv_length = 0;
	UInt8 *simple_tlv_ptr = NULL;
	UInt16 tlv_length = 0;
	UInt16 command_detail_length = 0;
	UInt16 device_identities_length = 0;
	UInt16 language_length = 0;

	simple_tlv_ptr = simple_tlv_objects;	

	// Fill Command detail data
	command_detail_length = ParseCommandDetails(simple_tlv_ptr, plang, STK_LANGUAGENOTIFICATION);
	if (!command_detail_length)
	{
		KRIL_DEBUG(DBG_ERROR,"ParseCommandDetails() return 0 Error!!\n");
		return 0;
	}
	simple_tlv_ptr += command_detail_length;

	// Fill Device identities
	device_identities_length = ParseDeviceIdentities(simple_tlv_ptr, STK_LANGUAGENOTIFICATION);
	if (!device_identities_length)
	{
		KRIL_DEBUG(DBG_ERROR,"ParseDeviceIdentities() return 0 Error!!\n");
		return 0;
	}
	
	simple_tlv_ptr += device_identities_length;

	// Fill language list
    language_length = ParseLanguage(simple_tlv_ptr, plang);
    if (!language_length)
    {
        KRIL_DEBUG(DBG_ERROR,"No specific Language");
    }

	simple_tlv_ptr += language_length;	

	// Decide the total simple TLV Length
	simple_tlv_length = command_detail_length + device_identities_length + language_length;

	// Fill the TLV data
	tlv_length = ParseTlvData(tlv_data_string, simple_tlv_objects, simple_tlv_length);

	return tlv_length;		 
}


void ParseCbData7To8bit( UInt8 *Dst,		// Pointer to the dest buffer
                  UInt8 *Src,		// Pointer to the source buffer
                  UInt8 NbDigits)	// Nb of digits in the source buffer
{
	UInt8 NbChar   = 0; // Nb of output characters
	UInt8 MaskSize = 7;

	while (NbChar < NbDigits)
	{
		UInt8 Mask;
		UInt8 Shift;

		Shift = MaskSize +1;

		// 7-bit Character is right aligned
		if (Shift==8)
			Shift = 0;

		// Get bits on first octet
		Dst[NbChar]  = ((*Src) >> Shift) & 0x7F;

		// Get bits on second if necessary
		if (Shift)
		{
			Src ++;
			Shift = 8 - Shift ;
			Mask  = (1 << MaskSize) - 1;
			Dst[NbChar] |= ((*Src) & Mask) << Shift;
		}

		NbChar ++;
		
		if ( MaskSize == 0 )
			MaskSize = 7;
		else
			MaskSize--;
	}

	Dst[NbDigits] = '\0';
}

//******************************************************************************
//
// Function Name: ProcessSATKSetupMenu
//
// Description:   Process the SATK Setup Menu
//
// Notes:
//
//******************************************************************************
void ProcessSATKSetupMenu(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    SetupMenu_t *pSetupMenu = pSATKEventData->u.setup_menu;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKSetupMenu(tlv_data_string, pSetupMenu);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKSetupMenu() return 0 Error!!\n");
        return;    
    }
   if((dataBuf->SimId) == SIM_DUAL_SECOND )  // gearn airplane mode control for SIMAT 
    {
       memset(satk_setup_menu_tlv_data_string_STK2, 0xFF, sizeof(char)*MAX_TLV_STRING_LENGTH);
       memcpy(satk_setup_menu_tlv_data_string_STK2, &tlv_data_string, sizeof(char)*MAX_TLV_STRING_LENGTH);
       satk_setup_menu_tlv_length_STK2 = tlv_length;
    
       
       KRIL_DEBUG(DBG_ERROR,"satk_setup_menu_tlv_length = %d\n", satk_setup_menu_tlv_length_STK2);
       KRIL_DEBUG(DBG_ERROR,"satk_setup_menu_tlv_data_string = %x \n", satk_setup_menu_tlv_data_string_STK2);
    
    }else{
//Irine_22June_airplanemode
    memset(satk_setup_menu_tlv_data_string, 0xFF, sizeof(char)*MAX_TLV_STRING_LENGTH);
    memcpy(satk_setup_menu_tlv_data_string, &tlv_data_string, sizeof(char)*MAX_TLV_STRING_LENGTH);
    satk_setup_menu_tlv_length = tlv_length;

    
    KRIL_DEBUG(DBG_ERROR,"satk_setup_menu_tlv_length = %d\n", satk_setup_menu_tlv_length);
    KRIL_DEBUG(DBG_ERROR,"satk_setup_menu_tlv_data_string = %x \n", satk_setup_menu_tlv_data_string);
        }


//[LTN_SW3_PROTOCOL] sj0212.park 2011.11.09 initial merge from Luisa
#if defined (CONFIG_LTN_COMMON)  //[latin_protocol] skh STK Issue :setup menu event will be sent when radio on.so we don't need to send it here.
   if(radio_on_setupmenu== false) //Do nothing
   {
      KRIL_DEBUG(DBG_ERROR,"Don't Send Setup Menu Proactive until radio on\n");
      return;
   }
#endif
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}


//******************************************************************************
//
// Function Name: ProcessSATKSelectItem
//
// Description:   Process the SATK Select Item
//
// Notes:
//
//******************************************************************************
void ProcessSATKSelectItem(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    SelectItem_t *pSelectItem = pSATKEventData->u.select_item;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKSelectItem(tlv_data_string, pSelectItem);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKSelectItem() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}


//******************************************************************************
//
// Function Name: ProcessSATKGetInput
//
// Description:   Process the SATK Get Input
//
// Notes:
//
//******************************************************************************
void ProcessSATKGetInput(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    GetInput_t *pGetInput = pSATKEventData->u.get_input;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKGetInput(tlv_data_string, pGetInput);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKGetInput() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}


//******************************************************************************
//
// Function Name: ProcessSATKGetInkey
//
// Description:   Process the SATK Get Inkey
//
// Notes:
//
//******************************************************************************
void ProcessSATKGetInkey(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    GetInkey_t *pGetInkey = pSATKEventData->u.get_inkey;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKGetInkey(tlv_data_string, pGetInkey);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKGetInkey() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));    
}


//******************************************************************************
//
// Function Name: ProcessSATKDisplayText
//
// Description:   Process the SATK Display Text
//
// Notes:
//
//******************************************************************************
void ProcessSATKDisplayText(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    DisplayText_t *pDisplayText = pSATKEventData->u.display_text;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKDisplayText(tlv_data_string, pDisplayText);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKDisplayText() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}


//******************************************************************************
//
// Function Name: ProcessSATKSendMOSMS
//
// Description:   Process the SATK Send MO SMS
//
// Notes:
//
//******************************************************************************
void ProcessSATKSendMOSMS(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    SendMOSMS_t *pSendMOSMS = pSATKEventData->u.send_short_msg;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKSendMOSMS(tlv_data_string, pSendMOSMS);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKSendMOSMS() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));        // gearn TR fail 
}


//******************************************************************************
//
// Function Name: ProcessSATKSendSs
//
// Description:   Process the SATK Send SS
//
// Notes:
//
//******************************************************************************
void ProcessSATKSendSs(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    SendSs_t *pSendSs = pSATKEventData->u.send_ss;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKSendSs(tlv_data_string, pSendSs);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKSendSs() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1)); // gearn TR fail
}


//******************************************************************************
//
// Function Name: ProcessSATKSendUssd
//
// Description:   Process the SATK Send USSD
//
// Notes:
//
//******************************************************************************
void ProcessSATKSendUssd(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    SendUssd_t *pSendUssd = pSATKEventData->u.send_ussd;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKSendUssd(tlv_data_string, pSendUssd);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKSendUssd() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1)); // gearn TR fail
}


//******************************************************************************
//
// Function Name: ProcessSATKPlayTone
//
// Description:   Process the SATK Play Tone
//
// Notes:
//
//******************************************************************************
void ProcessSATKPlayTone(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    PlayTone_t *pPlayTone = pSATKEventData->u.play_tone;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKPlayTone(tlv_data_string, pPlayTone);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKPlayTone() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}


//******************************************************************************
//
// Function Name: ProcessSATKSendStkDtmf
//
// Description:   Process the SATK Send STK DTMF
//
// Notes:
//
//******************************************************************************
void ProcessSATKSendStkDtmf(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    SendStkDtmf_t *pSendStkDtmf = pSATKEventData->u.send_dtmf;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKSendStkDtmf(tlv_data_string, pSendStkDtmf);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ProcessSATKSendStkDtmf() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_EVENT_NOTIFY, tlv_data_string, (tlv_length * 2 + 1));
}


//******************************************************************************
//
// Function Name: ProcessSATKSetupCall
//
// Description:   Process the SATK Setup Call
//
// Notes:
//
//******************************************************************************
void ProcessSATKSetupCall(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    SetupCall_t *pSetupCall = pSATKEventData->u.setup_call;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    int timeout[1] = {0};
    
    tlv_length = ParseSATKSetupCall(tlv_data_string, pSetupCall);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKSetupCall() return 0 Error!!\n");
        return;    
    }
    
    StkCall = TRUE;    // gearn setup call 
   KRIL_DEBUG(DBG_ERROR,"ProcessSATKSetupCall :%d\n", StkCall);
	
    //KRIL_SendNotify(RIL_UNSOL_STK_CALL_SETUP, timeout, sizeof(int));
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1)); // gearn TR fail
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_CALL_SETUP, timeout, sizeof(int));
}


//******************************************************************************
//
// Function Name: ProcessSATKIdleModeText
//
// Description:   Process the SATK Idle Mode Text
//
// Notes:
//
//******************************************************************************
void ProcessSATKIdleModeText(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    IdleModeText_t *pIdleModeText = pSATKEventData->u.idlemode_text;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKIdleModeText(tlv_data_string, pIdleModeText);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKIdleModeText() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}


//******************************************************************************
//
// Function Name: ProcessSATKRefresh
//
// Description:   Process the SATK Refresh
//
// Notes:
//
//******************************************************************************
void ProcessSATKRefresh(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    Refresh_t *pRefresh = pSATKEventData->u.refresh;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    UInt8 path_len;
    int data[2];

    KRIL_DEBUG(DBG_ERROR,"refreshType:%d appliType:%d FileIdList.number_of_file:%d\n",
        pRefresh->refreshType,
        pRefresh->appliType,
        pRefresh->FileIdList.number_of_file
        );

    tlv_length = ParseSATKRefresh(tlv_data_string, pRefresh);

	  if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKRefresh() return 0 Error!!\n");
        return;
    }

     switch (pRefresh->refreshType)  // gearn refresh AP
    {
        case SMRT_INIT_FULLFILE_CHANGED:
        case SMRT_INIT_FILE_CHANGED:
        case SMRT_INIT:
            data[0] = BCM_SIM_INIT;
            data[1] = 0;
             if(dataBuf->SimId == SIM_DUAL_FIRST) // gearn STK2 SIM refresh reset
             {
                 Msisdnck = FALSE;
                 KRIL_DEBUG(DBG_ERROR, " Msisdnck FALSE");
             }
             else if(dataBuf->SimId == SIM_DUAL_SECOND)
            {
                 Msisdnck_1 = FALSE;
                 KRIL_DEBUG(DBG_ERROR, " Msisdnck_1 FALSE");            
             }  
            KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_SIM_REFRESH, data, sizeof(int)*2);            
            break;
        
        case SMRT_FILE_CHANGED:
            data[0] = BCM_SIM_FILE_UPDATE;
            path_len = pRefresh->FileIdList.changed_file[0].path_len;
            data[1] = (int)pRefresh->FileIdList.changed_file[0].file_path[path_len-1];
            KRIL_DEBUG(DBG_INFO,"SMRT_FILE_CHANGED: data[1]:0x%X\n",data[1]);
            KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_SIM_REFRESH, data, sizeof(int)*2);            
            break;
        case SMRT_RESET:
             data[0] = BCM_SIM_RESET;
             data[1] = 0;

             if(dataBuf->SimId == SIM_DUAL_FIRST) // gearn STK2 SIM refresh reset
             {
             gIsStkRefreshReset = TRUE;
                 Msisdnck = FALSE;
                 KRIL_DEBUG(DBG_ERROR, " ProcessSATKRefresh 1 gIsStkRefreshReset: %d\n" ,gIsStkRefreshReset);
             }
             else if(dataBuf->SimId == SIM_DUAL_SECOND)
            {
                 gIsStkRefreshResetSTK2 = TRUE;
                 Msisdnck_1 = FALSE;
                 KRIL_DEBUG(DBG_ERROR, " ProcessSATKRefresh 1 gIsStkRefreshResetSTK2: %d\n" ,gIsStkRefreshResetSTK2);            
             }    
             KRIL_DEBUG(DBG_ERROR,"ParseSATKRefresh() SMRT_RESET!!\n");
             KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_SIM_REFRESH, data, sizeof(int)*2);
             //return;
             break;

        default:
            KRIL_DEBUG(DBG_ERROR,"Unknow refreshType:%d Error!!\n",pRefresh->refreshType);
             break;   // gearn refresh popup issue
    }    

	  // If refresh type is SMRT_RESET, RIL doesn't need to send terminal response to CP.
	  // And AP need to use RIL_REQUEST_RADIO_POWER to power down and power on SIM.
	  if (SMRT_RESET == pRefresh->refreshType)
	  {
      
           if(dataBuf->SimId == SIM_DUAL_FIRST) // gearn STK2 SIM refresh reset
           {
	      gIsStkRefreshReset = TRUE;
               KRIL_DEBUG(DBG_ERROR, " ProcessSATKRefresh 2  gIsStkRefreshReset: %d\n" ,gIsStkRefreshReset);
           }
           else if(dataBuf->SimId == SIM_DUAL_SECOND)
          {
               gIsStkRefreshResetSTK2 = TRUE;
               KRIL_DEBUG(DBG_ERROR, " ProcessSATKRefresh 2 gIsStkRefreshResetSTK2: %d\n" ,gIsStkRefreshResetSTK2);            
           }    

          
	  }
	  else
	  {
	      CAPI2_SatkApi_CmdResp(InitClientInfo(dataBuf->SimId), SATK_EVENT_REFRESH, SATK_Result_CmdSuccess, 0, NULL, 0);
	  }
    
	  KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
    }


//******************************************************************************
//
// Function Name: ProcessSATKLaunchBrowser
//
// Description:   Process the SATK Launch Browser
//
// Notes:
//
//******************************************************************************
void ProcessSATKLaunchBrowser(Kril_CAPI2Info_t *dataBuf)
{
    SATK_EventData_t *pSATKEventData = (SATK_EventData_t*)dataBuf->dataBuf;
    LaunchBrowserReq_t *pLaunchBrowserReq = pSATKEventData->u.launch_browser;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKLaunchBrowser(tlv_data_string, pLaunchBrowserReq);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKIdleModeText() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}

//******************************************************************************
//
// Function Name: ProcessSATKSetupEventList
//
// Description:   Process the SATK Setup Event List
//
// Notes:
//
//******************************************************************************
void ProcessSATKSetupEventList(Kril_CAPI2Info_t *dataBuf)
{
    UInt16 *pEventType = (UInt16*)dataBuf->dataBuf;
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKSetupEventList(tlv_data_string, pEventType);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKSetupEventList() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));    
}
//******************************************************************************
//
// Function Name: ProcessSATKLaunchBrowser
//
// Description:   Process the SATK Launch Browser
//
// Notes:
//
//******************************************************************************
void ProcessSATKSendLanguageNotificationInd(Kril_CAPI2Info_t *dataBuf)
{
	StkLangNotification_t *plang_data = (StkLangNotification_t *) dataBuf->dataBuf;
	
    char tlv_data_string[MAX_TLV_STRING_LENGTH];
    UInt16 tlv_length = 0;
    
    tlv_length = ParseSATKLanguageNotification(tlv_data_string, plang_data);
    if (!tlv_length)
    {
        KRIL_DEBUG(DBG_ERROR,"ParseSATKLanguageNotification() return 0 Error!!\n");
        return;    
    }
    
    KRIL_SendNotify(dataBuf->SimId, BRCM_RIL_UNSOL_STK_PROACTIVE_COMMAND, tlv_data_string, (tlv_length * 2 + 1));
}

//******************************************************************************
//
// Function Name: ProcessUSSDDataInd
//
// Description:   Process the USSD Data
//
// Notes:
//
//******************************************************************************
Boolean ProcessUSSDDataInd(Kril_CAPI2Info_t* data)
{
    USSDataInfo_t *rsp = (USSDataInfo_t *) data->dataBuf;
    KrilReceiveUSSDInfo_t rdata;
    Boolean isSendData = TRUE;
    int i;
 
    KRIL_DEBUG(DBG_INFO, "call_index:%d service_type:%d oldindex:%d newindex:%d prob_tag:%d prob_code:%d err_code:%d code_type:0x%x used_size:%d\n", rsp->call_index,rsp->ussd_data.service_type,rsp->ussd_data.oldindex,rsp->ussd_data.newindex,rsp->ussd_data.prob_tag,
        rsp->ussd_data.prob_code,rsp->ussd_data.err_code,rsp->ussd_data.code_type,rsp->ussd_data.used_size);
    memset(rdata.USSDString, 0, PHASE1_MAX_USSD_STRING_SIZE+1);
 
    if (USSD_REQUEST == rsp->ussd_data.service_type || 
        USSD_REGISTRATION == rsp->ussd_data.service_type)
    {
        rdata.type = 1;
    }
    else if (USSD_NOTIFICATION == rsp->ussd_data.service_type)
    {
        rdata.type = 0;
    }
    else if (USSD_RELEASE_COMPLETE_RETURN_RESULT == rsp->ussd_data.service_type)
    {
        rdata.type = 0;
        if (rsp->ussd_data.used_size == 0)
        {
            isSendData = FALSE;
        }
    }
    else if (USSD_FACILITY_RETURN_RESULT == rsp->ussd_data.service_type)
    {
        if (rsp->ussd_data.used_size > 0)
        {
         rdata.type = 1;
        }
        else
        {
            rdata.type = 0;
            isSendData = FALSE;
        }
    }
    else if (USSD_FACILITY_REJECT == rsp->ussd_data.service_type ||
             USSD_RELEASE_COMPLETE_REJECT == rsp->ussd_data.service_type)
    {
        rdata.type = 4;
        isSendData = FALSE;
    }
    else if (USSD_FACILITY_RETURN_ERROR == rsp->ussd_data.service_type ||
                USSD_RELEASE_COMPLETE_RETURN_ERROR == rsp->ussd_data.service_type)
    {
        rdata.type = 5;
    }
    else if (USSD_RESEND == rsp->ussd_data.service_type)
    {
        KRIL_DEBUG(DBG_ERROR, "USSD_RESEND\n");
        return FALSE;
    }
    rdata.Length = rsp->ussd_data.used_size;
    rdata.codetype = rsp->ussd_data.code_type;
 
    for (i = 0 ; i < rdata.Length ; i++)
        KRIL_DEBUG(DBG_TRACE2, "string:0x%x\n", rsp->ussd_data.string[i]);
 
    if(TRUE == isSendData)
    {
        memcpy(rdata.USSDString, rsp->ussd_data.string, rdata.Length);
    }
    KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_ON_USSD, &rdata, sizeof(KrilReceiveUSSDInfo_t));

    return TRUE;
}


//******************************************************************************
//
// Function Name: ProcessUSSDCallIndexInd
//
// Description:   Process the USSD call index
//
// Notes:
//
//******************************************************************************
Boolean ProcessUSSDCallIndexInd(Kril_CAPI2Info_t* data)
{
    StkReportCallStatus_t *call_status = (StkReportCallStatus_t *) data->dataBuf;
    KRIL_DEBUG(DBG_INFO, "MSG_USSD_CALLINDEX_IND index:%d call_type:%d status:%d\n", call_status->index,call_status->call_type,call_status->status);
    if((call_status->call_type == CALLTYPE_MTUSSDSUPPSVC) || (call_status->call_type == CALLTYPE_MOUSSDSUPPSVC))
    {
        if((call_status->status == CALLSTATUS_MT_CI_ALLOC) && (gUssdID[data->SimId] != CALLINDEX_INVALID))
        {
            // if there's on-going ussd session, still allow MT USSD event to come in
            gPreviousUssdID[data->SimId] = gUssdID[data->SimId];
        }
    }
    gUssdID[data->SimId] = call_status->index;
    return TRUE;
}


//******************************************************************************
//
// Function Name: ProcessUSSDSessionEndInd
//
// Description:   Process the USSD session end
//
// Notes:
//
//******************************************************************************
Boolean ProcessUSSDSessionEndInd(Kril_CAPI2Info_t* data)
{
    CallIndex_t *rsp = (CallIndex_t *) data->dataBuf;
    KRIL_DEBUG(DBG_INFO, "MSG_USSD_SESSION_END_IND index:%d\n", *rsp);

    if(gUssdID[data->SimId] == *rsp)
    {
        gUssdID[data->SimId] = CALLINDEX_INVALID;
    }
    else
    {
        KRIL_DEBUG(DBG_INFO, "MSG_USSD_SESSION_END_IND::SimId:%d gUssdID:%d\n", data->SimId, gUssdID[data->SimId]);
    }

    if(gPreviousUssdID[data->SimId] != CALLINDEX_INVALID)
    {
        gUssdID[data->SimId] = gPreviousUssdID[data->SimId];
        gPreviousUssdID[data->SimId] = CALLINDEX_INVALID;
     }
    KRIL_DEBUG(DBG_INFO, "MSG_USSD_SESSION_END_IND gUssdID:%d gPreviousUssdID:%d\n", gUssdID[data->SimId], gPreviousUssdID[data->SimId]);
    return TRUE;
}


//******************************************************************************
//
// Function Name: ProcessTimeZoneInd
//
// Description:   Process the Time Zone
//
// Notes:
//
//******************************************************************************
Boolean ProcessTimeZoneInd(Kril_CAPI2Info_t* data)
{
    TimeZoneDate_t *rsp = (TimeZoneDate_t*) data->dataBuf;
    KrilTimeZoneDate_t rdate;
    rdate.timeZone = rsp->timeZone;	
    rdate.dstAdjust = rsp->dstAdjust;
    rdate.Sec = rsp->adjustedTime.Sec;
    rdate.Min = rsp->adjustedTime.Min;
    rdate.Hour = rsp->adjustedTime.Hour;
    rdate.Week = rsp->adjustedTime.Week;
    rdate.Day = rsp->adjustedTime.Day;
    rdate.Month = rsp->adjustedTime.Month;
    rdate.Year = rsp->adjustedTime.Year;
    rdate.mcc = rsp->mcc;
    rdate.mnc = rsp->mnc;
 
    KRIL_DEBUG(DBG_INFO, "MSG_DATE_TIMEZONE_IND::timeZone:%d dstAdjust:%d Sec:%d Min:%d Hour:%d Week:%d Day:%d Month:%d Year:%d\n", rdate.timeZone, rdate.dstAdjust, rdate.Sec, rdate.Min, rdate.Hour, rdate.Week, rdate.Day, rdate.Month, rdate.Year);
    KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_NITZ_TIME_RECEIVED, &rdate, sizeof(KrilTimeZoneDate_t));
    return TRUE;
}


//******************************************************************************
//
// Function Name: ProcessRestrictedState
//
// Description:   Process Restricted Status
//
// Notes:
//
//******************************************************************************
void ProcessRestrictedState(SimNumber_t SimId)
{
    Boolean update = FALSE;

    if ((REG_STATE_NORMAL_SERVICE == sCgreg_state[SimId] || REG_STATE_ROAMING_SERVICE == sCgreg_state[SimId]) &&
        (REG_STATE_NORMAL_SERVICE == sCreg_state[SimId] || REG_STATE_ROAMING_SERVICE == sCreg_state[SimId])) //CS and PS allow
    {
        if (BCM_RIL_RESTRICTED_STATE_NONE != KRIL_GetRestrictedState(SimId))
        {
            KRIL_SetRestrictedState(SimId, BCM_RIL_RESTRICTED_STATE_NONE);
            update = TRUE;
        }
    }
    else if ((REG_STATE_NORMAL_SERVICE != sCgreg_state[SimId] || REG_STATE_ROAMING_SERVICE != sCgreg_state[SimId]) &&
        (REG_STATE_NORMAL_SERVICE == sCreg_state[SimId] || REG_STATE_ROAMING_SERVICE == sCreg_state[SimId]))  //CS allow and PS restricted
    {
        if (BCM_RIL_RESTRICTED_STATE_PS_ALL != KRIL_GetRestrictedState(SimId))
        {
            KRIL_SetRestrictedState(SimId, BCM_RIL_RESTRICTED_STATE_PS_ALL);
            update = TRUE;
        }
    }
    else if ((REG_STATE_NORMAL_SERVICE == sCgreg_state[SimId] || REG_STATE_ROAMING_SERVICE == sCgreg_state[SimId]) &&
        (REG_STATE_NORMAL_SERVICE != sCreg_state[SimId] || REG_STATE_ROAMING_SERVICE != sCreg_state[SimId]))  //CS restricted and PS allow
    {
        if(REG_STATE_LIMITED_SERVICE == sCreg_state[SimId] || REG_STATE_NO_SERVICE == sCreg_state[SimId])
        {
            if (BCM_RIL_RESTRICTED_STATE_CS_NORMAL != KRIL_GetRestrictedState(SimId))
            {
                KRIL_SetRestrictedState(SimId, BCM_RIL_RESTRICTED_STATE_CS_NORMAL);
                update = TRUE;
            }
        }
        else
        {
            if (BCM_RIL_RESTRICTED_STATE_CS_ALL != KRIL_GetRestrictedState(SimId))
            {
                KRIL_SetRestrictedState(SimId, BCM_RIL_RESTRICTED_STATE_CS_ALL);
                update = TRUE;
            }
        }
    }
    else // CS and PS no service
    {
        if (sCreg_state[SimId] == REG_STATE_LIMITED_SERVICE)
        {
            if (BCM_RIL_RESTRICTED_STATE_CS_NORMAL != KRIL_GetRestrictedState(SimId)) //CS emergency and PS restricted
            {
                KRIL_SetRestrictedState(SimId, BCM_RIL_RESTRICTED_STATE_CS_NORMAL | BCM_RIL_RESTRICTED_STATE_PS_ALL);
                update = TRUE;
            }
        }
        else if (REG_STATE_SEARCHING == sCreg_state[SimId] && REG_STATE_SEARCHING == sCgreg_state[SimId]) // CS and PS restricted
        {
            if ((BCM_RIL_RESTRICTED_STATE_CS_ALL | BCM_RIL_RESTRICTED_STATE_PS_ALL) != KRIL_GetRestrictedState(SimId))
            {
                KRIL_SetRestrictedState(SimId, BCM_RIL_RESTRICTED_STATE_CS_ALL | BCM_RIL_RESTRICTED_STATE_PS_ALL);
                update = TRUE;
            }
        }
    }
    KRIL_DEBUG(DBG_INFO, "MSG_REG_GPRS_IND::update:%d RestrictedState:%d\n", update, KRIL_GetRestrictedState(SimId));
    if (TRUE == update)
    {
        int state = KRIL_GetRestrictedState(SimId);
        KRIL_SendNotify(SimId, BRCM_RIL_UNSOL_RESTRICTED_STATE_CHANGED, &state, sizeof(int));
        update = FALSE;
    }
}

//******************************************************************************
//
// Function Name: ProcessGSMStatus
//
// Description:   Process GSM Status
//
// Notes:
//
//******************************************************************************
void ProcessGSMStatus(Kril_CAPI2Info_t* data)
{
    MSRegInfo_t *pMSRegInfo = (MSRegInfo_t*)data->dataBuf;
    KRIL_DEBUG(DBG_INFO, "MSG_REG_GSM_IND[%d]::GetGSMRegStatus:%d regState:%d GetGSNRegRat:%d rat:%d gprs_supported:%d egprs_supported:%d\n", data->SimId, sCreg_state[data->SimId], pMSRegInfo->regState, gRegInfo[data->SimId].netInfo.rat, pMSRegInfo->netInfo.rat, pMSRegInfo->netInfo.gprs_supported, pMSRegInfo->netInfo.egprs_supported);
    KRIL_DEBUG(DBG_INFO, "MSG_REG_GSM_IND[%d]::mcc:%d mnc:%d\n", data->SimId, pMSRegInfo->mcc, pMSRegInfo->mnc);
    if (pMSRegInfo->regState != REG_STATE_NO_CHANGE)
    {
        // send network state change notification only if regState has changed or rat has changed (MobC00149291)
        if (sCreg_state[data->SimId] != pMSRegInfo->regState || 
            gRegInfo[data->SimId].netInfo.rat != pMSRegInfo->netInfo.rat ||
            gRegInfo[data->SimId].mcc != pMSRegInfo->mcc ||
            gRegInfo[data->SimId].mnc != pMSRegInfo->mnc)
        {
            sCreg_state[data->SimId] = pMSRegInfo->regState;
            if ( (REG_STATE_SEARCHING == pMSRegInfo->regState) && KRIL_GetHandling2GOnlyRequest(data->SimId) )
            {
                KRIL_DEBUG(DBG_INFO, "MSG_REG_GSM_IND - handling 2G only change request - no network change notif sent\n" );
            }
            else
            {
                KRIL_SetHandling2GOnlyRequest( data->SimId, FALSE );
                KRIL_DEBUG(DBG_INFO, "Sending RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED 1 in ProcessGSMStatus\n" );
                KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
            }
        }
        else
        {
            if (1 == KRIL_GetLocationUpdateStatus(data->SimId) &&
               (gRegInfo[data->SimId].netInfo.rat != pMSRegInfo->netInfo.rat || 
                gRegInfo[data->SimId].mcc != pMSRegInfo->mcc ||
                gRegInfo[data->SimId].mnc != pMSRegInfo->mnc ||
                gRegInfo[data->SimId].lac !=  pMSRegInfo->lac || 
                gRegInfo[data->SimId].cell_id != pMSRegInfo->cell_id)) //+Creg=2 ; <state>, <lac>, <ci>, <AcT>
            {
            
				KRIL_DEBUG(DBG_INFO, "Sending RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED 2 in ProcessGSMStatus\n" );
                KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
            }
        }
        memcpy(&gRegInfo[data->SimId], pMSRegInfo, sizeof(MSRegInfo_t));
    }
}


//******************************************************************************
//
// Function Name: ProcessGPRSStatus
//
// Description:   Process GPRS Status
//
// Notes:
//
//******************************************************************************
void ProcessGPRSStatus(Kril_CAPI2Info_t* data)
{
    MSRegInfo_t *pMSRegInfo = (MSRegInfo_t*)data->dataBuf;
    KRIL_DEBUG(DBG_INFO, "MSG_REG_GPRS_IND[%d]::GetGPRSRegStatus:%d regState:%d rat:%d gprs_supported:%d egprs_supported:%d\n",data->SimId, sCgreg_state[data->SimId], pMSRegInfo->regState, pMSRegInfo->netInfo.rat, pMSRegInfo->netInfo.gprs_supported, pMSRegInfo->netInfo.egprs_supported);
    KRIL_DEBUG(DBG_INFO, "MSG_REG_GPRS_IND[%d]::hsdpa_supported:%d hsupa_supported:%d\n", data->SimId, pMSRegInfo->netInfo.hsdpa_supported, pMSRegInfo->netInfo.hsupa_supported);
    if (pMSRegInfo->regState != REG_STATE_NO_CHANGE)
    {
        if(sCgreg_state[data->SimId] != pMSRegInfo->regState)
        {
            sCgreg_state[data->SimId] = pMSRegInfo->regState;
            if ( (REG_STATE_SEARCHING == pMSRegInfo->regState) && KRIL_GetHandling2GOnlyRequest(data->SimId) )
            {
                KRIL_DEBUG(DBG_INFO, "MSG_REG_GPRS_IND - handling 2G only change request - no network change notif sent\n" );
            }
            else
            {
                KRIL_DEBUG(DBG_INFO, "Sending RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED in ProcessGPRSStatus\n" );
                KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
            }
			KRIL_DEBUG(DBG_INFO, "ProcessGPRSStatus::RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED pMSRegInfo->regState %d\n", pMSRegInfo->regState);
        }
#if 0 // remove the function to avoid the GPRS connection does not establish.
        ProcessRestrictedState(data->SimId);
#endif
    }

}


//******************************************************************************
//
// Function Name: ProcessUE3GStatus
//
// Description:   Process HSDPA Support Status
//
// Notes:
//
//******************************************************************************
void ProcessUE3GStatus(Kril_CAPI2Info_t* data)
{
    MSUe3gStatusInd_t *pUE3Ginfo = (MSUe3gStatusInd_t *) data->dataBuf;
    KRIL_DEBUG(DBG_INFO, "MSG_UE_3G_STATUS_IND::mask:%ld in_cell_dch_state:%d hsdpa_ch_allocated:%d\n", pUE3Ginfo->in_uas_conn_info.mask, pUE3Ginfo->in_uas_conn_info.in_cell_dch_state, pUE3Ginfo->in_uas_conn_info.hsdpa_ch_allocated);
    if (pUE3Ginfo->in_uas_conn_info.hsdpa_ch_allocated != gUE3GInfo[data->SimId].in_uas_conn_info.hsdpa_ch_allocated ||
        pUE3Ginfo->in_uas_conn_info.ue_out_of_service != gUE3GInfo[data->SimId].in_uas_conn_info.ue_out_of_service)
    {
    
		KRIL_DEBUG(DBG_INFO, "Sending RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED in ProcessUE3GStatus\n" );
        KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
    }
    memcpy(&gUE3GInfo[data->SimId], pUE3Ginfo, sizeof(MSUe3gStatusInd_t));
#ifdef CONFIG_CPU_FREQ_GOV_BCM21553
    {
        // this section of code is for enabling/disabling DVFS (Dynamic Voltage and Frequency Scaling)
        // when we have an active HSxPA connection; per the DVFS design doc, DVFS should be disabled
        // when HSxPA is active, and enabled when HSxPA is inactive

        // check if HSxPA is active
        Boolean bHspaChnlAlloc = (pUE3Ginfo->in_uas_conn_info.hsdpa_ch_allocated || pUE3Ginfo->in_uas_conn_info.hsupa_ch_allocated);

        KRIL_DEBUG(DBG_INFO, "sHspaChnlAlloc:%s bHspaChnlAlloc:%s\n", (sHspaChnlAlloc?"TRUE":"FALSE"), (bHspaChnlAlloc?"TRUE":"FALSE") );

        // only change dvfs state if HSxPA state changes...
        if ( bHspaChnlAlloc != sHspaChnlAlloc )
        {
            // sanity check...
            if ( NULL != sRIL_dvfs_client )
            {
                if ( bHspaChnlAlloc )
                {
                    KRIL_DEBUG(DBG_INFO, "HSxPA active, disabling DVFS\n" );
                    cpufreq_bcm_dvfs_disable( sRIL_dvfs_client );
                }
                else
                {
                    KRIL_DEBUG(DBG_INFO, "HSxPA inactive, enabling DVFS\n" );
                    cpufreq_bcm_dvfs_enable( sRIL_dvfs_client );
                }
            }
            else
            {
                KRIL_DEBUG(DBG_ERROR, "** sRIL_dvfs_client NULL\n" );
            }

            sHspaChnlAlloc = bHspaChnlAlloc;
        }
        else
        {
            // unchanged, so we do nothing here
            KRIL_DEBUG(DBG_INFO, "HSxPA state unchanged, not calling DVFS api\n" );
        }
    }
#endif
}


//******************************************************************************
//
// Function Name: ProcessRSSIInd
//
// Description:   Process RSSI information
//
// Notes:
//
//******************************************************************************
void ProcessRSSIInfo(Kril_CAPI2Info_t* data)
{
    RxSignalInfo_t *pSignalInfo = (RxSignalInfo_t*) data->dataBuf;
    if (REG_STATE_NORMAL_SERVICE == sCreg_state[data->SimId] ||
        REG_STATE_ROAMING_SERVICE == sCreg_state[data->SimId] ||
        REG_STATE_LIMITED_SERVICE == sCreg_state[data->SimId])
    {
        KrilSignalStrength_t signal_strnegth;
        signal_strnegth.RAT = gRegInfo[data->SimId].netInfo.rat;
        signal_strnegth.RxLev = pSignalInfo->rssi;
        signal_strnegth.RxQual	= pSignalInfo->qual;
        KRIL_DEBUG(DBG_INFO, "MSG_RSSI_IND::RAT:%d RxLev:%d RxQual:%d\n", signal_strnegth.RAT, signal_strnegth.RxLev, signal_strnegth.RxQual);
        KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_SIGNAL_STRENGTH, &signal_strnegth, sizeof(KrilSignalStrength_t));
    }
}


//******************************************************************************
//
// Function Name: ProcessSMSIndexInSIM
//
// Description:   Process The Index of Write SMS In SIM
//
// Notes:
//
//******************************************************************************
void  ProcessSMSIndexInSIM(Kril_CAPI2Info_t* data)
{
    SmsIncMsgStoredResult_t *pSmsIndex = (SmsIncMsgStoredResult_t *) data->dataBuf;
    KRIL_DEBUG(DBG_INFO, "ProcessSMSIndexInSIM::result:%d rec_no:%d waitState:%d\n", pSmsIndex->result, pSmsIndex->rec_no, pSmsIndex->waitState);

    if(SIMACCESS_SUCCESS == pSmsIndex->result)
    {
        KrilMsgIndexInfo_t msg;
        msg.index = pSmsIndex->rec_no;
        msg.index++;
        KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM, &msg, sizeof(KrilMsgIndexInfo_t));
    }
}


//******************************************************************************
//
// Function Name: ProcessNewSMSMessage
//
// Description:   Process The New SMS Message
//
// Notes:
//
//******************************************************************************
void  ProcessNewSMSMessage(Kril_CAPI2Info_t * data)
{
    SmsSimMsg_t * pmsg;
    if (MSG_SMSPP_APP_SPECIFIC_SMS_IND == data->msgType)
    {
        SmsAppSpecificData_t* appSpecData = (SmsAppSpecificData_t *)data->dataBuf;
        pmsg = &(appSpecData->fSmsMsgData);
    }
    else
    {
        pmsg = (SmsSimMsg_t *) data->dataBuf;
    }
    KRIL_SetSmsMti(data->SimId, pmsg->msgTypeInd); // store the MsgType for send SMS ACK message
#if 0//ndef OEM_RIL_ENABLE
    if (SMS_MSG_CLASS2 == pmsg->msg.msgRxData.codingScheme.MsgClass)
    {
        KrilWriteMsgInfo_t *tdata = kmalloc(sizeof(KrilWriteMsgInfo_t), GFP_KERNEL);
        if(!tdata) {
            KRIL_DEBUG(DBG_ERROR, "unable to allocate tdata buf\n");                
            return;         
        }
        tdata->Length = pmsg->pduSize;
        memcpy(tdata->Pdu, pmsg->PDU, tdata->Length);
        tdata->MsgState = 0; // Unread
        KRIL_DEBUG(DBG_INFO, "moreMsgFlag:%d\n", pmsg->msg.msgRxData.moreMsgFlag);
        tdata->MoreSMSToReceive = !(pmsg->msg.msgRxData.moreMsgFlag);
        SetIsRevClass2SMS(data->SimId, TRUE);
        if(!KRIL_DevSpecific_Cmd(BCM_KRIL_CLIENT, data->SimId, BRCM_RIL_REQUEST_WRITE_SMS_TO_SIM, tdata, sizeof(KrilWriteMsgInfo_t)))
        {
            KRIL_DEBUG(DBG_ERROR,"Command KRIL_REQUEST_INIT_CMD failed\n");
        }
        kfree(tdata);
    }
    else
#else
    if (SMS_MSG_CLASS2 == pmsg->msg.msgRxData.codingScheme.MsgClass)
    {
        SetIsRevClass2SMS(data->SimId, TRUE);

    }
#endif
    {
        KrilMsgPDUInfo_t msg;
        msg.pduSize = pmsg->pduSize;
        memcpy(msg.PDU, pmsg->PDU, pmsg->pduSize);
        KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_RESPONSE_NEW_SMS, &msg, sizeof(KrilMsgPDUInfo_t));
    }
}


//******************************************************************************
//
// Function Name: ProcessNewSMSReport
//
// Description:   Process The New SMS REport
//
// Notes:
//
//******************************************************************************
void  ProcessNewSMSReport(Kril_CAPI2Info_t * data)
{
    SmsSimMsg_t * pmsg = (SmsSimMsg_t *) data->dataBuf;
    KrilMsgPDUInfo_t *msg = kmalloc(sizeof(KrilMsgPDUInfo_t), GFP_KERNEL);
    if(!msg) {
        KRIL_DEBUG(DBG_ERROR, "unable to allocate msg buf\n");
        return;
    }
    KRIL_SetSmsMti(data->SimId, pmsg->msgTypeInd); // store the MsgType for send SMS ACK message
    msg->pduSize = pmsg->pduSize;
    msg->pduSize++;
    msg->PDU[0] = 0x00;
    memcpy(&msg->PDU[1], pmsg->PDU, pmsg->pduSize);
    KRIL_DEBUG(DBG_INFO, "pduSize:%d pduSize:%d Number:%s\n", msg->pduSize, pmsg->pduSize, pmsg->daoaAddress.Number);
    KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, msg, sizeof(KrilMsgPDUInfo_t));
    kfree(msg);
}


//******************************************************************************
//
// Function Name: ProcessSuppSvcNotification
//
// Description:   Process Supp Svc Notification
//
// Notes:
//
//******************************************************************************
void  ProcessSuppSvcNotification(Kril_CAPI2Info_t * data)
{
    SS_CallNotification_t *theSsNotifyPtr = (SS_CallNotification_t *) data->dataBuf;
    SsNotifyParam_t*  theNotifyParamPtr = &theSsNotifyPtr->notify_param;
    CCallType_t    theCallType;
    KrilSuppSvcNotification_t *ndata = kmalloc(sizeof(KrilSuppSvcNotification_t), GFP_KERNEL);
    if(!ndata) {
        KRIL_DEBUG(DBG_ERROR, "unable to allocate ndata buf\n");
        return;
    }
    KRIL_DEBUG(DBG_INFO, "index:%d NotifySS_Oper:%d fac_ie:%s\n", theSsNotifyPtr->index,  theSsNotifyPtr->NotifySS_Oper,  theSsNotifyPtr->fac_ie);
    KRIL_DEBUG(DBG_INFO, "call_state:%d cug_index:%d callingName:%s\n", theNotifyParamPtr->ect_rdn_info.call_state,  theNotifyParamPtr->cug_index, (char *)theNotifyParamPtr->callingName);
    theCallType = KRIL_GetCallType(data->SimId, theSsNotifyPtr->index);
    if (MOVOICE_CALL == theCallType ||
        MODATA_CALL == theCallType ||
        MOFAX_CALL == theCallType)
    {
        ndata->notificationType = 0;
        switch (theSsNotifyPtr->NotifySS_Oper)
        {
            case CALLNOTIFYSS_CFU_ACTIVE:
            case CALLNOTIFYSS_INCALL_FORWARDED:
                ndata->code = 0;
                break;

            case CALLNOTIFYSS_CFC_ACTIVE:
                ndata->code = 1;
                break;

            case CALLNOTIFYSS_OUTCALL_FORWARDED:
                ndata->code = 2;
                break;

            case CALLNOTIFYSS_CALL_WAITING:
                ndata->code = 3;
                break;

            case CALLNOTIFYSS_CUGINDEX:
                ndata->code = 4;
                break;

            case CALLNOTIFYSS_OUTCALL_BARRED:
            case CALLNOTIFYSS_BAOC:
            case CALLNOTIFYSS_BOIC:
            case CALLNOTIFYSS_BOIC_EX_HC:
                ndata->code = 5;
                break;

            case CALLNOTIFYSS_INCALL_BARRED: //Incomming call barred
            case CALLNOTIFYSS_INCOMING_BARRED: //All incoming calls barred
            case CALLNOTIFYSS_BAIC_ROAM:
            case CALLNOTIFYSS_BAC:
                ndata->code = 6;
                break;

            case CALLNOTIFYSS_CLIRSUPREJ:
                ndata->code = 6;
                break;

            case CALLNOTIFYSS_CALL_DEFLECTED:
                ndata->code = 6;
                break;

            default:
                kfree(ndata);
                return;
        }
    }
    else if (MTVOICE_CALL == theCallType ||
                MTDATA_CALL == theCallType ||
                MTFAX_CALL == theCallType)
    {
        ndata->notificationType = 1;
        switch (theSsNotifyPtr->NotifySS_Oper)
        {
            case CALLNOTIFYSS_FORWARDED_CALL:
                ndata->code = 0;
                break;

            case CALLNOTIFYSS_CUGINDEX:
                ndata->code = 1;
                break;


            case CALLNOTIFYSS_FORWARD_CHECK_SS_MSG:
                ndata->code = 6;
                break;

            case CALLNOTIFYSS_ECT_INDICATION:
            {
                if (ECTSTATE_ALERTING == theSsNotifyPtr->notify_param.ect_rdn_info.call_state)
                    ndata->code = 7;
                else
                    ndata->code = 8;

                if (TRUE == theSsNotifyPtr->notify_param.ect_rdn_info.present_allowed_add ||
                    TRUE == theSsNotifyPtr->notify_param.ect_rdn_info.present_restricted_add)
                {
                    ndata->type = 0x80 | (theSsNotifyPtr->notify_param.ect_rdn_info.phone_number.ton << 4) | theSsNotifyPtr->notify_param.ect_rdn_info.phone_number.npi;
                    strcpy(ndata->number, (char*)theSsNotifyPtr->notify_param.ect_rdn_info.phone_number.number);
                }
                break;
            }

            case CALLNOTIFYSS_DEFLECTED_CALL:
                ndata->code = 9;
                break;

            default:
                kfree(ndata);
                return;
        }
    }
    else
    {
        kfree(ndata);
        return;
    }
    if (CALLNOTIFYSS_CUGINDEX == theSsNotifyPtr->NotifySS_Oper)
    {
        ndata->index = theSsNotifyPtr->notify_param.cug_index;
    }
    else
    {
        ndata->index = 0;
    }
    KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_SUPP_SVC_NOTIFICATION, ndata, sizeof(KrilSuppSvcNotification_t));
    kfree(ndata);
}


void  ProcessSATKCCDisplayInd(SimNumber_t SimId, MsgType_t msgType, void * data)
{
	StkCallControlDisplay_t *pStkCallControlDisplay;
	KrilStkCallCtrlResult_t KrilStkCallCtrlResult;
	StkCallSetupFail_t *pStkCallSetupFail;
	int i;

       memset(&KrilStkCallCtrlResult, 0x00, sizeof(KrilStkCallCtrlResult_t));   //  gearn call controlmodification atoi 

	if(msgType == MSG_STK_CC_SETUPFAIL_IND){
		pStkCallSetupFail = (StkCallSetupFail_t*) data;
		KrilStkCallCtrlResult.call_type = MO_VOICE;
		KrilStkCallCtrlResult.control_result = CALL_CONTROL_NOT_ALLOWED;
		KRIL_SendNotify(SimId, RIL_UNSOL_STK_CALL_CONTROL_RESULT, &KrilStkCallCtrlResult, sizeof(KrilStkCallCtrlResult_t));
		return;		
	}
	
	pStkCallControlDisplay = (StkCallControlDisplay_t*) data;
	KRIL_DEBUG(DBG_INFO, "RIL_UNSOL_STK_CALL_CONTROL_RESULT::call_type:%d KrilStkCallCtrlResult.control_result :%d\n", pStkCallControlDisplay->newCCType , pStkCallControlDisplay->ccResult);
	switch(pStkCallControlDisplay->newCCType){
				
		case CALL_CONTROL_CS_TYPE:
			KrilStkCallCtrlResult.call_type = MO_VOICE;
			break;


		case CALL_CONTROL_SS_TYPE:
			KrilStkCallCtrlResult.call_type = MO_SS;
			break;

		case CALL_CONTROL_USSD_TYPE:
			KrilStkCallCtrlResult.call_type = MO_USSD;
			break;

		case CALL_CONTROL_MO_SMS_TYPE:
			KrilStkCallCtrlResult.call_type = MO_SMS;
			break;

		/* NEED TO CHECK */
		case CALL_CONTROL_UNDEFINED_TYPE:
			KrilStkCallCtrlResult.call_type = PDP_CTXT;
			break;
		default://yihwa
			KrilStkCallCtrlResult.call_type = MO_VOICE;
			break;

		}
			
	switch(pStkCallControlDisplay->ccResult){

		case CC_ALLOWED_NOT_MODIFIED: 
			KrilStkCallCtrlResult.control_result = CALL_CONTROL_ALLOWED_NO_MOD;
			break;
				
		case CC_NOT_ALLOWED: 
			KrilStkCallCtrlResult.control_result = CALL_CONTROL_NOT_ALLOWED;
			break;
					
		case CC_ALLOWED_MODIFIED: 
			KrilStkCallCtrlResult.control_result = CALL_CONTROL_ALLOWED_WITH_MOD;
			KrilStkCallCtrlResult.modadd_ton = pStkCallControlDisplay->ton;				
			KrilStkCallCtrlResult.modadd_npi = pStkCallControlDisplay->npi;				
			KrilStkCallCtrlResult.modadd_len = strlen(pStkCallControlDisplay->number);
			for(i=0; i< KrilStkCallCtrlResult.modadd_len;i++)   //  gearn call controlmodification atoi 
				{
					if (pStkCallControlDisplay->number[i] == '+')
					{
						
						KrilStkCallCtrlResult.modadd[i] = 0x91;
					}
					else
					{
						 KrilStkCallCtrlResult.modadd[i] = pStkCallControlDisplay->number[i] - '0';
					}
				}	
			//memcpy(KrilStkCallCtrlResult.modadd,pStkCallControlDisplay->number,KrilStkCallCtrlResult.modadd_len);
			break;

		case CC_STK_BUSY:
		case CC_SIM_ERROR:
			KrilStkCallCtrlResult.control_result = CALL_CONTROL_NOT_ALLOWED;
			break;
					
		default: 
			KrilStkCallCtrlResult.control_result = CALL_CONTROL_NO_CONTROL; //NEED TO CHECK NO_CONTROL result if CALL control is not activated by SIM
			break;

	}


	KrilStkCallCtrlResult.alpha_id_present = pStkCallControlDisplay->alphaIdValid;			
	KrilStkCallCtrlResult.alpha_id_len = pStkCallControlDisplay->displayTextLen;
	if(pStkCallControlDisplay->displayTextLen > 0)//yihwa
	memcpy(&KrilStkCallCtrlResult.alpha_id,pStkCallControlDisplay->displayText,sizeof(pStkCallControlDisplay->displayText));
			
	switch(pStkCallControlDisplay->oldCCType){
				
		case CALL_CONTROL_CS_TYPE:
			KrilStkCallCtrlResult.old_call_type = MO_VOICE;
			break;

		case CALL_CONTROL_SS_TYPE:
			KrilStkCallCtrlResult.old_call_type = MO_SS;
			break;

		case CALL_CONTROL_USSD_TYPE:
			KrilStkCallCtrlResult.old_call_type = MO_USSD;
			break;

		case CALL_CONTROL_MO_SMS_TYPE:
			KrilStkCallCtrlResult.old_call_type = MO_SMS;
			break;

				/* NEED TO CHECK */
		case CALL_CONTROL_UNDEFINED_TYPE:
			KrilStkCallCtrlResult.old_call_type = PDP_CTXT;
			break;
		default://yihwa
			KrilStkCallCtrlResult.old_call_type = MO_VOICE;
			break;
			

	}
	KrilStkCallCtrlResult.call_id = 0;	
	KRIL_DEBUG(DBG_INFO, "RIL_UNSOL_STK_CALL_CONTROL_RESULT::sim_appl_type:%d KrilStkCallCtrlResult.control_result :%d\n", KrilStkCallCtrlResult.call_type , KrilStkCallCtrlResult.control_result);

    KRIL_SendNotify(SimId, RIL_UNSOL_STK_CALL_CONTROL_RESULT, &KrilStkCallCtrlResult,sizeof(KrilStkCallCtrlResult_t));
}
/* SUN JU */

//******************************************************************************
//
// Function Name: ProcessSSNotification
//
// Description:   Handle the MSG_MNCC_CLIENT_NOTIFY_SS_IND message
//
// Notes:
//
//******************************************************************************
void  ProcessSSNotification(Kril_CAPI2Info_t * data)
{
    CC_NotifySsInd_t *theSsNotifyPtr = (CC_NotifySsInd_t *) data->dataBuf;
    SS_NotifySs_t*  theNotifyParamPtr = &theSsNotifyPtr->notifySs;
    CCallType_t    theCallType;
    KrilSuppSvcNotification_t *ndata = kmalloc(sizeof(KrilSuppSvcNotification_t), GFP_KERNEL);
    if(!ndata) {
        KRIL_DEBUG(DBG_ERROR, "Unable to allocate ndata buf\n");            
        return;
    }
    memset(ndata, 0, sizeof(KrilSuppSvcNotification_t));
    ndata->code = 0xFF; // If code is 0xff, we don't send the notification to URIL

    KRIL_DEBUG(DBG_INFO, "DialogId:%d include:%d ssCode:%d cug_index:%d callType:%d\n", data->DialogId, theNotifyParamPtr->include, theNotifyParamPtr->ssCode, theNotifyParamPtr->cugIndex, theSsNotifyPtr->callType);
    theCallType = theSsNotifyPtr->callType;

    if (MOVOICE_CALL == theCallType ||
        MODATA_CALL == theCallType ||
        MOFAX_CALL == theCallType)
    {
        ndata->notificationType = 0; //MO
        if ((theNotifyParamPtr->include & 0x01) && //SS Code
            (!(theNotifyParamPtr->include & 0x02) ||
            ((theNotifyParamPtr->include & 0x03) && ((theNotifyParamPtr->ssStatus & SS_STATUS_MASK_ACTIVE) == SS_STATUS_MASK_ACTIVE))))
        {
            switch (theNotifyParamPtr->ssCode)
            {
                case SS_CODE_CFU:
                    ndata->code = 0; //unconditional call forwarding is active
                    break;

                case SS_CODE_ACF:
                    ndata->code = 1; //some of the conditional call forwardings are active
                    break;

                case SS_CODE_BOC:
                case SS_CODE_BAOC:
                case SS_CODE_BOIC:
                case SS_CODE_BOIC_EXC_PLMN:
                    ndata->code = 5;
                    break;

                case SS_CODE_BIC: //Incomming call barred
                case SS_CODE_BAIC: //All incoming calls barred
                case SS_CODE_BAIC_ROAM:
                    ndata->code = 6;
                    break;

                case SS_CODE_ALL_CALL_RESTRICTION:
                    ndata->code = 5;
                    KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_SUPP_SVC_NOTIFICATION, ndata, sizeof(KrilSuppSvcNotification_t));
                    ndata->code = 6;
                    break;

                case SS_CODE_ALL_FORWARDING://CALLNOTIFYSS_CF_ACTIVE
                case SS_CODE_CFB://CALLNOTIFYSS_CFB_ACTIVE
                case SS_CODE_CFNRY://CALLNOTIFYSS_CFNRY_ACTIVE
                case SS_CODE_CFNRC://CALLNOTIFYSS_CFNRC_ACTIVE
                default:
                    break;
            }
        }

        if (theNotifyParamPtr->include & (0x01 << 2)) //SS Notification
        {
            if ((theNotifyParamPtr->ssNotific & SS_NOTIF_INCOMING_CALL_IS_FWD_TO_C) ==
                    SS_NOTIF_INCOMING_CALL_IS_FWD_TO_C)
            {
                ndata->code = 0;
            }
            else if ((theNotifyParamPtr->ssNotific & SS_NOTIF_OUTGOING_CALL_IS_FWD_TO_C) ==
                    SS_NOTIF_OUTGOING_CALL_IS_FWD_TO_C)
            {
                ndata->code = 2;
            }
        }
        else if (theNotifyParamPtr->include & (0x01 << 3)) //Call Waiting Indicator
        {
            ndata->code = 3;
        }
        // block temporarily because Totoro/Luisa don't support CUG
/*
        else if (theNotifyParamPtr->include & (0x01 << 6)) //CUG Index
        {
            ndata->code = 4;
            ndata->index = (int)theNotifyParamPtr->cugIndex;
        }
*/        
        else if (theNotifyParamPtr->include & (0x01 << 7)) //CLIR Suppression Rejected
        {
            ndata->code = 7;
        }
    }

    if ((MTVOICE_CALL == theCallType || MTDATA_CALL == theCallType || MTFAX_CALL == theCallType) &&
            (theNotifyParamPtr->include & 0x04) && 
            ((theNotifyParamPtr->ssNotific & SS_NOTIF_INCOMING_CALL_IS_FWD_CALL) ==
                                                      SS_NOTIF_INCOMING_CALL_IS_FWD_CALL))
    {
       ndata->notificationType = 1; // MT
       ndata->code = 0;
    }
    // block temporarily because Totoro/Luisa don't support CUG    
/*    
    else if ((MTVOICE_CALL == theCallType || MTDATA_CALL == theCallType || MTFAX_CALL == theCallType) &&
            (theNotifyParamPtr->include) & (0x01 << 6)) //CUG Index
    {
       ndata->notificationType = 1;
       ndata->code = 1;
       ndata->index = (int)theNotifyParamPtr->cugIndex;
    }
*/    
    /* following ss indications arrive during active call */
    else if (((MTVOICE_CALL == theCallType) || (MOVOICE_CALL == theCallType)) &&
              (theNotifyParamPtr->include & (0x01 << 4)) && //Call On Hold Indicator
              ((theNotifyParamPtr->callHold & SS_CALL_HOLD_IND_CALL_ON_HOLD) ==
                                                            SS_CALL_HOLD_IND_CALL_ON_HOLD))
    {
       ndata->notificationType = 1;
       ndata->code = 2;
    }
    else if (((MTVOICE_CALL == theCallType) || (MOVOICE_CALL == theCallType)) &&
              (theNotifyParamPtr->include & (0x01 << 4)) && //Call On Hold Indicator
              ((theNotifyParamPtr->callHold & SS_CALL_HOLD_IND_CALL_RETRIEVE) ==
                                                            SS_CALL_HOLD_IND_CALL_RETRIEVE))
    {
       ndata->notificationType = 1;
       ndata->code = 3;
    }
    else if (((MTVOICE_CALL == theCallType) || (MOVOICE_CALL == theCallType)) &&
             (theNotifyParamPtr->include & (0x01 << 5))) //MPTY Indicator
    {
       ndata->notificationType = 1;
       ndata->code = 4;
    }
    else if (theNotifyParamPtr->include & (0x01 << 8)) //ECT Indicator
    {
       ndata->notificationType = 1;
       if (theNotifyParamPtr->ectInd.ectCallState == SS_ECT_CALL_STATE_ALERTING)
       {
           ndata->code = 7;
       }
       else
       {
           ndata->code = 8;
       }				
       if (theNotifyParamPtr->ectInd.type == SS_PRESENTATION_ALLOWED_ADDRESS)
       {
           ndata->type = 0x80 | (theNotifyParamPtr->ectInd.partyAdd.ton << 4) | theNotifyParamPtr->ectInd.partyAdd.npi;
           strncpy(ndata->number, (char*)theNotifyParamPtr->ectInd.partyAdd.number, theNotifyParamPtr->ectInd.partyAdd.length);
       }
    }

    KRIL_DEBUG(DBG_INFO, "notificationType:%d code:%d index:%d \n", ndata->notificationType, ndata->code, ndata->index);
    if (ndata->code != 0xFF)
    {
        KRIL_SendNotify(data->SimId, BRCM_RIL_UNSOL_SUPP_SVC_NOTIFICATION, ndata, sizeof(KrilSuppSvcNotification_t));
    }
    kfree(ndata);
}


//******************************************************************************
// Function Name:	ProcessUssdData
//
// Description:		This is an internal function called by the USSD handler
//					to process the USSD data sent back by the network for 
//					MO/MT USSD requests as well notifications.
//******************************************************************************
static Result_t ProcessUssdData(SS_Operation_t operation, SS_UssdInfo_t* ussdInfoPtr, KrilReceiveUSSDInfo_t* krilUssdInfoPtr)
{
	UInt8			len1;
	UInt8			dcs;
	UInt8			offset = 0;
	Result_t		result = RESULT_OK;
	
	dcs = ussdInfoPtr->dcs;

	len1 = ussdInfoPtr->length;
		
	// memset(krilUssdInfoPtr->USSDString, 0x00, PHASE1_MAX_USSD_STRING_SIZE+1);

    krilUssdInfoPtr->codetype = (KRIL_GetCharacterSet(ussdInfoPtr->dcs) == KRIL_CHARACTER_SET_UCS2_16_BIT) ? UNICODE_UCS2 : UNICODE_UCS1;

	if (ussdInfoPtr->length)
	{
		//if phase2 ussd then check whether we need to convert from 7 bit to octets
		if ((operation == SS_OPERATION_CODE_PROCESS_UNSTRUCTURED_SS_REQ) || 
			(operation == SS_OPERATION_CODE_UNSTRUCTURED_SS_REQEST)||
			(operation == SS_OPERATION_CODE_UNSTRUCTURED_SS_NOTIFY))
		{
			if(KRIL_GetCharacterSet(ussdInfoPtr->dcs) == KRIL_CHARACTER_SET_GSM_7_BIT_DEFAULT)
			{ 
				krilUssdInfoPtr->codetype = UNICODE_GSM;
				len1 = KRIL_USSDSeptet2Octet(ussdInfoPtr->data, krilUssdInfoPtr->USSDString, ussdInfoPtr->length);
			}
			else  
			{
				if (ussdInfoPtr->dcs == 0x11)//UCS2 preceded by language indication 
				{
					if (ussdInfoPtr->length >= 2) offset=2;
				}

				len1 = ussdInfoPtr->length - offset;
				memcpy(	krilUssdInfoPtr->USSDString,
						ussdInfoPtr->data + offset,
						ussdInfoPtr->length - offset);
			}
		}
		else
		{
			len1= ussdInfoPtr->length - offset;
			memcpy(	krilUssdInfoPtr->USSDString,
					ussdInfoPtr->data + offset,
					ussdInfoPtr->length - offset);
		}	
	}

    krilUssdInfoPtr->Length = len1;

    return(result);
}

void ProcessUssdNotification(Kril_CAPI2Info_t* notify)
{
    KrilReceiveUSSDInfo_t *rdata;

    // Sanity check for SIM ID
    if ((notify->SimId != SIM_DUAL_FIRST) && (notify->SimId != SIM_DUAL_SECOND))
    {
       KRIL_DEBUG(DBG_ERROR, "Invalid SIM ID %d\n", notify->SimId);
       return;
    }

    // Sanity check for USSD session status
    if ((notify->msgType == MSG_MNSS_CLIENT_SS_SRV_RSP) ||
        (notify->msgType == MSG_MNSS_CLIENT_SS_SRV_REL))
    {
        // The message should be received for the SIM currently involved with an USSD session
        if (gUssdID[notify->SimId] == CALLINDEX_INVALID)
        {
           KRIL_DEBUG(DBG_ERROR, "Unexpcted USSD notification, msgType=0x%x\n", notify->msgType);
           return;
        }
    }

    rdata = kmalloc(sizeof(KrilReceiveUSSDInfo_t), GFP_KERNEL);
    if(!rdata) {
        KRIL_DEBUG(DBG_ERROR, "unable to allocate rdata buf\n");
        return;
    }
    memset(rdata, 0, sizeof(KrilReceiveUSSDInfo_t));

    if (notify->msgType == MSG_MNSS_CLIENT_SS_SRV_RSP)
    {
        SS_SrvRsp_t *ssSrv_rsp = (SS_SrvRsp_t *) (notify->dataBuf);

        KRIL_DEBUG(DBG_INFO, "SS_SrvRsp_t? callIndex:%d component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n",
        ssSrv_rsp->callIndex,ssSrv_rsp->component,ssSrv_rsp->operation, ssSrv_rsp->ssCode, ssSrv_rsp->type, ssSrv_rsp->param.ussdInfo.dcs, ssSrv_rsp->param.ussdInfo.length,  ssSrv_rsp->param.ussdInfo.data);

        if ((ssSrv_rsp->type == SS_SRV_TYPE_PH1_USSD_INFORMATION) ||
            (ssSrv_rsp->type == SS_SRV_TYPE_PH2_USSD_INFORMATION))
        {
            //if we get a facility ,it can be a return result or invoke 
            if( (ssSrv_rsp->component == SS_COMPONENT_TYPE_INVOKE) &&
                (ssSrv_rsp->operation == SS_OPERATION_CODE_UNSTRUCTURED_SS_REQEST))
            {
                rdata->type = 1;  // "1"   USSD-Request -- text in ((const char **)data)[1]
            }
            else
            {
                // should not reach here ??
                rdata->type = 0; // "0"   USSD-Notify -- text in ((const char **)data)[1]
            }
         }
         else
         {
             rdata->type = 0; // "0"   USSD-Notify -- text in ((const char **)data)[1]
         }

         // process the USSD data
         ProcessUssdData(ssSrv_rsp->operation, &ssSrv_rsp->param.ussdInfo, rdata);
         KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_ON_USSD, rdata, sizeof(KrilReceiveUSSDInfo_t));

    }
    else if (notify->msgType == MSG_MNSS_CLIENT_SS_SRV_REL)
    {
        SS_SrvRel_t *srvRel_rsp = (SS_SrvRel_t *) (notify->dataBuf);

        KRIL_DEBUG(DBG_INFO, "SS_SrvRel_t? callIndex:%d component:%d operation:%d ssCode:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n",
        srvRel_rsp->callIndex,srvRel_rsp->component,srvRel_rsp->operation, srvRel_rsp->ssCode, srvRel_rsp->type, srvRel_rsp->param.ussdInfo.dcs, srvRel_rsp->param.ussdInfo.length,  srvRel_rsp->param.ussdInfo.data);

        // The USSD session is released by network. Reset the session related global variables.
        gDialogID = 0;
        gUssdID[notify->SimId] = CALLINDEX_INVALID;

		rdata->ussdtype = 1;

        switch(srvRel_rsp->type)
        {
           case SS_SRV_TYPE_RETURN_ERROR:
           case SS_SRV_TYPE_LOCAL_ERROR:
           case SS_SRV_TYPE_REJECT:
           {
                rdata->type = 4; // "4"   Operation not supported
                ProcessUssdData(srvRel_rsp->operation, &srvRel_rsp->param.ussdInfo, rdata);
                KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_ON_USSD, rdata, sizeof(KrilReceiveUSSDInfo_t));
           }
           break;

           case SS_SRV_TYPE_USSD_SS_NOTIFY:
           case SS_SRV_TYPE_PH1_USSD_INFORMATION:
           case SS_SRV_TYPE_PH2_USSD_INFORMATION:
           case SS_SRV_TYPE_NONE:
           case SS_SRV_TYPE_CAUSE_IE:
           {
                rdata->type = 0; // "0"   USSD-Notify -- text in ((const char **)data)[1]
                ProcessUssdData(srvRel_rsp->operation, &srvRel_rsp->param.ussdInfo, rdata);
                KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_ON_USSD, rdata, sizeof(KrilReceiveUSSDInfo_t));
           }
           break;

           default:
                KRIL_DEBUG(DBG_INFO, "unhandled MSG_MNSS_CLIENT_SS_SRV_REL, srv_rel_rsp type 0x%x\n", srvRel_rsp->type);
           break;

        }
    }
    else if (notify->msgType == MSG_MNSS_CLIENT_SS_SRV_IND)
    {
       SS_SrvInd_t*	ssSrvInd = (SS_SrvInd_t *) (notify->dataBuf);

       if (gUssdID[notify->SimId] == CALLINDEX_INVALID)
       {
           // start a new network initiated USSD session
           gUssdID[notify->SimId] = 1;

           // invalidate the USSD session for the other SIM (No sanity check is enforced for fault tolerance. Need to monitor if there is any issue.)
           gUssdID[(notify->SimId == SIM_DUAL_FIRST)? SIM_DUAL_SECOND : SIM_DUAL_FIRST] = CALLINDEX_INVALID;
       }

       KRIL_DEBUG(DBG_INFO, "SS_SrvInd_t? callIndex:%d operation:%d type:%d ussdInfo.dcs:%d ussdInfo.length:%d ussdInfo.data:%s\n",
       ssSrvInd->callIndex, ssSrvInd->operation, ssSrvInd->type, ssSrvInd->param.ussdInfo.dcs, ssSrvInd->param.ussdInfo.length,  ssSrvInd->param.ussdInfo.data);

       if (ssSrvInd->type == SS_SRV_TYPE_PH2_USSD_INFORMATION)
       {
           rdata->type = 1; // "1"   USSD-Request -- text in ((const char **)data)[1]
       }
       else
       {
           rdata->type = 0; // "0"   USSD-Notify -- text in ((const char **)data)[1]
       }

       // process the USSD data
       ProcessUssdData(ssSrvInd->operation, &ssSrvInd->param.ussdInfo, rdata);
       KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_ON_USSD, rdata, sizeof(KrilReceiveUSSDInfo_t));
    }

    kfree(rdata);
}


//******************************************************************************
//
// Function Name: ProcessNotification
//
// Description:  Process the notify message from CP side.
//
// Notes:
//
//******************************************************************************
void ProcessNotification(Kril_CAPI2Info_t *notify)
{
    KRIL_DEBUG(DBG_TRACE, "SimId:%d, msgType:0x%lX!\n", notify->SimId, (UInt32)notify->msgType);

    switch((UInt32)notify->msgType)
    {
        case MSG_INCOMING_CALL_IND:
        {
            CallReceiveMsg_t * pIncomingCall = (CallReceiveMsg_t *) notify->dataBuf;
            KRIL_SetIncomingCallIndex(notify->SimId, pIncomingCall->callIndex);
            KRIL_SetCallNumPresent(notify->SimId, pIncomingCall->callIndex, pIncomingCall->callingInfo.present);
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
            KRIL_BroadcastCallStatus(notify->SimId, notify->msgType, NULL);            
            break;
        }

        case MSG_VOICECALL_WAITING_IND:
        {
            VoiceCallWaitingMsg_t * pWaitingCall = (VoiceCallWaitingMsg_t *) notify->dataBuf;
            KRIL_SetWaitingCallIndex(notify->SimId, pWaitingCall->callIndex);
            KRIL_SetCallNumPresent(notify->SimId, pWaitingCall->callIndex, pWaitingCall->callingInfo.present);
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
            KRIL_BroadcastCallStatus(notify->SimId, notify->msgType, NULL);                        
            break;
        }

        case MSG_REG_GSM_IND:
            ProcessGSMStatus(notify);
            break;

        case MSG_REG_GPRS_IND:
            ProcessGPRSStatus(notify);
            break;

        case MSG_UE_3G_STATUS_IND:
             ProcessUE3GStatus(notify);
             break;

        case MSG_SIM_INSTANCE_STATUS_IND:
        {
            SIM_INSTANCE_STATUS_t VMStatus = *((SIM_INSTANCE_STATUS_t *) notify->dataBuf);
            KRIL_DEBUG(DBG_INFO, "MSG_SIM_INSTANCE_STATUS_IND::status:%d\n", VMStatus);
            break;
        }

        case MSG_VCC_VM_PWR_SAVING_IND:
        {
            Boolean VMPWRMode = *((Boolean *) notify->dataBuf);
            KRIL_DEBUG(DBG_INFO, "MSG_VCC_VM_PWR_SAVING_IND::SimId:%d VMPWRMode:%d\n", notify->SimId, VMPWRMode);
            KRIL_SendNotify(notify->SimId, BRIL_UNSOL_VMPWR_SAVING_MODE, &VMPWRMode, sizeof(Boolean));
            break;
        }

        case MSG_SMSPP_STORED_IND: //CMTI:
        case MSG_SMSCB_STORED_IND: //CBMI
        case MSG_SMSSR_STORED_IND: //CDSI
            ProcessSMSIndexInSIM(notify);
            break;

        case MSG_SMSPP_APP_SPECIFIC_SMS_IND:
        case MSG_SMSPP_REGULAR_PUSH_IND: //MMS
        case MSG_SMSPP_SECURE_PUSH_IND:
        case MSG_SMSPP_DELIVER_IND: //+CMT:
        case MSG_SMSPP_OTA_SYNC_IND:
        case MSG_SMSPP_OTA_IND:
            ProcessNewSMSMessage(notify);
            break;

        case MSG_SMSSR_REPORT_IND: // CDS:
            ProcessNewSMSReport(notify);
            break;

        case MSG_GPRS_DEACTIVATE_IND:
        {
            GPRSDeactInd_t *pind = (GPRSDeactInd_t *) notify->dataBuf;
            UInt8 sim_id;

            KRIL_DEBUG(DBG_ERROR, "MSG_GPRS_DEACTIVATE_IND::cid %d, cause %d\n", pind->cid, pind->cause);
            for (sim_id=0; sim_id<DUAL_SIM_SIZE; sim_id++){
                ReleasePdpContext(sim_id, pind->cid);
            }
            break;
        }

        case MSG_PDP_DEACTIVATION_IND:
        {
            PDP_PDPDeactivate_Ind_t *pind = (PDP_PDPDeactivate_Ind_t *) notify->dataBuf;
            UInt8 sim_id;
            
            KRIL_DEBUG(DBG_ERROR, "MSG_PDP_DEACTIVATION_IND::cid %d,type[%s],add[%s]\n", pind->cid, pind->pdpType, pind->pdpAddress);
            for (sim_id=0; sim_id<DUAL_SIM_SIZE; sim_id++){
                ReleasePdpContext(sim_id, pind->cid);
            }
            break;
        }

        case MSG_CELL_INFO_IND:
            KRIL_DEBUG(DBG_INFO, "MSG_CELL_INFO_IND::not process\n");
            break;

        case MSG_SIM_DETECTION_IND:
        {
            SIM_DETECTION_t *pind = (SIM_DETECTION_t *) notify->dataBuf;
         //   RIL_RadioState radiostate;    // gearn sim card type
            RIL_SIM_Chaged SimStatusChage; 
            UInt8 msg[6+(ICC_DIGITS + 1)];
            
            KRIL_DEBUG(DBG_ERROR, "MSG_SIM_DETECTION_IND::sim_appl_type:%d ruim_supported:%d gpowerOffcard:%d\n", pind->sim_appl_type, pind->ruim_supported, gpowerOffcard);
            if(1 == gpowerOffcard) // if shut down the phone, don't need to send these information
            {
                return;
            }
            KRIL_SetSimAppType(notify->SimId, pind->sim_appl_type);

            SimStatusChage.simCardType = pind->sim_appl_type; 
            
            if (SIM_APPL_INVALID == pind->sim_appl_type)
            {
                Kril_SIMEmergency data;
                data.simAppType = pind->sim_appl_type;
                KRIL_SendNotify(notify->SimId, BRIL_UNSOL_EMERGENCY_NUMBER, &data, sizeof(Kril_SIMEmergency));
                SimStatusChage.radioState = BCM_RADIO_STATE_SIM_LOCKED_OR_ABSENT;

// we always set the data preferred on SIM1 only if one SIM not inserted.
                char rawdata[6]= {'B','R','C','M',0x00, 0x30}; // 0x00;BRIL_HOOK_SET_PREFDATA, 0x30; SIM1
                gdataprefer = 1;// change the datapreferred setting, let KRIL do the request again
                KRIL_DEBUG(DBG_INFO, "MSG_SIM_DETECTION_IND::rawdata[5]:%d sim_slot:%d\n", rawdata[5], pind->sim_slot);
                if(!KRIL_DevSpecific_Cmd(BCM_KRIL_CLIENT, notify->SimId, BRCM_RIL_REQUEST_OEM_HOOK_RAW, &rawdata, 6))
                {
                    KRIL_DEBUG(DBG_ERROR,"Command BRCM_RIL_REQUEST_OEM_HOOK_RAW failed\n");
                }
            }
            else
            {
                SimStatusChage.radioState = BCM_RADIO_STATE_SIM_READY;
            }

            // Do not send the radio state change notification to Android framework in flight mode to avoid to confusing its state machine.
  //          if (!gIsFlightModeOnBoot)   // gearn flihgt mode simcard type
   //         {
		  SimStatusChage.IsFlightModeOnBoot  = gIsFlightModeOnBoot;   // gearn flihgt mode simcard type
                KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, &SimStatusChage, sizeof(RIL_SIM_Chaged));   // gearn sim card type
   //         }
           // Deliver Master SIM mode information to framework
            msg[0] = (UInt8)'B';
            msg[1] = (UInt8)'R';
            msg[2] = (UInt8)'C';
            msg[3] = (UInt8)'M';
            msg[4] = (UInt8)BRIL_HOOK_UNSOL_MASTER_SIM_MODE;
            msg[5] = (UInt8)pind->master_sim_mode;
            memcpy(&msg[6],pind->iccid,(ICC_DIGITS + 1));
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_OEM_HOOK_RAW, msg, sizeof(msg));
            break;
        }

        case MSG_SIM_FATAL_ERROR_IND:
        {
            SIM_FATAL_ERROR_t *err = (SIM_FATAL_ERROR_t*) notify->dataBuf;
            UInt8 msg[6];
            
            KRIL_DEBUG(DBG_ERROR,"MSG_SIM_FATAL_ERROR_IND: %d SIM Error!!!\n", *err);
            
            msg[0] = (UInt8)'B';
            msg[1] = (UInt8)'R';
            msg[2] = (UInt8)'C';
            msg[3] = (UInt8)'M';
            msg[4] = (UInt8)BRIL_HOOK_UNSOL_SIM_ERROR;
            msg[5] = (UInt8)*err;
            //yihwa_invalid_sim
            CAPI2_SimApi_PowerOnOffCard (InitClientInfo(notify->SimId), FALSE, SIM_POWER_ON_INVALID_MODE);
            
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_OEM_HOOK_RAW, msg, sizeof(msg));
            break;
        }
        
        case MSG_USSD_DATA_IND:
            //ProcessUSSDDataInd(notify);
            break;

        case MSG_USSD_CALLINDEX_IND:
            ProcessUSSDCallIndexInd(notify);
            break;

        case MSG_USSD_SESSION_END_IND:
            ProcessUSSDSessionEndInd(notify);
            break;

        // for new SS USSD
        case MSG_MNSS_CLIENT_SS_SRV_IND:
        case MSG_MNSS_CLIENT_SS_SRV_RSP:
        case MSG_MNSS_CLIENT_SS_SRV_REL:
            ProcessUssdNotification(notify);
            break;

        case MSG_SMS_READY_IND:
            KRIL_DEBUG(DBG_INFO, "MSG_SMS_READY_IND::call RIL_UNSOL_DEVICE_READY_NOTI : simId=%d\n", notify->SimId);
#ifdef OEM_RIL_ENABLE
			KRIL_SendNotify(notify->SimId, RIL_UNSOL_DEVICE_READY_NOTI, NULL, 0);
#endif
            break;

        case MSG_SIM_CACHED_DATA_READY_IND:
            if(!KRIL_DevSpecific_Cmd(BCM_KRIL_CLIENT, notify->SimId, KRIL_REQUEST_QUERY_SMS_IN_SIM, NULL, 0))
            {
                KRIL_DEBUG(DBG_ERROR,"Command KRIL_REQUEST_QUERY_SMS_IN_SIM failed\n");
            }
            break;

        case MSG_DATE_TIMEZONE_IND:
            ProcessTimeZoneInd(notify);
            break;

        case MSG_PBK_READY_IND:
        {       char msg[5 + 33]  ;	///< NULL terminated IMSI string in ASCII format
			PBK_ENTRY_DATA_RSP_t *pbk_entry = (PBK_ENTRY_DATA_RSP_t*) notify->dataBuf;

			if ( pbk_entry->pbk_id == PB_MSISDN){
				/* handle the MSISDN message */
			//	KRIL_DEBUG(DBG_ERROR,"BRIL_HOOK_UNSOL_SIM_MSISDN_DATA: %s \n", pbk_entry->pbk_rec.number);
				if ((notify->SimId == SIM_DUAL_FIRST) && (Msisdnck == FALSE)){
        			memset(msg,0,sizeof(msg));
		
        			msg[0]=(UInt8)'B';
        			msg[1]=(UInt8)'R';
        			msg[2]=(UInt8)'C';
        			msg[3]=(UInt8)'M';
        			msg[4]=(UInt8)BRIL_HOOK_UNSOL_SIM_MSISDN_DATA;
        			memcpy(&(msg[5]),pbk_entry->pbk_rec.number,33);
				KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_OEM_HOOK_RAW, msg, sizeof(msg));
        			Msisdnck = TRUE;
                                }
				if ((notify->SimId == SIM_DUAL_SECOND) && (Msisdnck_1 == FALSE)){
        			memset(msg,0,sizeof(msg));
		
        			msg[0]=(UInt8)'B';
        			msg[1]=(UInt8)'R';
        			msg[2]=(UInt8)'C';
        			msg[3]=(UInt8)'M';
        			msg[4]=(UInt8)BRIL_HOOK_UNSOL_SIM_MSISDN_DATA;
        			memcpy(&(msg[5]),pbk_entry->pbk_rec.number,33);
				KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_OEM_HOOK_RAW, msg, sizeof(msg));
        			Msisdnck_1 = TRUE;
                                }
        	}else 
			{

            if(!KRIL_DevSpecific_Cmd(BCM_KRIL_CLIENT, notify->SimId, KRIL_REQUEST_QUERY_SIM_EMERGENCY_NUMBER, NULL, 0))
            {
                KRIL_DEBUG(DBG_ERROR,"Command KRIL_REQUEST_QUERY_SIM_EMERGENCY_NUMBER failed\n");
            }
			}

        	break;
		}
        case MSG_SIM_PIN_IND:
        {
            SimPinInd_t *pind = (SimPinInd_t*) notify->dataBuf;
            KRIL_DEBUG(DBG_INFO, "MSG_SIM_PIN_IND::PIN1 blk:%d,puk_blk:%d,enable:%d,verify:%d\n", pind->Pin1Blocked, pind->Pin1PukBlocked, pind->Pin1Enabled, pind->Pin1Verified);
            if (pind->Pin1Blocked || pind->Pin1PukBlocked || (pind->Pin1Enabled && (!pind->Pin1Verified)))
            {
                if(!KRIL_DevSpecific_Cmd(BCM_KRIL_CLIENT, notify->SimId, KRIL_REQUEST_QUERY_SIM_EMERGENCY_NUMBER, NULL, 0))
                {
                    KRIL_DEBUG(DBG_ERROR,"Command KRIL_REQUEST_QUERY_SIM_EMERGENCY_NUMBER failed\n");
                }
            }
            else
            {
                KRIL_DEBUG(DBG_INFO, "MSG_SIM_PIN_IND::not process\n");
            }
            break;
        }

        case MSG_HOMEZONE_STATUS_IND:
            KRIL_DEBUG(DBG_INFO, "MSG_HOMEZONE_STATUS_IND::not process\n");
            break;

        case MSG_RSSI_IND:
            ProcessRSSIInfo(notify);
            break;

        case MSG_VOICECALL_ACTION_RSP:
        {
            VoiceCallActionMsg_t *ndata = (VoiceCallActionMsg_t*) notify->dataBuf;
            KRIL_DEBUG(DBG_INFO, "MSG_VOICECALL_ACTION_RSP::callResult:%d callIndex:%d errorCause:%lu\n", (UInt8)ndata->callResult, ndata->callIndex, (UInt32)ndata->errorCause);
            break;
        }

        case MSG_VOICECALL_RELEASE_IND:
        {
            VoiceCallReleaseMsg_t *ndata = (VoiceCallReleaseMsg_t*) notify->dataBuf;
            KRIL_SetLastCallFailCause(notify->SimId, KRIL_MNCauseToRilError(ndata->exitCause));
            KRIL_DEBUG(DBG_INFO, "MSG_VOICECALL_RELEASE_IND::cause:%lu\n", (UInt32)ndata->exitCause);

            if (ndata->exitCause != MNCAUSE_RADIO_LINK_FAILURE_APPEARED ||  // Add the call status notification here, replace the call status change in MSG_CALL_STATUS_IND for CC_CALL_DISCONNECT
                    !(KRIL_GetCallType(notify->SimId, ndata->callIndex) ==MOVOICE_CALL && // send RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED only not MO call and not dialing or alerting state
                    (KRIL_GetCallState(notify->SimId, ndata->callIndex) == BCM_CALL_DIALING || KRIL_GetCallState(notify->SimId, ndata->callIndex) == BCM_CALL_ALERTING)))
            {
                KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
                KRIL_BroadcastCallStatus(notify->SimId, notify->msgType, NULL);                        
            }                
            break;
        }

        case MSG_VOICECALL_PRECONNECT_IND:
        {
            VoiceCallPreConnectMsg_t *ndata = (VoiceCallPreConnectMsg_t*) notify->dataBuf;
            KRIL_DEBUG(DBG_INFO, "MSG_VOICECALL_PRECONNECT_IND::callIndex:%d\n",ndata->callIndex);
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
            KRIL_BroadcastCallStatus(notify->SimId, notify->msgType, NULL);                        
            break;
        }

        case MSG_VOICECALL_CONNECTED_IND:
        {
            VoiceCallConnectMsg_t *ndata = (VoiceCallConnectMsg_t*) notify->dataBuf;
            KRIL_DEBUG(DBG_INFO, "MSG_VOICECALL_CONNECTED_IND::callIndex:%d\n",ndata->callIndex);
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
            KRIL_BroadcastCallStatus(notify->SimId, notify->msgType, NULL);                        
            break;
        }

        case MSG_CALL_STATUS_IND:
        {
            CallStatusMsg_t *pCallStatusMsg = (CallStatusMsg_t *) notify->dataBuf;
            KrilStkCallCtrlResult_t KrilStkCallCtrlResult;  // gearn setup call 

			
            KRIL_DEBUG(DBG_ERROR, "MSG_CALL_STATUS_IND::callstatus:%d\n", pCallStatusMsg->callstatus);
		if ((pCallStatusMsg->callstatus == CC_CALL_CALLING)&&(StkCall == TRUE))  // gearn setup call 
		{

			memset(&KrilStkCallCtrlResult, 0x00, sizeof(KrilStkCallCtrlResult_t));
				
			KrilStkCallCtrlResult.call_type = MO_VOICE;
			KrilStkCallCtrlResult.control_result = CALL_CONTROL_ALLOWED;
			KrilStkCallCtrlResult.alpha_id_len = NULL;
			memset(&KrilStkCallCtrlResult.alpha_id,0x00,sizeof(KrilStkCallCtrlResult.alpha_id));
			KrilStkCallCtrlResult.modadd_ton = NULL;
			KrilStkCallCtrlResult.modadd_npi = NULL;
			KrilStkCallCtrlResult.modadd_len = NULL;
			memset(&KrilStkCallCtrlResult.modadd,0x00,sizeof(KrilStkCallCtrlResult.modadd));
			KRIL_SendNotify(notify->SimId, RIL_UNSOL_STK_CALL_CONTROL_RESULT, &KrilStkCallCtrlResult,sizeof(KrilStkCallCtrlResult_t));
          
			 KRIL_DEBUG(DBG_ERROR, "MSG_CALL_STATUS_IND  RIL_UNSOL_STK_CALL_CONTROL_RESULT :call_type:%d , control_result:%d \n", KrilStkCallCtrlResult.call_type, KrilStkCallCtrlResult.control_result);

	        //    ProcessSATKCCDisplayInd(notify->SimId, notify->msgType,notify->dataBuf);
		}

#ifdef VIDEO_TELEPHONY_ENABLE
            if (MOVIDEO_CALL == pCallStatusMsg->callType && CC_CALL_ALERTING == pCallStatusMsg->callstatus)
            {
                int callIndex = pCallStatusMsg->callIndex;
                KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_VT_CALL_EVENT_PROGRESS_INFO_IND, &callIndex, sizeof(int));
            }
           // else //mask this if integrate to phone app
#endif //VIDEO_TELEPHONY_ENABLE            
            
            // Any call status change include VT call will be 
            // notified to phone application.
            if(//CC_CALL_BEGINNING == pCallStatusMsg->callstatus ||
               CC_CALL_ACTIVE == pCallStatusMsg->callstatus ||
               CC_CALL_HOLD == pCallStatusMsg->callstatus ||
               CC_CALL_WAITING == pCallStatusMsg->callstatus ||
               CC_CALL_ALERTING == pCallStatusMsg->callstatus ||
               CC_CALL_CALLING == pCallStatusMsg->callstatus     
              )
            {
                KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
                KRIL_BroadcastCallStatus(notify->SimId, notify->msgType, notify->dataBuf);                            
            }
            break;
        }

        case MSG_DTMF_STATUS_IND:
        {
            ApiDtmfStatus_t *pind = (ApiDtmfStatus_t *) notify->dataBuf;
            KRIL_DEBUG(DBG_INFO, "callIndex:%d dtmfIndex:%d dtmfTone:%d toneValume:%d isSilent:%d duration:%lu\n", pind->dtmfObj.callIndex, pind->dtmfObj.dtmfIndex, pind->dtmfObj.dtmfTone, pind->dtmfObj.toneValume, pind->dtmfObj.isSilent, pind->dtmfObj.duration);
            KRIL_DEBUG(DBG_INFO, "dtmfState:%d cause:%d\n", pind->dtmfState, pind->cause);

            if(DTMF_STATE_IDLE == pind->dtmfState && CC_OPERATION_SUCCESS == pind->cause)
            {


            }
            break;
        }
        
#ifdef VIDEO_TELEPHONY_ENABLE        
        case MSG_DATACALL_RELEASE_IND:
        {
            DataCallReleaseMsg_t *pCallReleaseMsg = (DataCallReleaseMsg_t*) notify->dataBuf;
            int vtCallRsp[2] = {0};
            vtCallRsp[0] = pCallReleaseMsg->callIndex;
            vtCallRsp[1] = pCallReleaseMsg->exitCause;
            KRIL_DEBUG(DBG_INFO, "MSG_DATACALL_RELEASE_IND::callIndex:%d exitCause: 0x%02X\n", pCallReleaseMsg->callIndex, pCallReleaseMsg->exitCause);
            
            KRIL_SetLastCallFailCause(notify->SimId, KRIL_MNCauseToRilError(pCallReleaseMsg->exitCause));
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_VT_CALL_EVENT_END, &vtCallRsp, sizeof(vtCallRsp));
            break;
        }
#endif //VIDEO_TELEPHONY_ENABLE

        case MSG_SS_NOTIFY_EXTENDED_CALL_TRANSFER:
        case MSG_SS_NOTIFY_CALLING_NAME_PRESENT:
        case MSG_SS_NOTIFY_CLOSED_USER_GROUP:
        case MSG_SS_CALL_NOTIFICATION:
             ProcessSuppSvcNotification(notify);
             break;

        case MSG_MNCC_CLIENT_NOTIFY_SS_IND:
             ProcessSSNotification(notify);
             break;

        case MSG_SATK_EVENT_SETUP_MENU:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_SETUP_MENU\n");
            ProcessSATKSetupMenu(notify);
            break;
        }

        case MSG_SATK_EVENT_SELECT_ITEM:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_SELECT_ITEM\n");
            ProcessSATKSelectItem(notify);
            break;
        }

        case MSG_SATK_EVENT_GET_INPUT:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_GET_INPUT\n");
            ProcessSATKGetInput(notify);
            break;
        }
        
        case MSG_SATK_EVENT_GET_INKEY:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_GET_INKEY\n");
            ProcessSATKGetInkey(notify);
            break;
        }
        
        case MSG_SATK_EVENT_DISPLAY_TEXT:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_DISPLAY_TEXT\n");
            ProcessSATKDisplayText(notify);
            break;
        }
                
        case MSG_SATK_EVENT_SEND_SHORT_MSG:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_SEND_SHORT_MSG\n");
            ProcessSATKSendMOSMS(notify);
            break;
        }
        
        case MSG_SATK_EVENT_SEND_SS:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_SEND_SS\n");
            ProcessSATKSendSs(notify);
            break;
        }
        
        case MSG_SATK_EVENT_SEND_USSD:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_SEND_USSD\n");
            ProcessSATKSendUssd(notify);
            break;
        }
        
        case MSG_SATK_EVENT_PLAY_TONE:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_PLAY_TONE\n");
            ProcessSATKPlayTone(notify);
            break;
        }
        
        case MSG_SATK_EVENT_SEND_DTMF:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_SEND_DTMF\n");
            ProcessSATKSendStkDtmf(notify);
            break;
        }
        
        case MSG_SATK_EVENT_SETUP_CALL:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_SETUP_CALL\n");
            ProcessSATKSetupCall(notify);
            break;
        }
        
        case MSG_SATK_EVENT_IDLEMODE_TEXT:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_IDLEMODE_TEXT\n");
            ProcessSATKIdleModeText(notify);
            break;
        }
        
        case MSG_SATK_EVENT_REFRESH:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_REFRESH\n");
            ProcessSATKRefresh(notify);
            break;
        }
        
        case MSG_SATK_EVENT_LAUNCH_BROWSER:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_LAUNCH_BROWSER\n");
            ProcessSATKLaunchBrowser(notify);
            break;
        }
 
        case MSG_SATK_EVENT_PROV_LOCAL_LANG:
        {
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_PROV_LOCAL_LANG\n");
            
            // Android doesn't support STK PROVIDE LOCAL INFORMATION(Language setting), so 
            // send the terminal response with result "Beyond ME capability".
            CAPI2_SatkApi_CmdResp(InitClientInfo(notify->SimId), SATK_EVENT_PROV_LOCAL_LANG, SATK_Result_BeyondMeCapability, 0, NULL, 0);
            break;
        }

		case MSG_STK_LANG_NOTIFICATION_IND:
		{
       
			KRIL_DEBUG(DBG_INFO,"MSG_STK_LANG_NOTIFICATION_IND\n");
			ProcessSATKSendLanguageNotificationInd(notify);
			break;

		}
        case MSG_SATK_EVENT_PROV_LOCAL_DATE:
        {
            ClientInfo_t clientInfo;
            SATKString_t intext;
            UInt8   string[7];
            struct timespec ts;
            struct rtc_time tm;
            unsigned long time;

            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_PROV_LOCAL_DATE\n");

            intext.unicode_type = UNICODE_GSM;
            intext.len = 7;
            intext.string = string;

            ts = current_kernel_time();
            time = ts.tv_sec - (sys_tz.tz_minuteswest * 60);
            rtc_time_to_tm(time, &tm);
            
            string[0] = (UInt8)(tm.tm_year + 1900 - 2000);
            string[1] = (UInt8)(tm.tm_mon + 1);
            string[2] = (UInt8)tm.tm_mday;
            string[3] = (UInt8)tm.tm_hour;
            string[4] = (UInt8)tm.tm_min;
            string[5] = (UInt8)tm.tm_sec;
            string[6] = (UInt8)(sys_tz.tz_minuteswest/15);

            CAPI2_SatkApi_CmdResp(InitClientInfo(notify->SimId), SATK_EVENT_PROV_LOCAL_DATE, SATK_Result_CmdSuccess, 0, &intext, 0);
            break;
        }

        case MSG_SIM_MMI_SETUP_EVENT_IND:
        {
            KRIL_DEBUG(DBG_ERROR,"MSG_SIM_MMI_SETUP_EVENT_IND\n");
            ProcessSATKSetupEventList(notify);
            break;
        }
       
        case MSG_SATK_EVENT_STK_SESSION_END:
        {    
            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_STK_SESSION_END\n");
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_STK_SESSION_END, NULL, 0);
            break;
        }

        case MSG_SMSCB_DATA_IND:
        {

#ifdef OEM_RIL_ENABLE

			// Samsung new CB data structure
            SmsStoredSmsCb_t *pind = (SmsStoredSmsCb_t*) notify->dataBuf;
			Kril_Cell_Broadcast_message cbMsg;
			UInt8 pageData[GSM_SMS_TPDU_STR_MAX_SIZE+1] = {0, };
			char temp[3] = {0, }; 
			int i = 0;

			KRIL_DEBUG(DBG_ERROR,"KRIL MSG_SMSCB_DATA_IND NoOctets[%d], Serial[%d], MsgId[%d], Dcs[%d], NoPage[%d], Msg[%s] \n", 
				pind->NoOctets,pind->Serial,pind->MsgId,pind->Dcs,pind->NoPage,pind->Msg);

            if (CB_DATA_PER_PAGE_SZ < pind->NoOctets)
            {
                KRIL_DEBUG(DBG_ERROR,"Warning!! MSG_SMSCB_DATA_IND over 82 byte msg size: %d\n", pind->NoOctets);
                pind->NoOctets = CB_DATA_PER_PAGE_SZ;
            }


			/* 1. CB Type -  0x01 :GSM , 0x02:UMTS.  Broadcom Platform automatcially converts 3G format  */
			cbMsg.cbType = 0x01; // GSM format always. Broadcom stack automatically converts UMTS CB format to GSM CB format

			/* 2. CB Pdu Length  */
			cbMsg.message_length = 6 + pind->NoOctets; // CB header length (fixed to 6 bytes: 3GPP 23.041 9.4.1.1) + contents length
         //[LTN_SW3_PROTOCOL] sj0212.park 2011.07.28 2G CB display corrupt problem CSP437946
         //cbMsg.message_length = 6 + CB_DATA_PER_PAGE_SZ; // CB header length (fixed to 6 bytes: 3GPP 23.041 9.4.1.1) + contents length
			
            pageData[0] = (pind->Serial & 0xFF00) >> 8;
            pageData[1] = pind->Serial & 0x00FF;
            pageData[2] = (pind->MsgId & 0xFF00) >> 8;
            pageData[3] = pind->MsgId & 0x00FF;
            pageData[4] = pind->Dcs;
            pageData[5] = ((pind->NoPage & 0x0F) << 4) | (pind->NbPages & 0x0F);

            memcpy(&(pageData[6]), pind->Msg, CB_DATA_PER_PAGE_SZ);

			/* 3. CB Pdu  */
            memset(cbMsg.message, 0x0, GSM_SMS_TPDU_STR_MAX_SIZE);
			for(i = 0; i < cbMsg.message_length; i++)
			{
				sprintf(temp, "%02X", pageData[i]);
				strcat(cbMsg.message, temp);
				memset(temp, 0, sizeof(temp));
			}


            KRIL_DEBUG(DBG_ERROR,"ProcessNotification [NEW_CB] cbType[%d], message_length[%d], message[%s] \n", cbMsg.cbType, cbMsg.message_length, cbMsg.message);
			KRIL_SendNotify(notify->SimId, RIL_UNSOL_RESPONSE_NEW_CB_MSG, &cbMsg, sizeof(Kril_Cell_Broadcast_message));
			
#else
			// Androiod CB data structure
            SmsStoredSmsCb_t *pind = (SmsStoredSmsCb_t*) notify->dataBuf;
            char data[88];

            KRIL_DEBUG(DBG_INFO,"MSG_SMSCB_DATA_IND\n");
            memset(data, 0x00, 88);
            data[0] = (pind->Serial & 0xFF00) >> 8;
            data[1] = pind->Serial & 0x00FF;
            data[2] = (pind->MsgId & 0xFF00) >> 8;
            data[3] = pind->MsgId & 0x00FF;
            data[4] = pind->Dcs;
            data[5] = ((pind->NoPage & 0x0F) << 4) | (pind->NbPages & 0x0F);

            if (CB_DATA_PER_PAGE_SZ < pind->NoOctets)
            {
                KRIL_DEBUG(DBG_ERROR,"Warning!! MSG_SMSCB_DATA_IND over 82 byte msg size: %d\n", pind->NoOctets);
                pind->NoOctets = CB_DATA_PER_PAGE_SZ;
            }

            memcpy(&data[6], pind->Msg, pind->NoOctets);
            KRIL_DEBUG(DBG_INFO,"Ser 0x%x, Msg 0x%x, Dcs %d, NoP %d, NbP %d\n", pind->Serial, pind->MsgId, pind->Dcs, pind->NoPage, pind->NbPages);
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS, data, 88);
#endif
            break;
        }

#ifdef OEM_RIL_ENABLE
			
		case MSG_STK_CC_SETUPFAIL_IND:
	        case MSG_STK_CC_DISPLAY_IND:
	        {
	            KRIL_DEBUG(DBG_INFO,"MSG_SATK_EVENT_LAUNCH_BROWSER\n");
	            ProcessSATKCCDisplayInd(notify->SimId, notify->msgType,notify->dataBuf);
            break;
        }

#endif

        case MSG_NETWORK_NAME_IND:
        {
            KRIL_DEBUG(DBG_INFO,"rxd MSG_NETWORK_NAME_IND, sending RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED\n");
            KRIL_SendNotify(notify->SimId, BRCM_RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);
            break;
        }
        
#ifdef BRCM_AGPS_CONTROL_PLANE_ENABLE
        case MSG_LCS_RRLP_DATA_IND:
        {
            LcsMsgData_t* pMsgData = (LcsMsgData_t*) notify->dataBuf;
            if(pMsgData && pMsgData->mData && pMsgData->mDataLen && (pMsgData->mDataLen < BRCM_AGPS_MAX_MESSAGE_SIZE))
            {
                AgpsCp_Data* pCpData = kmalloc(sizeof(AgpsCp_Data), GFP_KERNEL);
                if(pCpData)
                {
                    pCpData->protocol = AGPS_PROC_RRLP;
                    pCpData->cPlaneDataLen = pMsgData->mDataLen;
                    memcpy( pCpData->cPlaneData, pMsgData->mData, pMsgData->mDataLen);
                    KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRLP_DATA_IND dataLen=%lu data[0]=0x%x, dada[len-1]=0x%x\n", pMsgData->mDataLen, pMsgData->mData[0], pMsgData->mData[pMsgData->mDataLen-1]);
                    KRIL_SendNotify(notify->SimId, RIL_UNSOL_RESP_AGPS_DLINK_DATA_IND, pCpData, sizeof(AgpsCp_Data));
                    kfree(pCpData);
                }
            }
            break;
        }

        case MSG_LCS_RRC_ASSISTANCE_DATA_IND:
        {
            LcsMsgData_t* pMsgData = (LcsMsgData_t*) notify->dataBuf;
            if(pMsgData && pMsgData->mData && pMsgData->mDataLen && (pMsgData->mDataLen < BRCM_AGPS_MAX_MESSAGE_SIZE))
            {
                AgpsCp_Data* pCpData = kmalloc(sizeof(AgpsCp_Data), GFP_KERNEL);
                if(pCpData)
                {
                    pCpData->protocol = AGPS_PROC_RRC;
                    pCpData->cPlaneDataLen = pMsgData->mDataLen;
                    memcpy( pCpData->cPlaneData, pMsgData->mData, pMsgData->mDataLen);
                    KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRC_ASSISTANCE_DATA_IND dataLen=%lu\n", pMsgData->mDataLen);
                    KRIL_SendNotify(notify->SimId, RIL_UNSOL_RESP_AGPS_DLINK_DATA_IND, pCpData, sizeof(AgpsCp_Data));
                    kfree(pCpData);
                }
            }
            break;
        }

        case MSG_LCS_RRC_MEASUREMENT_CTRL_IND:
        {
            LcsRrcMeasurement_t* pMsgData = (LcsRrcMeasurement_t*) notify->dataBuf;
            if(pMsgData && pMsgData->mData && pMsgData->mDataLen && (pMsgData->mDataLen < BRCM_AGPS_MAX_MESSAGE_SIZE))
            {
                //AgpsCp_Data cpData = {0};
                AgpsCp_Data* pCpData = kmalloc(sizeof(AgpsCp_Data), GFP_KERNEL);
                if(pCpData)
                {
                    pCpData->protocol = AGPS_PROC_RRC;
                    pCpData->cPlaneDataLen = pMsgData->mDataLen;
                    memcpy( pCpData->cPlaneData, pMsgData->mData, pMsgData->mDataLen);
                    KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRC_MEASUREMENT_CTRL_IND dataLen=%lu\n", pMsgData->mDataLen);
                    KRIL_SendNotify(notify->SimId, RIL_UNSOL_RESP_AGPS_DLINK_DATA_IND, pCpData, sizeof(AgpsCp_Data));
                    kfree(pCpData);
                }
            }
            break;
        }
        case MSG_LCS_RRC_BROADCAST_SYS_INFO_IND:
        {
            //LcsRrcBroadcastSysInfo_t* pMsgData = (LcsRrcBroadcastSysInfo_t*) notify->dataBuf;
            //if(pMsgData && pMsgData->mData && pMsgData->mDataLen)
            //Not needed for now
            KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRC_BROADCAST_SYS_INFO_IND\n");
            break;
        }

        case MSG_LCS_RRC_UE_STATE_IND:
        {
            LcsRrcUeState_t* pMsgData = (LcsRrcUeState_t*) notify->dataBuf;
            if(pMsgData)
            {
                AgpsCp_UeStateData stateData = {0};
                stateData.protocol = AGPS_PROC_RRC;
                stateData.ueState = AGPS_UE_STATE_IDLE; 

                switch(pMsgData->mState)
                {
                    case LCS_RRC_STATE_CELL_DCH:        
                        stateData.ueState = AGPS_UE_STATE_CELL_DCH; 
                        break;

                    case LCS_RRC_STATE_CELL_FACH:       
                        stateData.ueState = AGPS_UE_STATE_CELL_FACH; 
                        break;

                    case LCS_RRC_STATE_CELL_PCH:        
                        stateData.ueState = AGPS_UE_STATE_CELL_PCH; 
                        break;

                    case LCS_RRC_STATE_URA_PCH:         
                        stateData.ueState = AGPS_UE_STATE_URA_PCH; 
                        break;

                    case LCS_RRC_STATE_IDLE:            
                        stateData.ueState = AGPS_UE_STATE_IDLE; 
                        break;
                }
                //KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRC_UE_STATE_IND stateData=%d\n", stateData);
                KRIL_DEBUG(DBG_ERROR," 1 MSG_LCS_RRC_UE_STATE_IND\n");
                KRIL_SendNotify(notify->SimId, RIL_UNSOL_RESP_AGPS_UE_STATE_IND, &stateData, sizeof(AgpsCp_UeStateData));
                KRIL_DEBUG(DBG_ERROR," 2 MSG_LCS_RRC_UE_STATE_IND\n");
            }
            break;
        }
        case MSG_LCS_RRC_STOP_MEASUREMENT_IND:
        {
            //Not needed for now
            KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRC_STOP_MEASUREMENT_IND\n");
            break;
        }

        case MSG_LCS_RRC_RESET_POS_STORED_INFO_IND:
        {
            AgpsCp_Protocol protocol = AGPS_PROC_RRC;
            KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRC_RESET_POS_STORED_INFO_IND AGPS_PROC_RRC\n");
            KRIL_SendNotify(notify->SimId, RIL_UNSOL_RESP_AGPS_RESET_STORED_INFO_IND, &protocol, sizeof(AgpsCp_Protocol));
            break;
        }

        case MSG_LCS_RRLP_RESET_POS_STORED_INFO_IND:
        {
            AgpsCp_Protocol protocol = AGPS_PROC_RRLP;
            KRIL_DEBUG(DBG_ERROR,"MSG_LCS_RRC_RESET_POS_STORED_INFO_IND AGPS_PROC_RRLP\n");
            KRIL_SendNotify(notify->SimId, RIL_UNSOL_RESP_AGPS_RESET_STORED_INFO_IND, &protocol, sizeof(AgpsCp_Protocol));
            break;
        }
#endif //BRCM_AGPS_CONTROL_PLANE_ENABLE

        default:
        {
            KRIL_DEBUG(DBG_INFO, "The msgType:0x%lX is not process yet...!\n", (UInt32)notify->msgType);
            break;
        }
    }
}

