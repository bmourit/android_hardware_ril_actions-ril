
/***************************************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-ps.c
 Author: fangyuchuang/00140058
 Version: V1.0
 Date: 2010/04/07
 Description:
    The file realize ps service function,include below interface:
    
	RIL_REQUEST_SETUP_DATA_CALL
	RIL_REQUEST_DEACTIVATE_DATA_CALL
	RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE
	RIL_REQUEST_DATA_CALL_LIST
****************************************************************************/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

  when        who      version    why 
 -------    -------    -------   --------------------------------------------
2011/5/14   z128629    V3B02D01  add tdscdma module data service function

===========================================================================*/

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/
#include "huawei-ril.h"
#include "ril-ps-api.h"
#include "ril-ps-sm.h"

/*===========================================================================

                    INTERNAL FUNCTION PROTOTYPES

===========================================================================*/

static void on_pdp_context_list_changed(void *param);
static void request_or_send_pdp_context_list(RIL_Token *token);
static void request_setup_default_pdp_ppp(int request, void *data, size_t datalen, RIL_Token token);
static void request_deative_default_pdp_ppp(int request, void *data, size_t datalen, RIL_Token token);
/*Begin to modify by h81003427 20120526 for ps domain incoming packet call*/
int  setup_ppp_connection(RIL_Token token, const char* apn, const char* user, const char* passwd, const char* pdptype, const char *authoption); //modified by wkf32792 for android 3.x
/*End to modify by h81003427 20120526 for ps domain incoming packet call*/
static void request_setup_default_pdp_rmnet(int request, void *data, size_t datalen, RIL_Token token);
static void request_deative_default_pdp_rmnet(int request, void *data, size_t datalen, RIL_Token token);
int  request_network_regist(void);
extern struct DEVICEPORTS Devices[];
extern int g_device_index;
extern int s_MaxError;// by alic
extern int g_IsDialSucess;
/*===========================================================================

                   INTERNAL DEFINITIONS AND TYPES

===========================================================================*/
/*the variable for rmnet*/
char oldapn[10];//alic old
//char ip_address[20];//alic old
char ip_address[64];
char dns_address[64];
char gateway_address[64];

//Bebin to be added by c00221986 for ndis
extern char PORT_ECM_KEYNAME[60];
extern HWRIL_NDIS_MODEM hw_ndis_or_modem;
//End being added by c00221986 for ndis

//char *rmnet0_response[3] = { "1", "rmnet0", ip_address};
//char *rmnet0_response[3] = { "1", "eth1", ip_address};
char *rmnet0_response[3] = { "1", PORT_ECM_KEYNAME, ip_address};
HWRIL_RMNET_PARAMETERS_ST ndis_st;
#define CALL_INACTIVE  					0
#define CALL_ACTIVE_PHYSLINK_DOWN    	1
#define CALL_ACTIVE_PHYSLINK_UP      	2
#ifdef ANDROID_3
	RIL_Data_Call_Response_v6 call_tbl_v6;
#else
	RIL_Data_Call_Response call_tbl ={1, CALL_INACTIVE, "ip", NULL, NULL};
#endif
 
#define APN_LEN 128
#define PPP_USERNAME_LEN 64
#define PPP_PASSWORD_LEN 128
#define PPP_PDP_TYPE_LEN 64  //added by wkf32792 for android 3.x 20110815
#define GATE_WAY_LEN 64      //added by wkf32792 for android 3.x 20110815
char g_apn[APN_LEN+1]="";
char g_gateways[GATE_WAY_LEN+1]="";       //added by wkf32792 for android 3.x 20110815
char g_pdptype[PPP_PDP_TYPE_LEN+1]="";   //added by wkf32792 for android 3.x 20110815

#define DEFAULT_CID          "1"
#define ACTIVEPDP  			 "AT+CGDATA=\"PPP\",1"  //added by c00221986 for balong data card
#define BALONG_ACTIVEPDP	 "ATD*99#"    //added by c00221986 for balong data card
extern Module_Type g_module_type;
extern int g_pspackage; //Added by x84000798 for PS package test
extern int g_provider; //Added by c00221986 for balong data card
extern MODEM_Vendor g_modem_vendor;

/*===========================================================================

                                FUNCTIONS

===========================================================================*/

/***************************************************
 Function:  request_last_data_call_fail_cause
 Description:  
    query the last data call failure cause code. 
 Calls: 
 Called By:
    hwril_request_ps
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

static void request_last_data_call_fail_cause(void *data, size_t datalen, RIL_Token token)
{
	int response[1];
	response[0] =PDP_FAIL_ERROR_UNSPECIFIED;
	int type = -1, code = -1;
       int err = -1;
       ATResponse *p_response = NULL;
       char *line;


       if (NETWORK_CDMA==g_module_type)
      	{
      	 	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
		return;
      	}
	    
	err = at_send_command_singleline("AT^GLASTERR=1", "^GLASTERR:", &p_response);
	if (err < 0 || p_response->success == 0) 
       {
             goto error;
       }

	line = p_response->p_intermediates->line;
       if (RIL_DEBUG) ALOGD("GLASTERR:--------------------------------------%s",line);
	   
	 err = at_tok_start (&line);    
        if (err < 0) 
        {
               goto error;
	 }
    
	 err = at_tok_nextint(&line, &type);
	 if (err < 0) 
	 {
		goto error;
	 }

	 err = at_tok_nextint(&line, &code);
	 if(RIL_DEBUG)ALOGD("--------------------------code=%d",code);
	/* Begin modified by x84000798 2012-06-26 DTS2012062605329 */
	if (err < 0 || 0 == code) 
	{
		goto error;
		//RIL_onRequestComplete(token, RIL_E_SUCCESS, response, sizeof(response));
		//return ;
	}
	/* End modified by x84000798 2012-06-26 DTS2012062605329 */
        
	 response[0] = code;

	 RIL_onRequestComplete(token, RIL_E_SUCCESS, response, sizeof(response));
	 at_response_free(p_response);
	 
        return ;
error:
	 RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	 at_response_free(p_response);
	 return ;
	 
}
//end add by hkf39947
/***************************************************
 Function:  request_network_regist
 Description:  
    get the ps service status. 
 Calls: 
 Called By:
    setup_ppp_connection
 Input:
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
int request_network_regist(void)
{
    int err;
    ATResponse *p_response = NULL;
    char *line;
    int srv_status, srv_domain;
    
	/* Begin added by c00221986 for DTS2013040706143 */
    if(RIL_DEBUG) ALOGD("----***********request_network_regist********---");
    	
    err = at_send_command_singleline("AT^SYSINFOEX", "^SYSINFOEX:", &p_response);
	
	if ((0 == err) && (1 == p_response->success))
    {
	
    line = p_response->p_intermediates->line;
	if(RIL_DEBUG) ALOGD("sysinfo:%s",line);
    err = at_tok_start (&line);    
		if (0 > err)
    {
        goto error;
	}
    
	err = at_tok_nextint(&line, &srv_status);
		if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextint(&line, &srv_domain);
		if (0 > err)
	{
		goto error;
	}
    
    if((2 == srv_status) && ((2 == srv_domain) || (3 == srv_domain)))
    {
        ;
    }
    else
    {
        goto error;
    }
    at_response_free(p_response);
    return 0;
	} 
	else
	{
	    err = at_send_command_singleline("AT^SYSINFO", "^SYSINFO:", &p_response);
		
		if (err < 0 || p_response->success == 0) 
	    {
	        goto error;
	    }
		
	    line = p_response->p_intermediates->line;
	if(RIL_DEBUG) ALOGD("sysinfo:%s",line);
	    err = at_tok_start (&line);    
	    if (err < 0) 
	    {
	        goto error;
		}
	    
		err = at_tok_nextint(&line, &srv_status);
		if (err < 0) 
		{
			goto error;
		}
		err = at_tok_nextint(&line, &srv_domain);
		if (err < 0) 
		{
			goto error;
		}
	    
	    if((2 == srv_status) && ((2 == srv_domain) || (3 == srv_domain)))
	    {
	        ;
	    }
	    else
	    {
	        goto error;
	    }
	    at_response_free(p_response);
	    return 0;
    }
    /* End   added by c00221986 for DTS2013040706143 */   
error:
    if(RIL_DEBUG) ALOGE( "ps cannot be used");
    at_response_free(p_response);
    return -1;
}

/***************************************************
 Function:  hwril_request_ps
 Description:  
    Handle the request about ps. 
 Calls: 
    request_setup_default_pdp_ppp
    request_deative_default_pdp_ppp
    request_setup_default_pdp_rmnet
    request_deative_default_pdp_rmnet
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
void hwril_request_ps (int request, void *data, size_t datalen, RIL_Token token)
{    
	switch (request) {
		case RIL_REQUEST_SETUP_DATA_CALL:
		{
//#ifdef HUAWEI_RMNET
			if(NDIS_PROCESS == hw_ndis_or_modem){
				request_setup_default_pdp_rmnet( request, data, datalen,token);
			}
//#else
			else{
				request_setup_default_pdp_ppp( request, data, datalen,token);
			}
//#endif
			break;
		}
		    
		case RIL_REQUEST_DEACTIVATE_DATA_CALL:
		{
//#ifdef HUAWEI_RMNET
			if(NDIS_PROCESS == hw_ndis_or_modem){
				request_deative_default_pdp_rmnet( request, data, datalen,token);
			}
//#else
			else{
				request_deative_default_pdp_ppp( request, data, datalen,token);	
			}
//#endif
			RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
			break;
		}
		    
		case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
		{
			//begin add by hkf39947 
			//RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
			request_last_data_call_fail_cause(data, datalen, token);
			//end add by hkf39947
			break;
		}

		case RIL_REQUEST_DATA_CALL_LIST:
		{
			request_or_send_pdp_context_list(&token);
			break;
		}

		default:
		{
			if(RIL_DEBUG) ALOGD("%s:invalid request:%d\n",__FUNCTION__, request); 
			break;
		}
	}
}

/***************************************************
 Function:  hwril_request_ps
 Description:  
    Handle the request about ps. 
 Calls: 
    request_setup_default_pdp_ppp
    request_deative_default_pdp_rmnet
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
void hwril_unsolicited_ps (const char *s)
{
    /* Really, we can ignore NW CLASS and ME CLASS events here,
     * but right now we don't since extranous
     * RIL_UNSOL_PDP_CONTEXT_LIST_CHANGED calls are tolerated
     */
    /* can't issue AT commands here -- call on main thread */
    int err;
    int conn;
    if (strStartsWith(s, "^NDISSTAT:"))
    {   
	    char *line =NULL;
	        
		line = strdup(s);
		at_tok_start(&line);
		err = at_tok_nextint(&line, &conn);
		if (err < 0) 
		{
		   ALOGE( "NDISSTAT is Error!!\n");
		   return;
		}
		switch(conn)
		{
			case 0:
			  	RIL_requestTimedCallback(DEND_report, NULL, &TIMEVAL_CALLSTATEPOLL);
				break;
		  	case 1:
		  		RIL_requestTimedCallback(DCONN_report, NULL, &TIMEVAL_CALLSTATEPOLL);
				break;
			default:
				break;
		}
	    if(line != NULL)
		{
			free(line);
		}
	}
}

/***************************************************
 Function:  request_setup_default_pdp_ppp
 Description:  
    setup the pdp , if apn is no equals. reset the pdp SM
 Calls: 
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
static void request_setup_default_pdp_rmnet(int request, void *data, size_t datalen, RIL_Token token)
{
	const char *apn;
	const char *user;
	const char *password;
    const char *authpref;
		 
	unsigned int user_len = 0;
	unsigned int password_len = 0;
	unsigned int apn_len = 0;
	
	apn = ((const char **)data)[2];
	user = ((const char **)data)[3];
	password  = ((const char **)data)[4];
	authpref =  ((const char **)data)[5];
    char pdp_type[PPP_PDP_TYPE_LEN+1]= "IP"; 
	
	reset_ndis_sm();                    
	ALOGD("********RIL_VERSION = %d **********", RIL_VERSION);
	if (4 <= RIL_VERSION)
	{
		const char *pdptype = NULL;
	    pdptype = ((const char **)data)[6];
	    unsigned int pdp_type_len = 0;
    
	    if( pdptype != NULL && (pdp_type_len = strlen(pdptype)))
		{
			if( pdp_type_len > PPP_PDP_TYPE_LEN )
			{
				ALOGD("warning:apn too long");
			}
		    pdp_type_len = (pdp_type_len > PPP_PDP_TYPE_LEN)? PPP_PDP_TYPE_LEN:pdp_type_len;
			strncpy(pdp_type, ((const char **)data)[6],pdp_type_len);
			pdp_type[pdp_type_len] = '\0';
			strcpy(g_pdptype, pdp_type);
		}
	}

	memset(&ndis_st,0x00,sizeof(ndis_st));
	if( apn != NULL && (apn_len = strlen(apn)))
	{
		if( apn_len > APN_LEN )
		{
			ALOGD("warning:apn too long");
		}
	    apn_len = (apn_len > APN_LEN)? APN_LEN:apn_len;
		strncpy(ndis_st.apn, apn, apn_len);   
		ndis_st.apn[apn_len]='\0';
	}
	
    if ((user != NULL)&&(user_len = strlen(user)))  
	{
		if( user_len > PPP_USERNAME_LEN )
		{
			ALOGD("warning:user too long");
		}
	        user_len = (user_len > PPP_USERNAME_LEN)? PPP_USERNAME_LEN:user_len;
		strncpy(ndis_st.user,user,user_len);
		ndis_st.user[user_len]='\0';
		if(RIL_DEBUG)LOGD("RIL_REQUEST_SETUP_DATA_CALL: user isn't null;user(%s),len=%d\n",ndis_st.user,user_len);
	}

	if ((password != NULL)&&(password_len = strlen(password)))  
	{
		if( password_len > PPP_PASSWORD_LEN )
		{
			ALOGD("warning:password too long");
		}
	        password_len = (password_len > PPP_PASSWORD_LEN)? PPP_PASSWORD_LEN:password_len;
		strncpy(ndis_st.passwd, password,password_len);
		ndis_st.passwd[password_len]='\0';
		if(RIL_DEBUG)LOGD("RIL_REQUEST_SETUP_DATA_CALL: passwd isn't NULL ,passwd(%s),len=%d\n", ndis_st.passwd,password_len);
	}

	switch(*authpref)
	{
	  case '0':
	  	ndis_st.authpref = 0;
	  	break;
	  case '1':
	  	ndis_st.authpref = 1;
	  	break;
	  case '2':
	  	ndis_st.authpref = 2;
	  	break;
	  case '3':
	  	ndis_st.authpref = 3;     //单板可能不支持参数3
	  	break;
	  default:
	  	ndis_st.authpref = 0;
	  	break;
	}
	ndis_st.token = token;
	ndis_st.ndis_state = NDIS_EVENT_REQUEST_PDP_ACTIVE;
	
	ndis_sm(&ndis_st);
	return ;
}

/***************************************************
 Function:  request_setup_default_pdp_ppp
 Description:  
    setup the pdp, setup the ppp context and start pppd
 Calls: 
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
static void request_setup_default_pdp_ppp(int request, void *data, size_t datalen, RIL_Token t)
{
	g_pspackage = 0; //Added by x84000798 for PS package test
	const char *apn;
	const char *user;
	const char *password;
	const char *auth_type;
	char *auth_option;
	int ret;
    unsigned int user_len = 0;
	unsigned int password_len = 0;
	unsigned int apn_len = 0;
	char ppp_user[PPP_USERNAME_LEN+1]={0};
	char ppp_passwd[PPP_PASSWORD_LEN+1]={0};
	char pdp_apn[APN_LEN+1]={0};
    char pdp_type[PPP_PDP_TYPE_LEN+1]= "IP"; //added by wkf32792 for android 3.x 20110815

	apn         = ((const char **)data)[2];
	user        = ((const char **)data)[3];
	password    = ((const char **)data)[4];
	auth_type	= ((const char **)data)[5];
//begin added by wkf32792 for android 3.x 20110815
//begin modified by x84000798 for BYD ppp if RIL_VERSION >= 4 
	LOGD("********RIL_VERSION = %d **********", RIL_VERSION);
	if (4 <= RIL_VERSION)
	{
		const char *pdptype = NULL;
	    pdptype = ((const char **)data)[6];
	    unsigned int pdp_type_len = 0;
    
	    if( pdptype != NULL && (pdp_type_len = strlen(pdptype)))
		{
			if( pdp_type_len > PPP_PDP_TYPE_LEN )
			{
			ALOGD("warning:apn too long");
			}
		    pdp_type_len = (pdp_type_len > PPP_PDP_TYPE_LEN)? PPP_PDP_TYPE_LEN:pdp_type_len;
			ALOGD("*************data[6] is %s*********", ((const char **)data)[6]);
			strncpy(pdp_type, ((const char **)data)[6],pdp_type_len);
			pdp_type[pdp_type_len] = '\0';
			ALOGD("**********pdp_type is %s **********", pdp_type);
		}
	}
//end modified by x84000798 for BYD ppp if RIL_VERSION >=4
//end added by wkf32792 for android 3.x 20110815

	if(RIL_DEBUG)ALOGD("RIL_REQUEST_SETUP_DATA_CALL: indicator:%s", ((const char **)data)[0]);
	if(RIL_DEBUG)ALOGD("RIL_REQUEST_SETUP_DATA_CALL: apn:%s", apn);
	if(RIL_DEBUG)ALOGD("RIL_REQUEST_SETUP_DATA_CALL: auth type:%s", ((const char **)data)[5]);

	if ((user != NULL)&&(user_len = strlen(user)))  
	{
		if( user_len > PPP_USERNAME_LEN )
		{
			ALOGD("warning:user too long");
		}
	    user_len = (user_len > PPP_USERNAME_LEN)? PPP_USERNAME_LEN:user_len;
		strncpy(ppp_user, ((const char **)data)[3],user_len);
		ppp_user[user_len]='\0';
		if(RIL_DEBUG)ALOGD("RIL_REQUEST_SETUP_DATA_CALL: user isn't null;user(%s),len=%d\n", ppp_user,user_len);
	}
	//Begin to be modified by c00221986 for CDMA datacard
	else//by alic ,使能，否则不能找到CDMA的网
	{
		if(NETWORK_CDMA==g_module_type)
		{
			strcpy(ppp_user, "card");
		}
		if(RIL_DEBUG)ALOGD("RIL_REQUEST_SETUP_DATA_CALL: user is NULL !(%s)\n", ppp_user);
	}//by alic ,使能
	//End being modified by c00221986 for CDMA datacard

	if ((password != NULL)&&(password_len = strlen(password)))  
	{
		if( password_len > PPP_PASSWORD_LEN )
		{
			ALOGD("warning:password too long");
		}
	    password_len = (password_len > PPP_PASSWORD_LEN)? PPP_PASSWORD_LEN:password_len;
		strncpy(ppp_passwd, ((const char **)data)[4],password_len);
		ppp_passwd[password_len]='\0';
		if(RIL_DEBUG)ALOGD("RIL_REQUEST_SETUP_DATA_CALL: passwd isn't NULL ,passwd(%s),len=%d\n", ppp_passwd,password_len);
	}
	//Begin to be modified by c00221986 for CDMA datacard
	else //by alic ,使能，否则不能找到CDMA的网
	{
		if(NETWORK_CDMA==g_module_type)
		{
			strcpy(ppp_passwd, "card");
		}
		if(RIL_DEBUG)ALOGD("RIL_REQUEST_SETUP_DATA_CALL: passwd is NULL!(%s)\n", ppp_passwd);
	}//by alic ,使能，否则不能找到CDMA的网
	//End being modified by c00221986 for CDMA datacard
    if( apn != NULL && (apn_len = strlen(apn)))
	{
		if( apn_len > APN_LEN )
		{
			ALOGD("warning:apn too long");
		}
	    apn_len = (apn_len > APN_LEN)? APN_LEN:apn_len;
		strncpy(pdp_apn, ((const char **)data)[2],apn_len);
		pdp_apn[apn_len]='\0';
	}
	/* For PPP, we only support one CID; for Direct IP, we can support multiple CID */
	/* Begin modified by x84000798 for DTS2012080605618 2012-8-6*/
	switch(*auth_type)
	{
		case '0':	//PAP and CHAP is never performed
			auth_option = "-pap -chap";
			break;
		case '1':	//PAP may be performed; CHAP is never performed
			auth_option = "-chap";
			break;
		case '2':	//CHAP may be performed; PAP is never performed
			auth_option = "-pap";
			break;
		case '3':	//PAP/CHAP may be performed - baseband dependent
			auth_option = "";
			break;
		default:
			auth_option = "";
			break;
	}
	ret = setup_ppp_connection(t, pdp_apn, ppp_user, ppp_passwd, pdp_type, auth_option);
	if(RIL_DEBUG)ALOGD("ret = setup_ppp_connection ret = %d\n",ret);
	if(-1 == ret)
	{
		goto error;
	}
	return;
	
error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	return;

}

/***************************************************
 Function:  request_deative_default_pdp_ppp
 Description:  
    deative the ppp
 Calls: 
    enablePPPInterface
 Called By:
    request_setup_default_pdp_ppp
 Input:
    token   - pointer to void struct
    apn - apn
    user - the user will login
    passwd - password
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
static void request_deative_default_pdp_ppp(int request, void *data, size_t datalen, RIL_Token token)
{
    /* BEGIN DTS2011111400455  lkf52201 2011-12-7 added */
	//int cid;
	//cid = atoi((const char *)DEFAULT_CID);
    disable_ppp_Interface();
    /* END DTS2011111400455  lkf52201 2011-12-7 added */
}

/***************************************************
 Function:  setup_ppp_connection
 Description:  
    if the apn is the same as the old, and the pppd is active, get the ip address, otherwise setup the ppp
 Calls: 
    enablePPPInterface
 Called By:
    request_setup_default_pdp_ppp
 Input:
    token   - pointer to void struct
    apn - apn
    user - the user will login
    passwd - password
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
/*Begin to modify by h81003427 20120526 for ps domain incoming packet call*/
int  setup_ppp_connection(RIL_Token  token, const char* apn, const char* user, const char* passwd, const char* pdptype, const char *authoption)
/*End to modify by h81003427 20120526 for ps domain incoming packet call*/
{
    ALOGD("--->>>setup_ppp_connection<<<---");
	ATResponse *p_response = NULL;
	int err = -1;
	int cid;
	char *result[3];
	char *cmd;
    char ipaddress[64]={0};

	cid = atoi(DEFAULT_CID);
	/* Workaround: if the APN name is same and the current APN is already active, we can return directly */
	result[0] = DEFAULT_CID;
	if (g_apn[0] && strcmp(apn, g_apn) == 0)
	{
		result[1] = "ppp0";
		err = get_interface_addr(cid, "ppp0", ipaddress);
		
		if (err == 0)
		{
			result[2] = ipaddress;
			if(RIL_DEBUG)ALOGD("The PDP CID %s is already active: IP address %s for Inteface %s", DEFAULT_CID, ipaddress, result[1]);
			RIL_onRequestComplete(token, RIL_E_SUCCESS, result, sizeof(result));
			return 1;
		}
	}
    //if(NETWORK_CDMA != g_module_type)
    //{
        //if( request_network_regist() < 0)
        //{	
           // ALOGD("can not setup data call\n");
        	//return -1;
        //}
   // }
	
    hwril_kill_pppd();//在此不能发送ATH或者+++命令
    
    if( NETWORK_WCDMA == g_module_type )
    {
		at_send_command("AT+CGACT=0", NULL);//去激活pdp
    }
    else if(MODEM_VENDOR_SCV != g_modem_vendor)// if(NETWORK_TDSCDMA == g_module_type) // DTS2011111400455 lkf52201 2011-12-7 modified
    {
		at_send_command("ATH", NULL);//去激活pdp
    }
	
	/* Step1: Define the CID */
    if (RIL_DEBUG)ALOGD("setup_ppp_connection : Step 1");

	if ((NETWORK_CDMA != g_module_type) || (MODEM_VENDOR_ZTE != g_modem_vendor))
	{
	    if(NETWORK_CDMA == g_module_type)
	    {	/* HW CDMA, SCV CDMA */
	        asprintf(&cmd, "AT^PPPCFG = \"%s\", \"%s\"", user, passwd);
	    }
	    else
	    { 	/* ZTE WCDMA, HW WCDMA, SCV WCDMA */
	        asprintf(&cmd, "AT+CGDCONT=%s,\"%s\",\"%s\"", DEFAULT_CID, pdptype, apn);  //modified by wkf32792 for android 3.x 20110815
	    }
		
		
		err = at_send_command(cmd, &p_response);
		free(cmd);

	if (err < 0 || p_response->success == 0)
		{
			if (MODEM_VENDOR_SCV != g_modem_vendor)
			{
				if(RIL_DEBUG)ALOGW("Fail to define the PDP context1: %s", DEFAULT_CID);
			//modified by x84000798 for DTS2012042600987 move it to disable_ppp_Interface
			//shut_down_ttyUSB0();//cathion:move it here from disable_ppp_Interface, because if will cause em820u data connection error
			/* Begin modified by x84000798 2012-06-26 DTS2012062605329 */
		 	g_apn[0]='\0';
			at_response_free(p_response);
			return -1;	
			}
		/* End modified by x84000798 2012-06-26 DTS2012062605329 */
		}
		at_response_free(p_response);
		p_response = NULL;
	}
  
	/* Step2: Active the PDP Context */
    if(RIL_DEBUG)ALOGD("setup_ppp_connection : Step 2");
    if(NETWORK_TDSCDMA == g_module_type)
    {
    	err = at_send_command("ATD*99#",&p_response); 
    	if (err < 0 || p_response->success == 0)
    	{
			goto error;
    	}
    	/* BEGIN DTS2011110902480  lkf52201 2011-11-09 added */
	    sleep(3);
    	/* END DTS2011110902480  lkf52201 2011-11-09 added */
    }
    else
    {
	    if(NETWORK_CDMA == g_module_type)
	    {
	        asprintf(&cmd, "atd#777");
	    }
	    else if (MODEM_VENDOR_HAUWEI == g_modem_vendor)/*再按厂商来分类*/
	    {
	    	// Begin to be modified by c00221986 for balong for DTS2012112003753 2012-11-21
	    	if (HUAWEIB == g_provider)
	    	{
	    		asprintf(&cmd, BALONG_ACTIVEPDP);
	    	}
	    	else
	    	{
	    		//asprintf(&cmd, "AT+CGDATA=\"PPP\",%s", DEFAULT_CID); == asprintf(&cmd, ACTIVEPDP);
					asprintf(&cmd, "ATDT*99#");//现在我们code中用的是这个
	    		//asprintf(&cmd, ACTIVEPDP);//经过实测，这一句不能给普通卡拨号哦。。。
	    	}
	    	// End modified by c00221986 for balong for DTS2012112003753 2012-11-21
	    }
	    else if ((MODEM_VENDOR_SCV == g_modem_vendor) 
	      || ((MODEM_VENDOR_ZTE == g_modem_vendor) && (strncmp(Devices[g_device_index].PID, "0124", 4)==0)))
	    {
	    	asprintf(&cmd, "ATDT*99***1#");
	    }
			else if (MODEM_VENDOR_ALCATEL == g_modem_vendor )
	    {
	    	asprintf(&cmd, "ATDT*99#");//ATD*99# ,ATDT*99#(可以用bus hound来抓modem口的所有的命令了，哈哈), ATDT*99***1#, ATD#777直接不行
	    }
		else
		{
			asprintf(&cmd, "ATD*99#");
		}
	    
	    /* dial_at_modem()函数将AT命令写入modem口,然后从中读取命令返回值,最大读取时间为30s,如果
	     * 能读取到CONNECT或者NO CARRIER,则立即返回;若30s后还读取不到,则返回AT_ERROR_TIMEOUT,
	     * RIL将挂断此次拨号,将拨号失败报给上层,上层将重新发起拨号. 2011-12-09 lkf52201 modified
	     */
        err = dial_at_modem(cmd); 
	    
	    free(cmd);
       
	    if(AT_ERROR_GENERIC == err)
	    {
	    	if(RIL_DEBUG)ALOGW("Fail to activate the PDP context: %s", DEFAULT_CID);
	    	goto error;
	    }
	    /* BEGIN 2011-12-09 lkf52201 added */
	    else if (AT_ERROR_TIMEOUT == err)
	    {
	        if (RIL_DEBUG)
	        {
	           		 ALOGD("Read CONNECT or NO CORRIER from modem port TIMEOUT!!!");
	        }
	        goto error;
	    }
	    /* END 2011-12-09 lkf52201 added */
    }
	/* Step3: Enable the network interface */
    if(RIL_DEBUG)ALOGD("setup_ppp_connection : Step 3");

    /* BEGIN DTS2011110902480  lkf52201 2011-11-09 modified */
	//sleep(3);
	/* END DTS2011110902480  lkf52201 2011-11-09 modified */
	int ret = enable_ppp_interface(token,cid, user, passwd, authoption);  
	if ( ret == -1)
	{
		goto error;
	}
	strcpy(g_apn,apn);//save last apn
	strcpy(g_pdptype, pdptype);    //added by wkf32792 for android 3.x 20110815
	s_MaxError = 0;
	g_IsDialSucess = 1;
	return 1;

 error:
    s_MaxError++;
    if(RIL_DEBUG)ALOGD("s_MaxError %d", s_MaxError);
 	g_apn[0]='\0';
 	disable_ppp_Interface();      // DTS2011111400455  lkf52201 2011-12-7 modified
	at_response_free(p_response);
	return -1;
}
/*add by f00140058 for ps ppp end*/

/***************************************************
 Function:  request_deative_default_pdp_rmnet
 Description:  
    deactive the pdp connection.
 Calls: 
    ndis_sm
 Called By:
    on_request
 Input:
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
static void request_deative_default_pdp_rmnet(int request, void *data, size_t datalen, RIL_Token token)
{	
	memset(&ndis_st,0x00,sizeof(ndis_st));
	ndis_st.token = token;
	ndis_st.ndis_state = NDIS_EVENT_REQUEST_PDP_DEACTIVE;
	ndis_sm(&ndis_st);
	return ;
}

/***************************************************
 Function:  request_or_send_pdp_context_list
 Description:  
    request the pdp context, and list the context in the list.
 Calls: 
 Called By:
    on_request
 Input:
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
static void request_or_send_pdp_context_list(RIL_Token *t)
{
	ATResponse *p_response;
	ATLine *p_cur;
	int err;
	int n = 0;
	char *out;


	err = at_send_command_multiline ("AT+CGACT?", "+CGACT:", &p_response);
	if (err < 0 || p_response->success == 0) {
		if (t != NULL)
			RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
		else
			RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);
		return;
	}

	for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next)
		n++;
	int i;
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3	     
    RIL_Data_Call_Response_v6 *responses = (RIL_Data_Call_Response_v6 *)alloca(n * sizeof(RIL_Data_Call_Response_v6));
    for (i = 0; i < n; i++) {
        responses[i].status = -1;
/* BEGIN DTS2012010602643 lkf52201 2012-01-07 added for android 4.0 */
#ifdef ANDROID_4
        responses[i].suggestedRetryTime = -1;
#endif
/* END DTS2012010602643 lkf52201 2012-01-07 added for android 4.0 */
        responses[i].cid = -1;
        responses[i].active = -1;
        responses[i].type = "";
        responses[i].ifname = "";
        responses[i].addresses = "";
        responses[i].dnses = "";
        responses[i].gateways = "";
    }

    RIL_Data_Call_Response_v6 *response = responses;
#else	
    RIL_Data_Call_Response *responses = (RIL_Data_Call_Response *)alloca(n * sizeof(RIL_Data_Call_Response));
    for (i = 0; i < n; i++) {
		responses[i].cid = -1;
		responses[i].active = -1;
		responses[i].type = "";
		responses[i].apn = "";
		responses[i].address = "";
	}

	RIL_Data_Call_Response *response = responses;
#endif   
//end modified by wkf32792 for android 3.x 20110815
	
	for (p_cur = p_response->p_intermediates; p_cur != NULL;
	p_cur = p_cur->p_next) {
		char *line = p_cur->line;

		err = at_tok_start(&line);
		if (err < 0)
		goto error;

		err = at_tok_nextint(&line, &response->cid);
		if (err < 0)
		goto error;

		err = at_tok_nextint(&line, &response->active);
		if (err < 0)
		goto error;
		/*Begin added by x84000798 for DTS2012031607141 */
		if (1 == response->active)
		{
			//若cid处于激活状态，上报的active值与之前拨号成功上报的值保持一致，即为2；
			++(response->active);
		}
		/*End added by x84000798 for DTS2012031607141*/

		response++;
	}

	at_response_free(p_response);

	err = at_send_command_multiline ("AT+CGDCONT?", "+CGDCONT:", &p_response);
	if (err < 0 || p_response->success == 0) {
		if (t != NULL)
			RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
		else
			RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);
		return;
	}

	for (p_cur = p_response->p_intermediates; p_cur != NULL;
	p_cur = p_cur->p_next) {
		char *line = p_cur->line;
		int cid;

		err = at_tok_start(&line);
		if (err < 0)
			goto error;

		err = at_tok_nextint(&line, &cid);
		if (err < 0)
			goto error;

		for (i = 0; i < n; i++) {
			if (responses[i].cid == cid)
				break;
		}

		if (i >= n) {
			/* details for a context we didn't hear about in the last request */
			continue;
		}
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3	   
        responses[i].status = 0;
#else	
#endif
//end modified by wkf32792 for android 3.x 20110815
		err = at_tok_nextstr(&line, &out);
		if (err < 0)
			goto error;

        if(RIL_DEBUG)ALOGD("address : %s",out);

		responses[i].type = (char*)alloca(strlen(out) + 1);
		strcpy(responses[i].type, out);

		err = at_tok_nextstr(&line, &out);
		if (err < 0)
			goto error;

//begin modified by wkf32792 for android 3.x 20110815		
#ifdef ANDROID_3
/*Begin modified by x84000798 for DTS2012031607141 */
        //responses[i].ifname = (char*)alloca(strlen(out) + 1);
        //strcpy(responses[i].ifname, out);
        //Ril 拨号时，强制把interface name 写成"ppp0",这里与拨号处理保持一致
        responses[i].ifname = "ppp0";
/*End modified by x84000798 for DTS2012031607141 */		
#else
        responses[i].apn = (char*)alloca(strlen(out) + 1);
		strcpy(responses[i].apn, out);
#endif
//end modified by wkf32792 for android 3.x 20110815		

		err = at_tok_nextstr(&line, &out);
		if (err < 0)
			goto error;
/* Begin modified by x84000798 2012-05-25 for DTS2012051803897 */   
#ifdef ANDROID_3
        // 如果AT+CGDCONT?命令返回的ip地址为空,则将其设为0.0.0.0;否则,上层会抛出异常
        if ('\0' == out[0])
        {   
            responses[i].addresses = (char *)alloca(strlen("0.0.0.0")+1);
            strcpy(responses[i].addresses, "0.0.0.0");
        }
        else
        {
            responses[i].addresses = (char*)alloca(strlen(out) + 1);
    		strcpy(responses[i].addresses, out);
        }
#else
        if ('\0' == out[0])
        {   
            responses[i].address = (char *)alloca(strlen("0.0.0.0")+1);
            strcpy(responses[i].address, "0.0.0.0");
        }
        else
        {
            responses[i].address = (char*)alloca(strlen(out) + 1);
    		strcpy(responses[i].address, out);
        }
#endif
/* End modified by x84000798 2012-05-25 for DTS2012051803897 */
	}

	at_response_free(p_response);
	
//begin modified by wkf32792 for android 3.x 20110815
	if (t != NULL) {	
        RIL_onRequestComplete(*t, RIL_E_SUCCESS, responses,
            n * sizeof(*response));
	}
	else {
	 
        RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
		    (void*)responses, n * sizeof(*response));
	}	
//end modified by wkf32792 for android 3.x 20110815

	return;

error:
	if (t != NULL)
		RIL_onRequestComplete(*t, RIL_E_GENERIC_FAILURE, NULL, 0);
	else
		RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,
		NULL, 0);

	at_response_free(p_response);
	return;
}

/***************************************************
 Function:  onunsol_data_call_list_chanage
 Description:  
    onunsol data call list change 
 Calls: 
 Called By:
    ppp_main_loop
 Input:
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
void onunsol_data_call_list_chanage( void)
{
	RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED,NULL, 0);
}
