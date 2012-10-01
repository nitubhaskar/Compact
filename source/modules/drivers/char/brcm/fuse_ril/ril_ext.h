/*
SRIL related define
*/


#define GSM_SMS_TPDU_STR_MAX_SIZE 490

typedef struct {

	int	bCBEnabled;/*< CB service state. If cb_enabled is true then cell broadcast service will be enabled */
	                              /* and underlying modem will enable CB Channel to receiving CB messages. Otherwise CB */    
	                              /* service will be disabled; underlying modem will deactivate the CB channel. */
	                              /* (enabled/disabled)   : Storage : MODEM Flash */ 
								  
	int	selectedId;/* < CBMI Identifier selected (all or some)  Storage : MODEM Flash */

	int	msgIdMaxCount; /*< CB Channel List Max Count , this number is different for 2G & 3G & is SIM specific Storage : MODEM Flash  */

	int	msgIdCount; /**< CB message ID count , My Channels list is updated in  Storage : MODEM Flash */

	char * msgIDs; /* < CB message ID information, */
				   /* My Channels List is updated here. Storage : SIM Memory  */
} RIL_CB_ConfigArgs ;

typedef struct {
	
	int cbType; // 0x01 :GSM , 0x02:UMTS
	int message_length;
	char message[GSM_SMS_TPDU_STR_MAX_SIZE]; //reference : 3GPP TS 23.041 9.4.1, 9.4.2
	
}RIL_CBMsg_Notification;



// SS defines
typedef enum
{
	SRIL_LOCK_READY = 0x00,
	SRIL_LOCK_PH_SIM = 0x01, // SIM LOCK (Personalisation)
	SRIL_LOCK_PH_FSIM = 0x02, //SIM LOCK (Personalisation)
	SRIL_LOCK_SIM = 0x03, // PIN LOCK
	SRIL_LOCK_FD = 0x04, // Fixed Dialing Memeory feature
	SRIL_LOCK_NETWORK_PERS = 0x05,
	SRIL_LOCK_NETWORK_SUBSET_PERS = 0x06,
	SRIL_LOCK_SP_PERS = 0x07,
	SRIL_LOCK_CORP_PERS = 0x08,
	SRIL_LOCK_PIN2 = 0x09,
	SRIL_LOCK_PUK2 = 0x0A,	  
	SRIL_LOCK_ACL = 0x0B,	 
	SRIL_LOCK_NO_SIM = 0x80 
}SRIL_Lock_Type_t;				

typedef enum
{
	SRIL_PIN_NOT_NEED = 0x00,
	SRIL_PIN = 0x01,
	SRIL_PUK = 0x02,
	SRIL_PIN2 = 0x03,
	SRIL_PUK2 = 0x04,
	SRIL_PERM_BLOCKED = 0x05,
	SRIL_PIN2_DISABLE = 0x06
}SRIL_LOCK_STATUS;

typedef struct {
	int num_lock_type;
	int lock_type;
} RIL_SIM_Lockinfo;

typedef struct {
	int num_lock_type;
	int lock_type;
	int lock_key;
	int num_of_retry;
} RIL_SIM_Lockinfo_Response;

typedef struct {				
 /* data is int[1] that has following elements: 			
				
   data[0] - A phonebook storage type selected. 			
			 It has following values:				
				
			 PB_DC	  0x01 ME dialed calls list stored in NV				
			 PB_EN	  0x02 SIM(or ME) emergency number				
			 PB_FD	  0x03 SIM fixed-dialing phonebook				
			 PB_LD	  0x04 SIM last-dialing phonebook				
			 PB_MC	  0x05 ME missed calls list 			
			 PB_ME	  0x06 ME phonebook 			
			 PB_MT	  0x07 Combined ME and SIM phonebook				
			 PB_ON	  0x08 SIM(or ME) own numbers ( MSISDNs) list				
			 PB_RC	  0x09 ME received calls list stored in NV				
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
   response[1] MAX_INDEX	 Maximum index of phonebook field entries
   response[2] MAX_ENTRY	 Maximum number of phonebook field entry field
   response[3] USED RECORD	 Record in use of phoebook field (except 3GPP_PBC )


 
   PB_FIELD_TYPE has following values:
 
   FIELD_3GPP_NAME	 = 0x01 : Name of current storageg
   FIELD_3GPP_NUMBER = 0x02 : Number of current storageg
   FIELD_3GPP_ANR	 = 0x03 : First additional numberg
   FIELD_3GPP_EMAIL  = 0x04 : First email addressg
   FIELD_3GPP_SNE	 = 0x05 : Second name entryg
   FIELD_3GPP_GRP	 = 0x06 : Grouping fileg
   FIELD_3GPP_PBC	 = 0x07 : Phonebook controlg
   FIELD_3GPP_ANRA	 = 0x08 : Second additional numberg
   FIELD_3GPP_ANRB	 = 0x09 : Third additional numberg
   FIELD_3GPP_ANRC	 = 0x0a : Fourth additional numberg
   FIELD_3GPP_EMAILA = 0x0b : Second email addressg
   FIELD_3GPP_EMAILB = 0x0c : Third email addressg
   FIELD_3GPP_EMAILC = 0x0d : Fourth email addressg
 */
}RIL_Usim_PB_Capa;

#define NUM_OF_ALPHA 3				
#define NUM_OF_NUMBER 4				
				
/* RIL_REQUEST_GET_PHONEBOOK_ENTRY response*/				
typedef struct {				
				
   int lengthAlphas[NUM_OF_ALPHA];	/* length of each alphaTags[i] */		
				
   int dataTypeAlphas[NUM_OF_ALPHA];	/*			
				
	 character set of each of <alphaTags[i]>.				
				
	 Now, following values are available:				
				
	   GSM 7BIT = 0x02: <alphaTag> is converted a string into				
						an 8-bit unpacked GSM alphabet byte array.				
				
	   UCS2 	= 0x03: <alphaTag> is converted a string into				
						an 16-bit universal multiple-octet coded				
						character set (ISO/IEC10646)				
				
						it will be read by UTF-16.				
				
	   others are not supported.				
				
	 dataTypeAlphas[NAME  = 0]; /* character set of name that corresponds to				
								<text> field of +CPBR.				
				
	 dataTypeAlphas[SNE   = 1]: character set of secondary name entry that corresponds to				
								<secondtext> field of +CPBR.				
				
								it is not used parameter now, because SNE field is not used.				
				
	 dataTypeAlphas[EMAIL = 2]: character set of first email address that corresopnds to				
								<email> field of +CPBR. 
								*/
				
   char* alphaTags[NUM_OF_ALPHA ];	/*hex string to specify following fields				
				
	 alphaTags[NAME  = 0]: name that corresponds to <text> field of +CPBR.				
					
	 alphaTags[SNE	 = 1]: second name entry that corresponnds to				
						   <secondtext> field of +CPBR. 			
				
						   it is not used parameter now.				
				
	 alphaTags[EMAIL = 2]: first email address that corresponds to				
						   <email> field of +CPBR.	*/			
				
				
   int lengthNumbers[NUM_OF_NUMBER ];	/* length of each numbers[i].	*/			
				
   int dataTypeNumbers[NUM_OF_NUMBER];	/* data type or format of each numbers[i].				
				
										   it is not used parameter now.	*/			
				
   char* numbers[NUM_OF_NUMBER ];	/*				
				
	 numbers[INDEX_NUMBER = 0] number that corresponds to <number> field of +CPBR.				
				
	 numbers[INDEX_ANR	  = 1] second additional number that corresponds to 			
								<adnumber> field of +CPBR.				
				
								it is not used parameter now.				
				
	 numbers[INDEX_ANRA   = 2]: third additional number.				
				
								it is not used parameter now.				
				
	 numbers[INDEX_ANRB   = 3]: fourth additional number.				
				
								it is not used parameter now. */				
				
   int recordIndex ;	/* location which the numbers are stored				
										  in the storage.				
										  it can be corresponded to <index> field of +CPBR.*/				
				
   int nextIndex ;	/* location of the next storage from current number.				
										  it can be corresponded to next <index> field				
										  of +CPBR. 	*/				
				
				
}RIL_Phonebk_Entry_rp;	


/**
 * RIL_OEM_REQUEST_BASE
 *
 * SAMSUNG defined Ril Request.
 */

#define RIL_OEM_REQUEST_BASE 10000

/** 
 * RIL_REQUEST_SET_CELL_BROADCAST_CONFIG

 * Request Cell Broadcast Configuration
 *
 * "data" is RIL_CB_ConfigArgs
 
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 * Contact : jiyoon.jeon@samsung.com
**/

#define RIL_REQUEST_SET_CELL_BROADCAST_CONFIG (RIL_OEM_REQUEST_BASE + 1)

/** 
 * RIL_REQUEST_GET_CELL_BROADCAST_CONFIG

 * Request Cell Broadcast Configuration
 *
 * "data" is NULL
 * "response" must be a " const RIL_CB_ConfigArgs** "
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *  Contact : jiyoon.jeon@samsung.com
**/
#define RIL_REQUEST_GET_CELL_BROADCAST_CONFIG (RIL_OEM_REQUEST_BASE + 2)  

/** 
 * RIL_REQUEST_CRFM_LINE_SMS_COUNT_MSG

 * Request Confirm Line SMS count
 *
 * "data" is int  *
 * ((int *)data)[0] is the count of SMS.
 *
 * "response" is NULL
 *
 *  Contact : jiyoon.jeon@samsung.com
**/
#define RIL_REQUEST_CRFM_LINE_SMS_COUNT_MSG (RIL_OEM_REQUEST_BASE + 3)

/** 
 * RIL_REQUEST_CRFM_LINE_SMS_READ_MSG

 * Request Confirm Line Sms Read
 *
 * "data" is RIL_LineSmsReadMsg
 * "response" must be a " const RIL_LineSmsReadMsg** "
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *  Contact : jiyoon.jeon@samsung.com
**/
#define RIL_REQUEST_CRFM_LINE_SMS_READ_MSG (RIL_OEM_REQUEST_BASE + 4)

/**
 * RIL_REQUEST_SEND_ENCODED_USSD
 *
 * Send a Encoded USSD message
 *
 * If a USSD session already exists, the message should be sent in the
 * context of that session. Otherwise, a new session should be created.
 *
 * The network reply should be reported via RIL_UNSOL_ON_USSD
 *
 * Only one USSD session may exist at a time, and the session is assumed
 * to exist until:
 *   a) The android system invokes RIL_REQUEST_CANCEL_USSD
 *   b) The implementation sends a RIL_UNSOL_ON_USSD with a type code
 *      of "0" (USSD-Notify/no further action) or "2" (session terminated)
 *
 * "data" is RIL_EncodedUSSD
 *  
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 */

#define RIL_REQUEST_SEND_ENCODED_USSD (RIL_OEM_REQUEST_BASE + 5)

/** 
 * RIL_REQUEST_SET_PDA_MEMORY_STATUS
 *
 * set the PDA Memory Status
 *
 * "data" is int  *
 * ((int *)data)[0] is PDA MEMORY status
 *
 * 0x01= PDAMemory full
 * 0x02 = PD Memory Available
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 * 
 *Contact : seshu.t@samsung.com
**/

#define RIL_REQUEST_SET_PDA_MEMORY_STATUS (RIL_OEM_REQUEST_BASE + 6)

/** 
 * RIL_REQUEST_GET_PHONEBOOK_STORAGE_INFO
 *
 * get the phonebook storage informations
 *
 * "data" is int  *
 * ((int *)data)[0] is PDA MEMORY status
 *
 * 0x01= PDAMemory full
 * 0x02 = PD Memory Available
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 * 
 *Contact : lucky29.park@samsung.com
**/


#define RIL_REQUEST_GET_PHONEBOOK_STORAGE_INFO (RIL_OEM_REQUEST_BASE + 7) 
#define RIL_REQUEST_GET_PHONEBOOK_ENTRY (RIL_OEM_REQUEST_BASE + 8) 
#define RIL_REQUEST_ACCESS_PHONEBOOK_ENTRY (RIL_OEM_REQUEST_BASE + 9)

//[CONFIG_VT_SUPPORT
/** 
 * RIL_REQUEST_DIAL_VIDEO_CALL
 *
 * Initiate Video call 
 *
 * Valid errors:
 *  SUCCESS 
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE
 */
#define RIL_REQUEST_DIAL_VIDEO_CALL (RIL_OEM_REQUEST_BASE + 10) 
//]

#define RIL_REQUEST_CALL_DEFLECTION (RIL_OEM_REQUEST_BASE + 11)

/**
 * RIL_REQUEST_READ_SMS_FROM_SIM
 *
 * read a SMS message from SIM memory.
 *
 * "data" is int  *
 * ((int *)data)[0] is the record index of the message to delete.
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  GENERIC_FAILURE
 *
 */

#define RIL_REQUEST_READ_SMS_FROM_SIM (RIL_OEM_REQUEST_BASE + 12)
#define RIL_REQUEST_USIM_PB_CAPA (RIL_OEM_REQUEST_BASE + 13)


/**
 * RIL_REQUEST_LOCK_INFO
 *
 * Request lock infomation
 *
 * "data" is int *
 * ((int *)data)[0] is Num of Lock Type
 * ((int *)data)[1] is Lock Type
 * "response" is const RIL_SIM_Lockinfo_Response
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */  
 
#define RIL_REQUEST_LOCK_INFO (RIL_OEM_REQUEST_BASE + 14)
/**
 * RIL_REQUEST_SEND_MOBILE_TRACKER_SMS
 *
 * Send an Mobile Tracker SMS message
 *
 * "data" is const char **
 * ((const char **)data)[0] is SMSC address in GSM BCD format prefixed
 *      by a length byte (as expected by TS 27.005) or NULL for default SMSC
 * ((const char **)data)[1] is SMS in PDU format as an ASCII hex string
 *      less the SMSC address
 *      TP-Layer-Length is be "strlen(((const char **)data)[1])/2"
 *
 * "response" is a const RIL_SMS_Response *
 *
 * Based on the return error, caller decides to resend if sending sms
 * fails. SMS_SEND_FAIL_RETRY means retry (i.e. error cause is 332)
 * and GENERIC_FAILURE means no retry (i.e. error cause is 500)
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  SMS_SEND_FAIL_RETRY
 *  FDN_CHECK_FAILURE
 *  GENERIC_FAILURE
 *
 */

#define RIL_REQUEST_SEND_MOBILE_TRACKER_SMS (RIL_OEM_REQUEST_BASE + 15)

#define RIL_REQUEST_DIAL_EMERGENCY_CALL (RIL_OEM_REQUEST_BASE + 16)

#define RIL_REQUEST_GET_STOREAD_MSG_COUNT (RIL_OEM_REQUEST_BASE + 17)

#define RIL_REQUEST_STK_SIM_INIT_EVENT (RIL_OEM_REQUEST_BASE + 18)

