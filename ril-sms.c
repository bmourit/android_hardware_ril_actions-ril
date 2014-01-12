/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-sms.c
 Author: liubing/00141886 
 Version: V1.0
 Date: 2010/04/07
 Description:
         Handle request and unsolicited about sms.
 Function List:
 History:
 <author>                      <time>                    <version>               <desc>
 liubing                      10/04/07                      1.0       
 fangyuchuang            10/07/28                     1.1  
*******************************************************/

#include "huawei-ril.h"
#include <telephony/ril_cdma_sms.h>

/*BEGIN Added by qkf33988 2010-11-13 CMTI issue*/
#ifdef   CMTI_ISSUE
#include <cutils/sockets.h>
#include <sys/epoll.h>
#include <fcntl.h>
#define MESSAGE_STATUS_REPORT          0
#define NEW_MESSAGE                              1
#define PDU_ERROR                                  -1 
extern int del_sms_control[2]; 
extern pthread_mutex_t s_readdelmutex;
extern Module_Type g_module_type ;

/* BEGIN DTS2011120501094  lixianyi 2011-12-5 modified*/
extern int  get_new_sim_sms_or_sms_report_and_del(char* command);
/* BEGIN DTS2011120501094  lixianyi 2011-12-5 modified */


/* Begin to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
//When using CMGD to delete sms on sim for Infineon,there will be errors,
//so here modify AT command from CMGD to CRSM;howerver you must be careful
//to use CMGD CMGL for QUALCOMM such as MU509 and EM770W and so on
int g_fileid = -1;
char g_smspath[64] = {0};  
/* End to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
/* Begin to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
extern int g_provider;
/* End to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
extern int pdu_type(const char*  pdu);
extern int hexStringToBytes(char * s,char *hs ,int len);
#endif
/*END Added by qkf33988 2010-11-13 CMTI issue*/

/*BEGIN Added by kf33986 2011-4-19 CDMA SMS*/
typedef struct 
{
    int imPos;
    char pdu[1024];
}Cdma_Pdu;


#ifdef   CMTI_ISSUE
#define CMD_CMTI    0xFF
#define CMD_CDSI    0xEE
#define CMD_GSM        0xEF/*Modefied by qkf33988 2011-4-19 CDMA SMS*/
#define CMD_CDMA      0xFE/*Added by qkf33988 2011-4-19 CDMA SMS*/
#endif

/* Begin modified by x84000798 for DTS2012103107785 2012-11-03*/
#define OUTSTANDING_CMT_SMS 	0
#define OUTSTANDING_CDS_REPORT	1
int g_outstanding_sms = 0;
static pthread_mutex_t s_pdu_mutex = PTHREAD_MUTEX_INITIALIZER;
typedef struct wait_pdu_s
{
	int type;
	char *pdu;
	struct wait_pdu_s *next;
}wait_pdu;
wait_pdu *g_wait_pdu = NULL;
SMS_Project g_sms_project = SMS_UNKNOW;
/* End   modified by x84000798 for DTS2012103107785 2012-11-03*/

/************************************************
 Function:  writebits
 Description:  
    向一指定内存按位写入数据
 Calls: 
 Called By: encodepdu
 Input:
    pd-Cdma_Pdu结构体指针，将数据写到结构体成员pdu中
    bits-要写的位数
    data-要写入的数据
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void writebits(Cdma_Pdu *pd, int bits, int data)
{   
        int inde = 0;//修改pclint警告 KF36756 2011-04-21
        int offset = 0; 
        if ((bits < 0) ||(bits > 8))
            return;
        data &= ((~0) >> (32 - bits));
        inde = pd->imPos >> 3;
        offset = 16 - (pd->imPos  & 0x07) - bits;  
        data <<= offset;
        pd->imPos  += bits;
        pd->pdu[inde] |= data >> 8;
        if (offset < 8) pd->pdu[inde + 1] |= data & 0xFF;          
}
typedef struct
{
    int imPos;
    char *sour;
    int ibitEnd;
}CharStr;
/************************************************
 Function:  writebits
 Description:  
    从一指定内存中按位读出数据
 Calls: 
 Called By: decode_Addr
 Input:
    pd-CharStr结构体指针，将数据从结构体成员sour所指向的内存中读出
    bits-要读的位数    
 Output:
    data-读出数据存放的地址
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void getbits(CharStr * pd, int bits, unsigned char *data)
{
    int inde = 0;//修改pclint警告 KF36756 2011-04-21
    int offset = 0;
    int tmp = 0; 
    if ((bits < 0) || (bits > 8))
        return;
    inde = pd->imPos >> 3;
    offset = pd->imPos & 0x07;
   tmp = (((*(pd->sour + inde)  << 8) |*(pd->sour + inde +1))<< offset) & 0x0ffff;
    //ALOGD("tmp ---%04x",tmp);
    //ALOGD(" pd->imPos ----%d", pd->imPos);
    //ALOGD(" bits ----%d", bits);
    *data = tmp >> (16 - bits);
    pd->imPos += bits;
}
/************************************************
 Function:  encodepdu
 Description:  
    将上层传下来的RIL_CDMA_SMS_Message结构体拆离
    并组成PDU包
 Calls: 
 Called By: request_write_sms_to_ruim
                    request_send_cdma_sms
 Input:
   data-上层传下来的RIL_CDMA_SMS_Message结构体指针
   dest-组成的PDU包转成字符串后存放的地址
   Args-RIL_CDMA_SMS_WriteArgs结构体指针
            为兼容将CDMA短信写到RUIM卡中增加的参数
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void encodepdu(void *data, char *dest ,RIL_CDMA_SMS_WriteArgs *Args)
{
        RIL_CDMA_SMS_Message *message;
        Cdma_Pdu cpdu;
        int iAddParLenb = 0;
        int i = 0;//修改pclint警告 KF36756 2011-04-21
        //char c;
        memset(&cpdu,0,sizeof(Cdma_Pdu));
        message = (RIL_CDMA_SMS_Message *)data;
        writebits(&cpdu, 8, 0);//message type
        writebits(&cpdu, 8, 0);//Teleservice identifer type
        writebits(&cpdu, 8, 2);//Teleservice identifer parameter lengh
        //Teleservice identifer
        if(RIL_DEBUG)ALOGD("message->uTeleserviceID-----%04x",message->uTeleserviceID);
        writebits(&cpdu, 8, message->uTeleserviceID >> 8);
        writebits(&cpdu, 8, message->uTeleserviceID);
        //destination address type
        if ((Args != NULL) && (Args->status == 0 ||Args->status ==1))
        {
                writebits(&cpdu, 8,2);
        }
        else
        {
                writebits(&cpdu, 8,4);
        }
        if(message->sAddress.digit_mode == RIL_CDMA_SMS_DIGIT_MODE_4_BIT)
        {
                //if(RIL_DEBUG) ALOGD("sAddress.number_of_digits---%d",message->sAddress.number_of_digits);
                iAddParLenb = 10 + message->sAddress.number_of_digits *4;
                //if(RIL_DEBUG) ALOGD("iAddParLenb---%d--%d--%d",iAddParLenb,(iAddParLenb/8 + (iAddParLenb % 8)? 1:0),(iAddParLenb/8 + ((iAddParLenb % 8)? 1:0)));
                writebits(&cpdu, 8,(iAddParLenb/8 + ((iAddParLenb % 8)? 1:0))); //destination address parameter lengh
                writebits(&cpdu,2,0);
                writebits(&cpdu,8,message->sAddress.number_of_digits);
                for (i = 0; i < message->sAddress.number_of_digits ; i++)
                {
                        writebits(&cpdu, 4, message->sAddress.digits[i]);
                }
                writebits(&cpdu, (iAddParLenb % 8) ? (8-iAddParLenb % 8) : 0, 0);
        }
        else if (message->sAddress.digit_mode == RIL_CDMA_SMS_DIGIT_MODE_8_BIT)
        {
                //if(RIL_DEBUG) ALOGD("sAddress.number_of_digits---%d",message->sAddress.number_of_digits);
                iAddParLenb = 10 + message->sAddress.number_of_digits *8;
                //if(RIL_DEBUG) ALOGD("iAddParLenb---%d--%d--%d",iAddParLenb,(iAddParLenb/8 + (iAddParLenb % 8)? 1:0),(iAddParLenb/8 + ((iAddParLenb % 8)? 1:0)));
                writebits(&cpdu, 8,(iAddParLenb/8 + ((iAddParLenb % 8)? 1:0))); //destination address parameter lengh
                writebits(&cpdu,2,2);
                writebits(&cpdu,8,message->sAddress.number_of_digits);
                for (i = 0; i < message->sAddress.number_of_digits ; i++)
                {
                        writebits(&cpdu, 8, message->sAddress.digits[i]);
                }
                writebits(&cpdu, (iAddParLenb % 8) ? (8-iAddParLenb % 8) : 0, 0);
        }
        //user data
        writebits(&cpdu, 8, 8);  
        writebits(&cpdu, 8, message->uBearerDataLen);
        if(RIL_DEBUG)ALOGD("message->uBearerDataLen-----%04x",message->uBearerDataLen);
        for (i = 0; i < message->uBearerDataLen; i++)
        {
                writebits(&cpdu,8,message->aBearerData[i]);
        }
        for ( i = 0; i < (cpdu.imPos >> 3); i++)//修改pclint警告 KF36756 2011-04-21
        {
                sprintf(dest+i*2,"%02X",cpdu.pdu[i]);
        }
}
/************************************************
 Function:  decode_TeleserviceID
 Description:  
    解析上报PDU包中的TeleserviceID,并保存在RIL_CDMA_SMS_Message
    结构体对应的成员中
 Calls: 
 Called By: cdma_pdu_decode
 Input:
   mes-RIL_CDMA_SMS_Message结构体指针，将解析的TeleverviceID保存
            在这个指针指定的RIL_CDMA_SMS_Message的对应成员中
   sourc-char型的指针，这个指针就是TeleverviceID在上报的PDU中的位置
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void decode_TeleserviceID(RIL_CDMA_SMS_Message * mes,char *sourc)
{
        mes->uTeleserviceID = (sourc[1] << 8) | sourc[2];
}
/************************************************
 Function:  decode_Addr
 Description:  
    解析上报PDU包中的地址(电话号码)
 Calls: 
 Called By: cdma_pdu_decode
 Input:
   mes-RIL_CDMA_SMS_Message结构体指针，将解析的电话号码保存
            在这个指针指定的RIL_CDMA_SMS_Message的对应成员中
   sourc-char型的指针，这个指针就是sAddress在上报的PDU中的位置
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void decode_Addr(RIL_CDMA_SMS_Message * mes, char * sourc)
{
        CharStr str;
        int i;
        memset(&str,0,sizeof(CharStr));
        unsigned char cDigitMode = 0;//修改pclint警告 KF36756 2011-04-21
        unsigned char cNumberMode = 0;
        unsigned char ucHigh = 0;
        str.imPos = 0;
        str.sour = sourc +1;
        str.ibitEnd = (*sourc)*8;
        //ALOGD("str.imPos =  str.sour =  str.ibitEnd = %d", str.ibitEnd);
        getbits(&str,1,&cDigitMode);
        //ALOGD("cDigitMode---%d ",cDigitMode);
        getbits(&str,1,&cNumberMode);
        //ALOGD("cNumberMode---%d ",cNumberMode);
        getbits(&str,8,&ucHigh);
        //getbits(&str,8,&ucLow);
        //ALOGD("ucHigh---%d ",ucHigh);
        //mes->sAddress.number_of_digits = (ucHigh << 8) |ucLow;
        mes->sAddress.number_of_digits = ucHigh ;
        if (cDigitMode == RIL_CDMA_SMS_DIGIT_MODE_4_BIT)
        {
                for (i = 0 ; i < mes->sAddress.number_of_digits; i++)
                {
                        getbits(&str,4,&ucHigh);
                        mes->sAddress.digits[i] = ucHigh;
                }
        }
        else if (cDigitMode == RIL_CDMA_SMS_DIGIT_MODE_8_BIT)
        {
                 for (i = 0 ; i < mes->sAddress.number_of_digits; i++)
                {
                        getbits(&str,8,&ucHigh);
                        mes->sAddress.digits[i] = ucHigh;
                }
        }
}
/************************************************
 Function:  decode_BearerData
 Description:  
    解析上报PDU包中的BearerData
 Calls: 
 Called By: cdma_pdu_decode
 Input:
   mes-RIL_CDMA_SMS_Message结构体指针，将解析的BearerData保存
            在这个指针指定的RIL_CDMA_SMS_Message的对应成员中
   sourc-char型的指针，这个指针就是BearerData在上报的PDU中的位置
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void decode_BearerData(RIL_CDMA_SMS_Message * mes, char * sourc)
{
    mes->uBearerDataLen = sourc[0];
    memcpy(mes->aBearerData,sourc + 1,sourc[0]);
    //memcpy(mes->aBearerData,sourc - 1,sourc[0]+2);
}
/************************************************
 Function:  cdma_pdu_decode
 Description:  
    解析上报PDU包
 Calls: 
 Called By: RecievePduSms
 Input:
   mes-RIL_CDMA_SMS_Message结构体指针，将解析的PDU包保存在
            这个指针指定的RIL_CDMA_SMS_Message结构体中
   sourc-char型的指针，指向上报PDU的首地址
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void cdma_pdu_decode(RIL_CDMA_SMS_Message * mes, char * sourc )
{
        char hexstr[1024] = "";
        int ilen = 0, i = 0, iNum = 0;
        int iID = -1;
        ilen = strlen(sourc)/2;
        hexStringToBytes(sourc,hexstr ,1024);
        //00 02 08
        i= 0;
        while(i < ilen && iNum < 3)
        {
            i++;
            iID = hexstr[i];
            switch(iID)
            {
                case 0:
                    i++; //传进去的是长度的地址                    
                    decode_TeleserviceID(mes,hexstr +i);
                    i = i + hexstr[i];
                    iNum++;
                    break;
                case 2:
                case 4:
                    i++;                    
                    decode_Addr(mes,hexstr +i);
                    i = i + hexstr[i];
                    iNum++;
                    break;
                case 8:
                    i++;
                    decode_BearerData(mes,hexstr +i);
                    i = i + hexstr[i];
                    iNum++;
                    break;
                 default:
                    i++;
                    i = i + hexstr[i];
                    break;
            }
            
        }    
}
/************************************************
 Function:  RecievePduSms
 Description:  
    解析上报的PDU包，并上报给上层收到新短信
 Calls: 
 Called By: read_sms_on_sim_and_unsol_response
 Input:
   pud-char型指针，指向上报的PDU首地址
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
void RecievePduSms(char * pdu,const char mode) // Modified by x84000798 2012-06-11 DTS2012061209625
{
        RIL_CDMA_SMS_Message *tt;
        //int i ;
        tt = (RIL_CDMA_SMS_Message *)malloc(sizeof(RIL_CDMA_SMS_Message));//修改pclint警告 KF36756 2011-04-21
        memset(tt,0,sizeof(RIL_CDMA_SMS_Message));
        cdma_pdu_decode(tt,pdu);
		
	if(CMD_CMTI== mode)
	{
		RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_CDMA_NEW_SMS, (void*)tt, sizeof(RIL_CDMA_SMS_Message));
	}
	else
	{
		RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, (void*)tt, sizeof(RIL_CDMA_SMS_Message));
	}	
        
        free(tt);
}
/************************************************
 Function:  request_write_sms_to_ruim
 Description:  
    将CDMA短信保存在RUIM中
 Calls: 
 Called By: hwril_request_sms
 Input:
   data-RIL_CDMA_SMS_WriteArgs结构体指针
   datalen-参数长度
   t-request标号
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
static void request_write_sms_to_ruim(void * data, size_t datalen, RIL_Token t)
{
        int err;
        char *cmd1;
        RIL_CDMA_SMS_WriteArgs *WriteArgs;
        ATResponse *p_response = NULL;
        char pdudata[2048] = "";
        WriteArgs = (RIL_CDMA_SMS_WriteArgs *)data;
        encodepdu(&(WriteArgs->message), pdudata,WriteArgs);
        asprintf(&cmd1, "AT^HCMGW=%d,%d", strlen(pdudata)/2,WriteArgs->status);
        err =  at_send_command_sms(cmd1,pdudata,"^HCMGW:",&p_response);
        free(cmd1);
       if (err < 0 || p_response->success == 0) goto error;

	/* FIXME fill in messageRef and ackPDU */
	/*BEGIN Added by QKF33988 2010-11-30 DTS2010112405154 SMS Statues Report*/
	int response = 0;
	if(p_response->p_intermediates != NULL)
	{
		char *line = p_response->p_intermediates->line;
                 // if(RIL_DEBUG)ALOGD("p_intermediates->line :%s",line);
		err = at_tok_start(&line);
		
		if (err < 0) {
			goto error;
		}

		err = at_tok_nextint(&line, &response);

		if (err < 0) {
			goto error;
		}
	}
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
	at_response_free(p_response);
	return;
error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
        
}

#if 0
static void unsol_cdma_ruim_sms_full(void)
{
        RIL_onUnsolicitedResponse (RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL, NULL, NULL);
        return ;
}
#endif
/*END Added by kf33986 2011-4-19 CDMA SMS*/
/************************************************
 Function:  request_send_cdma_sms
 Description:  
    发送CDMA短信
 Calls: 
 Called By: hwril_request_sms
 Input:
   data-RIL_CDMA_SMS_WriteArgs结构体指针
   datalen-参数长度
   t-request标号
 Output:
    none
 Return:
    none
 Others:
    lkf333986 2011-04-19
**************************************************/
static void request_send_cdma_sms(void * data, size_t datalen, RIL_Token t)
{
        if(RIL_DEBUG)ALOGD("--------------requeset_send_cdma_sms-------");
        int err;
        char *cmd1;
        //int type = -1;//修改pclint警告 KF36756 2011-04-21
        //RIL_CDMA_SMS_Message *messag;
        RIL_SMS_Response response;
        ATResponse *p_response = NULL;
        char pdudata[2048] = "";
        encodepdu(data, pdudata,NULL);
        
        asprintf(&cmd1, "AT^HCMGS=%d", strlen(pdudata)/2);
        err =  at_send_command_sms(cmd1,pdudata,"^HCMGS:",&p_response);
        //if(RIL_DEBUG) ALOGD("pdudata: %s",pdudata);
        free(cmd1);
        if (err < 0 || p_response->success == 0) goto error;

	memset(&response, 0, sizeof(response));
	int mr = 0;
	if(p_response->p_intermediates != NULL)
	{
		char *line = p_response->p_intermediates->line;
                 if(RIL_DEBUG)ALOGD("p_intermediates->line :%s",line);
		err = at_tok_start(&line);
		
		if (err < 0) {
			goto error;
		}

		err = at_tok_nextint(&line, &mr);

		if (err < 0) {
			goto error;
		}
	}
	response.messageRef = mr;
	response.ackPDU=NULL;
	response.errorCode=0;
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
	at_response_free(p_response);
	
	return;
error:
         if(RIL_DEBUG)ALOGD("error");
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
}

static void request_send_sms(void *data, size_t datalen, RIL_Token t)
{
	int err;
	const char *smsc;
	const char *pdu;
	int tpLayerLength;
	char *cmd1, *cmd2;
	RIL_SMS_Response response;
	ATResponse *p_response = NULL;

	smsc = ((const char **)data)[0];
	pdu = ((const char **)data)[1];

	tpLayerLength = strlen(pdu)/2;

	if(RIL_DEBUG) ALOGD( "smsc : %s",smsc);
	if (smsc == NULL) {
		smsc= "00";
	}
	asprintf(&cmd1, "AT+CMGS=%d", tpLayerLength);

	asprintf(&cmd2, "%s%s", smsc, pdu);

	if(RIL_DEBUG) ALOGD( "cmd2 :%s",cmd2);
	err = at_send_command_sms(cmd1, cmd2, "+CMGS:", &p_response);

	free(cmd1);
	free(cmd2);
	
	if (err < 0 || p_response->success == 0) goto error;

	memset(&response, 0, sizeof(response));

	/* FIXME fill in messageRef and ackPDU */
	/*BEGIN Added by QKF33988 2010-11-30 DTS2010112405154 SMS Statues Report*/
	int mr = 0;
	if(p_response->p_intermediates != NULL)
	{
		char *line = p_response->p_intermediates->line;

		err = at_tok_start(&line);
		
		if (err < 0) {
			goto error;
		}

		err = at_tok_nextint(&line, &mr);

		if (err < 0) {
			goto error;
		}
	}
	response.messageRef = mr;
	response.ackPDU=NULL;
	response.errorCode=0;
	/*END Added by QKF33988 2010-11-30 DTS2010112405154*/
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));
	at_response_free(p_response);
	return;
	
error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
}

/***************************************************
 Function:  dequedue_pdu
 Description:  dequeue pdu in the pdu list
 Calls: none
 Called By: reques_sms_acknowledge
 Input: 	none
 Output:    none
 Return:    sms pdu
 Others:    Added by x84000798 for DTS2012103107785 2012-11-03
**************************************************/
wait_pdu *dequeue_pdu()
{
	wait_pdu *wpdu = NULL;
	if (NULL != g_wait_pdu)
	{
		wpdu = g_wait_pdu;
		g_wait_pdu = wpdu->next;
		wpdu->next = NULL;
	}
	return wpdu;
}

/***************************************************
 Function:  enquedue_pdu
 Description:  endueue pdu waiting for acknowledge for previous sms
 Calls: RIL_onUnsolicitedResponse
 Called By: cmt_new_sms
 Input: 
 	type - unsolicited type
 	pdu  - pointer to hex string
 Output:    none
 Return:    none
 Others:    Added by x84000798 for DTS2012103107785 2012-11-03
**************************************************/
void enqueue_pdu(int type, const char *pdu)
{
	wait_pdu *wpdu = malloc(sizeof(*wpdu));
	if (NULL == wpdu)
	{
		ALOGD("%s() malloc failed", __FUNCTION__);
		return;
	}
	memset(wpdu, 0, sizeof(*wpdu));
	wpdu->type = type;
	wpdu->pdu  = strdup(pdu);
	if (NULL == wpdu->pdu)
	{
		ALOGD("%s() strdup sms pdu failed", __FUNCTION__);
		return;
	}
	if (NULL == g_wait_pdu)
	{
		g_wait_pdu = wpdu;
	}
	else
	{
		wait_pdu *p = g_wait_pdu;
		while (NULL != p->next)
		{
			p = p->next;
		}
		p->next = wpdu;
	}	
}

/***************************************************
 Function:  cmt_new_sms
 Description:  handle new sms
 Calls: RIL_onUnsolicitedResponse
 Called By: hwril_unsolicited_sms
 Input: pdu - pointer to hex string
 Output:    none
 Return:    none
 Others:    Added by x84000798 for DTS2012103107785 2012-11-03
**************************************************/
void cmt_new_sms(const char *pdu)
{
	pthread_mutex_lock(&s_pdu_mutex);
	if (g_outstanding_sms)
	{
    /* No RIL_UNSOL_RESPONSE_NEW_SMS or RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT
     * messages should be sent until a RIL_REQUEST_SMS_ACKNOWLEDGE has been received for
     * previous new SMS.
     */	
		ALOGD("Waiting for acknowledge for previous sms, enqueueing PDU");
		enqueue_pdu(OUTSTANDING_CMT_SMS, pdu);
	}
	else
	{
		g_outstanding_sms = 1;
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS, pdu, strlen(pdu));
	}
	pthread_mutex_unlock(&s_pdu_mutex);
}

/***************************************************
 Function:  cmt_new_sms
 Description:  handle new sms
 Calls: RIL_onUnsolicitedResponse
 Called By: hwril_unsolicited_sms
 Input: pdu - pointer to hex string
 Output:    none
 Return:    none
 Others:    Added by x84000798 for DTS2012103107785 2012-11-03
**************************************************/
void cds_new_sms_report(const char *pdu)
{
	pthread_mutex_lock(&s_pdu_mutex);
	if (g_outstanding_sms)
	{
    /* No RIL_UNSOL_RESPONSE_NEW_SMS or RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT
     * messages should be sent until a RIL_REQUEST_SMS_ACKNOWLEDGE has been received for
     * previous new SMS.
     */	
		ALOGD("Waiting for acknowledge for previous sms, enqueueing PDU");
		enqueue_pdu(OUTSTANDING_CDS_REPORT, pdu);
	}
	else
	{
		g_outstanding_sms = 1;
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, pdu, strlen(pdu));
	}
	pthread_mutex_unlock(&s_pdu_mutex);
}

/* Begin modified by x84000798 for DTS2012103107785 2012-11-03*/
static void request_sms_acknowledge(void *data, size_t datalen, RIL_Token t)
{
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	wait_pdu *wpdu = NULL;

	pthread_mutex_lock(&s_pdu_mutex);
	wpdu = dequeue_pdu();
	if (NULL != wpdu)
	{
		int unsol_response = 0;
		if (OUTSTANDING_CMT_SMS == wpdu->type)
		{
			unsol_response = RIL_UNSOL_RESPONSE_NEW_SMS;
		}
		else if (OUTSTANDING_CDS_REPORT == wpdu->type)
		{
			unsol_response = RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT;
		}

		RIL_onUnsolicitedResponse(unsol_response, wpdu->pdu, strlen(wpdu->pdu));
		free(wpdu->pdu);
		free(wpdu);
	}
	else
	{
		g_outstanding_sms = 0;
	}
	pthread_mutex_unlock(&s_pdu_mutex);
}
/* Begin modified by x84000798 for DTS2012103107785 2012-11-03*/

static void request_cdma_sms_acknowledge(void *data, size_t datalen, RIL_Token t)
{
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	return;
}

static void request_write_sms_to_sim(void *data, size_t datalen, RIL_Token t)
{

	RIL_SMS_WriteArgs *p_args;
	char *cmd;
	int length;
	int err;
	char* pdu; 
	ATResponse *p_response = NULL;
	
   	if( NULL == data )
    	goto error;
	
	p_args = (RIL_SMS_WriteArgs *)data;
	length = strlen(p_args->pdu)/2;
	if(p_args->smsc == NULL)
	{
		if(RIL_DEBUG) ALOGD( "---begin to write 1");
		asprintf(&cmd, "AT+CMGW=%d,%d", length, p_args->status);
		err = at_send_command_sms(cmd, p_args->pdu, "+CMGW:", &p_response);
	}
	else
	{
		if(RIL_DEBUG) ALOGD( "---begin to write 1");
		asprintf(&pdu, "%s%s",  p_args->smsc, p_args->pdu);
		asprintf(&cmd, "AT+CMGW=%d,%d", length, p_args->status);
		err = at_send_command_sms(cmd, pdu, "+CMGW:", &p_response);
		free(pdu);
	}
	free(cmd);
	
	if (err < 0 || p_response->success == 0) 
	goto error;
	/* Begin modified by x84000798 2012-06-04 DTS2012060806018 */
	char *line = NULL;
	int i_index = 0;
	line = p_response->p_intermediates->line;
	err = at_tok_start(&line);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextint(&line, &i_index);
	if (0 > err)
	{
		goto error;
	}	
	RIL_onRequestComplete(t, RIL_E_SUCCESS, &i_index, sizeof(int *));
	/* End modified by x84000798 2012-06-04 DTS2012060806018 */
	at_response_free(p_response);
    if(RIL_DEBUG) ALOGD( "---end of  writing ");
	return;
error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
}

/* Begin to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
static void request_delete_sms_on_sim(void *data, size_t datalen, RIL_Token t)
{
       //attention:do not delete this part of codes,maybe useful after.
#if 0
        
	char * cmd;
	int err;
	ATResponse *p_response = NULL;
	
	/* Begin to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
    int i_index = ((int *)data)[0];
    const char c_data[ ] = "00FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";
	asprintf(&cmd, "AT+CRSM=220,%d,%d,4,176,\"%s\",\"%s\"",g_fileid, i_index,c_data,g_smspath);
    /* End to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
	
	err = at_send_command(cmd, &p_response);
	free(cmd);
	
	if (err < 0 || p_response->success == 0) {
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	} else {
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	}
	at_response_free(p_response);
#endif
  
   ATResponse *p_response = NULL;
   int err = -1;
   char *cmd = NULL;

   switch(g_provider)
   {
	   case QUALCOMM:
	   default:   
		   asprintf(&cmd,"AT+CMGD=%d",((int*)data)[0]-1);
		   break;
       case HUAWEIB:
       case DATANG:
       case ICERA:
       case MARVELL:
       case HUAWEIH:
	   case T3G:
	   case STE:
	   case NXP:
	   case INFINEON:
		   asprintf(&cmd,"AT+CMGD=%d",((int*)data)[0]);
		   break; 
   }
   if(RIL_DEBUG)ALOGD("*****************cmd is:%s**************",cmd);
   err = at_send_command(cmd, &p_response);
   free(cmd);

   if (err < 0 || p_response->success == 0) 
   {
	   RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
   } 
   else 
   {
	   RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
   }
   at_response_free(p_response);
   return;
}

static void request_delete_sms_on_ruim(void *data, size_t datalen, RIL_Token t)
{
 //attention:do not delete this part of codes,maybe useful after.
#if 0
    char * cmd;
	int err;
	ATResponse *p_response = NULL;
	
	/* Begin to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
    int i_index = ((int *)data)[0];
    const char c_data[ ] = "00FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF"
                           "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF";
	asprintf(&cmd, "AT+CRSM=220,%d,%d,4,255,\"%s\",\"%s\"",g_fileid, i_index,c_data,g_smspath);
    /* End to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
    
	err = at_send_command(cmd, &p_response);
	free(cmd);
	
	if (err < 0 || p_response->success == 0) {
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	} else {
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	}
	at_response_free(p_response);
#endif

   ATResponse *p_response = NULL;
   int err = -1;
   char *cmd = NULL;
 
   switch(g_provider)
   {
	   case QUALCOMM:
	   default:   
		   asprintf(&cmd,"AT+CMGD=%d",((int*)data)[0]-1);
		   break;
       case HUAWEIB:
       case DATANG:
       case ICERA:
       case MARVELL:
       case HUAWEIH:
	   case T3G:
	   case STE:
	   case NXP:
	   case INFINEON:
		   asprintf(&cmd,"AT+CMGD=%d",((int*)data)[0]);
		   break; 
   }
   if(RIL_DEBUG)ALOGD("***********ruim******cmd is:%s**************",cmd);
   err = at_send_command(cmd, &p_response);
   free(cmd);

   if (err < 0 || p_response->success == 0) 
   {
	   RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
   } 
   else 
   {
	   RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
   }
   at_response_free(p_response);
   return;
}
 /* End to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
 
void  request_get_smsc_address(RIL_Token t)
{  
    char *line,*addr;
    int err;
    ATResponse *p_response = NULL;
    
    err = at_send_command_singleline("AT+CSCA?", "+CSCA:", &p_response);

   if(err <0)                                      
         goto error;  
         
    else if (err == 0 && p_response->success == 0)  
         goto error;

        line = p_response->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0) {
        goto error;
    }

     err = at_tok_nextstr(&line, &addr);

    if (err < 0) {
        goto error;
    }
    if(RIL_DEBUG) ALOGD( "smsc address is %s",addr);
     RIL_onRequestComplete(t, RIL_E_SUCCESS, addr, strlen(addr));  
    at_response_free(p_response);
    return; 
error:
    at_response_free(p_response);    
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return ;              
}
/*begin modified by lifei kf34826  2010.12.25*/
void  request_set_smsc_address(void *data, size_t datalen, RIL_Token t)
{  
    char *cmd;
    const char *addr;
    int err;
    ATResponse *p_response = NULL;

    addr = (const char *)data;
    if(RIL_DEBUG) ALOGD( "set smsc address is %s",addr);
    asprintf(&cmd, "AT+CSCA=\"%s\"", addr);
    err = at_send_command(cmd, &p_response);
    free(cmd);
    if (err < 0 || p_response->success == 0) {
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	} else {
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	} 
    return ;              
}
/*end modified by lifei kf34826 2010.12.25*/

/* Begin modified by x84000798 2012-06-20*/
static void request_gsm_sms_broadcast_activation(void *data, size_t datalen, RIL_Token t)
{
	char *cmd = NULL;
	char *line = NULL;
	ATResponse *p_response = NULL;
	int mode, mt, bm, ds, bfr, skip;
	int err, activation;

    /* AT+CNMI=[<mode>[,<mt>[,<bm>[,<ds>[,<bfr>]]]]] */
    err = at_send_command_singleline("AT+CNMI?", "+CNMI:", &p_response);
	if (0 > err || 0 == p_response->success)
	{
		goto error;
	}
    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (0 > err)
        goto error;
    err = at_tok_nextint(&line, &mode);
    if (0 > err)
        goto error;
    err = at_tok_nextint(&line, &mt);
    if (0 > err)
        goto error;
    err = at_tok_nextint(&line, &skip);
    if (0 > err)
        goto error;
    err = at_tok_nextint(&line, &ds);
    if (0 > err)
        goto error;
    err = at_tok_nextint(&line, &bfr);
    if (0 > err)
        goto error;

    /* 0 - Activate, 1 - Turn off */
    activation = *((const int *)data);
    if (activation == 0)
        bm = 2;
    else
        bm = 0;
	if (RIL_DEBUG)
	if (RIL_DEBUG)ALOGD("broadcast mode is : %d", mode);

	asprintf(&cmd, "AT+CNMI=%d, %d, %d, %d, %d", mode, mt, bm, ds, bfr);
	err = at_send_command(cmd, NULL);
	free(cmd);//add by x84000798
	if (err < 0)
		goto error;
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	return ;

error:
	if (RIL_DEBUG)
		ALOGD("request_gsm_sms_broadcast_activation failure!!!");
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	return;
}
/* End modified by x84000798 2012-06-20*/

/* begin added by x84000798 2012.3.29*/
/*************************************************
 Function:  request_gsm_set_broadcast_sms_config
 Description:  
    set the configuration of gsm broadcast sms 
 Calls: 
    NULL
 Called By:
    hwril_request_sms
 Input:
    const RIL_GSM_BroadcastSmsConfigInfo **
 Output:
    NULL
 Return:
    none
 Others:
    add by x84000798
**************************************************/
static void request_gsm_set_broadcast_sms_config(void *data, size_t datalen, RIL_Token t)
{
	int err;
	int mode;
	int count = (datalen/sizeof(RIL_GSM_BroadcastSmsConfigInfo *));
	char mids[30] = {0};
	const RIL_GSM_BroadcastSmsConfigInfo **config = (const RIL_GSM_BroadcastSmsConfigInfo **)data;

	int i = 0;
	for (i=0; i<count; i++)
	{
		char *cmd = NULL;
		/*be careful, selected is 0 means not accepted,1 means accepted*/
		/*but mode is 0 means accepted, 1 means not accepted*/
		((config[i]->selected)==0) ? (mode=1) : (mode=0);
		int j = ((config[i]->fromServiceId) == (config[i]->toServiceId));
		if ( j )
		{
			sprintf(mids, "%d", config[i]->fromServiceId);
		}
		else
		{
			sprintf(mids, "%d-%d", config[i]->fromServiceId, config[i]->toServiceId); 
		}
		asprintf(&cmd, "AT+CSCB=%d,\"%s\"", mode, mids);
		err = at_send_command(cmd, NULL);
		free(cmd);
		if (err < 0)
		{
			goto error;
		}
	}
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	return;

error:
	if (RIL_DEBUG)
	{
		ALOGE("request_gsm_set_broadcast_sms_config failure!!!");
	}
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	return;
}



/*************************************************
 Function:  request_gsm_get_broadcast_sms_config
 Description:  
    get the configuration of gsm broadcast sms 
 Calls: 
    NULL
 Called By:
    hwril_request_sms
 Input:
    NULL
 Output:
    const RIL_GSM_BroadcastSmsConfigInfo **
 Return:
    none
 Others:
    add by x84000798
**************************************************/
static void request_gsm_get_broadcast_sms_config(void *data, size_t datalen, RIL_Token t)
{
	int err, mode; 
	int count = 0;		//count of the RIL_GSM_BroadcastSmsConfigInfo* in the  pointer array
	char *line = NULL;
	char *allmids = NULL;
	char *tmp_allmids = NULL;
	RIL_GSM_BroadcastSmsConfigInfo **result = NULL;
	ATResponse *p_response = NULL;

	err = at_send_command_singleline("AT+CSCB?", "+CSCB:", &p_response);
	if (err < 0 || 0 == p_response->success)
	{
		goto error;
	}

	line = p_response->p_intermediates->line;

	err = at_tok_start(&line);
	if (err < 0)
	{
		goto error;
	}
	err = at_tok_nextint(&line, &mode);
	if (err < 0) 
	{
		goto error;
	}
	err = at_tok_nextstr(&line, &allmids);
	if (err < 0)
	{
		goto error;
	}
	if (RIL_DEBUG)
	{
		ALOGD("all mids is %s", allmids);
	}
	int i = 0;
	for (tmp_allmids = allmids; *tmp_allmids != '\0'; tmp_allmids++)	//calculate the number of ',' in mids
	{
		if (*tmp_allmids == ',')
		{
			count++;
		}
	}
	if (0 == count && !strlen(allmids))	//if AT+CSCB? return +CSCB:0,"","" or +CSCB:1,"",""
	{
		goto error;
	}
	count += 1;
	int j = 0;
	while (j < count)
	{
		char *rangeofServiceId = NULL;	//such as 12 or 23-34
		at_tok_nextstr(&allmids, &rangeofServiceId);
		result[j]->selected = 1;//Notify,all the config we can poll is accepted
		result[i]->fromServiceId = atoi(rangeofServiceId);
		char *toserviceId = strchr(rangeofServiceId, '-');
		if (toserviceId != NULL)	//such as 23-34
		{
			toserviceId++;
			result[i]->toServiceId = atoi(toserviceId);			
		}
		else
		{
			result[i]->toServiceId = atoi(rangeofServiceId);
		}
		j++;
	}
	const RIL_GSM_BroadcastSmsConfigInfo **realresult = (const RIL_GSM_BroadcastSmsConfigInfo **)result;
	RIL_onRequestComplete(t, RIL_E_SUCCESS, realresult, count*sizeof(RIL_GSM_BroadcastSmsConfigInfo *));
	at_response_free(p_response);
	return;

error:
	if (RIL_DEBUG)
	{
		ALOGE("request_gsm_get_broadcast_sms_config failure!!!");
	}
	at_response_free(p_response);
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	return;	
}
/* end added by x84000798 2012.3.29*/

/***************************************************
 Function:  hwril_request_sms
 Description:  
    Handle the request about sms. 
 Calls: 
    request_send_sms
    request_sms_acknowledge
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
    Modeify by fangyuchuang/00140058 for for balong android
**************************************************/
void hwril_request_sms (int request, void *data, size_t datalen, RIL_Token token)
{    

	switch (request) 
	{
		case RIL_REQUEST_SEND_SMS:
		{
			request_send_sms(data, datalen, token);
			break;
		}

		case RIL_REQUEST_SMS_ACKNOWLEDGE:
		{
			request_sms_acknowledge(data, datalen, token);
			break;
		}

		case RIL_REQUEST_WRITE_SMS_TO_SIM:
		{
			request_write_sms_to_sim(data, datalen, token);
			break;
		}

		case RIL_REQUEST_DELETE_SMS_ON_SIM:
		{
			request_delete_sms_on_sim(data, datalen, token);
			break;
		}
        case RIL_REQUEST_GET_SMSC_ADDRESS:
		{
            request_get_smsc_address(token);
			break;
		}
		case RIL_REQUEST_SET_SMSC_ADDRESS:
		{
            request_set_smsc_address(data, datalen,token);
			break;
		}
		/*BEGIN Added by kf33986 2011-4-19 CDMA SMS*/
		case RIL_REQUEST_CDMA_SEND_SMS:
		{
		    request_send_cdma_sms(data,datalen,token);
		    break;
		}
		case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE:
		{
        	request_cdma_sms_acknowledge(data, datalen, token);
		    break;
		}
		case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM:
		{
		    request_delete_sms_on_ruim(data, datalen, token);
		    break;
		}
		case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM:
		{
		    request_write_sms_to_ruim(data, datalen, token);
		    break;   
		}
		/*END Added by kf33986 2011-4-19 CDMA SMS*/
		
		/* begin added by lkf52201 2011.10.11 */
		case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:
		{
			request_gsm_sms_broadcast_activation(data, datalen, token);
			break;//add by x84000798
		}
		/* end added by lkf52201 2011.10.11 */
		
		/* begin added by x84000798 2012.3.29*/
		case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG:
		{
			request_gsm_get_broadcast_sms_config(data, datalen, token);
			break;
		}
		case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG:
		{
			request_gsm_set_broadcast_sms_config(data, datalen, token);
			break;
		}
		/* end added by x84000798 2012.3.29*/
		default:
		{
			RIL_onRequestComplete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			break;
		}
	}
}
/*BEGIN Added by qkf33988 2010-11-13 CMTI issue*/
#ifdef   CMTI_ISSUE


static int epoll_register( int  epoll_fd, int  fd )
{
	struct epoll_event  ev;
	int                 ret, flags;

	/* important: make the fd non-blocking */
	flags = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	ev.events  = EPOLLIN;
	ev.data.fd = fd;
	do {
		ret = epoll_ctl( epoll_fd, EPOLL_CTL_ADD, fd, &ev );
	} while (ret < 0 && errno == EINTR);
	return ret;
}


static int epoll_deregister( int  epoll_fd, int  fd )
{
	int  ret;
	do {
		ret = epoll_ctl( epoll_fd, EPOLL_CTL_DEL, fd, NULL );
	} while (ret < 0 && errno == EINTR);
	return ret;
}

#endif
/*END Added by qkf33988 2010-11-13 CMTI issue*/

/*BEGIN Added by qkf33988 2010-11-16 CMTI issue*/
#ifdef   CMTI_ISSUE
int read_sms_on_sim_and_unsol_response(const char id,const char mode)
{
	ATResponse *p_response;
	ATLine *p_cur;	
	char atcmd[50];
	char  *line;
	int state;
	int reserved;
	int length;
	char *pdu;
	int err;
	
    /* BEGIN DTS2011120206709 kf32792 20111205 deleted */
	// static char pre_pdu[500];
    /* END DTS2011120206709 kf32792 20111205 deleted */
    
	/*GEGIN Added by kf33986 2011-4-19 CDMA SMS*/
	if(NETWORK_CDMA==g_module_type)
       {
	    sprintf(atcmd, "AT^HCMGR=%d", id);
	    err = at_send_command_multiline(atcmd, "^HCMGR:", &p_response);
       }
	/*ENDAdded by kf33986 2011-4-19 CDMA SMS*/
       else
       {
	    sprintf(atcmd, "AT+CMGR=%d", id);
	    err = at_send_command_multiline(atcmd, "+CMGR:", &p_response);
        }
	 
	if (err < 0 || p_response->success == 0)
	{
		return -1;
	}
	p_cur = p_response->p_intermediates;
	if(p_cur == NULL)
	{
		at_response_free(p_response);
		return -1;
	}
	line = p_cur->line;
	err = at_tok_start(&line);
	if (err < 0) goto delete;
	
	err = at_tok_nextint(&line, &state);
	if (err < 0 ) goto delete;
	
	err = at_tok_nextint(&line, &reserved);
	err = at_tok_nextint(&line, &length);
	if (err < 0) goto delete;
	
	p_cur = p_cur->p_next;
	if(NULL==p_cur)  goto delete;
	
	line = p_cur->line;
	err = at_tok_nextstr(&line, &pdu);
	if (err < 0) goto delete;

	/* BEGIN DTS2011120206709 kf32792 20111205 deleted */
	/*
	if(0 ==strcmp(pre_pdu,pdu))
	{
		goto delete;
	}
	
	strcpy(pre_pdu,pdu);
	*/
    /* END DTS2011120206709 kf32792 20111205 deleted */
    
	if(NETWORK_CDMA==g_module_type)
	{
		RecievePduSms(pdu,mode);
	}
	else
	{
		if(CMD_CMTI== mode)
		{
			if(RIL_DEBUG)ALOGD("___________There has a new message ______________");
			RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NEW_SMS, pdu, length);
		}
		else
		{
			RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, pdu, length);
		}
	}

	
delete:
	//delete sms from SIM
	at_response_free(p_response);
	sprintf(atcmd, "AT+CMGD=%d", id);
	err = at_send_command(atcmd, NULL);
	return 1;
	//Unsolicited the new sm

}

/***************************************************
 Function:  hwril_unsolicited_sms
 Description:  
    Handle the unsolicited about sms. 
    This is called on atchannel's reader thread. 
 Calls: 
    RIL_onUnsolicitedResponse
 Called By:
    on_unsolicited
 Input:
    s       - pointer to unsolicited intent(e.g. +CMT)
    sms_pdu - pointer to hex string
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/

void hwril_unsolicited_sms (const char *s, const char *sms_pdu)
{
	char  *line = NULL;
	int err;
    /* Begin to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-05*/
    int i_index= -1;
	char *psz_sms_mem = NULL;				
    char cmd[2];
		        
	if (strStartsWith(s,"+CMT:"))
	{
		/* Begin modified by x84000798 for DTS2012103107785 2012-11-03*/
		cmt_new_sms(sms_pdu);
		/* End   modified by x84000798 for DTS2012103107785 2012-11-03 */
	}

	/*BEGIN Modified by qkf33988 2010-11-13 CMTI issue*/
#ifdef   CMTI_ISSUE
	else if( (strStartsWith(s, "+CMTI:"))||(strStartsWith(s, "+CDSI:")) )
	{
		/*TD SMS report with cmt*/
		/*if(NETWORK_TDSCDMA == g_module_type)
		{
		    sleep(1);
		}*/
		line = strdup(s);
		err = at_tok_start(&line);
		if (err < 0)
		{
		    goto error;
		}

		err = at_tok_nextstr(&line, &psz_sms_mem);
		if (err < 0)
		{
		     goto error;
		}

		if(strncmp(psz_sms_mem, "SM",2) != 0)
		{
		     goto error;
		} 

		err = at_tok_nextint(&line, &i_index);
		if (err < 0)
		{
		    goto error;
		}

		if(strStartsWith(s, "+CMTI:"))
		{
			if(NETWORK_CDMA == g_module_type)
			{
				cmd[0] = CMD_CMTI;
				cmd[1] = (i_index& 0xFF);

				do 
				{ 
				    err=write( del_sms_control[0], cmd, 2 ); 
				}while (err < 0 && errno == EINTR);

				if (err != 2)
				{
				   ALOGD("%s: could not send  command: err=%d: %s",
							__FUNCTION__, err, strerror(errno)); 
				}
		  	}
		  	else
			{
				/* Begin modified by x84000798 for DTS2012110300583 2012-11-03*/
				if (SMS_NEW == g_sms_project)	// new sms project
				{
					if(QUALCOMM == g_provider)
					{
					    i_index++;
					}

					RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM, &i_index, sizeof(int *));
				}
				else	// old sms project
				{
					cmd[0] = CMD_CMTI;
					cmd[1] = (i_index& 0xFF);
					do 
					{ 
					    err=write( del_sms_control[0], cmd, 2 ); 
					}while (err < 0 && errno == EINTR);
					if (err != 2)
					{
					   ALOGD("%s: could not send  command: err=%d: %s",
								__FUNCTION__, err, strerror(errno)); 
					}
		  		}
		  		/* End   modified by x84000798 for DTS2012110300583 2012-11-03*/
			}
		}
		else if(strStartsWith(s, "+CDSI:"))
		{ 
			cmd[0]=CMD_CDSI;
			cmd[1] = (i_index& 0xFF);
			 
			do 
			{ 
			    err=write( del_sms_control[0], cmd, 2 ); 
			}while (err < 0 && errno == EINTR);

			if (err != 2)
			{
			ALOGD("%s: could not send  command: err=%d: %s",
					__FUNCTION__, err, strerror(errno)); 
			}	
		}
	}
#endif
       /*END Modified by qkf33988 2010-11-13 CMTI issue*/
       /* End to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-05*/
	else if (strStartsWith(s, "+CDS:")) {
		/* Begin modified by x84000798 for DTS2012103107785 2012-11-03*/
		cds_new_sms_report(sms_pdu);
		/* End   modified by x84000798 for DTS2012103107785 2012-11-03*/
	}
	else if (strStartsWith(s, "^SMMEMFULL")){
		//BEGIN modify by KF34305 20111206 for DTS2011111804270
		if(NETWORK_CDMA == g_module_type)
		{
			RIL_onUnsolicitedResponse (RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL, NULL, 0);
		}
		else
		{
			RIL_onUnsolicitedResponse (RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
		}
		//END modify by KF34305 20111206 for DTS2011111804270
		
		/*char cmd[2];
		cmd[0]=CMD_CMTI;
		cmd[1] = (0& 0xFF);
		do { err=write( del_sms_control[0], cmd, 2 ); }
		while (err < 0 && errno == EINTR);
		if (err != 2)
		{
			ALOGD("%s: could not send  command: err=%d: %s",
				__FUNCTION__, err, strerror(errno)); 
		}*/
	}
	/* begin added by lkf52201 2011.10.11 */
	else if (strStartsWith(s, "+CBM:")) //modified by x84000798 for "+CBM"
	{
		if (RIL_DEBUG)
			ALOGD("unsol_response_new_broadcase_sms ---------");
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NEW_BROADCAST_SMS, sms_pdu, strlen(sms_pdu));
	}
	/* end added by lkf52201 2011.10.11 */
	/* Begin to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-05*/
	if(line != NULL) 
	{
	        free(line);
	        line = NULL;
	}        
	/* End to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-05*/
	return ;

/*start modify by fKF34305 20101213  解决pclint warning*/
#ifdef   CMTI_ISSUE
   error:
    /* Begin to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-05*/
    if(line != NULL) 
	{
	        free(line);
	        line = NULL;
	} 
	/* End to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-05*/
	if(RIL_DEBUG) ALOGE( "%s: Error parameter in ind msg: %s", __FUNCTION__, s);  
	return;
#endif	
/*end modify by fKF34305 20101213*/
}


void *hwril_read_sms_then_delete_sms (void *arg)
{
	ATResponse *p_response;
	ATLine *p_cur;	
	char *atcmd;
	char  *line;
	int err;
	int i;
	int         epoll_fd   = epoll_create(2);
	int         control_fd = del_sms_control[1];
	epoll_register( epoll_fd, control_fd );
	for(;;)
	{
		struct epoll_event   events[2];
		int                  ne, nevents;

		nevents = epoll_wait( epoll_fd, events, 2, -1 );
		if (nevents < 0) {
			if (errno != EINTR)
				ALOGE("epoll_wait() unexpected error: %s", strerror(errno));
			continue;
		}
		if(RIL_DEBUG)ALOGD("read msg thread received %d events", nevents);
		for (ne = 0; ne < nevents; ne++) 
		{
			if ((events[ne].events & (EPOLLERR|EPOLLHUP)) != 0) 
			{
				ALOGE("EPOLLERR or EPOLLHUP after epoll_wait() !?");
				continue;
			}
			if ((events[ne].events & EPOLLIN) != 0) 
			{
				if(RIL_DEBUG)ALOGD("-------ne=%d,nevents=%d---------",ne,nevents);
				int  fd = events[ne].data.fd;
				if (fd == control_fd)
				{
					char  cmd[2];
					int   ret;
					if(RIL_DEBUG)ALOGD("read_sms_control fd event");
					do 
					{
						ret = read( fd, cmd, 2 );
					} while (ret < 0 && errno == EINTR);
					if(RIL_DEBUG)ALOGD("--------READ CMD 0x%x,0x%x---------",cmd[0],cmd[1]);
#if 1
					//read_sms_on_sim_and_unsol_response(cmd[1],cmd[0]);//CMGR
					if(get_new_sim_sms_or_sms_report_and_del(cmd) == -1)
						ALOGD("get_new_sim_sms_or_sms_report_and_del err");
#else
					if(read_all_sim_sms_report_and_del() <= 0)//Modify to CMGL,  qkf33988 2010-11-18 22:33:49
					{
						ALOGD("not find new message to read from sim");
					}
#endif
			      }  
			}
		}
	}
	return (void*) 0;
}
/*END Added by qkf33988 2010-11-13 CMTI issue*/

#endif   //CMTI_ISSUE
