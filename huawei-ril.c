/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: huaweu-ril.c
 Author: liubing/00141886
 Version: V1.1
 Date: 2010/04/07
 Description:
         
 Function List:
 History:
 <author>             <time>          <version>               <desc>
 liubing                 10/04/07         1.0       
 fangyuchuang      10/07/28         1.1         
*******************************************************/

#include "huawei-ril.h"
#include <dirent.h>

#define GET_PID 

/* Global varialble used in ril layer */
#define ok_error(num) ((num)==EIO)

RIL_RadioState sState = RADIO_STATE_UNAVAILABLE;
extern int at_request;
extern Module_Type g_module_type ;
MODEM_Vendor g_modem_vendor;
extern CDMA_SubType g_cdma_subtype;
extern int g_csd;		//added by x84000798
extern int g_pspackage;	//added by x84000798
extern int s_MaxError;
extern int g_IsDialSucess; 

struct DEVICEPORTS Devices[]={
	#include "PID_table.h"
};
int g_device_index = 0;
int MAX_PID = 0;
int s_port = -1;
char * s_device_path = NULL;
char * s_modem_path = NULL;
int  s_device_socket = 0;
int s_closed = 0;  /* trigger change to this with s_state_cond */
 /* Begin to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
int g_provider = -1;
 /* End to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
/*BEGIN Added by z128629 DTS2011032203061:obtain tty by read usb description file after usb reset */
char SERIAL_PORT_MODEM_KEYNAME[60] = "";
char SERIAL_PORT_DIAG_KEYNAME[60] = "";
char SERIAL_PORT_PCUI_KEYNAME[60] = "";
/*END Added by z128629 DTS2011032203061:obtain tty by read usb description file after usb reset */

char PORT_ECM_KEYNAME[60] = "";
HWRIL_NDIS_MODEM hw_ndis_or_modem = MODEM_PROCESS;
pthread_mutex_t s_state_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_state_cond = PTHREAD_COND_INITIALIZER;

/*BEGIN Added by qkf33988 DTS2010110103862 CMTI方式短信上报问题 2010-11-13*/
#ifdef   CMTI_ISSUE
static pthread_t s_tid_read_sms;
static pthread_t s_cmd_for_reader_thread;
pthread_mutex_t s_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t s_cmd_cond = PTHREAD_COND_INITIALIZER;
extern int enable_ppp_interface(RIL_Token token,int cid, const char* user, const char* passwd, const char *authoption);
extern int setup_ppp_connection(RIL_Token  token, const char* apn, const char* user, const char* passwd, const char* pdptype, const char *authoption);
int  del_sms_control[2];
extern pthread_mutex_t s_readdelmutex ;
pthread_mutex_t s_hwril_readsms_mutex ;
extern void *hwril_read_sms_then_delete_sms (void *arg);
#endif
/*END Added by qkf33988 DTS2010110103862 CMTI方式短信上报问题 2010-11-13*/

/* I/F implemented in ril.cpp */
extern const char * requestToString(int request);
extern void onSmsInitReady(void* param);
extern void on_sim_ready();
extern void deconfigure_interface();
extern int get_sim_lock_status(int* status,int* times);     //add by lifei kf34826 2010.11.24
extern void request_stk_service_is_running( void *data, size_t datalen, RIL_Token token);

/* I/F implemented in ril-xxx.c */
void hwril_request_cc (int request, void *data, size_t datalen, RIL_Token token);
void hwril_request_dev (int request, void *data, size_t datalen, RIL_Token token);
void hwril_request_mm (int request, void *data, size_t datalen, RIL_Token token);
void hwril_request_sms (int request, void *data, size_t datalen, RIL_Token token);
void hwril_request_ps (int request, void *data, size_t datalen, RIL_Token token);
void hwril_request_sim (int request, void *data, size_t datalen, RIL_Token token);
void hwril_request_ss (int request, void *data, size_t datalen, RIL_Token token);

void hwril_unsolicited_cc (const char *s);
void hwril_unsolicited_dev (const char *s);
void hwril_unsolicited_mm (const char *s);
void hwril_unsolicited_sms (const char *s, const char *smsPdu);
void hwril_unsolicited_ps (const char *s);
void hwril_unsolicited_sim (const char *s);
void hwril_unsolicited_ss (const char *s);
void reportSignalStrength(void *param);
    
/* static I/F which will be called by Android upper layer */
static void on_request (int request, void *data, size_t datalen, RIL_Token token);
static RIL_RadioState on_current_state();
static int on_supports (int requestCode);
static void on_cancel (RIL_Token token);
static void on_confirm(int channelID, ATResponse* response, int cookie);
static const char *on_get_version();

/* Internal function declaration */
static void on_unsolicited (const char *s, const char *smsPdu);


static const RIL_RadioFunctions s_callbacks = {
	RIL_VERSION,
	on_request,
	on_current_state,
	on_supports,
	on_cancel,
	on_get_version
};

RIL_RadioState getRadioState()
{
	return sState;
}

/*************************************************
 Function:  get_sim_status
 Description:  
    reference ril.h
 Calls: 
    at_send_command_singleline
 Called By:
    hwril_request_sim
    poll_sim_state
 Input:
    none
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
int get_sim_status()
{
	ATResponse *p_response = NULL;
	int err = 0;
	int ret = 0;
	int status = 0;    
	int times = 0;    
	char *cpinLine;
	char *cpinResult;

	if( CDMA_NV_TYPE == g_cdma_subtype ) 
	{
		ret = SIM_READY;
		return ret;
	}

	if (((sState == RADIO_STATE_OFF) || (sState == RADIO_STATE_UNAVAILABLE))  && (NETWORK_CDMA != g_module_type)){
		ret = SIM_NOT_READY;
		goto done;
	}

	err = at_send_command_singleline("AT+CPIN?", "+CPIN:", &p_response);
	
	if (err != 0 ) { 	
		ret = SIM_NOT_READY;
		goto done;
	}
	switch (at_get_cme_error(p_response)) {
		
		case CME_SUCCESS:
			break;

		case CME_SIM_NOT_VALID:
		case CME_SIM_NOT_INSERTED:
			ret = SIM_ABSENT;
			goto done;

		default:
			ret = SIM_NOT_READY;
			goto done;
	}
	//CPIN? has succeeded, now look at the result
	cpinLine = p_response->p_intermediates->line;
	err = at_tok_start (&cpinLine);

	if (err < 0) {
		ret = SIM_NOT_READY;
		goto done;
	}

	err = at_tok_nextstr(&cpinLine, &cpinResult);

	if (err < 0) {
		ret = SIM_NOT_READY;
		goto done;
	}
	if ((0 == strcmp (cpinResult, "SIM PIN"))  || (0 == strcmp (cpinResult, "R-UIM PIN"))){//Modify by fKF34305 for DTS2011082600432
		ret = SIM_PIN;
		goto done;
	} else if ((0 == strcmp (cpinResult, "SIM PUK"))  || (0 == strcmp (cpinResult, "R-UIM PUK"))){//Modify by fKF34305 for DTS2011082600432

		ret = SIM_PUK;
		goto done;
	} else if (0 == strcmp (cpinResult, "PH-NET PIN")) {
		return SIM_NETWORK_PERSONALIZATION;
	} else if (0 != strncmp (cpinResult, "READY", 5)) {
	    // we're treating unsupported lock types as "sim absent"
		ret = SIM_ABSENT;
		goto done;
	}
    err = get_sim_lock_status( &status,&times);
    if(RIL_DEBUG)ALOGD("Status is %d",status);
	switch(status)
	{
		case 2:
		default:
			break;
		case 1:
		case 3:
			ret =  SIM_NETWORK_PERSONALIZATION;
			goto done;
	}
	at_response_free(p_response);
	p_response = NULL;
	cpinResult = NULL;

	ret = SIM_READY;
	return ret;
done:
	if(RIL_DEBUG) ALOGD( "get_sim_status done err= %d,ret = %d\n",err,ret);
	at_response_free(p_response);
	return ret;
}

 void setScreenOff()
{
    if(RIL_DEBUG) ALOGD( "***setScreenOff***\n");
    at_send_command("AT^CURC=0", NULL);
    at_send_command("AT+CREG=0", NULL);
    at_send_command("AT+CGREG=0", NULL);   
}

 void setScreenOn()
{
    if(RIL_DEBUG) ALOGD( "***setScreenOn***\n");
    ATResponse *p_response = NULL;
    at_send_command_singleline("AT+CGACT?", "+CGACT:", &p_response);
    at_response_free(p_response);
    at_send_command("AT^CURC=1", NULL);
    at_send_command("AT+CREG=2", NULL);
    at_send_command("AT+CGREG=2", NULL); 
}

/***************************************************
 Function:  poll_sim_state
 Description:  
    poll sim state. 
 Calls: 
    set_radio_state
    RIL_requestTimedCallback
 Called By:
    set_radio_state
 Input:
    param - pointer to void
 Output:
    none
 Return:
    none
 Others:
    none 
**************************************************/
static void poll_sim_state(void *param)
{
	if(RIL_DEBUG) ALOGD( "***poll_sim_state***\n");

	if ( CDMA_NV_TYPE == g_cdma_subtype ) 
	{
		set_radio_state(RADIO_STATE_NV_READY);
		return;
	}

	if ((sState != RADIO_STATE_SIM_NOT_READY)
		&&(sState != RADIO_STATE_RUIM_NOT_READY)) {
		return;
	}
	switch(get_sim_status()) {           
		case SIM_ABSENT:
		case SIM_PIN:
		case SIM_PUK:
		case SIM_NETWORK_PERSONALIZATION:
		default:	
	              if(NETWORK_CDMA == g_module_type)
			{
				set_radio_state(RADIO_STATE_RUIM_LOCKED_OR_ABSENT); 
	 	  	}
			else
			{
				set_radio_state(RADIO_STATE_SIM_LOCKED_OR_ABSENT); 
			}
		return;
		case SIM_NOT_READY:
			RIL_requestTimedCallback (poll_sim_state, NULL, &TIMEVAL_SIMPOLL);
			return;

		case SIM_READY:	
	              if ( NETWORK_CDMA == g_module_type )
			{
				set_radio_state(RADIO_STATE_RUIM_READY);
			}
			else
			{
				set_radio_state(RADIO_STATE_SIM_READY);
			}	
            setScreenOff();
            setScreenOn();
			return;
	}
}

/***************************************************
 Function:  set_radio_state
 Description:  
    set radio state. 
 Calls: 
    poll_sim_state
    pthread_cond_broadcast
 Called By:
    poll_sim_state
    initialize_callback
    on_at_reader_closed
    request_radio_power
 Input:
    newState - radio state 
 Output:
    none
 Return:
    none
 Others:
    none 
**************************************************/
void set_radio_state(RIL_RadioState newState)
{
	RIL_RadioState oldState;               

	pthread_mutex_lock(&s_state_mutex);

	oldState = sState;

	if (s_closed > 0) 
	{
		/* If we're closed, the only reasonable state is RADIO_STATE_UNAVAILABLE
		* This is here because things on the main thread may attempt to change the radio state 
		* after the closed event happened in another thread 
		*/
		newState = RADIO_STATE_UNAVAILABLE;
	}

	if(RIL_DEBUG)ALOGD("oldState %d, sState %d, newState%d", oldState, sState, newState);
	if ((sState != newState) || (s_closed > 0)) 
	{
		sState = newState;
		if(RIL_DEBUG) ALOGD("set_radio_state: sState=%d", sState);
		pthread_cond_broadcast (&s_state_cond);
	}

	pthread_mutex_unlock(&s_state_mutex);

	if (sState != oldState)
	{             
		RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED, NULL, 0);

		//[Jerry] Waiting for unsol msg like "+CPIN:xxx", not using poll because it is MIPS consuming
		/* FIXME onRadioPowerOn() cannot be called
		* from the AT reader thread
		* Currently, this doesn't happen, but if that changes then these
		* will need to be dispatched on the request thread
		*/ 
	
        if ((sState == RADIO_STATE_SIM_NOT_READY)||(sState == RADIO_STATE_RUIM_NOT_READY)||(sState == RADIO_STATE_NV_NOT_READY)) {
			poll_sim_state(NULL);
		}else if((sState == RADIO_STATE_SIM_READY)||(sState == RADIO_STATE_RUIM_READY)||(sState == RADIO_STATE_NV_READY)) {
			on_sim_ready();
		}
	
	}  
}



/***************************************************
 Function:  on_current_state
 Description:  
    Return current radio state 
 Calls: 
    return global varialble sState
 Called By:
    
 Input:
    newState - radio state 
 Output:
    none
 Return:
    none
 Others:
    RADIO_STATE_UNAVAILABLE should be the initial state.
**************************************************/
static RIL_RadioState on_current_state()
{
    return sState;
}
/**
 * Call from RIL to us to find out whether a specific request code
 * is supported by this implementation.
 *
 * Return 1 for "supported" and 0 for "unsupported"
 */
/***************************************************
 Function:  on_supports
 Description:  
    Find out whether a specific request code is supported 
    by this implementation. 
 Calls: 
    return global varialble sState
 Called By:
    RIL_Init
 Input:
    newState - radio state 
 Output:
    none
 Return:
    none
 Others:
    RADIO_STATE_UNAVAILABLE should be the initial state.
**************************************************/
static int on_supports (int requestCode)
{
    if(RIL_DEBUG) ALOGD("on_supports is called, but not implemented yet\n");
    return 1;
}

static void on_cancel (RIL_Token token)
{
    if(RIL_DEBUG) ALOGD("on_cancel is called, but not implemented yet\n");
}

static const char * on_get_version(void)
{
    return "V100R005B003D15SP00C03";
}

static const char* moduleToString(Module_Type type)
{
	switch(type)
	{
		case NETWORK_WCDMA:return "WCDMA";
		case NETWORK_CDMA:return "CDMA";
		case NETWORK_TDSCDMA:return "TDSCDMA";
		default:return "UNKNOWN";
	}
}

static const char* cdmaSubTypeToString(CDMA_SubType type)
{
	switch(type)
	{
		case CDMA_RUIM_TYPE:return "RUIM";
		case CDMA_NV_TYPE:return "NV";
		default:return "UNKNOWN";
	}
}

/***************************************************
 Function:  get_phone_sim_relation
 Description:  
    get_phone_sim_relation by AT^SYSINFO
 Calls: 
    at_send_command_singleline
 Called By:
    get_sim_status
    get_network_type
 Input:
     void
 Output:
    void
 Return:
    none
 Others:
add by fKF34305 20101217 增加了CDMA 机卡合一模式
modify by fKF34305 20111011 在puk 锁之后判断机卡关系
**************************************************/
static int get_phone_sim_relation()
{
	if(RIL_DEBUG) ALOGD("----***********get_phone_sim_relation********---");
    ATResponse *p_response = NULL;
    int err,simstaus;
    char *line;
    int srv_status, srv_domain, raom_status, sys_mode, sim_state;
    
	if(RIL_DEBUG) ALOGD("----***********get_phone_sim_relation********---");
    err = at_send_command_singleline("AT^SYSINFO", "^SYSINFO:", &p_response);
    
    if (err < 0 || p_response->success == 0) { 	
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start (&line);    
    if (err < 0) {
        goto error;
    }

	err = at_tok_nextint(&line, &srv_status);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &srv_domain);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &raom_status);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &sys_mode);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &sim_state);
	if (err < 0) {
		goto error;
	}
	switch(sim_state)
	{
		case 240:
			g_cdma_subtype = CDMA_NV_TYPE;
			break;
    	case   1:
			g_cdma_subtype = CDMA_RUIM_TYPE;
			break;
		default:
			simstaus = get_sim_status();
			if((SIM_PIN == simstaus) || (SIM_PUK== simstaus))
				g_cdma_subtype = CDMA_RUIM_TYPE;
			else
				g_cdma_subtype = CDMA_UNKNOWN_TYPE;
			break;
	}
	
    if(RIL_DEBUG)ALOGD("cdma sub type is %s",cdmaSubTypeToString(g_cdma_subtype));
	at_response_free(p_response);
	p_response = NULL;
	return g_cdma_subtype;
	
error: 
	at_response_free(p_response);
	p_response = NULL;
	return g_cdma_subtype;
}


/***************************************************
 Function:  get_network_type
 Description:  
    get_network_type in
    AT^HS
 Calls: 
    at_send_command_singleline
 Called By:
    initialize_callback
 Input:
     void
 Output:
    void
 Return:
    none
 Others: this function is very important and it can not be fail
 Added by fKF34305 20101211  增加了CDMA制式
 Modified by lkf34826 20110514 增加TD制式
**************************************************/
static void get_network_type()
{
	ATResponse *p_response = NULL;       // DTS2011123101074 lkf52201 2011-12-31 added
	char *line;
	int err=0;
	int t_count=15;
	int hs_count=3;
	char *id;
	int protocol,is_offline,product_class = 0;

	if((g_device_index >= MAX_PID)||(g_device_index < 0))
		return;
	
	if((Devices[g_device_index].networktype == NETWORK_CDMA)
		&&((NULL != strstr(Devices[g_device_index].VID, "19d2")) 
			|| (NULL != strstr(Devices[g_device_index].VID, "05c6"))))
	{
		g_module_type = NETWORK_CDMA;
		do
		{
			get_phone_sim_relation();
			t_count--;
			if(CDMA_UNKNOWN_TYPE != g_cdma_subtype)
				break;
			sleep(2);
		}while(t_count);
	}
	else if ( (NULL != strstr(Devices[g_device_index].VID, "21f5")) 
				&& (Devices[g_device_index].networktype == NETWORK_TDSCDMA))
	{
		g_module_type = NETWORK_TDSCDMA;
	}
	else 
	{
		while(hs_count-- > 0)
		{
		    /* BEGIN DTS2011123101074 lkf52201 2011-12-31 added */
		    if (NULL != p_response)
		    {
		        at_response_free(p_response);
		        p_response = NULL;
		    }
		    /* END   DTS2011123101074 lkf52201 2011-12-31 added */
		    
			//AT^HS=<id>,<action>
			err = at_send_command_singleline("AT^HS=0,0", "^HS:", &p_response);
			if (err < 0 || p_response->success == 0) continue;

			line = p_response->p_intermediates->line;
			err = at_tok_start(&line);
			if (err < 0) continue;
			
		  	err = at_tok_nextstr(&line, &id);
		  	if (err < 0) continue;
		  	
		    err = at_tok_nextint(&line, &protocol);
		  	if (err < 0) continue;
		      
		  	err = at_tok_nextint(&line, &is_offline);
		  	if (err < 0) continue;
		      
		  	err = at_tok_nextint(&line, &product_class);
		  	if (err < 0) continue;
			//if(RIL_DEBUG)ALOGD("product_class=%d",product_class);
			break;
		}
		switch(product_class)
		{
			case 0:
			case 9:
			default:
				g_module_type = NETWORK_WCDMA;
				break;
			case 3:
				g_module_type = NETWORK_TDSCDMA;
				break;
			case 1:
			case 2:
				g_module_type = NETWORK_CDMA;
				//Begin to be modified by c00221986 for CDMA data card
				at_send_command("AT^RSSIREP=1", NULL);//开启CDMA信号主动上报
	      		//at_send_command("AT^RSSIREP=0", NULL);//关闭CDMA信号主动上报
	      		//End being modified by c00221986 for CDMA data card
		        do
		        {
					get_phone_sim_relation();
					t_count--;
					if(CDMA_UNKNOWN_TYPE != g_cdma_subtype)
						break;
					sleep(2);
		        }while(t_count);
				break;
		}
	}

	if(RIL_DEBUG)ALOGD("module type is %s",moduleToString(g_module_type));
	at_response_free(p_response);
	p_response = NULL;
	
}

/* Begin to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
/***************************************************
 Function:   get_provider_platform_info
 Description:  
    for supporting multy-platform,should get platform info
    AT^PLATFORM?
 Calls: 
    
 Called By:
     initialize_callback
 Input:
    none
 Output:
    none
 Return:
    none
 Others:
    none 
**************************************************/
static void get_provider_platform_info(void)
{

   ATResponse* p_response = NULL;
   int err = -1, provider = -1;
   char* line = NULL;

   err = at_send_command_singleline("AT^PLATFORM?","^PLATFORM:",&p_response);
   
   if(err < 0 || p_response->success == 0 )
   {
        goto error;
   }
   else
   {
        line = p_response->p_intermediates->line;
        err = at_tok_start(&line);
        if (err < 0) 
        {
             goto error;
        }

        err = at_tok_nextint(&line, &provider);

       if (err < 0)
       {
            goto error;
       } 
   }
   if(RIL_DEBUG)ALOGD("***********************provider is:%d********",provider);
   g_provider = (provider_info) provider;

   return ;
error:
   g_provider = QUALCOMM;//QUALCOMM default

   return;
}
/* End to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
/***************************************************
 Function:  initialize_callback
 Description:  
    Initialize everything that can be configured while we're still in
    AT+CFUN=0
 Calls: 
    
 Called By:
    main_loop
 Input:
    param - pointer to void
 Output:
    none
 Return:
    none
 Others:
    none 
**************************************************/
static void initialize_callback(void *param)
{
    set_radio_state (RADIO_STATE_OFF);
    
 	at_handshake();

    /* note: we don't check errors here. Everything important will
       be handled in onATTimeout and onATReaderClosed */

    /*  atchannel is tolerant of echo but it must */
    /*  have verbose result codes */
    // at_send_command("ATE0Q0V1", NULL);
    
    at_send_command("ATE0", NULL);

    if (g_modem_vendor == MODEM_VENDOR_ALCATEL){
    	at_send_command("ATE0Q0V1", NULL);
    }else
    {
    at_send_command("ATQ0V1", NULL);
  
    /*  No auto-answer */
    at_send_command("ATS0=0", NULL);
	  }
    /*  Extended errors */
    if (g_modem_vendor == MODEM_VENDOR_ALCATEL)
			at_send_command("AT+CMEE=2", NULL);//for alcatel
		else
    at_send_command("AT+CMEE=1", NULL);

    get_network_type(); 

    /* Begin to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
    get_provider_platform_info();	
    /* End to modify by hexiaokong kf39947 for DTS2012020100980 2012-02-27*/
    if(NETWORK_TDSCDMA == g_module_type)
    {
        /*  Switch  PCM voice */
		//at_send_command("AT^SWSPATH=2", NULL);//20110525  add by fKF34305
		at_send_command("AT*PSSIMSTAT=1", NULL);
    }
	
    if (is_radio_on() > 0) 
    {	
        if(NETWORK_CDMA == g_module_type)
	 {
		if(CDMA_NV_TYPE == g_cdma_subtype)
		{
			set_radio_state(RADIO_STATE_NV_NOT_READY);
		}
		else
		{
			set_radio_state(RADIO_STATE_RUIM_NOT_READY);
		}     
	}
	else
	{
		set_radio_state(RADIO_STATE_SIM_NOT_READY);
	}		
   }
	init_sigaction();//Added by x84000798 for ndis:网络驱动接口规范
	 //add by hexiaokong kf39947 for switching voice mode to PCM begin

   // switch_voice_mode_to_pcm( );
    
     //add by hexiaokong kf39947 for switching voice mode to PCM end
}

#if 0
//add by hexiaokong kf39947 for switching voice mode to PCM   begin
void switch_voice_mode_to_pcm( void )
{
    ATResponse *p_response = NULL;
    int response = 0;
    char *line;
    int err = 0;
	
    err = at_send_command_singleline("AT^SWSPATH?", "^SWSPATH:", &p_response);
    if (err < 0 || p_response->success == 0)
    {
        goto error;
    }
    
    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0 )
    {
        goto error;
    }

    err = at_tok_nextint(&line, &response);
    if (err < 0 )
    {
        goto error;
    }

    if (response != 2)
    {
   		err = at_send_command("AT^SWSPATH=2",NULL);
        if (err < 0 )
        {
             goto error;
        }
        if(RIL_DEBUG)ALOGD("Succed to switch voice mode to PCM");
    } 
    at_response_free(p_response);
    return;
error:
    ALOGD("Fail to switch voice mode to PCM");  
    at_response_free(p_response);
    return;    
}       

//add by hexiaokong kf39947 for switching voice mode to PCM   end
#endif

 
static void wait_for_close()
{
    pthread_mutex_lock(&s_state_mutex);

    while (s_closed == 0) {
        pthread_cond_wait(&s_state_cond, &s_state_mutex);
    }

    pthread_mutex_unlock(&s_state_mutex);
}

/* Called on command or reader thread */
static void on_at_reader_closed()
{
    if(RIL_DEBUG) ALOGD( "AT channel closed\n");
    at_close();
    s_closed = 1;

    set_radio_state (RADIO_STATE_UNAVAILABLE);
}

/* Called on command thread */
void on_at_timeout()
{
    if(RIL_DEBUG) ALOGD( "AT channel timeout; closing\n");
    at_close();
    s_closed = 1;

    /* FIXME cause a radio reset here */
   
    deconfigure_interface();
    set_radio_state (RADIO_STATE_UNAVAILABLE);
    //tell the cpmd to restart the modem
}
static void usage(char *s)
{
    fprintf(stderr, "huawei-ril requires: -p <tcp port> or -d /dev/tty_device\n");//Huawei ril?
}



/***************************************************
 Function:  getportonpid
 Description:  
    Obtain pcui port by usb device pid 
 Calls: 
 Called By:
    RIL_Init
 Input:
    none
 Output:
    none
 Return:
    none
 Others:
    add by lizheng lkf33986 2010-11-13
    modify by fKF34305 20111111
    modify by lkf52201 2011-12-31
**************************************************/
static int getportonpid()
{
    	int ret = 0;
	DIR    *pDir1; // /sys/bus/usb/drivers/option目录下的目录流
	DIR    *pDir2; // /sys/bus/usb/drivers/option目录下的目录流
	DIR    *pDir_acm;// /sys/bus/usb/drivers/option/x-x:x.x 目录下的目录流
	DIR    *pDir_usb;// /sys/bus/usb/drivers/option/x-x:x.x/tty 目录下的目录流
	char dirdir[1024] = ""; //打开目录流的路径
	char * pPID;

	FILE   *pfmodalias;// 打开/sys/bus/usb/drivers/option/%s/modalias 文件指针
	FILE   *pfbInterfaceNumber;// 打开 /sys/bus/usb/drivers/option/%s/bInterfaceNu 的文件指针
	FILE   *pfbInterfaceProtocol;// 打开 /sys/bus/usb/drivers/option/%s/bInterfaceProtocol 的文件指针
	

	char cdirmodalias[1024] = ""; //打开文件的路径
	char cdirbInterfaceNumber[1024] = "" ;
	char cdirbInterfaceProtocol[1024] = "" ;

	char cmodalias[1024] = ""; //保存modalias中的数据
	char cbInterfaceNumber[1024] = "";//保存bInterfaceNumber中的数据
	char cbInterfaceProtocol[1024] = "";//保存bInterfaceNumber中的数据

	char dirtty[1024] = ""; //打开目录流的路径
	 int i;
	
	//ALOGD("max_pid=%d",MAX_PID);
	struct dirent *pNext1 = NULL;
	struct dirent *pNext2 = NULL;
	struct dirent *pNext_acm = NULL;
	struct dirent *pNext_usb = NULL;

	PID_Project p_project_status = PID_UNKNOWN;

	//变量的初始化
	memset(dirdir,'\0', sizeof(dirdir));
	memset(cdirmodalias,'\0', sizeof(cdirmodalias));
	memset(cdirbInterfaceNumber,'\0', sizeof(cdirbInterfaceNumber));
	memset(cdirbInterfaceProtocol,'\0', sizeof(cdirbInterfaceProtocol));
	memset(cmodalias,'\0', sizeof(cmodalias));
	memset(cbInterfaceNumber,'\0', sizeof(cbInterfaceNumber));
	memset(cbInterfaceProtocol,'\0', sizeof(cbInterfaceProtocol));
	memset(dirtty,'\0', sizeof(dirtty));
	g_device_index = 0xffff;
	MAX_PID = sizeof(Devices)/sizeof(Devices[0]);
	
	pDir_usb= opendir("/sys/bus/usb/drivers/option");

	if (NULL != pDir_usb){
	//首先判断设备是否为USB类型
	while(NULL != (pNext_usb= readdir(pDir_usb)))
	{
		sprintf(dirdir,"/sys/bus/usb/drivers/option/%s",pNext_usb->d_name);  // 读取/sys/bus/usb/drivers/option/x-x:x.x 目录流
		sprintf(cdirmodalias, "/sys/bus/usb/drivers/option/%s/modalias", pNext_usb->d_name);  //文件modalias路径
		sprintf(cdirbInterfaceProtocol, "/sys/bus/usb/drivers/option/%s/bInterfaceProtocol", pNext_usb->d_name);  //文件modalias路径
		sprintf(cdirbInterfaceNumber, "/sys/bus/usb/drivers/option/%s/bInterfaceNumber", pNext_usb->d_name);  //文件modalias路径

		if(NULL == strstr(pNext_usb->d_name,":"))
			continue;
		/* Begin modified by x84000798 for PID solution for DTS2012112004053 2012-11-21*/
		//先去读pfbInterfaceProtocol 的值判断模块采用的PID方案。若是新PID方案，不用查PID表
					//如果已经判断为华为设备，则先去读pfbInterfaceProtocol 的值。
					if(NULL != (pfbInterfaceProtocol = fopen(cdirbInterfaceProtocol,"r")))
					{
						fgets(cbInterfaceProtocol,1024,pfbInterfaceProtocol);
						if(NULL != strstr(cbInterfaceProtocol,"ff"))      
							{
								p_project_status = PID_OLD;
							}
							else
							{
								p_project_status = PID_NEW;
							}
						fclose(pfbInterfaceProtocol); // DTS2011123100128 lkf52201 2011-12-31 added
					}
		if (PID_NEW == p_project_status)
		{
			pDir1 = opendir(dirdir);
			while(NULL != (pNext1 = readdir(pDir1)))
			{								
				if(NULL != strstr(pNext1->d_name,"ttyUSB"))
				{
					if(strstr(cbInterfaceProtocol,"01") || strstr(cbInterfaceProtocol,"10")) //modified by c00221986 for xi an
					{
						sprintf(SERIAL_PORT_MODEM_KEYNAME,"/dev/%s",pNext1->d_name);
						ALOGD("modem port is %s\n",SERIAL_PORT_MODEM_KEYNAME);
						ret |= 1 << 0 ;
					}
					else if(strstr(cbInterfaceProtocol,"02") || strstr(cbInterfaceProtocol,"12")) //modified by c00221986 for xi an
					{
						sprintf(SERIAL_PORT_PCUI_KEYNAME,"/dev/%s",pNext1->d_name);
						ALOGD("pcui port is %s\n",SERIAL_PORT_PCUI_KEYNAME);
						ret |= 1 << 1 ;//modify by KF34305 20111206 for DTS2011120604273
					}
					else if(strstr(cbInterfaceProtocol,"03") || strstr(cbInterfaceProtocol,"13")) //modified by c00221986 for xi an
					{
						sprintf(SERIAL_PORT_DIAG_KEYNAME,"/dev/%s",pNext1->d_name);
						ALOGD("diag port is %s\n",SERIAL_PORT_DIAG_KEYNAME);
						ret |= 1 << 2 ;//modify by KF34305 20111206 for DTS2011120604273
					}
					else
					{
						ALOGD("----The port is not recognized----");
					}
				}
			}
			if (NULL != pDir1)
			{
			    closedir(pDir1);
			}			
		}
		//Old PID Solution
		else
		{
			if(NULL != (pfmodalias = fopen(cdirmodalias,"r")))
			{
				fgets(cmodalias,1024,pfmodalias);
				pPID = malloc(6);
				for(i = 0; i < MAX_PID; i++)
				{
					memset(pPID, '\0', 6);
					pPID[0] = 'p';
					strncpy((pPID+1), Devices[i].PID, 4);
					//if(NULL != strstr(cmodalias,Devices[i].PID))
					if(NULL != strstr(cmodalias, pPID))  	 
					{
						//by alic
						g_device_index = i;
						//如果已经判断为华为设备，则先去读pfbInterfaceProtocol 的值。这里这段没有必要的。属于冗余代码
						if(NULL != (pfbInterfaceProtocol = fopen(cdirbInterfaceProtocol,"r")))
						{
							fgets(cbInterfaceProtocol,1024,pfbInterfaceProtocol);
							if(NULL != strstr(cbInterfaceProtocol,"ff"))      
								{
									p_project_status = PID_OLD;
								}
								else
								{
									p_project_status = PID_NEW;
								}
							fclose(pfbInterfaceProtocol); // DTS2011123100128 lkf52201 2011-12-31 added
						}
					if(NULL != (pfbInterfaceNumber = fopen(cdirbInterfaceNumber,"r")))
					{
						fgets(cbInterfaceNumber,1024,pfbInterfaceNumber);
						pDir1 = opendir(dirdir);
						while(NULL != (pNext1 = readdir(pDir1)))
						{
							if(RIL_DEBUG)ALOGD("DIR=%s,d_name=%s,modem=%s,pcui=%s,diag=%s,%d\n",pNext_usb->d_name,pNext1->d_name, Devices[i].modem.number, Devices[i].pcui.number,Devices[i].diag.number,p_project_status);
							if(NULL != strstr(pNext1->d_name,"ttyUSB"))
							{
								if(PID_OLD == p_project_status)     
								{
									if(NULL != strstr(cbInterfaceNumber,Devices[i].modem.number)) 
									{
										sprintf(SERIAL_PORT_MODEM_KEYNAME,"/dev/%s",pNext1->d_name);
										if(RIL_DEBUG)ALOGD("modem port is %s\n",SERIAL_PORT_MODEM_KEYNAME);
										ret |= 1 << 0 ;//modify by kf33986 DTS2010102503678 解决当USB总线重启后，tyyUSB设备没有完全建立就查找端口，导致端口无法打开
									}
									else if(NULL != strstr(cbInterfaceNumber,Devices[i].pcui.number))
									{
										sprintf(SERIAL_PORT_PCUI_KEYNAME,"/dev/%s",pNext1->d_name);
										if(RIL_DEBUG)ALOGD("pcui port is %s\n",SERIAL_PORT_PCUI_KEYNAME);
										ret |= 1 << 1 ;//modify by kf33986 DTS2010102503678 解决当USB总线重启后，tyyUSB设备没有完全建立就查找端口，导致端口无法打开
									} 
									else if(NULL != strstr(cbInterfaceNumber,Devices[i].diag.number))
									{
										sprintf(SERIAL_PORT_DIAG_KEYNAME,"/dev/%s",pNext1->d_name);
										if(RIL_DEBUG)ALOGD("diag port is %s\n",SERIAL_PORT_DIAG_KEYNAME);
										ret |= 1 << 2 ;//modify by kf33986 DTS2010102503678 解决当USB总线重启后，tyyUSB设备没有完全建立就查找端口，导致端口无法打开
									}
									
								}
								else
								{
									if(strstr(cbInterfaceProtocol,"01"))
									{
										sprintf(SERIAL_PORT_MODEM_KEYNAME,"/dev/%s",pNext1->d_name);
										if(RIL_DEBUG)ALOGD("3 modem port is %s\n",SERIAL_PORT_MODEM_KEYNAME);
										ret |= 1 << 0 ;
									}
									else if(strstr(cbInterfaceProtocol,"02"))
									{
										sprintf(SERIAL_PORT_PCUI_KEYNAME,"/dev/%s",pNext1->d_name);
										if(RIL_DEBUG)ALOGD("3 pcui port is %s\n",SERIAL_PORT_PCUI_KEYNAME);
										ret |= 1 << 1 ;//modify by KF34305 20111206 for DTS2011120604273
									}
									else if(strstr(cbInterfaceProtocol,"03"))
									{
										sprintf(SERIAL_PORT_DIAG_KEYNAME,"/dev/%s",pNext1->d_name);
										if(RIL_DEBUG)ALOGD("3 diag port is %s\n",SERIAL_PORT_DIAG_KEYNAME);
										ret |= 1 << 2 ;//modify by KF34305 20111206 for DTS2011120604273
									}
									else
									{
										ALOGD("----The port is not recognized----");
									}
								}
							}
						}
			/* BEGIN DTS2011123100128  lkf52201 2011-12-31 added */
						if (NULL != pDir1)
						{
						    closedir(pDir1);
						}
						fclose(pfbInterfaceNumber); 
					}
				}				
			}
				free(pPID);
				pPID = NULL;
			fclose(pfmodalias); 
			/* END DTS2011123100128  lkf52201 2011-12-31 added */
		}		
	}
	}
    /* BEGIN DTS2011123100128  lkf52201 2011-12-31 modified */
	if (NULL != pDir_usb)
	{
	    closedir(pDir_usb);
	}
	/* END DTS2011123100128  lkf52201 2011-12-31 modified */
	if(ret)
	{
		return ret;
	}
	}
	//其次判断设备是否为ACM类型
	pDir_acm = opendir("/sys/bus/usb/drivers/cdc_acm");
	if(NULL == pDir_acm)
		return 0;
	while(NULL != (pNext_acm= readdir(pDir_acm)))
	{
		sprintf(dirdir,"/sys/bus/usb/drivers/cdc_acm/%s",pNext_acm->d_name);  // 读取/sys/bus/usb/drivers/option/x-x:x.x 目录流
		sprintf(cdirmodalias, "/sys/bus/usb/drivers/cdc_acm/%s/modalias", pNext_acm->d_name);  //文件modalias路径
		sprintf(cdirbInterfaceNumber, "/sys/bus/usb/drivers/cdc_acm/%s/bInterfaceNumber", pNext_acm->d_name); //文见bInterfaceNumber路径
        sprintf(dirtty,"/sys/bus/usb/drivers/cdc_acm/%s/tty",pNext_acm->d_name);

		if(NULL == strstr(pNext_acm->d_name,":"))
			continue;
		if(NULL != (pfmodalias = fopen(cdirmodalias,"r")))
		{
			fgets(cmodalias,1024,pfmodalias);
			pPID = malloc(6);
			for(i = 0; i < MAX_PID; i++)
			{
				memset(pPID, '\0', 6);
				pPID[0] = 'p';
				strncpy((pPID+1), Devices[i].PID, 4);
				//if(NULL != strstr(cmodalias,Devices[i].PID)) //查询PID
				if(NULL != strstr(cmodalias, pPID))							
				{
					g_device_index = i;
					if(NULL != (pfbInterfaceNumber = fopen(cdirbInterfaceNumber,"r")))
					{
						fgets(cbInterfaceNumber,1024,pfbInterfaceNumber);
						pDir1 = opendir(dirdir);

						while(NULL != (pNext1 = readdir(pDir1)))
						{   
							if(NULL != strstr(pNext1->d_name,"tty"))
							{   							
							    pDir2 = opendir(dirtty);
								while(NULL != (pNext2 = readdir(pDir2)))
								{										
									if(NULL != strstr(pNext2->d_name,"ttyACM"))
                                   				{ 
										if(NULL != strstr(cbInterfaceNumber,Devices[i].modem.number)) 
										{
											sprintf(SERIAL_PORT_MODEM_KEYNAME,"/dev/%s",pNext2->d_name);
											if(RIL_DEBUG)ALOGD("2 modem port is %s\n",SERIAL_PORT_MODEM_KEYNAME);
											ret |= 1 << 0 ;//modify by kf33986 DTS2010102503678 解决当USB总线重启后，tyyUSB设备没有完全建立就查找端口，导致端口无法打开
										}
										else if(NULL != strstr(cbInterfaceNumber,Devices[i].diag.number))
										{
											sprintf(SERIAL_PORT_DIAG_KEYNAME,"/dev/%s",pNext2->d_name);
											if(RIL_DEBUG)ALOGD("2 diag port is %s\n",SERIAL_PORT_DIAG_KEYNAME);
											ret |= 1 << 1 ;//modify by kf33986 DTS2010102503678 解决当USB总线重启后，tyyUSB设备没有完全建立就查找端口，导致端口无法打开
										}
										else if(NULL != strstr(cbInterfaceNumber,Devices[i].pcui.number))
										{
											sprintf(SERIAL_PORT_PCUI_KEYNAME,"/dev/%s",pNext2->d_name);
											if(RIL_DEBUG)ALOGD("2 pcui port is %s\n",SERIAL_PORT_PCUI_KEYNAME);
											ret |= 1 << 2 ;//modify by kf33986 DTS2010102503678 解决当USB总线重启后，tyyUSB设备没有完全建立就查找端口，导致端口无法打开
										}
									}
								}
			/* BEGIN DTS2011123100128  lkf52201 2011-12-31 added */
								if (NULL != pDir2)
								{
								    closedir(pDir2);
								}
							}	
						}
						if (NULL != pDir1)
						{
						    closedir(pDir1);
						}
						fclose(pfbInterfaceNumber);
					}
				}				
			}
			free(pPID);
			pPID = NULL;
			fclose(pfmodalias);
			/* END DTS2011123100128  lkf52201 2011-12-31 added */
		}		
	}

	/* BEGIN DTS2011123100128  lkf52201 2011-12-31 modified */
	if (NULL != pDir_acm)
	{
	    closedir(pDir_acm);
	}
	/* END DTS2011123100128  lkf52201 2011-12-31 modified */
	return ret;
}

/***************************************************
 Function:  getecmport
 Description:  
    Obtain ecm port
 Calls: 
 Called By:
    RIL_Init
 Input:
    none
 Output:
    none
 Return:
    none
 Others:
    add by c00221986 2013-02-17
**************************************************/
static HWRIL_NDIS_MODEM getecmport()
{
	DIR    *pDir_usb;                         // /sys/class/net 目录下的目录流
	FILE   *pfmodalias;                      // 打开/sys/class/net/%s/modalias 文件指针
	char    cdirmodalias[1024] = "";        //打开文件的路径
	char    cmodalias[1024] = "";          //保存modalias中的数据
	HWRIL_NDIS_MODEM     ret = MODEM_PROCESS;
	memset(cdirmodalias,'\0', sizeof(cdirmodalias));
	memset(cmodalias,'\0', sizeof(cmodalias));
	struct dirent *pNext_usb = NULL;
	pDir_usb = opendir("/sys/class/net");
	while(NULL != (pNext_usb= readdir(pDir_usb)))
	{
		sprintf(cdirmodalias, "/sys/class/net/%s/device/modalias", pNext_usb->d_name);  //文件modalias路径
		if(NULL != (pfmodalias = fopen(cdirmodalias,"r")))
		{
			fgets(cmodalias,1024,pfmodalias);
			if(NULL != strstr(cmodalias,"v12D1"))
			{
				sprintf(PORT_ECM_KEYNAME,"%s",pNext_usb->d_name);
				ALOGD("ECM port is %s\n",PORT_ECM_KEYNAME);
				ret = NDIS_PROCESS;
			}
			fclose(pfmodalias);
		}		 
	}
	if (NULL != pDir_usb)
	{
	    closedir(pDir_usb);
	}
	return ret;
}

/*Begin modified by x84000798 for DTS2012073007745 2012-8-11l*/
static void * send_atcmd_when_necessary_for_reader_thread(void  *arg)
{
    int err = -1;
    while(1)
    {
        pthread_mutex_lock(&s_cmd_mutex);
        ALOGD("--->>>here wait for signal from reader thread by unsolicited cmd report !");
        pthread_cond_wait(&s_cmd_cond, &s_cmd_mutex);
		if (1 == g_csd)
    	{
    		ALOGD("--->>>RIL don't realize CSD call, send ATH !!!");
    		at_send_command("ATH", NULL);
			g_csd = 0;
			pthread_mutex_unlock(&s_cmd_mutex);
			continue;
		}
		if (1 == g_pspackage)
		{
	        err = dial_at_modem("ATA"); 
			if (AT_ERROR_TIMEOUT == err)
			{
				ALOGE("--->>>Read CONNECT from modem port TIMEOUT!!!");
			}
			else
			{
				ALOGD("--->>>go to setup data connection after receiving the signal--");
				err = enable_ppp_interface((void *)0, 1, "", "", ""); // if module implement ps incoming packet call, RIL just needs to call this interface.
				//setup_ppp_connection((void*)0, "1234", "", "", "IP");
	    	}
			pthread_mutex_unlock(&s_cmd_mutex);
			continue;
		}
    }
	return (void *)0;
}
/*End modified by x84000798 for DTS2012073007745 2012-8-11*/


/***************************************************
 Function:  main_loop
 Description:  
    Open client,init fd attribute
 Calls: 
    socket_loopback_client
    socket_local_client
    at_set_on_reader_closed
 Called By:
    RIL_Init
 Input:
    param - pointer to void
 Output:
    none
 Return:
    none
 Others:
    none 
**************************************************/
static void *
main_loop(void *param)
{
    int fd;
    int ret;
	int tmp;

    at_set_on_reader_closed(on_at_reader_closed);
    at_set_on_timeout(on_at_timeout);

    for (;;) {
        fd = -1;
        while  (fd < 0) {
		//Begin to be added by c00221986 for ndis
			ALOGD("_____Enum ECM ports..._____");
			if(NDIS_PROCESS == getecmport()){
				hw_ndis_or_modem = NDIS_PROCESS;
				ALOGD("_____Open ECM ports success(NDIS)_____");
			}else{
				ALOGD("_____Opening MODEM ports(MODEM)..._____");
			}
		//End being added by c00221986 for ndis

		/*BEGIN Added by fKF34305 based on z128629 2010-11-13 Obtain pcui port by usb device pid ,we will use parameter passed from rild
		  if we get pid fail,In order to solve the module caused rild restart when insert or out*/
		  
		if(RIL_DEBUG)ALOGD("_____Enum ports._____");
		if(getportonpid() & 0x05)//check out the PCUI port  //modify by kf33986 DTS2010102503678 解决当USB总线重启后，tyyUSB设备没有完全建立就查找端口，导致端口无法打开
		{
			s_device_path = SERIAL_PORT_PCUI_KEYNAME;

			fd = open (s_device_path, O_RDWR);
			if(fd >= 0) 
			{
				ALOGD("____Open success____");
				break;
			}
			else
				{
					ALOGD("____Open %s error:%d", s_device_path, fd);
				}
		}
		else
			{
				ALOGD(" getportonpid error.");
			}
		sleep(1);

		/*END Added by fKF34305 based on z128629 2010-11-13 Obtain pcui port by usb device pid*/
		
        }
		
		s_MaxError = 0;
		g_IsDialSucess = 0;
		
        struct termios tios;
        if (tcgetattr(fd, &tios) < 0) {
            if (!ok_error(errno))
                 ALOGD("tcgetattr: (line %d)",__LINE__);
            return (void *)-1;
	 	}

        tios.c_cflag     &= ~(CSIZE | CSTOPB | PARENB | CLOCAL);
        tios.c_cflag     |= CS8 | CREAD | HUPCL;
        tios.c_iflag      = IGNBRK | IGNPAR;
        tios.c_oflag      = 0;
        tios.c_lflag      = 0;
        tios.c_cc[VMIN]   = 1;
        tios.c_cc[VTIME]  = 0;
        tios.c_cflag ^= (CLOCAL | HUPCL);
  
	 if(strstr(s_device_path,"ACM") != NULL)
	    tmp=tcsetattr(fd, TCSANOW, &tios);
     else
	 	tmp=tcsetattr(fd, TCSAFLUSH, &tios);
	 
	 if(RIL_DEBUG)ALOGD("tcsetattr tmp=%d,error is %s",tmp,strerror(errno));
	 
	 while (tmp < 0 && !ok_error(errno))
        if (errno != EINTR)
            ALOGD("tcsetattr: (line %d)",__LINE__);
            tcflush(fd, TCIOFLUSH);	
		
        s_closed = 0;
        ret = at_open(fd, on_unsolicited);
        if (ret < 0) {
            if(RIL_DEBUG) ALOGE( "AT error %d on at_open\n", ret);
            return 0;
        }
		  
	if((g_device_index >= MAX_PID)||(g_device_index < 0))
		return 0;

	if(NULL != strstr(Devices[g_device_index].VID, "12d1"))
	{
		g_modem_vendor = MODEM_VENDOR_HAUWEI;
	}
	else if(NULL != strstr(Devices[g_device_index].VID, "19d2"))
	{
		g_modem_vendor = MODEM_VENDOR_ZTE;
	}
	else if ((NULL != strstr(Devices[g_device_index].VID, "05c6"))
		|| (NULL != strstr(Devices[g_device_index].VID, "21f5"))
		|| (NULL != strstr(Devices[g_device_index].VID, "20a6")))
	{
		g_modem_vendor = MODEM_VENDOR_SCV;
	}else if (NULL != strstr(Devices[g_device_index].VID, "1bbb"))
	{
		g_modem_vendor = MODEM_VENDOR_ALCATEL;
	}
	else
	{
		g_modem_vendor = MODEM_VENDOR_UNKNOWN;
	}	
		
	sleep(2);
        	
        RIL_requestTimedCallback(initialize_callback, NULL, &TIMEVAL_IMMEDIATE);
        // Give initializeCallback a chance to dispatched, since
        // we don't presently have a cancellation mechanism
        sleep(1);

        wait_for_close();
        if(RIL_DEBUG) ALOGD( "Re-opening after close");
	 	//exit(1);  //modify by fKF34305 2011-7-28 inorder to prevent rild restart when EOF

    }
}





pthread_t s_tid_mainloop;

/*************************************************
 Function:  RIL_Init
 Description:  
    reference ril.h
 Calls: 
    pthread_attr_init
    pthread_attr_setdetachstate
 Called By:
    rild.c main
 Input:
    env  - environment point defined as RIL_Env
    argc - number of arguments
    argv - list fo arguments
 Output:
    none
 Return:
    s_callbacks
 Others:
    none
**************************************************/
const RIL_RadioFunctions *RIL_Init(const struct RIL_Env *env, int argc, char **argv)
{
    int opt;
    pthread_attr_t attr;

    s_rilenv = env;

    /* [Jerry] the arg "-d /dev/ttyS0" defined in /<nfsroot>/system/build.prop is not used now. 
     *            dev name is defined in descriptions[i].ttyName, and opened in mainLoop()
     */
    s_device_path = "/dev/ttyUSB2" ;
    while ( -1 != (opt = getopt(argc, argv, "p:d:s:u:"))) 
    {
        switch (opt) {
            case 'p':
                s_port = atoi(optarg);
                if (s_port == 0) {
                    usage(argv[0]);
                    return NULL;
                }
                if(RIL_DEBUG) ALOGD("Opening loopback port %d\n", s_port);
                break;

            case 'd':
                s_device_path = optarg;
                break;

            case 'u':
            	s_modem_path = optarg;
            	break;
            	
            case 's':
                s_device_path   = optarg;
                s_device_socket = 1;
                break;

            default:
                usage(argv[0]);
                break; //change return to break
        }
    }
 
		/*BEGIN Added by z128629 2010-11-13 Obtain pcui port by usb device pid ,we will use parameter passed from rild
		  if we get pid fail*/
 //if(RIL_DEBUG) ALOGD("Opening tty device %s\n", s_device_path);
 //if(RIL_DEBUG) ALOGD("Opening tty device %s\n", s_device_path);
 /*BEGIN Added by qkf33988 2010-11-13 CMTI issue*/
#ifdef   CMTI_ISSUE	
	int ret;
	pthread_attr_t attr_read_sms;	 
	pthread_attr_init (&attr_read_sms);
	pthread_attr_setdetachstate(&attr_read_sms, PTHREAD_CREATE_DETACHED);

	pthread_mutex_init(& s_readdelmutex,NULL);
	pthread_mutex_init(& s_hwril_readsms_mutex,NULL);

	if ( socketpair( AF_LOCAL, SOCK_STREAM, 0, del_sms_control ) < 0 ) {
		ALOGE("could not create thread control socket pair: %s", strerror(errno));
		return (void*)-1;
	} 
	ret = pthread_create(&s_tid_read_sms, &attr_read_sms, hwril_read_sms_then_delete_sms, &attr_read_sms);
	if (ret < 0) {
		perror ("pthread_create");
		return (void*)-1;
	}
         
#endif
/*END Added by qkf33988 2010-11-13 CMTI issue */
/*Begin to add by h81003427 20120526 for ps domain incoming packet call*/
    pthread_attr_t attr_cmd_for_reader_thread;
    pthread_attr_init (&attr_cmd_for_reader_thread);
    pthread_attr_setdetachstate(&attr_cmd_for_reader_thread, PTHREAD_CREATE_DETACHED);
    ALOGD("--->>>new a pthread here<<<---");
    pthread_create(&s_cmd_for_reader_thread, &attr_cmd_for_reader_thread, send_atcmd_when_necessary_for_reader_thread, NULL);
/*End to add by h81003427 20120526 for ps domain incoming packet call*/
  
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&s_tid_mainloop, &attr, main_loop, NULL);
    //ALOGD("main_loop pthread_create  %d",ret);

    return &s_callbacks;
}

static ServiceType judge_service_on_request(int request)
{
    ServiceType service = SERVICE_TOTAL;
    
    switch(request) {
        case RIL_REQUEST_GET_SIM_STATUS:
        case RIL_REQUEST_SIM_IO:            
        case RIL_REQUEST_ENTER_SIM_PIN:
        case RIL_REQUEST_ENTER_SIM_PUK:
        case RIL_REQUEST_ENTER_SIM_PIN2:
        case RIL_REQUEST_ENTER_SIM_PUK2:            
        case RIL_REQUEST_CHANGE_SIM_PIN:
        case RIL_REQUEST_CHANGE_SIM_PIN2:
        case RIL_REQUEST_ENTER_NETWORK_DEPERSONALIZATION: 
        case RIL_REQUEST_GET_IMSI:
        case RIL_REQUEST_STK_GET_PROFILE:
        case RIL_REQUEST_STK_SET_PROFILE:
        case RIL_REQUEST_STK_SEND_ENVELOPE_COMMAND:
        case RIL_REQUEST_STK_SEND_TERMINAL_RESPONSE:
        case RIL_REQUEST_STK_HANDLE_CALL_SETUP_REQUESTED_FROM_SIM:
	 case RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING:
            service = SERVICE_SIM;
            break;
                
        case RIL_REQUEST_SIGNAL_STRENGTH:

//begin modified by wkf32792 for android 3.x 20110815      
#ifdef ANDROID_3
        case RIL_REQUEST_VOICE_REGISTRATION_STATE:         
        case RIL_REQUEST_DATA_REGISTRATION_STATE: 
#else
        case RIL_REQUEST_REGISTRATION_STATE:  
        case RIL_REQUEST_GPRS_REGISTRATION_STATE:
#endif
//end modified by wkf32792 for android 3.x 20110815       
        case RIL_REQUEST_OPERATOR:
        case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
        case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
        case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
        case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
        case RIL_REQUEST_SCREEN_STATE:
        case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
        case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
        case RIL_REQUEST_GET_NEIGHBORING_CELL_IDS:
        case RIL_REQUEST_SET_LOCATION_UPDATES:        
            service = SERVICE_MM;
            break;

        case RIL_REQUEST_GET_CURRENT_CALLS:
        case RIL_REQUEST_DIAL:
        case RIL_REQUEST_HANGUP:
        case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
        case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
        case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE:
        case RIL_REQUEST_CONFERENCE:
        case RIL_REQUEST_UDUB:
        case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
        case RIL_REQUEST_DTMF: 
        case RIL_REQUEST_ANSWER: 
        case RIL_REQUEST_DTMF_START:
        case RIL_REQUEST_DTMF_STOP:
        case RIL_REQUEST_SEPARATE_CONNECTION:
        case RIL_REQUEST_EXPLICIT_CALL_TRANSFER:
        case RIL_REQUEST_SET_MUTE:
        case RIL_REQUEST_GET_MUTE:
		case RIL_REQUEST_SET_CALL_WAITING:
        case RIL_REQUEST_QUERY_CALL_WAITING:
        case RIL_REQUEST_SET_CALL_FORWARD:
        case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS:
        case RIL_REQUEST_GET_CLIR:
        case RIL_REQUEST_SET_CLIR:
		case RIL_REQUEST_QUERY_CLIP:
	     case RIL_REQUEST_CDMA_BURST_DTMF:
	    case RIL_REQUEST_CDMA_FLASH:
            service = SERVICE_CC;
            break;

        case RIL_REQUEST_RADIO_POWER:
        case RIL_REQUEST_GET_IMEI:
        case RIL_REQUEST_GET_IMEISV:
        case RIL_REQUEST_BASEBAND_VERSION:
        case RIL_REQUEST_RESET_RADIO:
        case RIL_REQUEST_OEM_HOOK_RAW:
        case RIL_REQUEST_OEM_HOOK_STRINGS:
        case RIL_REQUEST_SET_BAND_MODE:
        case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:
        case RIL_REQUEST_DEVICE_IDENTITY:
        case RIL_REQUEST_CDMA_SUBSCRIPTION:
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3	     
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE:
#else	
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION:
#endif
//end modified by wkf32792 for android 3.x 20110815
        case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:
		case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:
		case RIL_REQUEST_QUERY_FACILITY_LOCK:
        case RIL_REQUEST_SET_FACILITY_LOCK:
            service = SERVICE_DEV;
            break;

        case RIL_REQUEST_SEND_SMS:
        case RIL_REQUEST_SEND_SMS_EXPECT_MORE:
        case RIL_REQUEST_SMS_ACKNOWLEDGE:
        case RIL_REQUEST_WRITE_SMS_TO_SIM:
        case RIL_REQUEST_DELETE_SMS_ON_SIM:
		case RIL_REQUEST_GET_SMSC_ADDRESS:
		case RIL_REQUEST_SET_SMSC_ADDRESS:
        case RIL_REQUEST_CDMA_SEND_SMS:// add by kf 33986 2011-4-11 cdma sms
        case RIL_REQUEST_CDMA_SMS_ACKNOWLEDGE:// add by kf 33986 2011-4-11 cdma sms
        case RIL_REQUEST_CDMA_DELETE_SMS_ON_RUIM:// add by kf 33986 2011-4-11 cdma sms
        case RIL_REQUEST_CDMA_WRITE_SMS_TO_RUIM:// add by kf 33986 2011-4-11 cdma sms
	 case RIL_REQUEST_GSM_SMS_BROADCAST_ACTIVATION:  // add by kf52201 2011.10.11
		case RIL_REQUEST_GSM_GET_BROADCAST_SMS_CONFIG: //add by x84000798 for CBS
	 	case RIL_REQUEST_GSM_SET_BROADCAST_SMS_CONFIG: //add by x84000798 for CBS
            service = SERVICE_SMS;
            break;

        case RIL_REQUEST_SETUP_DATA_CALL:
        case RIL_REQUEST_DEACTIVATE_DATA_CALL:
    	case RIL_REQUEST_LAST_DATA_CALL_FAIL_CAUSE:
    	case RIL_REQUEST_DATA_CALL_LIST:
            service = SERVICE_PS;
            break;

        case RIL_REQUEST_SEND_USSD:
        case RIL_REQUEST_CANCEL_USSD:
        
        case RIL_REQUEST_CHANGE_BARRING_PASSWORD:
        case RIL_REQUEST_SET_SUPP_SVC_NOTIFICATION:
            service = SERVICE_SS;
            break;
        default:
	     if(RIL_DEBUG)ALOGD("Not found the request : %s in Function: %s", requestToString(request),__FUNCTION__);
            //if(RIL_DEBUG) ALOGD("invalid request:%d\n",request);
    }

    return service;
}

static ServiceType judge_service_on_unsolicited(const char *s)
{
    int i, total, service;
    struct { 
        char *prefix;
        ServiceType service;
    } matchTable[] = {
        { "+CREG:",             SERVICE_MM  },
        { "+CGREG:",            SERVICE_MM  },
        { "^MODE:",             SERVICE_MM  },
        { "^RSSI:",             SERVICE_MM  }, 
        { "^CSNR:",             SERVICE_MM  },	//added by x84000798 
        { "^CRSSI:",            SERVICE_MM  }, 
        { "^HDRRSSI:",          SERVICE_MM  }, 
        { "^SRVST:",            SERVICE_MM  },
        { "^HCSQ:", 						SERVICE_MM  },	//Added by c00221986 for DTS2013040706143
        { "RING",               SERVICE_CC  },
        { "NO CARRIER",         SERVICE_CC  },
        { "+CCWA",              SERVICE_CC  },
        { "+CCCM:",             SERVICE_CC  },        
        { "+CLCC:",             SERVICE_CC  },                
        { "^CONN:",             SERVICE_CC  },      
        { "^CEND:",             SERVICE_CC  },    
        { "^ORIG:",             SERVICE_CC  }, 
        { "+CRING:",						SERVICE_CC	},	// Add by x84000798
		{ "HANGUP:",        SERVICE_CC },
        { "+CMT:",              SERVICE_SMS },
        { "+CMTI:",             SERVICE_SMS },
	    { "+CBM:",		        SERVICE_SMS },  // add by kf52201
        { "+CDS:",              SERVICE_SMS },
        { "+CDSI:",             SERVICE_SMS },
        { "^SMMEMFULL:",        SERVICE_SMS },
        { "+CSSU:",             SERVICE_SS  },
        { "+CUSD:",             SERVICE_SS  },
        { "^DCONN",             SERVICE_PS  },
        { "^DEND",              SERVICE_PS  },
        { "^NDISSTAT:",       	SERVICE_PS  },	//Added by x84000798 for ndis
  	    { "^SIMST",             SERVICE_SIM },
  	    { "^STIN",              SERVICE_SIM },
        { "+CUSATEND",					SERVICE_SIM },	//Added by x84000798 for DTS2012073007612 2012-07-31
        { "+CUSATP:",						SERVICE_SIM },	//Added by x84000798 for DTS2012073007612 2012-07-31
        { "^NWTIME:",           SERVICE_MM  },
        { "*PSSIMSTAT:",        SERVICE_SIM }

    };

    total  = sizeof(matchTable) / sizeof(matchTable[0]);
    for (i = 0 ; i < total ; i++) 
    {
        if(strStartsWith(s, matchTable[i].prefix)) 
        {
            service = matchTable[i].service;
            return (ServiceType)service;
        }
    }
    
    return SERVICE_TOTAL;
}

/***************************************************
 Function:  on_request
 Description:  
    division service upon request value 
 Calls: 
    judge_service_on_request
    RIL_onRequestComplete
 Called By:
    called by Android upper layer 
 Input:
    request - correspond the request value in ril.h
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
static void on_request (int request, void *data, size_t datalen, RIL_Token token)
{
    ServiceType service;
    at_request = request;
    
    if(RIL_DEBUG)ALOGD("%s: %s, sState:%d", __FUNCTION__, requestToString(request), sState);


     if(request == RIL_REQUEST_REPORT_STK_SERVICE_IS_RUNNING)
    {
	    request_stk_service_is_running(data, datalen,  token);
	    return;
    }


    /* RIL_REQUEST_SCREEN_STATE is supported in any radio state */
    if (request == RIL_REQUEST_SCREEN_STATE) 
    {
        hwril_request_mm(request, data, datalen, token);
        return;
    }

    /* Handle RIL request when radio state is UNAVAILABLE */
    if (sState == RADIO_STATE_UNAVAILABLE) {
        if(request == RIL_REQUEST_GET_SIM_STATUS)
        {
            // DTS2012020104688  lkf52201 2012-02-9 modified
            hwril_request_sim(request, data, datalen, token);
            return;
        }        
        else 
        {
            RIL_onRequestComplete(token, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
            return;
        }
    } else if (sState == RADIO_STATE_OFF){    
        // Handle RIL request when radio state is OFF 
        if (request == RIL_REQUEST_GET_SIM_STATUS) {
            // DTS2012020104688  lkf52201 2012-02-9 modified
            hwril_request_sim(request, data, datalen, token);
            return;
        } else if(request ==  RIL_REQUEST_RADIO_POWER) {
            hwril_request_dev(request, data, datalen, token);
            return;
        } 
		/*Begin modify by fKF34305 20111013 for Query DEVICE ID when CFUN=0*/
		else if ((sState == RADIO_STATE_OFF || sState == RADIO_STATE_SIM_NOT_READY)
        		&& !(request == RIL_REQUEST_RADIO_POWER || 
 	             request == RIL_REQUEST_GET_SIM_STATUS ||
 	             request == RIL_REQUEST_GET_IMEISV ||
 	             request == RIL_REQUEST_DEVICE_IDENTITY ||    //Add by KF34305 20111203 for DTS2011111806661
 	             request == RIL_REQUEST_GET_IMEI ||
 	             request == RIL_REQUEST_BASEBAND_VERSION ||
 	             request == RIL_REQUEST_SCREEN_STATE)) 
 	             /*End modify by fKF34305 20111013 for Query DEVICE ID when CFUN=0*/{                  
            RIL_onRequestComplete(token, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
            return;
        }
    }

    // Handle other cases 
    
    service = judge_service_on_request(request);
    switch (service) {
        case SERVICE_CC:
	     hwril_request_cc(request, data, datalen, token);
	     break;
        case SERVICE_DEV:
            hwril_request_dev(request, data, datalen, token);
            break;            
        case SERVICE_MM:
            hwril_request_mm(request, data, datalen, token);
            break;            
        case SERVICE_SMS:
            hwril_request_sms(request, data, datalen, token);
            break;            
        case SERVICE_PS:
        	if(RIL_DEBUG) ALOGD("SERVICE_PS request: %s", requestToString(request));
            hwril_request_ps(request, data, datalen, token);
            break;            
        case SERVICE_SIM:
            hwril_request_sim(request, data, datalen, token);
            break;            
        case SERVICE_SS:
            hwril_request_ss(request, data, datalen, token);
            break;            
        default:
            RIL_onRequestComplete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
            if(RIL_DEBUG) ALOGD("%s: invalid service ID:%d\n", __FUNCTION__, service);
            break;
    }
    if(RIL_DEBUG) ALOGD("onRequest: %s, Local implementation,  AT cmd sent", requestToString(request));

}

/***************************************************
 Function:  on_request
 Description:  
    division unsolicited upon unsolicited intent(s) 
 Calls: 
    judge_service_on_request
    RIL_onRequestComplete
 Called By:
    Called by atchannel when an unsolicited line appears 
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

static void on_unsolicited (const char *s, const char *smsPdu)
{
    int service; 

    /* Ignore unsolicited responses until we're initialized.
     * This is OK because the RIL library will poll for initial state
     */
    if (sState == RADIO_STATE_UNAVAILABLE) 
    {
        return;
    }

    service = judge_service_on_unsolicited(s);  //one channel for one service
    switch (service) {
        case SERVICE_CC:
            hwril_unsolicited_cc(s);
            break;
        case SERVICE_DEV:
            hwril_unsolicited_dev(s);
            break;            
        case SERVICE_MM:
            hwril_unsolicited_mm(s);
            break;            
        case SERVICE_SMS:
            hwril_unsolicited_sms(s, smsPdu);
            break;            
        case SERVICE_PS:
            hwril_unsolicited_ps(s);
            break;            
        case SERVICE_SIM:
            hwril_unsolicited_sim(s);
            break;            
        case SERVICE_SS:
            hwril_unsolicited_ss(s);
            break;            
        default:
            if(RIL_DEBUG) ALOGD("%s: Unexpected service type:%d", __FUNCTION__, service);
            break;
    }
}


