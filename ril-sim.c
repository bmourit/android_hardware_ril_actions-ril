/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-sim.c
 Author: liubing/00141886
 Version: V1.0
    Handle request and unsolicited about sim.include below interface:
    
	RIL_REQUEST_SIM_IO
    RIL_REQUEST_GET_SIM_STATUS
    RIL_REQUEST_ENTER_SIM_PIN
    RIL_REQUEST_ENTER_SIM_PUK
    RIL_REQUEST_CHANGE_SIM_PIN
    RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION
    RIL_REQUEST_GET_IMSI

    RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED
 
*******************************************************/
/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

  when        who      version    why 
 -------    -------    -------   --------------------------------------------
2010/09/14  l141886    V1B01D01  add wcdma sim interface
2010/12/14  fkf34035   V2B01D01  add cdma sim interface
2011/05/14  lkf34826   V3B02D01  add tdscdma sim interface

===========================================================================*/

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/

#include "atchannel.h"
#include "huawei-ril.h"
#include <ctype.h>

/*===========================================================================

                   INTERNAL DEFINITIONS AND TYPES

===========================================================================*/
#define STK_PROACTIVE_COMMAND                     0
#define STK_EVENT_NOTIFY                            1
#define STK_SESSION_END                                   99
int isUSIMCARD = 0;
extern struct DEVICEPORTS Devices[];
extern int g_device_index;
extern MODEM_Vendor g_modem_vendor;
extern Module_Type g_module_type;
extern CDMA_SubType g_cdma_subtype;
extern RIL_RadioState sState;
extern pthread_mutex_t s_hwril_readsms_mutex ;
extern int hexCharToInt(char c);
extern int hexStringToBytes(char * s,char *hs ,int len);
extern RIL_RadioState sState;
extern int get_sim_status();
int read_sms_on_sim_and_unsol_response(const char id,const char mode);
void is_NV_full(void *param);
void is_sim_full(void *param);

/* 当上层下发RIL_REQUEST_RADIO_POWER请求来设置radio为on，即下发的参数为>0时，
 * 将该变量设为1，此时如果再次调用on_sim_ready()函数，则会调用函数
 * get_unread_sms_and_del_when_ril_init()去读取SIM卡未读短信，并上报给上层。
 */
int has_set_radio_on = 0;   // DTS2011120501094 lkf52201 2011-12-5 modified

/* Begin to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
extern int g_fileid;
extern char g_smspath[64];
/* End to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
STK_Project g_stk_project = STK_UNKNOW; //Added by x84000798 for DTS2012073007612 2012-07-31 
extern SMS_Project g_sms_project;
/*===========================================================================

                                FUNCTIONS

===========================================================================*/

static char *StrToUpper(char * str)
{
    if( NULL == str)
    {
		return NULL;
	}
    char *s=str;
	
    while(*s != '\0')
    {	
    	if(*s <= 'z' && *s >= 'a' )
    	{
            *s=*s-('a'-'A');
    	}
    	s++;
    }
    return str;
}

/*************************************************
 Function:  request_sim_io
 Description:  
    reference ril.h
 Calls: 
    at_send_command_singleline
    RIL_onRequestComplete
 Called By:
    hwril_request_sim
 Input:
    data    - pointer to void
    datalen - data len
    t       - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    Modeify by fangyuchuang/00141886 for sunshaohua version
    Modify by hushanshan for change usim apdu structure
**************************************************/


static void  request_sim_io(void *data, size_t datalen, RIL_Token t)
{
	int err;
	char *cmd = NULL;
	char *line;
	char  hs[20];
	unsigned short file_size; 
	RIL_SIM_IO_Response sr;
	ATResponse *p_response = NULL;
RIL_onRequestComplete(t, RIL_E_SUCCESS, "", 0);
return; /* phchen */

	memset(&sr, 0, sizeof(sr));
	
    if( NULL == data )
    	goto error;
   
//begin modified by wkf32792 for android 3.x 20110815 
#ifdef ANDROID_3	    
     RIL_SIM_IO_v6 *p_args;
     p_args = (RIL_SIM_IO_v6 *)data;
#else	
     RIL_SIM_IO *p_args;
     p_args = (RIL_SIM_IO *)data;
#endif
//end modified by wkf32792 for android 3.x 20110815
	

	if(p_args->path != NULL){
		if((strlen(p_args->path)%4) != 0)
			goto error;
	}
	
    if(p_args->data != NULL)
    {
		p_args->data = StrToUpper(p_args->data);
    }
	
	/* FIXME handle pin2 */    
	// goto sim_io_error;
    //modified by wkf32792 begin for DTS2011062001722
	
    /* Begin to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
    if (p_args->data == NULL) {
	   asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,,\"%s\"", 
	   p_args->command, p_args->fileid,
	   p_args->p1, p_args->p2, p_args->p3, p_args->path);
	   
      if(p_args->fileid == 28476)
      {             
          sprintf(g_smspath,"%s",p_args->path);
          g_fileid = p_args->fileid;
      }
      
    } else {	   
	   asprintf(&cmd, "AT+CRSM=%d,%d,%d,%d,%d,\"%s\",\"%s\"",
	   p_args->command, p_args->fileid,
	   p_args->p1, p_args->p2, p_args->p3, p_args->data, p_args->path);   // p_args->p1获取的响应时间，是不是对ril有影响
	   
       if(p_args->fileid == 28476)
       {          
           sprintf(g_smspath,"%s",p_args->path);
           g_fileid = p_args->fileid; 
       }
       
	}	//modified by wkf32792 end for DTS2011062001722
	/* End to modify by hexiaokong kf39947 for DTS2011122605765 2012-01-13*/
	
	err = at_send_command_singleline(cmd, "+CRSM:", &p_response);

	if (err < 0 || p_response->success == 0) {
		goto error;
	}

	line = p_response->p_intermediates->line;

	err = at_tok_start(&line);
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &(sr.sw1));
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &(sr.sw2));
	if (err < 0) goto error;

	if (at_tok_hasmore(&line)) {
		err = at_tok_nextstr(&line, &(sr.simResponse));
		if (err < 0) goto error;
		
		
		if(('6' == sr.simResponse[0]) 
			&& ('2' == sr.simResponse[1]) 
			&& 192 ==p_args->command 
			&& ('0' == sr.simResponse[6])
			&& ('5' == sr.simResponse[7]))
		{//is usim card
            isUSIMCARD = 1;
			if(RIL_DEBUG) 
			{
			if(RIL_DEBUG) ALOGD("USIM : sr.simResponse = %s,len=%d",sr.simResponse,strlen(sr.simResponse));
			}
			hexStringToBytes(sr.simResponse,hs,15);

			sr.simResponse[0] = '0';
			sr.simResponse[1] = '0';
			sr.simResponse[2] = '0';
			sr.simResponse[3] = '0';
            //file_size = Record length * Number of records
			file_size = ((hs[6]<<8)+hs[7]) * hs[8];
			//copy file size
			sprintf(&(sr.simResponse[4]),"%04x",file_size);
			//copy file id
			sr.simResponse[8] = sr.simResponse[22];
			sr.simResponse[9] = sr.simResponse[23];
			sr.simResponse[10] = sr.simResponse[24];
			sr.simResponse[11] = sr.simResponse[25];
            //TYPE_EF RESPONSE_DATA_FILE_TYPE
			sr.simResponse[13] = '4';
            //EF_TYPE_LINEAR_FIXED RESPONSE_DATA_STRUCTURE
			sr.simResponse[26] = '0';
			sr.simResponse[27] = '1';
			//Record length
			sr.simResponse[28] = sr.simResponse[14];
			sr.simResponse[29] = sr.simResponse[15];
			sr.simResponse[30] = 0x0;//the end

			if(RIL_DEBUG) 
			{
			if(RIL_DEBUG) ALOGD("response simResponse = %s,len=%d",sr.simResponse,strlen(sr.simResponse));
			}
		}
		
	}

	RIL_onRequestComplete(t, RIL_E_SUCCESS, &sr, sizeof(sr));
	at_response_free(p_response);
	free(cmd);
	return;
	
error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	if(p_response != NULL)
	{
		at_response_free(p_response);
	}
    free(cmd);
	cmd = NULL;
	return;
}


void init_stk_unsolv(void)
{
	at_send_command("AT^CSMN", NULL);
} 

/*BEGIN Modified by qkf33988 2010-11-13 CMTI issue*/
#ifdef   CMTI_ISSUE
int g_max_sim_count = 50;
int  str2hex(const char *s,char *c)
{
	if (s == NULL) return -1;
	*c =  (char) ((hexCharToInt(s[0]) << 4) 
                                | hexCharToInt(s[1]));
	return 0;
}
#define MESSAGE_STATUS_REPORT          0
#define NEW_MESSAGE                              1
#define PDU_ERROR                                  -1 
int pdu_type(const char*  pdu)
{
	char sca_len;
	char sms_type;
	if(str2hex(pdu,&sca_len) == -1 )
	{
		ALOGE("str2hex error!");
		return  PDU_ERROR;
	}
	if(str2hex(pdu+2*(sca_len+1),&sms_type) == -1)
	{
		ALOGE("str2hex error!");
		return PDU_ERROR;
	}
	if((sms_type & 0x3) == 0x2)
	{
		return MESSAGE_STATUS_REPORT;
	}
	else
	{
		return NEW_MESSAGE;
	}
}

//#if 0  // DTS2011120501094 lkf52201 2011-12-5 modified
/*************************************
 * 当系统初始化时，调用该函数获取SIM卡里面的未读短信，
 * 上报给上层，并删除该短信。
 * input: NULL
 * return:count fo delete
 **************************************/
int  get_unread_sms_and_del_when_ril_init(void)
{
    /* Begin to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-12*/
    ATResponse *p_response = NULL;
    /* End to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-12*/
	ATLine *p_cur;	
	char *cmd;
	char  *line;
	int err;
	int i;
	int msg_index;   
	int state;
	int reserved;
	int length;
	char *pdu;
//	char sca_len;
//	char sms_type;
	int count=0;

    /* BEGIN DTS2011120501094  lixianyi 2011-12-5 added */
    /* BEGIN add by lkf52201 2011-11-28 为了规避CDMA不支持SIM IO的问题，和SE、MDE讨论后，采用了下面的方案 */
    if (RIL_DEBUG)
    {
        ALOGD("Enter get_unread_sms_and_del_when_ril_init ........");
    }
    if (NETWORK_CDMA == g_module_type)
    {
        err = at_send_command_multiline("AT^HCMGL=0", "^HCMGL:", &p_response);
	    if (0 != err || 0 == p_response->success)
	    {
	        if (RIL_DEBUG) ALOGD("AT^HCMGL ERROR !!");
	        goto err;
		}
		for (i = 0, p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next, ++i)
		{
			line = p_cur->line;
			err = at_tok_start(&line);
			if (err < 0) 
			{
			    break;
			}
			err = at_tok_nextint(&line, &msg_index);
			if (err < 0) 
			{
			    break;
			}
			read_sms_on_sim_and_unsol_response(msg_index, 0xFF);
		}
		count = i;
    }
    /* END add by lkf52201 2011-11-28 为了规避CDMA不支持SIM IO的问题 */
    /* END DTS2011120501094  lixianyi 2011-12-5 added */
    
    /* Begin modified by x84000798 for DTS2012110300583  2012-11-03*/
    else
    {
	   	if (SMS_OLD == g_sms_project)
	    {
    	err = at_send_command_multiline("AT+CMGL=0", "+CMGL:", &p_response);
    	if (err != 0 || p_response->success == 0)
    	{
	    		ALOGD("AT+CMGL ERR!");
    		goto err;
    	}
    	
    	for (i = 0, p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next,i++)
    	{
    		line = p_cur->line;
    		err = at_tok_start(&line);
    		if (err < 0) break;
    		err = at_tok_nextint(&line, &msg_index);
    		if (err < 0) break;
    		err = at_tok_nextint(&line, &state);
    		if (err < 0) break;
    		err = at_tok_nextint(&line, &reserved);
    		err = at_tok_nextint(&line, &length);
    		if (err < 0) break;
    		p_cur = p_cur->p_next;
    		if(NULL==p_cur)  break;
    		line = p_cur->line;
    		err = at_tok_nextstr(&line, &pdu);
    		if (err < 0) break;
    		
    		asprintf(&cmd, "AT+CMGD=%d", msg_index);
    		err = at_send_command(cmd, NULL);
    		free(cmd);   // DTS2011120501094 lkf52201 2011-12-5 added
    		if(err != 0)
    		{
	    			ALOGD("AT+CMGD ERR!");
    			goto err;
    		}
    		// free(cmd);  // DTS2011120501094 lkf52201 2011-12-5 deleted
    		count++;
            /*
    		if(str2hex(pdu,&sca_len) == -1 )
    		{
    			ALOGE("str2hex error!");
    			goto err;
    		}

    		if(str2hex(pdu+2*(sca_len+1),&sms_type) == -1)
    		{
    			ALOGE("str2hex error!");
    			goto err;
    		}
    		*/
    		if(MESSAGE_STATUS_REPORT == pdu_type(pdu))
    		{
    			RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NEW_SMS_STATUS_REPORT, pdu, length);
    		}
    		else if(NEW_MESSAGE== pdu_type(pdu))
    		{
    			RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NEW_SMS, pdu, length);
    		}
    		else
    		{
    			goto err;
    		}
    	}
    }
	    else
	    {
	    	ALOGD("New sms project, don't get unread sms when ril init !");
	    }
    }
	/* End   modified by x84000798 for DTS2012110300583  2012-11-03*/
    
    /* BEGIN DTS2011120501094  lixianyi 2011-12-5 added */
    if (RIL_DEBUG)
	{
	    ALOGD("delete number = %d..........", count);
	}
    /* END DTS2011120501094  lixianyi 2011-12-5 added */
    /* Begin to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-12*/
    if(p_response != NULL)
    {
        at_response_free(p_response);
	}
	return count;
err:
    if(p_response != NULL)
    {
        at_response_free(p_response);
	}
	return 0;   // DTS2011120501094 lkf52201 2011-12-5 modified
}
  /* End to modify by hexiaokong h81003427 for DTS2012050403497  2012-05-12*/
//#else   // DTS2011120501094 lkf52201 2011-12-5 modified

#define CMD_GSM             0xEF/*Modefied by qkf33988 2011-4-19 CDMA SMS*/ 
#define CMD_CDMA       0xFE/*Modefied by qkf33988 2011-4-19 CDMA SMS*/ 
//#define MAX_SIM_MSG     30

/* BEGIN DTS2011120501094  lixianyi 2011-12-5 deleted */
#if 0
int find_ids(char *line,char*ids)
{
	if(line == NULL || ids == NULL)
	{
		return -1;
	}
	char *id = ids;
	int i;

	for(i=0;i<g_max_sim_count;line++)
	{
		char * str,*end;
		if(*line == '(')
		{
			memset((void*)id,(int)0xFF,g_max_sim_count);
			i=0;
		}
		else if(isdigit(*line))
		{
			id[i++] = strtoul(line,&end,10);
			line = end;
		}
		if(*line == ')')
			break;
	}
	return i;
}
#endif
/* END DTS2011120501094  lixianyi 2011-12-5 deleted */

/*************************************
input: NULL
return:count fo delete
**************************************/
int  get_new_sim_sms_or_sms_report_and_del(char* command)
{
    /* BEGIN DTS2011120501094  lixianyi 2011-12-5 deleted */
    /*
	ATResponse *p_response;
	ATLine *p_cur;	
	char *cmd;
	char  *line;
	int err;
	int i;

	char * id;
	id = (char *)malloc(g_max_sim_count);
	memset(id,0,g_max_sim_count);
	int del_count=0;
	int cur_count=0;
	*/
    pthread_mutex_lock(&s_hwril_readsms_mutex); 
    /* END DTS2011120501094  lixianyi 2011-12-5 deleted */


	if(NETWORK_CDMA==g_module_type)
    {
        if(RIL_DEBUG)ALOGD("NETWORK_CDMA==g_module_type");
        /* BEGIN DTS2011120501094  lixianyi 2011-12-5 deleted */
        /*
	    if (read_sms_on_sim_and_unsol_response(command[1],command[0]) > 0)
		del_count++;
		*/
		/* END DTS2011120501094 lixianyi 2011-12-5 deleted */
	}
	else
	{
	    if(RIL_DEBUG)ALOGD("NETWORK_GSM==g_module_type");
	    /* BEGIN DTS2011120501094  lixianyi 2011-12-5 deleted */
	    /*
	    if (read_sms_on_sim_and_unsol_response(command[1], command[0]) > 0)
		del_count++;
		*/
		/* END DTS2011120501094  lixianyi 2011-12-5 deleted */
	}
	read_sms_on_sim_and_unsol_response(command[1], command[0]);

    /* BEGIN DTS2011120501094  lixianyi 2011-12-5 deleted */
    pthread_mutex_unlock(&s_hwril_readsms_mutex);
    return 1;
    /*
	at_response_free(p_response);
	free(id);
	pthread_mutex_unlock(&s_hwril_readsms_mutex); 
	return del_count;
err:
	at_response_free(p_response);
	free(id);
	pthread_mutex_unlock(&s_hwril_readsms_mutex); 
	return -1;
	*/
    /* END DTS2011120501094  lixianyi 2011-12-5 deleted */
}

//#endif   // DTS2011120501094 lkf52201 2011-12-5 modified
#endif
/*END Modified by qkf33988 2010-11-13 CMTI issue*/

/************************************************
 Function:  is_sim_full
 Description:  
    设置短信存储器为SIM卡，并判断SIM卡短信是否满
    SMMEMFULL,只有在满了再往SIM卡写信息的时候，才会出现这个主动上报
    所以采用CPMS，当SIM卡状态正常后，判断sim卡短信容量是否满。    
 Calls: 
 Called By:
 	poll_sim_state
 	
 Input:
    none
 Output:
    none
 Return:
    none
 Others:
    add by hushanshan 20101020
**************************************************/
void is_sim_full(void *param)
{
    ATResponse *p_response = NULL;
    char *line = NULL;
    int err = -1;
    int cur_count = 0;
    int del_count = 0;  // DTS2011120501094 lkf52201 2011-12-5 added
    int max_count = 0;
	
	if(RIL_DEBUG) ALOGE( "is_sim_full");   
    /* ZTE AC2736, AC580, responsePrefix is "+CPMS:"  BY PHCHEN */
	//if((NETWORK_CDMA != g_module_type)
	//	||(MODEM_VENDOR_ZTE != g_modem_vendor))
	//{
		err = at_send_command_singleline("AT+CPMS=\"SM\",\"SM\",\"SM\"","+CPMS:", &p_response);
	//}
	//else
	//{
	//	err = at_send_command_singleline("AT+CPMS=\"SM\",\"SM\",\"SM\"","+CCPMS:", &p_response);
	//}

    if (err < 0 || p_response->success == 0) 
    {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start (&line);    
    if (err < 0 ) 
    {
        goto error;
    }

    err = at_tok_nextint(&line, &cur_count);
    if (err < 0 ) 
    {
        goto error;
    }

    err = at_tok_nextint(&line, &max_count);
    if (err < 0 ) 
    {
        goto error;
    }

    /* BEGIN DTS2011120501094  lixianyi 2011-12-5 added */
    del_count = get_unread_sms_and_del_when_ril_init();
    cur_count -= del_count;
    /* END DTS2011120501094  lixianyi 2011-12-5 added */
    
/*BEGIN Modified by qkf33988 2010-11-13 CMTI issue*/
#ifdef   CMTI_ISSUE 
    if(cur_count == max_count)
    {
    	   if(NETWORK_CDMA == g_module_type)
		   RIL_onUnsolicitedResponse (RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL, NULL, 0);  
	   else
	   	   RIL_onUnsolicitedResponse (RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);  
    }
#endif
/*END Modified by qkf33988 2010-11-13 CMTI issue*/

    at_response_free(p_response);
    return ;
error:
	// maybe request too much time; delete it by phchen
	//RIL_requestTimedCallback (is_sim_full, NULL, &TIMEVAL_SIMPOLL);
    at_response_free(p_response);
    return ;
}

/************************************************
 Function:  is_NV_full
 Description:  
    设置短信存储器为ME，并判断ME短信是否满
    SMMEMFULL,只有在满了再往ME写信息的时候，才会出现这个主动上报
    所以采用CPMS，当ME状态正常后，判断ME短信容量是否满。    
 Calls: 
 Called By:
 	poll_sim_state
 	
 Input:
    none
 Output:
    none
 Return:
    none
 Others:
    add by z128629 2011-4-20 增加机卡合一短信存储体设置，为了代码可读性，与之前的SIM卡存储体分开
**************************************************/
void is_NV_full(void *param)
{
    ATResponse *p_response = NULL;
    char *line = NULL;
    int err = -1;
    int cur_count = 0;
    int del_count = 0;  // DTS2011120501094 lkf52201 2011-12-5 added
    int max_count = 0;
	
	if(RIL_DEBUG) ALOGE( "is_NV_full");  
    err = at_send_command_singleline("AT+CPMS=\"ME\",\"ME\",\"ME\"","+CPMS:", &p_response);
    if (err < 0 || p_response->success == 0) 
    {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start (&line);    
    if (err < 0 ) 
    {
        goto error;
    }

    err = at_tok_nextint(&line, &cur_count);
    if (err < 0 ) 
    {
        goto error;
    }

    err = at_tok_nextint(&line, &max_count);
    if (err < 0 ) 
    {
        goto error;
    }
    
    /* BEGIN DTS2011120501094  lixianyi 2011-12-5 added */
    del_count = get_unread_sms_and_del_when_ril_init();
    cur_count -= del_count;
    /* END DTS2011120501094  lixianyi 2011-12-5 added */
    
/*BEGIN Modified by qkf33988 2010-11-13 CMTI issue*/
#ifdef   CMTI_ISSUE 
    if(cur_count == max_count)
    {
	   RIL_onUnsolicitedResponse (RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL, NULL, 0);  
    }
#endif
/*END Modified by qkf33988 2010-11-13 CMTI issue*/

    at_response_free(p_response);
    return ;
error:
	RIL_requestTimedCallback (is_NV_full, NULL, &TIMEVAL_SIMPOLL);
    at_response_free(p_response);
    return ;
}

/************************************************
 Function:  stk_solution_mode
 Description:  
	judge the stk solution is raw data mode or standard mode
 Calls: 
 Called By: on_sim_ready
 Input:
    none
 Output:
    none
 Return:
    int:stk solution mode(0:old  1:new 2:unknow(not suport))
 Others:
	added by x84000798 STK 
**************************************************/
STK_Project stk_solution_mode()
{
	int err, mode, rawmode;
	char *line;
	ATResponse *p_response = NULL;
	
	err = at_send_command_singleline("AT^STSF?", "^STSF:", &p_response);
	if ((0 > err)|| (0 == p_response->success))
	{
		goto error;
	}
	line = p_response->p_intermediates->line;
	err = at_tok_start(&line);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextint(&line, &mode);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextint(&line, &rawmode);
	if (0 > err)
	{
		goto error;
	}
	at_response_free(p_response);
	
	if (2 == rawmode)
	{
		return STK_NEW;
	}
	else
	{
		return STK_OLD;
	}
error:
	at_response_free(p_response);
	return STK_UNKNOW;
}


/************************************************
 Function:  on_sim_ready
 Description:  
    init sim,went the sim status is set RADIO_STATE_SIM_READY,this function is called
 Calls: 
 Called By:
 	request_enter_sim_pin
 	set_radio_state
 Input:
    none
 Output:
    none
 Return:
    none
 Others:
    none
    Modified by z128629 2011-4-20 V3D03
    将跟SIM卡有关的AT命令全部放到该函数中，将之前的on_sim_ready_immediate删除掉
**************************************************/
void on_sim_ready()
{
	sleep(1); //MU509 sim card init slowly, so sleep 1 second
	char *line = NULL;
	char *rawdata = NULL;
	ATResponse *p_response = NULL;
	if (NETWORK_CDMA == g_module_type)
	{
		/*CDMA SMS Setting*/
		at_send_command_singleline("AT+CSMS=0", "+CSMS:", NULL);
		at_send_command("AT+CMGF=0",NULL);//目前CDMA只支持PDU格式，后续支持TEXT模式，需要修改
		at_send_command("AT+CNMI=1,1,0,2,0", NULL);
		at_send_command("AT+CCWA=1", NULL);
	}
	else
	{
		/*  Network registration events */
		at_send_command("AT+CREG=2", NULL);

	    //  GPRS registration events 
	    at_send_command("AT+CGREG=2", NULL);

	     // Call Waiting notifications 
	     /*Modify by fKF34305 2011-4-2 解决问题单DTS2011032805137  */
	    at_send_command("AT+CCWA=1", NULL);

	    /*  Alternating voice/data off */
	    at_send_command("AT+CMOD=0", NULL);

	    /*  Not muted */
	    //at_send_command("AT+CMUT=0", NULL);

	    /*  +CSSU unsolicited supp service notifications */
	    at_send_command("AT+CSSN=0,1", NULL);

	    /*  no connected line identification */
	    at_send_command("AT+COLP=0", NULL);

	    /*  HEX character set */
	    if (g_modem_vendor == MODEM_VENDOR_ALCATEL){
	    	at_send_command("AT+CSCB=1", NULL);//for alcatel
	    	at_send_command("AT+CMMS=2", NULL);//for alcatel
	    	at_send_command("AT+CMGF=0", NULL);//for alcatel
    		at_send_command("AT+CSCS=\"UCS2\"", NULL);//for alcatel
    		at_send_command("AT+CPBS=\"SM\"", NULL);//for alcatel
    		at_send_command("AT+CPMS=\"SM\"", NULL);//for alcatel
    	}
    	else 
	    at_send_command("AT+CSCS=\"IRA\"", NULL); 
	    
	    /*  USSD unsolicited */
	    at_send_command("AT+CUSD=1", NULL);

		/* USSD mode 单板透传方案*/
        at_send_command("AT^USSDMODE=1", NULL);

	    /*  SMS PDU mode */
	    at_send_command("AT+CMGF=0", NULL); 
		
	    /*  关闭boot主动上报 */
	    at_send_command("AT^BOOT=0,0", NULL); 

	    /*close DSFLOWRP*/
	    at_send_command("AT^DSFLOWRPT=0", NULL); 

        /*activation the stk*/
		/* Begin modifed by x84000798 for DTS2012073007612 2012-07-31 */
	    //at_send_command("AT^STSF=1,0", NULL); 
		g_stk_project = stk_solution_mode();
		ALOGD("----g_stk_project is %d ----", g_stk_project);
		if (STK_NEW == g_stk_project)
		{
			/* Begin modified by x84000798 for DTS2012081708293 DTS2012081800366 2012-09-03 */
			at_send_command("AT^STSF=1,2", NULL); 
			int err = at_send_command_singleline("AT^CUSATM?", "^CUSATM:", &p_response);
			if (0 > err || 0 == p_response->success)
			{
				ALOGD("SEND AT^CUSATM? ERROR ");
			}
			if ((1 == p_response->success))
			{
				line = p_response->p_intermediates->line;
				at_tok_start(&line);
				at_tok_nextstr(&line, &rawdata);
				if ((NULL != rawdata) && ('\0' != *rawdata))
				{
					RIL_onUnsolicitedResponse (RIL_UNSOL_STK_PROACTIVE_COMMAND, rawdata, sizeof(rawdata));
				}
			}
			at_response_free(p_response);
			p_response = NULL;
			/* End modified by x84000798 for DTS2012081708293 DTS2012081800366 2012-09-03 */
		}
		else if(STK_OLD == g_stk_project)
		{
			/*get stk mainmenu*/
			at_send_command("AT^STSF=1,0", NULL); 
			at_send_command("AT^STGI=0,0", NULL);
		}
		else
		{
			ALOGD("This module don't suport STK!!!");
		}
		/* End modified by x84000798 for DTS2012073007612 2012-07-31 */     

		/* Modified by x84000798 for DTS2012090300608 2012-09-03 */
        at_send_command_singleline("AT+CSMS=0", "+CSMS:", NULL);
		
		/* Added by x84000798 to active PS incoming */
		at_send_command("AT+CRC=1", NULL);
		
		/* Always send SMS messages directly to the TE
	 	*
		* mode = 1 // discard when link is reserved (link should never be
		*             reserved)
		* mt = 2   // most messages routed to TE
		* bm = 2   // new cell BM's routed to TE
		* ds = 1   // Status reports routed to TE
		* bfr = 1  // flush buffer
		*/
		
		if ( NETWORK_WCDMA == g_module_type )
		{
			/* Begin modified by x84000798 for DTS2012110300583  2012-11-03*/	
			int error = at_send_command("AT+CNMI=2,2,2,1,0", &p_response); //Modified by x84000798 2012-06-04 DTS2012060802811
			if (0 > error || 0 == p_response->success)
			{
				if(g_modem_vendor == MODEM_VENDOR_ALCATEL)
					at_send_command("AT+CNMI=2,1,0,2,0", NULL);//断消息设置，for alcatel
				else
				at_send_command("AT+CNMI=2,1,2,2,0", NULL);
				g_sms_project = SMS_OLD;
			}
			else
			{
				g_sms_project = SMS_NEW;
			}
			ALOGD("g_sms_project = %d, 0 is old sms project, 1 is new sms project", g_sms_project);
			at_response_free(p_response);
			p_response = NULL;
			/* End   modified by x84000798 for DTS2012110300583 2012-11-03*/
		}
		else
		{
			at_send_command("AT+CNMI=1,2,2,1,0", NULL); //TD模块使用CMT方式
		}

    }
    
// phchen
//	if (1 == has_set_radio_on)  // DTS2011120501094 lkf52201 2011-12-5 modified
//	{
//		if(RIL_DEBUG) ALOGE( "has_set_radio_on g_cdma_subtype: %d, %s", g_cdma_subtype, Devices[g_device_index].PID);  
//			 
//    	/*设置短信存储体，并判断SIM卡中短信是否已满*/
//    	if(CDMA_NV_TYPE == g_cdma_subtype)
//    	{
//    		is_NV_full(NULL);
//    	}
//    	else
//    	{
//		if(strncmp(Devices[g_device_index].PID, "140c", 4)) 
//		{
//			is_sim_full(NULL);
//		}
//    	}
//	}

	
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3
	RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,NULL, 0); 
#else
    RIL_onUnsolicitedResponse ( RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED,NULL, 0);
#endif
//end modified by wkf32792 for android 3.x 20110815

#if 0
	at_send_command_singleline("AT+CSMS=0", "+CSMS:", NULL);
	
	/* Always send SMS messages directly to the TE
	 *
	 * mode = 1 // discard when link is reserved (link should never be
	 *             reserved)
	 * mt = 2   // most messages routed to TE
	 * bm = 2   // new cell BM's routed to TE
	 * ds = 1   // Status reports routed to TE
	 * bfr = 1  // flush buffer
	 */
	 /*BEGIN Modified by qkf33988 DTS2010110103862 CMTI issue 2010-11-13*/ 
#ifdef   CMTI_ISSUE	 
	/*the short message will be discard when at channel is busy if we use cmt , so we use cmti */
        /*BEGIN Added by qkf33988 2011-4-19 CDMA SMS*/ 
	if(NETWORK_CDMA==g_module_type)
	{
        at_send_command("AT+CNMI=1,1,0,2,0", NULL); 
	}
	else
	{
	    at_send_command("AT+CNMI=2,1,2,2,0", NULL); 
	}
	/*END Added by qkf33988 2011-4-19 CDMA SMS*/ 
#else
	at_send_command("AT+CNMI=1,2,2,1,0", NULL);
#endif
#endif
	/*END Modified by qkf33988 DTS2010110103862 CMTI issue 2010-11-13*/ 
	
}
/************************************************
 Function:  getNumRetries
 Description:  
    check the PIN/PUK remain times
 Calls: 
 Called By:
 	request_enter_sim_pin
 	request_set_facility_lock
 Input:
    none
 Output:
    none
 Return:
    PIN/PUK remain times
 Others:
    lkf34826 2011-03-24
**************************************************/

int getNumRetries (int request) 
{
    ATResponse *p_response = NULL;
    int err;
    int times = -1;
	char *code;
	char *line;
	int tmp;
    int pin_times;
	int puk_times;

    err = at_send_command_singleline("AT^CPIN?", "^CPIN:", &p_response);
	if (err < 0 || p_response->success == 0)
	{		
		goto final;
	}

	line = p_response->p_intermediates->line;

    at_tok_start(&line);

	at_tok_nextstr(&line, &code);

	at_tok_nextint(&line, &tmp);

	at_tok_nextint(&line, &puk_times);

	at_tok_nextint(&line, &pin_times);
	
    switch (request) 
	{
    case RIL_REQUEST_ENTER_SIM_PIN:
	case RIL_REQUEST_CHANGE_SIM_PIN:
	case RIL_REQUEST_SET_FACILITY_LOCK:
         times = pin_times;
        break;
    case RIL_REQUEST_ENTER_SIM_PUK:
         times = puk_times;
        break;
      
    default:
         times = -1;
        break;
    }

final:
	at_response_free(p_response);
    return times;
}
/*************************************************
 Function:  request_enter_sim_pin
 Description:  
    reference ril.h
 Calls: 
    RIL_onUnsolicitedResponse
 Called By:
    hwril_request_sim
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    none
    Modified by z128629 2011-4-20 V3D03
    DTS2011041400304:在输入PIN码解锁后，因为SIM卡初始化还需要一段时间，需要重新polling SIM卡状态
    相应的，RIL_REQUEST_CHANGE_SIM_PIN、RIL_REQUEST_ENTER_SIM_PUK
    RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION、RIL_REQUEST_SET_FACILITY_LOCK都要进行相同的处理
    备注:目前由于某些单板SIM卡状态发生改变后，不上报^SIMST，所以在这些地方分别需要做polling处理，
    如果所有单板都支持^SIMST上报，可以将polling处理放到该主动上报处
*******************************************************************************************/
static void  request_enter_sim_pin(void*  data, size_t  datalen, RIL_Token  t,int request)
{
	ATResponse   *p_response = NULL;
	int  err;
	int times = -1;//[1] = {NULL};
	char*  cmd = NULL;
	const char**  strings = (const char**)data;

	switch (request )
	{
		case RIL_REQUEST_ENTER_SIM_PIN:
		{
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3
            if (datalen == 2 * sizeof(char*) ) {			    
			    asprintf(&cmd, "AT+CPIN=\"%s\"", strings[0]);
            }
#else
            if (datalen == sizeof(char*)) {			    
			    asprintf(&cmd, "AT+CPIN=\"%s\"", strings[0]);
            }
#endif
//end modified by wkf32792 for android 3.x 20110815
			else {
				RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE,  &times, sizeof(int*));
				break;
			}                
			err=at_send_command (cmd, &p_response); 
			free(cmd); 
			if(RIL_DEBUG) ALOGE( "request_enter_sim_pin  err=%d,p_response->success = %d\n",err,p_response->success);
            /*BEGIN Added by lkf34826  DTS2011032203169 V3D01:增加返回剩余PIN码输入次数 */
			times = getNumRetries(request);
			if(RIL_DEBUG) ALOGE("remain times: %d ",times);
			if ( err == 0 )
			{
				if(p_response->success == 0)
				{
					RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &times, sizeof(int*));
					at_response_free(p_response);  
					break;    
				}
				else
				{ 
					at_response_free(p_response); 
					RIL_onRequestComplete(t, RIL_E_SUCCESS, &times, sizeof(int*));
                    if(NETWORK_CDMA == g_module_type)
					{
						if(CDMA_RUIM_TYPE == g_cdma_subtype)
						{
							set_radio_state(RADIO_STATE_RUIM_NOT_READY);
						}
						else
						{
							set_radio_state(RADIO_STATE_NV_NOT_READY);
						}
					}
					else
					{
						set_radio_state(RADIO_STATE_SIM_NOT_READY);
					}
					break;
				}
			}
			else
			{	
				RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE,  &times, sizeof(int*));
				at_response_free(p_response);  
				break; 
			}     
		}  

		case RIL_REQUEST_CHANGE_SIM_PIN:
		{
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3
			if ( datalen == 3 * sizeof(char*) ) { 
				asprintf(&cmd, "AT+CPWD=\"SC\",\"%s\",\"%s\"", strings[0], strings[1]);
			}
#else
            if ( datalen == 2 * sizeof(char*) ) { 
				asprintf(&cmd, "AT+CPWD=\"SC\",\"%s\",\"%s\"", strings[0], strings[1]);
			}
#endif
//end modified by wkf32792 for android 3.x 20110815
			else{
				RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE,  &times, sizeof(int*));
				at_response_free(p_response);
				break;
			}   
			err=at_send_command (cmd, &p_response);
			free(cmd); 
			if(RIL_DEBUG) ALOGE( "request_enter_sim_pin  err=%d,p_response->success = %d\n", err, p_response->success);
            /*BEGIN Added by lkf34826  DTS2011032203169 V3D01:增加返回剩余PIN码输入次数 */
            times = getNumRetries(request);
			if(RIL_DEBUG) ALOGE("remain times: %d ",times);
			if (err < 0 || p_response->success == 0) {
           
				RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &times, sizeof(int*));
				if(times == 0)
				{
	
                    if(NETWORK_CDMA == g_module_type)
					{
						if(CDMA_RUIM_TYPE == g_cdma_subtype)
						{
							set_radio_state(RADIO_STATE_RUIM_NOT_READY);
						}
						else
						{
							set_radio_state(RADIO_STATE_NV_NOT_READY);
						}
					}
					else
					{
						set_radio_state(RADIO_STATE_SIM_NOT_READY);
					}
				}
				at_response_free(p_response); 
				break;
			
			}
			else
			{
				  RIL_onRequestComplete(t, RIL_E_SUCCESS, &times, sizeof(int*));
				  at_response_free(p_response);
				  break;
			}
			/*END Added by lkf34826  DTS2011032203169 V3D01:增加返回剩余PIN码输入次数 */ 
		}

		case   RIL_REQUEST_ENTER_SIM_PUK:
		{
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3
			if ( datalen == 3 * sizeof(char*)) {  
				asprintf(&cmd, "AT+CPIN=\"%s\",\"%s\"", strings[0], strings[1]);
			}
#else
            if ( datalen == 2 * sizeof(char*) ) {  
				asprintf(&cmd, "AT+CPIN=\"%s\",\"%s\"", strings[0], strings[1]);
			}
#endif
//end modified by wkf32792 for android 3.x 20110815
			else{
				RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE,  &times, sizeof(int*));
				at_response_free(p_response);
				break;
			}   
			err=at_send_command (cmd,&p_response);  
			free(cmd); 
			if(RIL_DEBUG) ALOGE( "request_enter_sim_pin  err=%d,p_response->success = %d\n",err,p_response->success);
             /*BEGIN Added by lkf34826  DTS2011032203169 V3D01:增加返回剩余PUK码输入次数 */
			times = getNumRetries(request);
			if(RIL_DEBUG) ALOGE("remain times: %d ",times);
			if (err < 0 || p_response->success == 0)
			{          
				RIL_onRequestComplete(t, RIL_E_PASSWORD_INCORRECT, &times, sizeof(int*));
				at_response_free(p_response);  
				break; 				
			}
			else
			{ 
				at_response_free(p_response); 
				RIL_onRequestComplete(t, RIL_E_SUCCESS, &times, sizeof(int*));
                if(NETWORK_CDMA == g_module_type)
				{
					if(CDMA_RUIM_TYPE == g_cdma_subtype)
					{
						set_radio_state(RADIO_STATE_RUIM_NOT_READY);
					}
					else
					{
						set_radio_state(RADIO_STATE_NV_NOT_READY);
					}
				}
				else
				{
					set_radio_state(RADIO_STATE_SIM_NOT_READY);
				}
				
				break;
			}
		}
		
		default:{
			RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
			break;
		}
	}   
}

static void  request_get_sim_imsi(void*  data, size_t  datalen, RIL_Token  t)
{
	int err;
	ATResponse *p_response = NULL;
	int count = 3; //Added by c00221986 to test apn
	while(count > 0){
		sleep(1);
		err = at_send_command_numeric("AT+CIMI", &p_response);
		count--;
		if((0 == err) && (1 == p_response->success)){
			RIL_onRequestComplete(t, RIL_E_SUCCESS,p_response->p_intermediates->line, sizeof(char *));
			at_response_free(p_response);
			break;
		}
		/*if (err == 0) 
		{
			if(p_response->success == 0)
			continue;
			//RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
			else
			RIL_onRequestComplete(t, RIL_E_SUCCESS,p_response->p_intermediates->line, sizeof(char *));
			break;
		}else{
			continue;
			//RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		}
		at_response_free(p_response);
		*/
	}
	if (err < 0 || p_response->success == 0){
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		at_response_free(p_response);
	}

	
}

/*************************************************
 Function:  request_stk_service_is_running
 Description:  
    for stk service running. we useAT^STSF for fit balong
 Calls: 
    
 Called By:
    hwril_request_sim
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    modify by dKF36756 2011-3-9 增加了激活STK  功能的函数
**************************************************/


void request_stk_service_is_running( void *data, size_t datalen, RIL_Token token)
{	
	int err = 0;
	ATResponse *p_response = NULL;

	/* Begin modifed by x84000798 for DTS2012073007612 2012-07-31 */
	if (STK_NEW == g_stk_project)
	{
		err = at_send_command("AT^STSF=1,2", &p_response);
	}
	else
	{
		err = at_send_command("AT^STSF=1,0", &p_response);
	}
	if(err<0 || p_response->success == 0)
	{
	   LOGE("Activation the STK is  Failed!!!\n");
	   goto error;
	}
	/* BEGIN DTS2011123101074 lkf52201 2011-12-31 added */
	at_response_free(p_response);
	p_response = NULL;
	/* END   DTS2011123101074 lkf52201 2011-12-31 added */

	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	return;
error:
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
}
/*************************************************
 Function:  request_stk_terminal_response
 Description:  
    for stk terminal response. we useAT^STGR for fit balong
 Calls: 
    
 Called By:
    hwril_request_sim
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    modify by dKF36756 2011-3-9 增加了STK  功能终端响应的函数
**************************************************/
void request_stk_terminal_response(void *data, size_t datalen, RIL_Token t)
{    
	char *cmd;
	char* RawData = NULL;            
	ATResponse *p_response = NULL;
	int err = 0;
     /*BEGIN Added by donghu kf36756 2011.03.18 */
    if(data == NULL)
    {
      ALOGE("Send STK Terminal response is NULL!\n");
	  goto error;
    }
     /*END Added by donghu kf36756 2011.03.18 */
	RawData =  (char*) data;
	if(RIL_DEBUG)ALOGD("Terminal Response is :%s",(char *)RawData);

	/* Begin modifed by x84000798 for DTS2012073007612 2012-07-31 */
	if ( STK_NEW == g_stk_project)
	{
		asprintf(&cmd, "AT+CUSATT=\"%s\"", (char *)RawData);
	}
	else
	{
		asprintf(&cmd, "AT^STGR=1,\"%s\"",(char *)RawData);
	}
	/* End modifed by x84000798 for DTS2012073007612 2012-07-31 */
	err = at_send_command(cmd, &p_response);
	free(cmd);
	if(err<0 || p_response->success == 0)
	{
	  ALOGE("Send STK Terminal response Failed!\n");
	  goto error;
	}
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	at_response_free(p_response);
	return ;
error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
}

/*************************************************
 Function:  request_stk_envelope
 Description:  
    for stk envelope request. we use AT^STGR for fit balong
 Calls: 
    
 Called By:
    hwril_request_sim
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    modify by dKF36756 2011-3-9 增加了STK  功能信封命令发送的函数
**************************************************/
void request_stk_envelope(void *data, size_t datalen, RIL_Token t)
{
	char *cmd = NULL;
	char *line = NULL;
	char *response = NULL;
	char *RawData = NULL;
	ATResponse *p_response = NULL;
	int err = 0;
     /*BEGIN Added by donghu kf36756 2011.03.18 */
    if(data == NULL)
    {
      ALOGE("Send STK  envelope  is NULL!\n");
	  goto error;
    }
     /*END Added by donghu kf36756 2011.03.18 */
	RawData =  (char*) data;
	RawData = StrToUpper(RawData);   //为了对应单板将RAW DATA 的小写字母转换为大写
	/* Begin modifed by x84000798 for DTS2012073007612 2012-07-31 */
	if (STK_NEW == g_stk_project)
	{
		asprintf(&cmd, "AT+CUSATE=\"%s\"", (char *)RawData);
		err = at_send_command_singleline(cmd, "+CUSATE:", &p_response);
		free(cmd);
		if (0 > err || 0 == p_response->success)
		{
			goto error;
		}
		line = p_response->p_intermediates->line;		
		err = at_tok_start(&line);
		if (0 > err)
		{
			goto error;
		}
		err = at_tok_nextstr(&line, &response);
		if (0 > err)
		{
			goto error;
		}
		RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char *));
		at_response_free(p_response);
	}
	else
	{
		asprintf(&cmd, "AT^STGR=0,\"%s\"",(char *)RawData);
		err = at_send_command(cmd, &p_response);
		free(cmd);
		if(err<0 || p_response->success == 0)
		{
	  ALOGE("Send STK  envelope  Failed!\n");
		  goto error;
		}
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
		at_response_free(p_response);
	}
	/* End modifed by x84000798 for DTS2012073007612 2012-07-31 */
	return ;
error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;	
}

/*************************************************
 Function:  request_stk_setup_call
 Description:  
    for stk envelope request. we use AT^CSEN for fit balong
 Calls: 
    
 Called By:
    hwril_request_sim
 Input:
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
void request_stk_setup_call(void *data, size_t datalen, RIL_Token t)
{
	char *cmd;

	if(((int *)data)[0] == 0){
		asprintf(&cmd, "AT^CSTC=0");
	}else{
		asprintf(&cmd, "AT^CSTC=1");
	}
	at_send_command(cmd, NULL);
	free(cmd);
	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
}

/* Begin added by x84000798 for DTS2012073007612 2012-07-31 */
/*************************************************
 Function:  request_stk_get_profile
 Description:  
 Calls: 
 Called By:
    hwril_request_sim
 Input:
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
void request_stk_get_profile(void *data, size_t datalen, RIL_Token token)
{
	int err, profile_storage;
	char *line, *profile;
	ATResponse *p_response = NULL;

	err = at_send_command_singleline("AT+CUSATR=3", "+CUSATR:", &p_response);
	if (0 > err || 0 == p_response->success)
	{
		goto error;
	}
	line = p_response->p_intermediates->line;
	err = at_tok_start(&line);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextint(&line, &profile_storage);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextstr(&line, &profile);
	if (0 > err)
	{
		goto error;
	}
	RIL_onRequestComplete(token, RIL_E_SUCCESS, profile, sizeof(char *));
	at_response_free(p_response);
	return;

error:
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
}


/*************************************************
 Function:  request_stk_set_profile
 Description:  
 Calls: 
 Called By:
    hwril_request_sim
 Input:
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
void request_stk_set_profile(void *data, size_t datalen, RIL_Token token)
{
	int err;
	char *cmd;
	char *profile = NULL;
	ATResponse *p_response = NULL;

	profile = (char *)data;
	asprintf(&cmd, "AT+CUSATW=0,\"%s\"", profile);
	err = at_send_command_singleline(cmd, "+CUSATW:", &p_response);
	free(cmd);
	if (0 > err || 0 == p_response->success)
	{
		goto error;
	}
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	at_response_free(p_response);
	return;
error:
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return;
}
/* End added by x84000798 for DTS2012073007612 2012-07-31 */

int get_sim_type()
{
	int err;
	ATResponse *p_response = NULL;
	char *line;
	int type = RIL_APPTYPE_USIM;
	
	err = at_send_command_singleline("AT^CARDMODE", "^CARDMODE:", &p_response);
	if(err<0 || p_response->success == 0)  
	{
		goto end;
	}
	line = p_response->p_intermediates->line;
	if(RIL_DEBUG)ALOGD("sim card type is  %s",line);
	err = at_tok_start(&line);
	if(err<0) goto end;
	err = at_tok_nextint(&line, &type);
	if(RIL_DEBUG)ALOGD("parameter is status:%d",type);
	if(err<0) goto end;
end:
	at_response_free(p_response); 
	return type;
}

/*************************************************
 Function:  request_get_sim_status
 Description:  
    get the sim status and inform the telephony
 Calls: 
    
 Called By:
    hwril_request_sim
 Input:
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
void request_get_sim_status(void *data, size_t datalen, RIL_Token token)
{
    if(RIL_DEBUG)ALOGD("isUSIMCARD  %d",isUSIMCARD);
    static RIL_AppStatus app_status_array[] = {
	/* SIM_ABSENT = 0 */
	{ RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN,	RIL_PERSOSUBSTATE_UNKNOWN,
	  NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
	/* SIM_NOT_READY = 1 */
	{ RIL_APPTYPE_SIM,     RIL_APPSTATE_DETECTED,		RIL_PERSOSUBSTATE_UNKNOWN,
	  NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
	/* SIM_READY = 2 */
	{ RIL_APPTYPE_SIM,     RIL_APPSTATE_READY,			RIL_PERSOSUBSTATE_READY,
	  NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
	/* SIM_PIN = 3 */
	{ RIL_APPTYPE_SIM,     RIL_APPSTATE_PIN,			RIL_PERSOSUBSTATE_UNKNOWN,
	  NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
	/* SIM_PUK = 4 */
	{ RIL_APPTYPE_SIM,     RIL_APPSTATE_PUK,			RIL_PERSOSUBSTATE_UNKNOWN,
	  NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
	/* SIM_NETWORK_PERSONALIZATION = 5 */
	{ RIL_APPTYPE_SIM,     RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
	  NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
	 //add by wkf32792 for android 3.x begin
	 // RUIM_ABSENT = 6
        { RIL_APPTYPE_UNKNOWN, RIL_APPSTATE_UNKNOWN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // RUIM_NOT_READY = 7
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_DETECTED, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // RUIM_READY = 8
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_READY, RIL_PERSOSUBSTATE_READY,
          NULL, NULL, 0, RIL_PINSTATE_UNKNOWN, RIL_PINSTATE_UNKNOWN },
        // RUIM_PIN = 9
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_PIN, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN },
        // RUIM_PUK = 10
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_PUK, RIL_PERSOSUBSTATE_UNKNOWN,
          NULL, NULL, 0, RIL_PINSTATE_ENABLED_BLOCKED, RIL_PINSTATE_UNKNOWN },
        // RUIM_NETWORK_PERSONALIZATION = 11
        { RIL_APPTYPE_RUIM, RIL_APPSTATE_SUBSCRIPTION_PERSO, RIL_PERSOSUBSTATE_SIM_NETWORK,
           NULL, NULL, 0, RIL_PINSTATE_ENABLED_NOT_VERIFIED, RIL_PINSTATE_UNKNOWN }
		//add by wkf32792 for android 3.x end
	};
		
	RIL_CardState card_state;
	int num_apps, i, sim_status;
	RIL_AppType sim_type = RIL_APPTYPE_USIM;
	//RIL_CardStatus *p_card_status = NULL;

	sim_status = get_sim_status();

	if( NETWORK_CDMA == g_module_type )
	{
		sim_type=RIL_APPTYPE_RUIM;
	}
	else
	{
		sim_type = (RIL_AppType)get_sim_type();
	}
	if(RIL_DEBUG)ALOGD("sim type is %d",sim_type);

	if (sim_status == SIM_ABSENT)
	{
		card_state = RIL_CARDSTATE_ABSENT;
		num_apps = 0;
	}
	else
	{
		card_state = RIL_CARDSTATE_PRESENT;
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3	   
        num_apps = 2;
#else	
        num_apps = 1;
#endif
		
	}

	/* Allocate and initialize base card status. */
#ifdef ANDROID_3	  
    RIL_CardStatus_v6* p_card_status = (RIL_CardStatus_v6 *)malloc(sizeof(RIL_CardStatus_v6));
#else	
    RIL_CardStatus* p_card_status = (RIL_CardStatus *)malloc(sizeof(RIL_CardStatus));
#endif
	
	
	if(p_card_status == NULL)
	{
		RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
		return ;
	}
	p_card_status->card_state = card_state;
	p_card_status->universal_pin_state = RIL_PINSTATE_UNKNOWN;
	p_card_status->gsm_umts_subscription_app_index = RIL_CARD_MAX_APPS;
	p_card_status->cdma_subscription_app_index = RIL_CARD_MAX_APPS;
	p_card_status->num_applications = num_apps;
#ifdef ANDROID_3    
    p_card_status->ims_subscription_app_index = RIL_CARD_MAX_APPS;
#else	
#endif
    
	/* Initialize application status. */
	for (i = 0; i < RIL_CARD_MAX_APPS; i++)
	{
		p_card_status->applications[i] = app_status_array[SIM_ABSENT];
                p_card_status->applications[i].app_type = sim_type;
	}

	/* Pickup the appropriate application status that reflects sim_status for gsm. */
	if (num_apps != 0)
	{
		/* Only support one app, gsm. */
		p_card_status->num_applications = 1;
		if ( NETWORK_CDMA == g_module_type )
		{
			p_card_status->cdma_subscription_app_index = 0;
		}
		else
		{
			p_card_status->gsm_umts_subscription_app_index = 0;
		}
		/* Get the correct app status. */
		p_card_status->applications[0] = app_status_array[sim_status];
        p_card_status->applications[0].app_type = sim_type;
#ifdef ANDROID_3	   
        p_card_status->applications[1] = app_status_array[sim_status + RUIM_ABSENT];
        p_card_status->applications[1].app_type = sim_type;	    
#endif
        
    }
//end modified by wkf32792 for android 3.x 20110815

	RIL_onRequestComplete(token, RIL_E_SUCCESS, (char*)p_card_status, sizeof(*p_card_status));
	/* Begin added by c00221986 for DTS2013040706143*/
	//by alic 添加进去之后CDMA不主动请求RSSI值了；但是。。。如果不再设置一下的话，又不拨号了。。。
	//按道理，是不应该影响上报RSSI值的。就是说有时候会乱报卡的状态发生变化。为了恢复java的流程吧？
	if (SIM_READY == sim_status)
	{
		ALOGD("-->>Here is added, update the radio state in get_sim_status");
		if(NETWORK_CDMA == g_module_type)
		{
			if(CDMA_RUIM_TYPE == g_cdma_subtype)
			{
				set_radio_state(RADIO_STATE_RUIM_READY);
			}
			else
			{
				set_radio_state(RADIO_STATE_NV_READY);
			}
		}
		else
		{
			set_radio_state(RADIO_STATE_SIM_READY);
		}
	}
	/* Begin added by c00221986 for DTS2013040706143*/
	free(p_card_status);
	p_card_status = NULL;
}
/*************************************************
 Function:  get_sim_lock_status
 Description:  
    get the sim cardlock status
 Calls: 
    at_send_command_singleline
    at_tok_start
    at_tok_nextint
    at_response_free
 Called By:
    request_enter_network_depersonalization
    get_sim_status
 Input:
    none
 Output:
    int *status    cardlock status
    int *times	   remain unlock times
 Return:
    int if error -1 or not 0
 Others:
    add by lifei kf34826 2010.11.24
**************************************************/
int get_sim_lock_status(int *status, int *times)
{ 
   int err;
   ATResponse *p_response;
   char *line;
   p_response = NULL;
   	
   err = at_send_command_singleline("AT^CARDLOCK?", "^CARDLOCK:", &p_response);
   if(err<0 || p_response->success == 0)  
   goto error;
   line = p_response->p_intermediates->line;
   if(RIL_DEBUG)ALOGD("CARDLOCK is  %s",line);
   err = at_tok_start(&line);
   if(err<0)   goto error;
   err = at_tok_nextint(&line, status);
   if(err<0)   goto error;
   err = at_tok_nextint(&line, times);
   if(RIL_DEBUG)ALOGD("parameter is status:%d,times:%d",*status,*times);
   if(err<0)   goto error;
   at_response_free(p_response); 
   return 0;
error:
   at_response_free(p_response); 
   return -1;
}
/*************************************************
 Function:  request_enter_network_depersonalization
 Description:  
    request unlock the cardlock
 Calls: 
    get_sim_lock_status
    at_get_cme_error
    at_response_free
    at_send_command
    set_radio_state
 Called By:
    hwril_request_sim
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
    add by lifei kf34826 2010.11.24
**************************************************/
void request_enter_network_depersonalization( void *data, size_t datalen, RIL_Token token)
{
	if(RIL_DEBUG)ALOGD("*********************start  deactivated  network personlization*******************");
	int err,status,times; 
	int ret = -1;
	ATResponse *p_response;
	char * cmd;        
	p_response = NULL;

       asprintf(&cmd, "AT^CARDLOCK=\"%s\"", ((const char **)(data))[0]);
	
	err = at_send_command(cmd, &p_response);
	free(cmd);
	if( err<0 )
	{
		goto error;
	}
	else
	{   
		ret = at_get_cme_error(p_response);
		if(p_response->success == 1)
		{
			if(RIL_DEBUG)ALOGD("unlock CARDLOCK is  Success!!");
			RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0); 
			at_response_free(p_response);
            if(NETWORK_CDMA == g_module_type)
			{
				if(CDMA_RUIM_TYPE == g_cdma_subtype)
				{
					set_radio_state(RADIO_STATE_RUIM_NOT_READY);
				}
				else
				{
					set_radio_state(RADIO_STATE_NV_NOT_READY);
				}
			}
			else
			{
				set_radio_state(RADIO_STATE_SIM_NOT_READY);
			}				  

			return;
		}
		else
		{
			err= get_sim_lock_status( &status,  &times);
		}
	}            
error:         	
	if(ret == CME_PASSWORD_INCORRECT)
	{
		RIL_onRequestComplete(token, RIL_E_PASSWORD_INCORRECT, &times,sizeof(times));
	}	
	else
	{
		RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	at_response_free(p_response);
	return;	  
}

/***************************************************
 Function:  hwril_request_sim
 Description:  
    Handle the request about sim. 
 Calls: 
    request_sim_io
    request_enter_sim_pin
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
void hwril_request_sim (int request, void *data, size_t datalen, RIL_Token token)
{    

	switch (request) 
	{          
		case RIL_REQUEST_SIM_IO:
		{
			request_sim_io(data,datalen,token);
			break;
		}

		case RIL_REQUEST_GET_SIM_STATUS: 
		{
            if(RIL_DEBUG)ALOGD("***request_get_sim_status***\n");
			request_get_sim_status(data,datalen,token);
		    break;
		}

		case RIL_REQUEST_ENTER_SIM_PIN:
		case RIL_REQUEST_ENTER_SIM_PUK:
		case RIL_REQUEST_ENTER_SIM_PIN2:
		case RIL_REQUEST_ENTER_SIM_PUK2:
		case RIL_REQUEST_CHANGE_SIM_PIN:
		case RIL_REQUEST_CHANGE_SIM_PIN2:
		{
			request_enter_sim_pin(data, datalen, token, request);
			break;
		}

            case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION:
		{
        	request_enter_network_depersonalization(data, datalen,  token);
			break;
		}
		case RIL_REQUEST_GET_IMSI:
		{
			request_get_sim_imsi(data, datalen,  token);
			break;
		}

		case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
		{
			request_stk_envelope(data, datalen,  token);
			break;	  
		}
		
		case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
		{
		    request_stk_terminal_response(data, datalen,  token);
		    break;
		}
		
		case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:
		{
			request_stk_setup_call(data, datalen,  token);
			break;
		}

		case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:
		{	
			request_stk_service_is_running(data, datalen,  token);
			break;
		}
		/* Begin added by x84000798 for DTS2012073007612 2012-07-31 */
		case RIL_REQUEST_STK_GET_PROFILE:
		{
			request_stk_get_profile(data, datalen, token);
			break;
		}
		case RIL_REQUEST_STK_SET_PROFILE:
		{
			request_stk_set_profile(data, datalen, token);
			break;
		}
		/* End   added by x84000798 for DTS2012073007612 2012-07-31 */
		default:
		{
			RIL_onRequestComplete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			break;
		}
	}
}

/***************************************************
 Function:  hwril_unsolicited_sim
 Description:  
    Handle the unsolicited about sim. 
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

void hwril_unsolicited_sim (const char *s)
{
	char  *line;
	char  *line1;
	int err; 
	
	if (strStartsWith(s,"^SIMST:"))
	{
		if(RIL_DEBUG)ALOGD("SIM Card Status is Changed!!");
		RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_SIM_STATUS_CHANGED, NULL, 0);   
	}       
	else if(strStartsWith(s, "*PSSIMSTAT"))//MT509
	{
		int sim_state = 1;//out of card
		err = -1;
		line = strdup(s);
		at_tok_start(&line);
		err = at_tok_nextint(&line, &sim_state);		
	      
        if(((0 == sim_state) && (sState != RADIO_STATE_SIM_READY ))
			|| ((1 == sim_state)&& (sState == RADIO_STATE_SIM_READY )))//Modified by 84000798 2012-06-11 DTS2012061209625
		{	
			set_radio_state(RADIO_STATE_SIM_NOT_READY);
		}
	}

	/*BEGIN modify by dKF36756 2011-3-9 增加了STK  功能的主动上报*/
	else if(strStartsWith(s, "^STIN:"))
	{ 
	    int CmdType = -1;
		char * RawData = NULL;
				
		line = strdup(s);
		line1= line;;
		at_tok_start(&line);
		err = at_tok_nextint(&line, &CmdType);
		
		if (err < 0) 
		{
			ALOGE( "%s","^STIN parse error 1");
			return;
		}
		err = at_tok_nextstr(&line, &RawData);

		if (err < 0) 
		{
			ALOGE( "%s","^STIN parse error 2");
			return;			
		}

		if( at_tok_hasmore(&line))
		{
		   ALOGE("Error STIN !!!");
		   return;
		}
		switch(CmdType)
		{
		  case STK_PROACTIVE_COMMAND:
			   RIL_onUnsolicitedResponse (RIL_UNSOL_STK_PROACTIVE_COMMAND, (void*)RawData,  sizeof(RawData)); 
			   break;
		  case  STK_EVENT_NOTIFY:
		  	   RIL_onUnsolicitedResponse (RIL_UNSOL_STK_EVENT_NOTIFY, (void*)RawData,  sizeof(RawData));
			   break;
		  case  STK_SESSION_END:
			     RIL_onUnsolicitedResponse (RIL_UNSOL_STK_SESSION_END, NULL,  0);
			   break;
		  default:
		  	   break;
		  	   		  	    
		}
	    free(line1);
	}
	/*END modify by dKF36756 2011-3-9 增加了STK  功能的主动上报*/

	// +CUSATP: "D0 0D 81 03 01 05 00 82 02 81 82 99 02 09 0A"  
	/* Begin modifed by x84000798 for DTS2012073007612 2012-07-31 */
	else if(strStartsWith(s,"+CUSATEND"))
	{
		RIL_onUnsolicitedResponse (RIL_UNSOL_STK_SESSION_END, NULL,  0);
	}
	else if(strStartsWith(s,"+CUSATP:"))
	{
		char ch[3] = {0};
		char *RawData = NULL;
		char *tmp = NULL;
		line = strdup(s);
		line1 = line;
		err = at_tok_start(&line1);
		if (0 > err)
		{
			free(line);
			return;
		}
		err = at_tok_nextstr(&line1, &RawData);
		if (0 > err)
		{	
			free(line);
			return;
		}
		tmp = RawData;
		tmp += 2;
		strncpy(ch, tmp, 2);
		ch[2] = '\0';
		/*
		 * 如果上报上来的字符串第二个字节为81，则判断第七个字节如果是05,11,12,13,14,上报EVENT_NOTIFY，其余上报PROACTIVE_COMMAND
		 * 否则判断第六个字节如果是 05,11,12,13,14,上报EVENT_NOTIFY，其余上报PROACTIVE_COMMAND
		 * 01 - sim refresh
		 * 05 - setup envent list
		 * 10 - setup call
		 * 11 - send ss
		 * 12 - send ussd
		 * 13 - send short message
		 * 14 - send dtmf
		 */
		if (strstr(ch, "81"))
		{
			tmp += 10;
			strncpy(ch, tmp, 2);
			ch[2] = '\0';
			if (strstr(ch, "05") || strstr(ch, "11") || strstr(ch, "12") || strstr(ch, "13") || strstr(ch, "14") || strstr(ch, "40") || strstr(ch, "41") || strstr(ch, "42") || strstr(ch, "43"))//Modify by c00221986 for BIP
			{
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_EVENT_NOTIFY, RawData,  sizeof(RawData));
			}
			else if(strstr(ch, "01"))
			{
				//don't support SIM_REFRESH, report STK_SESSION_END to Framework !
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_SESSION_END, NULL,  0);
			}
			else if(strstr(ch, "10"))
			{
				//don't support SETUP_CALL, report STK_SESSION_END to Framework !
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_SESSION_END, NULL,  0);
			}
			else
			{
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_PROACTIVE_COMMAND, RawData,  sizeof(RawData));
			}
		}
		else
		{
			tmp += 8;
			strncpy(ch, tmp, 2);
			ch[2] = '\0';
			if (strstr(ch, "05") || strstr(ch, "11") || strstr(ch, "12") || strstr(ch, "13") || strstr(ch, "14") || strstr(ch, "40") || strstr(ch, "41") || strstr(ch, "42") || strstr(ch, "43"))//Modify by c00221986 for BIP
			{
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_EVENT_NOTIFY, RawData,  sizeof(RawData));
			}
			else if(strstr(ch, "01"))
			{
				//don't support SIM_REFRESH, report STK_SESSION_END to Framework !
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_SESSION_END, NULL,  0);
			}
			else if(strstr(ch, "10"))
			{
				//don't support SETUP_CALL, report STK_SESSION_END to Framework !
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_SESSION_END, NULL,  0);
			}
			else
			{
				RIL_onUnsolicitedResponse (RIL_UNSOL_STK_PROACTIVE_COMMAND, RawData,  sizeof(RawData));
			}
		}
		free(line);		
	}
	/* End modifed by x84000798 for DTS2012073007612 2012-07-31 */
	return;
}

