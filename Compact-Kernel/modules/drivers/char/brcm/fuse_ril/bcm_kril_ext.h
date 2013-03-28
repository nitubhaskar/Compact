/*
SRIL related defines
*/
#define GSM_SMS_TPDU_STR_MAX_SIZE 490
#define BCM_RemoveCBMI 0x002
typedef void * RIL_Token;
typedef struct {				
 int bCBEnabled; /* CB service state */				
 int selectedId; /* CBMI Identifier selected */				
 int msgIdMaxCount; /* CB Channel List Max Count */				
 int msgIdCount; /* CB message ID count */				
 int msgIDs[CHNL_IDS_SIZE]; /* CB message ID information*/
} Kril_Cell_Broadcast_config;	


typedef struct {
	int cbType; // 0x01 :GSM , 0x02:UMTS
	int message_length;
	char message[GSM_SMS_TPDU_STR_MAX_SIZE]; //reference : 3GPP TS 23.041 9.4.1, 9.4.2
} Kril_Cell_Broadcast_message;	



typedef struct { 				
 unsigned long total_cnt; /*  the record index of the  message */				
 unsigned long used_cnt; /*  the record index of the  message */				
}KrilStoredMsgCount;				

typedef struct { 				
 unsigned long length; /*  the record index of the  message */				
 char pdu[180*2];
}KrilSMSfromSIM;				

typedef struct {
    int command;    /* one of the commands listed for TS 27.007 +CRSM*/
    int fileid;     /* EF id */
    char *path;     /* "pathid" from TS 27.007 +CRSM command.
                       Path is in hex asciii format eg "7f205f70"
                       Path must always be provided.
                     */
    int p1;
    int p2;
    int p3;
    char *data;     /* May be NULL*/
    char *pin2;     /* May be NULL*/
} RIL_SIM_IO;

typedef struct {
    int 	size;		  // @field structure size in bytes
    int     dataLen;	  // @field data size in bytes
    int 	params;    // @field indicates valid parameters
    int 	status; 	// @field additional status for message
    char    *data;	 // @field message itself
}RIL_SS_Release_Comp_Msg;

#define RIL_PARAM_SSDI_STATUS	1
#define RIL_PARAM_SSDI_DATA 		2

typedef struct {
    int sw1;
    int sw2;
    char *simResponse;  /* In hex string format ([a-fA-F0-9]*). */
} RIL_SIM_IO_Response;

// -----------------------------------------------------------------------------
//
// @doc EXTERNAL
//
// @ SRIL RIL_REQUEST_LOCK_INFO
//
// @comm None
//
// -----------------------------------------------------------------------------

typedef enum
{
	KRIL_LOCK_PIN1,
	KRIL_LOCK_PIN2,
	KRIL_LOCK_PUK2	
}KrilLockType_t;

typedef enum
{
	KRIL_PIN_NOT_NEED = 0x00,
	KRIL_PIN = 0x01,
	KRIL_PUK = 0x02,
	KRIL_PIN2 = 0x03,
	KRIL_PUK2 = 0x04,
	KRIL_PERM_BLOCKED = 0x05,
	KRIL_PIN2_DISABLE = 0x06			 
}KrilLockStatus_t;

typedef struct
{
	int lock_type;
	int lock_status;
	int remaining_attempt;	
}KrilLockInfo_t;

/*+ Ciphering Mode sh0515.lee /Integrity Mode sh0515.lee +*/
typedef enum
{
	KRIL_CIPHERING_MODE,
	KRIL_INTEGRATE_MODE,

	KRIL_CLASSMARK_ID_END	
} KrilClassMarkId_t;

typedef struct
{
	KrilClassMarkId_t classmark_id;
	union
	{
		int					is_supported;
		// can add other ClassMark setting below
	}data_u;
}KrilStackNvramClassMark_t;
/*- Ciphering Mode sh0515.lee /Integrity Mode sh0515.lee -*/


/*+ Band Selection sh0515.lee +*/
typedef struct
{
	int curr_rat;
	int new_band;
}KrilSrilSetBand_t;
/*- Band Selection sh0515.lee -*/

/*+20110824 HKPARK SLAVE BAND SELECTION*/
typedef struct
{
	int curr_rat2;
	int new_band2;
}KrilSrilSetBand2_t;
/*-20110824 HKPARK SLAVE BAND SELECTION*/

#define	PB_NUM_MAX	42
#define	PB_ALPHA_MAX	82
#define	PB_EMAIL_MAX	102

typedef struct { 				
 /* data is int[1] that has following elements:				
 				
   data[0] - A phonebook storage type selected.				
             It has following values:				
 				
             PB_DC    0x01 ME dialed calls list stored in NV				
             PB_EN    0x02 SIM(or ME) emergency number				
             PB_FD    0x03 SIM fixed-dialing phonebook				
             PB_LD    0x04 SIM last-dialing phonebook				
             PB_MC    0x05 ME missed calls list				
             PB_ME    0x06 ME phonebook				
             PB_MT    0x07 Combined ME and SIM phonebook				
             PB_ON    0x08 SIM(or ME) own numbers ( MSISDNs) list				
             PB_RC    0x09 ME received calls list stored in NV				
             PB_SIM   0x0A 2G SIM phonebook				
             PB_SDN   0x0B Service Dialing Number				
             PB_3GSIM 0x0C 3G SIM phonebook				
             PB_ICI   0x0D Incoming Call Information				
             PB_OCI   0x0E Outgoing Call Information				
             PB_AAS   0x0F Additional Number Alpha String				
             PB_GAS   0x10 Grouping Information Alpha String				
*/				
				
 int data;				
				
}SRIL_Phonebk_Storage_Info_request;		

typedef struct { 				
/*				
   response[0] - Total slot count.				
                 The number of the total phonebook memory.				
                 according to Phone book storage.				
   response[1] - Used slot count.				
                 The number of the used phonebook memory.				
   response[2] - First index.				
                 Minimum index of phonebook entries.				
   response[3] - Maximum number of phonebook entry text field.				
   resopnse[4] - Maximum number of phonebook entry number field.				
*/				
				
  int total;				
  int used;				
  int first_id;				
  int max_text;				
  int max_num;				
  				
}RIL_Phonebk_Storage_Info;

#define MAX_3GPP_TYPE 13
#define MAX_DATA_LEN 4
typedef struct {

 
	int response[MAX_3GPP_TYPE][MAX_DATA_LEN];
	
/*
	"response" is int[MAX_3GPP_TYPE(13) MAX_DATA_LEN(4)] type.
 
   response[0] PB_FIELD_TYPE
   response[1] MAX_INDEX     Maximum index of phonebook field entries
   response[2] MAX_ENTRY     Maximum number of phonebook field entry field
   response[3] USED RECORD   Record in use of phoebook field (except 3GPP_PBC )


 
   PB_FIELD_TYPE has following values:
 
   FIELD_3GPP_NAME   = 0x01 : Name of current storageg
   FIELD_3GPP_NUMBER = 0x02 : Number of current storageg
   FIELD_3GPP_ANR    = 0x03 : First additional numberg
   FIELD_3GPP_EMAIL  = 0x04 : First email addressg
   FIELD_3GPP_SNE    = 0x05 : Second name entryg
   FIELD_3GPP_GRP    = 0x06 : Grouping fileg
   FIELD_3GPP_PBC    = 0x07 : Phonebook controlg
   FIELD_3GPP_ANRA   = 0x08 : Second additional numberg
   FIELD_3GPP_ANRB   = 0x09 : Third additional numberg
   FIELD_3GPP_ANRC   = 0x0a : Fourth additional numberg
   FIELD_3GPP_EMAILA = 0x0b : Second email addressg
   FIELD_3GPP_EMAILB = 0x0c : Third email addressg
   FIELD_3GPP_EMAILC = 0x0d : Fourth email addressg
 */
}RIL_Usim_PB_Capa;

typedef struct 
{
	long			index;
	long			next_index;
	unsigned char	number[PB_NUM_MAX];
	unsigned long   length_number;
	unsigned long	num_datatpye;
	unsigned char	name[ PB_ALPHA_MAX ];
	unsigned long   length_name;
	unsigned long	name_datatpye;
	unsigned char	email[ PB_EMAIL_MAX ];
	unsigned long   length_email;
	unsigned long	email_datatpye;

}KrilPhonebookGetEntry_t;	//HJKIM_ADN

typedef struct 
{
	int command;   
	int fileid;    
	int index;		
	char alphaTag[PB_ALPHA_MAX];   
	int alphaTagDCS;
	int alphaTagLength;
	char number[PB_NUM_MAX];    
	int numberLength;	
	char email[PB_EMAIL_MAX];
	int emailTagDCS;	//HJKIM_emailDCS
	int emailLength;
	char pin2[9];    
} KrilPhonebookAccess_t;	//HJKIM_ADN

//	ycao@032311

/* See RIL_REQUEST_LAST_CALL_FAIL_CAUSE */
typedef enum {
    CALL_FAIL_UNOBTAINABLE_NUMBER = 1,
    CALL_FAIL_NORMAL = 16,
    CALL_FAIL_BUSY = 17,
    CALL_FAIL_CONGESTION = 34,
    CALL_FAIL_ACM_LIMIT_EXCEEDED = 68,
    CALL_FAIL_CALL_BARRED = 240,
    CALL_FAIL_FDN_BLOCKED = 241,
    CALL_FAIL_IMSI_UNKNOWN_IN_VLR = 242,
    CALL_FAIL_IMEI_NOT_ACCEPTED = 243,
    CALL_FAIL_CDMA_LOCKED_UNTIL_POWER_CYCLE = 1000,
    CALL_FAIL_CDMA_DROP = 1001,
    CALL_FAIL_CDMA_INTERCEPT = 1002,
    CALL_FAIL_CDMA_REORDER = 1003,
    CALL_FAIL_CDMA_SO_REJECT = 1004,
    CALL_FAIL_CDMA_RETRY_ORDER = 1005,
    CALL_FAIL_CDMA_ACCESS_FAILURE = 1006,
    CALL_FAIL_CDMA_PREEMPTED = 1007,
    CALL_FAIL_CDMA_NOT_EMERGENCY = 1008, /* For non-emergency number dialed
                                            during emergency callback mode */
    CALL_FAIL_CDMA_ACCESS_BLOCKED = 1009, /* CDMA network access probes blocked */
    CALL_FAIL_ERROR_UNSPECIFIED = 0xffff
} RIL_LastCallFailCause;

typedef enum{	
	MO_VOICE=0x00,
    MO_SMS=0x01,
    MO_SS=0x02,
    MO_USSD=0x03,
    PDP_CTXT=0x04,    
}RIL_CallType;


typedef enum {	
    CALL_CONTROL_NO_CONTROL=0,	
    CALL_CONTROL_ALLOWED_NO_MOD=1,
    CALL_CONTROL_NOT_ALLOWED=2,
    CALL_CONTROL_ALLOWED_WITH_MOD=3,    
}RIL_CallControlResultCode;


typedef struct {                                 
   char call_type;                                         
   char control_result;                            
   char alpha_id_present;                                
   char alpha_id_len;                                
   char alpha_id[64];                                
   char call_id;                                
   char old_call_type;                                   
   char modadd_ton;                                
   char modadd_npi;                                
   char modadd_len;                                
   char modadd[200];                                
}KrilStkCallCtrlResult_t; 

typedef enum {
    RADIO_STATE_OFF = 0,                   /* Radio explictly powered off (eg CFUN=0) */
    RADIO_STATE_UNAVAILABLE = 1,           /* Radio unavailable (eg, resetting or not booted) */
    RADIO_STATE_SIM_NOT_READY = 2,         /* Radio is on, but the SIM interface is not ready */
    RADIO_STATE_SIM_LOCKED_OR_ABSENT = 3,  /* SIM PIN locked, PUK required, network
                                              personalization locked, or SIM absent */
    RADIO_STATE_SIM_READY = 4,             /* Radio is on and SIM interface is available */
    RADIO_STATE_RUIM_NOT_READY = 5,        /* Radio is on, but the RUIM interface is not ready */
    RADIO_STATE_RUIM_READY = 6,            /* Radio is on and the RUIM interface is available */
    RADIO_STATE_RUIM_LOCKED_OR_ABSENT = 7, /* RUIM PIN locked, PUK required, network
                                              personalization locked, or RUIM absent */
    RADIO_STATE_NV_NOT_READY = 8,          /* Radio is on, but the NV interface is not available */
    RADIO_STATE_NV_READY = 9               /* Radio is on and the NV interface is available */
} RIL_RadioState;


typedef struct {
  RIL_RadioState   radioState ;    /* radioState*/
  int             simCardType;  /* simCardType */
  int             IsFlightModeOnBoot;  // gearn flihgt mode simcard type
} RIL_SIM_Chaged;  // gearn sim card type


//SAMSUNG_SELLOUT_FEATURE
typedef struct
{
	unsigned char	bSelloutEnable;
	unsigned char	OperateMode;	
	unsigned char	ProdutInfo;
	unsigned char	bSmsSending;
	char			Mcc[4];
	char			Mnc[3];
}SellOutSMS;

#define FLASH_READ_NV_DATA( _Buf_, _Idx_ )		Flash_Read_NV_Data( _Buf_, _Idx_ )
#define FLASH_WRITE_NV_DATA( _Buf_, _Idx_ )	Flash_Write_NV_Data( _Buf_, _Idx_ )
typedef struct
{
    UInt8 total_retry_count;
    UInt8 current_retry_count;
    unsigned long retry_duration;
	VoiceCallParam_t call_param;
    char address[BCM_MAX_DIGITS+1];
} KrilCallRetryInfo_t;

#define RIL_OEM_REQUEST_BASE 10000
#define RIL_REQUEST_SET_CELL_BROADCAST_CONFIG (RIL_OEM_REQUEST_BASE + 1)
#define RIL_REQUEST_GET_CELL_BROADCAST_CONFIG (RIL_OEM_REQUEST_BASE + 2)
#define RIL_REQUEST_SEND_ENCODED_USSD (RIL_OEM_REQUEST_BASE + 5)
#define RIL_REQUEST_GET_PHONEBOOK_STORAGE_INFO (RIL_OEM_REQUEST_BASE + 7)
#define RIL_REQUEST_GET_PHONEBOOK_ENTRY (RIL_OEM_REQUEST_BASE + 8)
#define RIL_REQUEST_ACCESS_PHONEBOOK_ENTRY (RIL_OEM_REQUEST_BASE + 9)
#define RIL_REQUEST_DIAL_VIDEO_CALL (RIL_OEM_REQUEST_BASE + 10)
#define RIL_REQUEST_CALL_DEFLECTION (RIL_OEM_REQUEST_BASE + 11)
#define RIL_REQUEST_READ_SMS_FROM_SIM (RIL_OEM_REQUEST_BASE + 12)
#define RIL_REQUEST_USIM_PB_CAPA (RIL_OEM_REQUEST_BASE + 13)
#define RIL_REQUEST_LOCK_INFO (RIL_OEM_REQUEST_BASE + 14)
#define RIL_REQUEST_DIAL_EMERGENCY_CALL (RIL_OEM_REQUEST_BASE + 16)
#define RIL_REQUEST_GET_STOREAD_MSG_COUNT (RIL_OEM_REQUEST_BASE + 17)

#define RIL_OEM_UNSOL_RESPONSE_BASE 11000
#define RIL_UNSOL_RESPONSE_NEW_CB_MSG  (RIL_OEM_UNSOL_RESPONSE_BASE + 0)
#define RIL_UNSOL_RELEASE_COMPLETE_MESSAGE	(RIL_OEM_UNSOL_RESPONSE_BASE + 1)
#define RIL_UNSOL_STK_CALL_CONTROL_RESULT (RIL_OEM_UNSOL_RESPONSE_BASE + 3)
#define RIL_UNSOL_RESPONSE_LINE_SMS_READ  (RIL_OEM_UNSOL_RESPONSE_BASE + 6)
#define RIL_UNSOL_DEVICE_READY_NOTI (RIL_OEM_UNSOL_RESPONSE_BASE + 8)
#define RIL_UNSOL_AM	(RIL_OEM_UNSOL_RESPONSE_BASE + 10)
#define RIL_UNSOL_SIM_SMS_STORAGE_AVAILALE (RIL_OEM_UNSOL_RESPONSE_BASE + 15)

