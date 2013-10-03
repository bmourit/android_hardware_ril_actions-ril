/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-ss.c
 Author: liubing/00141886
 Version: V1.0
 Description:
 
    Handle request and unsolicited about ss.include below interface:
         
	RIL_REQUEST_SEND_USSD
	RIL_REQUEST_CANCEL_USSD
	RIL_REQUEST_QUERY_FACILITY_LOCK
	RIL_REQUEST_SET_FACILITY_LOCK

	RIL_UNSOL_ON_USSD
 
*******************************************************/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

  when        who      version                     why 
 -------    -------    -------                     --------------------------------------------
2010/09/14  l141886    V1B01D01                    add wcdma module ss function
2010/12/14  fkf34035   V2B01D01                    add cdma module ss function
2011/05/14  lkf33986   V3B02D01                    add ussd function
2011/11/04  wkf32792   RIL-V100R005B002D02SP00C03  implement new ussd solution


===========================================================================*/

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/

#include "huawei-ril.h"
#include "gsm_7bit.h" //DTS2011100905256 wkf32792 20111110 added

/*===========================================================================

                   INTERNAL DEFINITIONS AND TYPES

===========================================================================*/
#define USSD_NOTIFY                             0
#define USSD_REQUEST                            1
#define USSD_SESSION_TERMINATED_BY_NETWORK      2
#define USSD_OTHER_LOCAL_CLENT_HAS_RESPONDED    3
#define USSD_OPERATION_NOT_SUPPORTED            4
#define USSD_NETWORK_TIMEOUT                    5
 /*!< 有效数据长度最大为160 Oct，7bit时为160*8/7=182.8 */
#define MAX_USSD_LENGTH 184   //DTS2011100905256 wkf32792 20111110 added
extern Module_Type g_module_type;
extern CDMA_SubType g_cdma_subtype;

/* BEGIN DTS2011100905256 wkf32792 20111110 modified */
static void  request_send_ussd(void* data, size_t datalen, RIL_Token t)
{
    if (datalen >= MAX_USSD_LENGTH)
    {
    	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    	return;
    }
	int err;
    char* cmd = NULL;
    char* string = NULL;
    WCHAR Unicode[MAX_USSD_LENGTH] = {0};
    char  UssdString[MAX_USSD_LENGTH] = {0};     //发送CUSD命令所需要的字符串
    int i = 0, UnicodeLen = 0;
	ATResponse* p_response;
	
	string = (char*)(data);
	UnicodeLen = Utf8ToUnicode(string, Unicode, strlen(string)); 
	for(i = 0; i < UnicodeLen; i++)
    {
        if (!(GSM_7BitAlphabet(Unicode[i])))
        {
            /* unicode编码方式 */
            unsigned char byteOrderChar[MAX_USSD_LENGTH] = {0};
            int nBytesLen = WidecharToByte(Unicode, byteOrderChar, UnicodeLen); //转换到正确的字节顺序
            BytesToHexstring(byteOrderChar, UssdString, nBytesLen); //转换到16进制字符
            asprintf(&cmd, "AT+CUSD=1,\"%s\",72", UssdString);    
            break;
        }
    }   

    if (i == UnicodeLen)
    {       
        /* gsm-7bit编码方式 */
        char  Gsm7bit[MAX_USSD_LENGTH] = {0};  //原始gsm-7bit字符
        unsigned char  CompreGsm7bit[MAX_USSD_LENGTH] = {0}; //压缩后的gsm-7bit字符
        int iLength = UnicodeToGsm7bit(Unicode, Gsm7bit, UnicodeLen);  //宽字符转换成gsm-7bit
        int CompressGsm7bitLen = CompressGsm7bit(Gsm7bit, CompreGsm7bit, iLength);   //压缩gsm-7bit
        BytesToHexstring((const unsigned char*)CompreGsm7bit, UssdString, CompressGsm7bitLen); //转换到16进制字符
        asprintf(&cmd, "AT+CUSD=1,\"%s\",15", UssdString);
    }
   
	err = at_send_command(cmd, &p_response);
	free(cmd);
	if(err == 0)
	{
		if(p_response->success == 0)
			RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);   
		else
			RIL_onRequestComplete(t, RIL_E_SUCCESS,NULL, 0);
	}
	else 
	{
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	at_response_free(p_response);
	return;
}
/* END DTS2011100905256 wkf32792 20111110 modified */




/***************************************************
 Function:  hwril_request_ss
 Description:  
    Handle the request about service state. 
 Calls: 
    request_send_ussd
    request_query_facility_lock
 Called By:
    on_request
 Input:
    request - division specific request upon request value
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
void hwril_request_ss (int request, void *data, size_t datalen, RIL_Token token)
{    
	ATResponse *p_response;
	int err;

	switch (request) 
	{
		case RIL_REQUEST_SEND_USSD:
		{
			request_send_ussd(data, datalen, token);
			break;
		}

		case RIL_REQUEST_CANCEL_USSD:
		{
			p_response = NULL;
			err = at_send_command("AT+CUSD=2", &p_response);

			if (err < 0 || p_response->success == 0) {
				RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
			} else {
				RIL_onRequestComplete(token, RIL_E_SUCCESS,NULL, 0);
			}
			at_response_free(p_response);
			break;
		}

		

		default:
		{
			RIL_onRequestComplete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			break;
		}
	}
}

/***************************************************
 Function:  hwril_unsolicited_ss
 Description:  
    Handle the unsolicited about service state. 
    This is called on atchannel's reader thread. 
 Calls: 
    RIL_onUnsolicitedResponse
 Called By:
    on_unsolicited
 Input:
    s - pointer to unsolicited intent
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/

/* BEGIN DTS2011100905256 wkf32792 20111110 modified */
void hwril_unsolicited_ss (const char *s)
{
	char *line = NULL;
	char *line1 = NULL;
	int i = 0;
	char * tmp = NULL;
	int err = 0;
	if (strStartsWith(s,"+CUSD:"))
	{	
		int code = -1;
		int dcs = -1;
		char *str = NULL;
		int nDataLen = 0;
		char *data[2] ={NULL,NULL};
		char hexstr[MAX_USSD_LENGTH+1] = {0};
		char szTemp[MAX_USSD_LENGTH+1] = {0};
		WCHAR wszTemp[MAX_USSD_LENGTH+1] = {0};
		char szUTF8[(MAX_USSD_LENGTH+1)*3] = {0};
		line = strdup(s);
		line1 = line;
		tmp = line;
		while ((i < 2) && (*tmp !='\0'))
		{
	        if (*tmp == ',')
	        i++;
	        tmp++;
		} 
		
		at_tok_start(&line);
	    err = at_tok_nextint(&line, &code);
	    switch(code)
		{
		    
		   case USSD_NOTIFY:
		   case USSD_REQUEST:	
		        /* need to decode */
		        if (2 == i) 
				{
				    err = at_tok_nextstr(&line, &str);
				    err = at_tok_nextint(&line, &dcs);
				  
				    nDataLen = hexStringToBytes(str, hexstr ,1024);//该函数定义在ril-sim.c中，需要修改返回值
				    switch(dcs >> 4)
				    {
					    case 0: //Language using the GSM 7 bit default alphabet
					    {
				            switch(dcs & 0xF)
				            {	                    
			                    case 0x04:  //Spanish
			                    case 0x08:  //Portuguese
			                    case 0x0C:  //Turkish
			                    {
			                    	LANGUAGEID lanID = (LANGUAGEID)(dcs & 0xF);
			                    	int iLength = UncompressGsm7bit(hexstr, szTemp, nDataLen);		                                
			                        //不同国家可能会有不同的7bit表格，此处，暂时用通用7bit表	                        
			                        iLength = Gsm7bitToUnicodeExt(szTemp, wszTemp, iLength, lanID);
			                        UnicodeToUtf8(wszTemp, szUTF8, iLength);   
			                        break;                                                      	
			                    }
			                    case 0x00:  //German
			                    case 0x01:  //English
			                    case 0x02:  //Italian
			                    case 0x03:  //French
			                    case 0x05:  //Dutch
			                    case 0x06:  //Swedish
			                    case 0x07:  //Danish	                    
			                    case 0x09:  //Finnish
			                    case 0x0A:  //Norwegian
			                    case 0x0B:  //Greek	                    
			                    case 0x0D:  //Hungarian
			                    case 0x0E:  //Polish
			                    {		                                
			                        int iLength = UncompressGsm7bit(hexstr, szTemp, nDataLen);		                                
			                        //不同国家可能会有不同的7bit表格，此处，暂时用通用7bit表
			                        iLength = Gsm7bitToUnicode(szTemp, wszTemp, iLength);
			                        UnicodeToUtf8(wszTemp, szUTF8, iLength);   
			                        break;
			                    }                        
			                    default:
			                    {	 
			                    	ALOGD("default");
			                        int iLength = UncompressGsm7bit(hexstr, szTemp, nDataLen);	                       
			                        iLength = Gsm7bitToUnicode(szTemp, wszTemp, iLength);                    
			                        UnicodeToUtf8(wszTemp, szUTF8, iLength);		                        
			                        break;                        
			                    }                        
				             }
				             break;
					    }		    
					    case 0x01:  //
					    {
				            switch(dcs & 0xF)
				            {
				                //GSM 7 bit default alphabet; message preceded by language indication.
				                //The first 3 characters of the message are a two-character representation of the language
				                //encoded according to ISO 639 [12], followed by a CR character. The CR character is then
				                //followed by 90 characters of text.

				                case 0:
				                // nyx add  默认是7bit 解码
				                {		                    
				                     int iLength = UncompressGsm7bit(hexstr, szTemp, nDataLen);

				                     //不同国家可能会有不同的7bit表格，此处，暂时用通用7bit表
				                     iLength = Gsm7bitToUnicode(szTemp, wszTemp, iLength);
				                     UnicodeToUtf8(wszTemp, szUTF8, iLength);		                    
				                     break;
				                }
				                // nyx end

				                //UCS2; message preceded by language indication
				                //The message starts with a two GSM 7-bit default alphabet character representation of the language encoded according to ISO 639 [12].
				                //This is padded to the octet boundary with two bits set to 0 and then followed by 40 characters of UCS2-encoded message.
				                //An MS not supporting UCS2 coding will present the two character language identifier followed by improperly interpreted user data.
				                case 1:
				                // nyx add  阿拉伯版本时 , ussd 返回的国家码是17, 应当作ucs2解
				                {	                    
				                     int iLength = BytesToWidechar(hexstr, wszTemp, nDataLen);
				                     UnicodeToUtf8(wszTemp, szUTF8, iLength);
				                     break;
				                }
				                // nyx end
				                //0010..1111	Reserved
				                default:
				                     break;
				            }
				            break;
					    }   
					    case 0x02:
					    {
				            switch(dcs & 0xF)
				            {
				                case 0://0000           Czech
				                case 1://0001           Hebrew
				                case 2://0010           Arabic
				                case 3://0011           Russian
				                case 4://0100           Icelandic
				                {
				                    int iLength = BytesToWidechar(hexstr, wszTemp, nDataLen);
				                    UnicodeToUtf8(wszTemp, szUTF8, iLength);
				                    break;
				                }
				                //0101..1111	Reserved for other languages using the GSM 7 bit default alphabet, with unspecified handling at the MS
				                default:
				                    break;
				            }
				            break;
					    }		 
					    case 0x09:  //Message with User Data Header (UDH) structure:
					    {
					        switch(dcs & 0x0C)
					        {
			                    case 0:  //GSM 7 bit default alphabet
			                    {		                                
			                        int iLength = UncompressGsm7bit(hexstr, szTemp, nDataLen);
			                        iLength = Gsm7bitToUnicode(szTemp, wszTemp, iLength);
			                        UnicodeToUtf8(wszTemp, szUTF8, iLength);	
			                        break;
			                    }		                            
			                    case 0x04:  //8 bit data
			                    {
			                        int iLength = gsmDecode8bit(hexstr, wszTemp, nDataLen);
			                        UnicodeToUtf8(wszTemp, szUTF8, iLength);
			                        break;
			                    }
			                    case 0x08:  //USC2 (16 bit) [10]
			                    {
			                        int iLength = BytesToWidechar(hexstr, wszTemp, nDataLen);
			                        UnicodeToUtf8(wszTemp, szUTF8, iLength);
			                        break;
			                    }
			                    default:
			                        break;
					        }
					        break;
					    }
				    
				    	case 0x0A:	//Reserved coding groups
				    	case 0x0B:	//Reserved coding groups
				    	case 0x0C:	//Reserved coding groups
				    	case 0x0D:	//Reserved coding groups
					    {
					        //Reserved coding groups
					        break;
					    }		      
					    case 0x0E:  //Defined by the WAP Forum [15]
					    {
					        //Defined by the WAP Forum [15]
					        break;
					    }		        
					    case 0x0F:  //Data coding / message handling
					    {
					        //Data coding / message handling
					        break;
					    }
				        
					    default:  //General Data Coding indication
				        {
				            //Bit 5, if set to 0, indicates the text is uncompressed
				            //Bit 5, if set to 1, indicates the text is compressed using the compression algorithm defined in 3GPP TS 23.042 [13]
				            if (dcs & 0x20) // Bit 5 set 1.
				            {
				                switch(dcs & 0x0C)
				                {
			                        case 0:  //GSM 7 bit default alphabet
			                        {		                                        
			                            int iLength = UncompressGsm7bit(hexstr, szTemp, nDataLen);
			                            iLength = Gsm7bitToUnicode(szTemp, wszTemp, iLength);
			                            UnicodeToUtf8(wszTemp, szUTF8, iLength);	
			                            break;
			                        }                            
			                        case 4:  //8 bit data
			                        {
			                            int iLength = gsmDecode8bit(hexstr, wszTemp, nDataLen);
			                            UnicodeToUtf8(wszTemp, szUTF8, iLength);
			                            break;
			                        }
			                        case 8:  //USC2 (16 bit) [10]
			                        {	                        	
			                            int iLength = BytesToWidechar(hexstr, wszTemp, nDataLen);	                            
			                            UnicodeToUtf8(wszTemp, szUTF8, iLength);
			                            break;
			                        }
			                        default:
			                            break;
				                }
				            }
				            else
				            {
				                switch(dcs & 0x0C)
				                {
			                        case 0:  //GSM 7 bit default alphabet
			                        {	                                        
			                            int iLength = Gsm7bitToUnicode(hexstr, wszTemp, nDataLen);
			                            UnicodeToUtf8(wszTemp, szUTF8, iLength);
			                            break;
			                        }                            
			                        case 4:  //8 bit data
			                        {
			                            int iLength = gsmDecode8bit(hexstr, wszTemp, nDataLen);
			                            UnicodeToUtf8(wszTemp, szUTF8, iLength);
			                            break;
			                        }
			                        case 8:  //USC2 (16 bit) [10]
			                        {	                        	
			                            int iLength = BytesToWidechar(hexstr, wszTemp, nDataLen);	                            
			                            UnicodeToUtf8(wszTemp, szUTF8, iLength);	                           
			                            break;
			                        }
			                        default:
			                            break;
				                }
				            }
				            break;
					    }		     
				    }
				}				
				data[1] = szUTF8;
				break;
		   case USSD_SESSION_TERMINATED_BY_NETWORK:
		   case USSD_OTHER_LOCAL_CLENT_HAS_RESPONDED:
		   case USSD_OPERATION_NOT_SUPPORTED :
		   case USSD_NETWORK_TIMEOUT:
		   default:
		   	    data[1] = "";
		   	    break;
		}
	    
        asprintf(&data[0], "%d", code);	
		if(RIL_DEBUG) ALOGD("code type:%s \nstr: %s",data[0],data[1]);
		RIL_onUnsolicitedResponse (RIL_UNSOL_ON_USSD, (void*)data, sizeof(data));   
		free(data[0]);
		free(line1);
	} 
	return;
}
/* END DTS2011100905256 wkf32792 20111110 modified */

