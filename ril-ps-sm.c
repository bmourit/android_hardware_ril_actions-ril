/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-ps-sm.c
 Author: fangyuchuang/00140058
 Version: V1.0
 Date: 2010/04/07
 Description:
           this file include an ndis machine, the ndis machine manage the PDP state. 
           there also some function that set the network setting include set the ip,dns,gateway
 Function List:
 History:
 <author>       <time>        <version>        <desc>
 f00140058      10/04/08        1.0
fangyuchuang  10/07/28        1.1
*******************************************************/
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <alloca.h>
#include <getopt.h>
#include <sys/socket.h>
#include <cutils/sockets.h>
#include <termios.h>
#include <sys/time.h> 
#include <signal.h> 
#include <utils/Log.h>

#include "huawei-ril.h"
#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include "ril-ps-api.h"
#include "ril-ps-sm.h"

/*-------------------------------------------------------
		 Macro for ps state mechine constant
  -------------------------------------------------------*/
extern  void request_or_send_pdp_context_list(RIL_Token *t);
extern void onunsol_data_call_list_chanage( void);
//extern  RIL_Data_Call_Response call_tbl;
extern char g_pdptype[65];
extern char ip_address[64];
extern char dns_address[64];
extern char gateway_address[64];
extern char *rmnet0_response[3];

//Bebin to be added by c00221986 for ndis
extern char PORT_ECM_KEYNAME[60];
//End being added by c00221986 for ndis

extern HWRIL_RMNET_PARAMETERS_ST ndis_st;
static int ndis_state = 0;
static int activetimeout = 0;
static int deactivetimeout = 0;
void ndis_ative_timeout(void);

pthread_mutex_t s_ndis_mutex;  //Added by lifei lkf34826 2011.03.16
struct itimerval time_value;

/***************************************************
 Function:  dhcp_up_handle
 Description:  
 	use "at+dhcp" to get the ip\dns\getway and set network device 
 Calls: 
 	
 Called By:
	none
 Input:
      data - NULL
      token - RIL_REQUEST_SETUP_DEFAULT_PDP
 Output:
      no
 Return:
	-1 - set the network attribute fail
	 0  - set the network attribute successful
 Others:
	none
**************************************************/
int dhcp_up_handle(void)
{
	char *cmd;
	int err,ret;
	ATResponse *p_response = NULL;
	char *line;
	char  *ip_ptr, *netmask_ptr, *gateway_ptr, *dhcp_ptr, *pdns_ptr,  *sdns_ptr;

	if(RIL_DEBUG) ALOGD( "dhcp_up_handle \n");

	asprintf(&cmd, "AT^DHCP?");
	err = at_send_command_singleline(cmd, "^DHCP:", &p_response);
	if (err < 0 || p_response->success == 0) {
		goto error;
	}  

	line = p_response->p_intermediates->line;
	err = at_tok_start(&line);
	if (err < 0) goto error;
	err = at_tok_nextstr(&line, &ip_ptr);
	if (err < 0) goto error;
	err = at_tok_nextstr(&line, &netmask_ptr);
	if (err < 0) goto error;
	err = at_tok_nextstr(&line, &gateway_ptr);
	if (err < 0) goto error;
	err = at_tok_nextstr(&line, &dhcp_ptr);
	if (err < 0) goto error;
	err = at_tok_nextstr(&line, &pdns_ptr);
	if (err < 0) goto error;
	err = at_tok_nextstr(&line, &sdns_ptr);
	if (err < 0) goto error;

	if(RIL_DEBUG) ALOGD( "ip = %s, netmask=%s, gate=%s, pdns=%s \n", ip_ptr, netmask_ptr, gateway_ptr, pdns_ptr);

	ret = configure_interface(ip_ptr, gateway_ptr, pdns_ptr, sdns_ptr);
	if(ret < 0){
		if(RIL_DEBUG) ALOGD( "configureInterface error !\n");
		deconfigure_interface();
		goto error;
	}

	free(cmd);
	at_response_free(p_response);
	return 0;
error:
	free(cmd);
	at_response_free(p_response);
	return -1;
}

/***************************************************
 Function:  active_ndis
 Description:  
 	send the "AT^NDISDUP=1,1,\"%s\" ,atvice the Modem PS
 Calls: 
 	at_send_command()
 Called By:
	none
 Input:
      data - NULL
      token - RIL_REQUEST_SETUP_DEFAULT_PDP
 Output:
      no
 Return:
      no
 Others:
	    Modify by lkf34826 2011.3.9
**************************************************/
static int active_ndis(HWRIL_RMNET_PARAMETERS_ST *rmnet_st)
{	
	char *cmd;
	int err = 0;
	ATResponse *p_response = NULL;

    if(RIL_DEBUG) ALOGD( "Active Ndis   ndis_state = %d\n", ndis_state);	

	activetimeout = 1;
	if((rmnet_st->user[0]=='\0' )||(rmnet_st->passwd[0] =='\0')  )
	{
		asprintf(&cmd, "AT^NDISDUP=1, 1,\"%s\"", rmnet_st->apn);
	} 
	else
	{
		if((rmnet_st->authpref) == 0 )
		{
			asprintf(&cmd, "AT^NDISDUP=1, 1,\"%s\",\"%s\",\"%s\"", rmnet_st->apn,
			rmnet_st->user,rmnet_st->passwd);
		}
		else
		{
			asprintf(&cmd, "AT^NDISDUP=1, 1,\"%s\",\"%s\",\"%s\",%d", rmnet_st->apn,
			rmnet_st->user,rmnet_st->passwd,rmnet_st->authpref);
		}
	} 
	err = at_send_command(cmd, &p_response);
	free(cmd); 
	if(err < 0 || p_response->success == 0)
	{
		err = -1;
	}
	return err;  // lkf34826 modified for lint
}

/***************************************************
 Function:  active_ndis
 Description:  
 	send the "AT^NDISDUP=1,0,\"%s\" ,deatvice the Modem PS
 Calls: 
 	start_timer
 	deconfigure_interface
 Called By:
	ndis_sm
 Input:
      data - NULL
      token - RIL_REQUEST_DEACTIVATE_DEFAULT_PDP
 Output:
      no
 Return:
      no
 Others:
	none
**************************************************/
static int deactive_ndis(void)
{
	if(RIL_DEBUG) ALOGD( "DEACTIVE  Ndis \n");	
	char *cmd;
	int err;

	deactivetimeout = 1;

	deconfigure_interface();
	asprintf(&cmd, "AT^NDISDUP=1,0");
	err = at_send_command(cmd, NULL);
	free(cmd);
    return err;
}

/***************************************************
 Function:  active_pdp_timeout
 Description:  
 	call ndisi_sm when ative the PDP time out
 Calls: 
      ndis_sm()
 Called By:
      time_out_handler()
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
        Modify by lkf34826  2011.3.9
**************************************************/
static void active_pdp_timeout(void)
{
	if(RIL_DEBUG) ALOGE( "********************ActivePDPTimeOut ********************\n");
    ndis_st.ndis_state = NDIS_EVENT_REPORT_DCONN;
	ndis_sm(&ndis_st);
}

/***************************************************
 Function:  deactive_timeout_handle
 Description:  
 	call ndis_sm when modem unsolicited DCONN
 Calls: 
 Called By:
      none
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
     none
**************************************************/
void deactive_timeout_handle(void)
{
	if(RIL_DEBUG)ALOGD( "deactive_tiveout_handle  activetimeout=%d ,deactivetimeout=%d\n",activetimeout,deactivetimeout);
	char *cmd;
	int err;
	asprintf(&cmd, "AT^NDISDUP= 1,0");
	err = at_send_command(cmd, NULL);
	free(cmd);
}

/***************************************************
 Function:  deactive_pdp_timeout
 Description:  
 	call ndisi_sm when ative the PDP time out
 Calls: 
      deactive_timeout_handle
 Called By:
      time_out_handler
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
        Modify by lkf34826 2011.3.9
**************************************************/
static void deactive_pdp_timeout(void)
{
	if(RIL_DEBUG) ALOGE( "********************DeactivePDPTimeOut******************** \n");
	deactive_timeout_handle();
	ndis_st.ndis_state = NDIS_EVENT_REPORT_DEND;
	ndis_sm(&ndis_st);	
}

/***************************************************
 Function:  DEND_report
 Description:  
 	call ndis_sm when modem unsolicited DEND
 Calls: 
      ndis_sm
 Called By:
      none
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
        Modyfy by lkf34826 2011.3.9
**************************************************/
void DEND_report(void* param)
{
   
	if(RIL_DEBUG) ALOGD( "DendReport \n");
	deactivetimeout = 0;
	ndis_st.ndis_state = NDIS_EVENT_REPORT_DEND;
	ndis_sm(&ndis_st);
	//memset(&call_tbl,0,sizeof(RIL_Data_Call_Response));
	onunsol_data_call_list_chanage();
}

/***************************************************
 Function:  DCONN_report
 Description:  
 	call ndis_sm when modem unsolicited DCONN
 Calls: 
      ndis_sm
 Called By:
      none
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
        Modify by lkf34826 2011.3.9
**************************************************/
void  DCONN_report(void* param)
{
	activetimeout = 0;
	ndis_st.ndis_state = NDIS_EVENT_REPORT_DCONN;
	ndis_sm(&ndis_st);
}

/***************************************************
 Function:  delet_timer
 Description:  
 	when ative or deactive time out , system will call this function,
 	it will change the state machine's state
 Calls: 
 Called By:
      none
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
     none
**************************************************/
void delet_timer(void)
{
	time_value.it_interval.tv_sec=0;
	time_value.it_interval.tv_usec=0;
	time_value.it_value.tv_sec=0;
	time_value.it_value.tv_usec=0;
	setitimer(ITIMER_REAL,&time_value,NULL);
}

/***************************************************
 Function:  time_out_handler
 Description:  
 	when ative or deactive time out , system will call this function, 
 	it will change the state machine's state
 Calls: 
	active_pdp_timeout()
	deactive_pdp_timeout()
 Called By:
      init_sigaction()
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
     none
**************************************************/
static void time_out_handler(int signo) 
{     
	if(RIL_DEBUG) ALOGD( "pdpsm handler activetimeout=%d ,deactivetimeout=%d\n",activetimeout,deactivetimeout);

	delet_timer();

	if((activetimeout != 0) && (deactivetimeout != 0)){
		activetimeout = 0;
		deactivetimeout = 0;
		if(RIL_DEBUG) ALOGE( "PDP pdpsm handler error !\n");
		return;
	}

	if((activetimeout == 0) && (deactivetimeout == 0)){
		return ;
	}

	if(activetimeout != 0){
		activetimeout = 0;
		active_pdp_timeout();	
		return;
	}

	if(deactivetimeout != 0){
		deactivetimeout = 0;
		deactive_pdp_timeout();
		return;
	}
} 

/***************************************************
 Function:  reset_ndis_sm
 Description:  
 	reset ndis sm to initial state
 Calls: 
 Called By:
      none
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
        Modify by lkf34826   2011.3.9
**************************************************/
void reset_ndis_sm(void)
{
	ndis_state = NDIS_STATE_DEATIVED;	
}

/***************************************************
 Function:  init_sigaction
 Description:  
 	initial the sigaction register time out handler,the time out handle is time_out_handler
 	this function must call went ril is initialed
 Calls: 
       init_callback
 Called By:
      none
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
     none
**************************************************/
void init_sigaction(void) 
{ 
	if(RIL_DEBUG) ALOGD( "init_sigaction \n");

	if(signal(SIGALRM, time_out_handler)==SIG_ERR)
	{
		if(RIL_DEBUG) ALOGE( "Unable to create handler for SIGALRM\n");
		exit(0);
	}
} 

/***************************************************
 Function:  start_timer
 Description:  
 	start the timer for active or deative the PDP, the time interval is 20 seconds
 Calls: 
 Called By:
      deactive_ndis
      active_ndis
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
     modify by lkf34826 2011.3.9
**************************************************/
void start_timer(void)
{	
	time_value.it_interval.tv_sec=10;
	time_value.it_interval.tv_usec=0;
	time_value.it_value.tv_sec=10;
	time_value.it_value.tv_usec=0;
	setitimer(ITIMER_REAL, &time_value, NULL);
}

/***************************************************
 Function:  ndis_ative_timeout
 Description:  
 	handler the state ndis ative timeout. check the link. if there is a link ,change the ndis_state
 	to NDIS_EVENT_REPORT_DCONN otherwith ,change the ndis_state to NDIS_EVENT_REQUEST_PDP_DEACTIVE.
 Calls: 
       request_setup_default_pdp_rmnet
 Called By:
      none
 Input:
      no
 Output:
      no
 Return:
      no
 Others:
        Modify by lkf34826 2011.3.9
**************************************************/
void ndis_ative_timeout(void)
{
#if 0 
	ATResponse *p_response;
	ATLine *p_cur;
	int err;
	int n = 0;

	err = at_send_command_multiline ("AT+CGACT?", "+CGACT:", &p_response);
    if (err < 0 || p_response->success == 0)
	{
	 return;
	}

	for (p_cur = p_response->p_intermediates; p_cur != NULL;p_cur = p_cur->p_next)
	{
	  n++;
	}
	RIL_Data_Call_Response *responses = (RIL_Data_Call_Response *)alloca(n * sizeof(RIL_Data_Call_Response));

	int i;
	for (i = 0; i < n; i++) {
		responses[i].cid = -1;
		responses[i].active = -1;
		responses[i].type = "";
		responses[i].apn = "";
		responses[i].address = "";
	}

	RIL_Data_Call_Response *response = responses;
	for (p_cur = p_response->p_intermediates; p_cur != NULL;
	p_cur = p_cur->p_next) {
		char *line = p_cur->line;

		err = at_tok_start(&line);
		if (err < 0)
			goto error;

		err = at_tok_nextint(&line, &response->active);
		if (err < 0)
			goto error;

		err = at_tok_nextint(&line, &response->cid);
		if (err < 0)
			goto error;

		if(response->active == 1)
			goto ative;
		response++;
	}    
error:
	at_response_free(p_response);
	ndis_st.ndis_state = NDIS_EVENT_REQUEST_PDP_DEACTIVE;
	ndis_sm(&ndis_st);
	return ;
#endif
ative:
	//at_response_free(p_response);
	ndis_st.ndis_state = NDIS_EVENT_REPORT_DCONN;
	ndis_sm(&ndis_st);
	return ;
}

/***************************************************
 Function:  ndis_sm
 Description:  
      the ndis state mechine, can refer the PXA310+Balong design explain
 Calls: 
 active_ndis
 deactive_ndis
 dhcp_up_handle
 ndis_ative_timeout
 ndis_deative_timeout
 
 Called By:
	active_pdp_timeout
	deactive_pdp_timeout
	DEND_report
	DCONN_report
	request_deative_default_pdp_rmnet
 Input:
      data - sting of APN
      token - ril return to framework, the framewok identify while request return by it 
      ndis_event - which event happen 
 Output:
      no
 Return:
      no
        Modify by lkf34826 2011.3.9
**************************************************/
void ndis_sm(HWRIL_RMNET_PARAMETERS_ST *rmnet_st)
{
    assert(rmnet_st);
	pthread_mutex_lock(&s_ndis_mutex);
	
	RIL_Token Token = rmnet_st->token;
	HWRIL_NDIS_State new_ndis_state = rmnet_st->ndis_state;

	if(RIL_DEBUG) ALOGD( "ndisi_state:%d ,ndis_event:%d\n", ndis_state, new_ndis_state);
	
	switch(ndis_state){
		case NDIS_STATE_DEATIVED:
			if(new_ndis_state ==  NDIS_EVENT_REQUEST_PDP_ACTIVE)
			{
			   	if(active_ndis(rmnet_st) >= 0)
			   	{
			     	start_timer();
			     	if(RIL_DEBUG) ALOGD( "ndisi_state:NDIS_STATE_DEATIVED ,ndis_event:NDIS_EVENT_REQUEST_PDP_ACTIVE\n");
			     	ndis_state = NDIS_STATE_ATIVING;
			   	}
			   	else
				{
			      	if(RIL_DEBUG) ALOGE( "ndisi_state:NDIS_STATE_DEATIVED ,ndis_event:NDIS_EVENT_REQUEST_PDP_ACTIVE\n");
					at_send_command("AT^NDISDUP=1,0", NULL);
                  	RIL_onRequestComplete(Token, RIL_E_GENERIC_FAILURE, NULL, 0);
			   	}
			}
			else if(new_ndis_state ==  NDIS_EVENT_REPORT_DEND)
			{
				if(RIL_DEBUG) ALOGE( "netdevice ndisi_state:NDIS_STATE_ATIVED , ndis_event:NDIS_EVENT_REPORT_DEND\n");
			}
			else if(new_ndis_state ==  NDIS_EVENT_REQUEST_PDP_DEACTIVE)
			{
				at_send_command("AT^NDISDUP=1,0", NULL);
               	RIL_onRequestComplete(Token, RIL_E_SUCCESS, NULL, 0);			
			}
			else //NDIS_EVENT_REPORT_DCONN
			{
			   	if(RIL_DEBUG) ALOGD( "ndisi_state:NDIS_STATE_DEATIVED ,ndis_event:%d\n", new_ndis_state);
			}
		    break;
			
		case NDIS_STATE_ATIVED:
			if(new_ndis_state == NDIS_EVENT_REQUEST_PDP_DEACTIVE)
			{
            	if(RIL_DEBUG) ALOGD( " ndisi_state:NDIS_STATE_ATIVED , ndis_event:NDIS_EVENT_REQUEST_PDP_DEACTIVE \n"); 			    
				if(deactive_ndis()>= 0)
				{
					start_timer();
				 	ndis_state = NDIS_STATE_DEATIVING;
				}
				else
				{ 
					if(RIL_DEBUG) ALOGE( " ndisi_state:NDIS_STATE_ATIVED , ndis_event:NDIS_EVENT_REQUEST_PDP_DEACTIVE \n");
                	RIL_onRequestComplete(Token, RIL_E_GENERIC_FAILURE, NULL, 0);
				}
			}
			else if(new_ndis_state == NDIS_EVENT_REPORT_DCONN)
			{
				if(RIL_DEBUG) ALOGD( "netdevice ndisi_state:NDIS_STATE_ATIVED , ndis_event:NDIS_EVENT_REPORT_DCONN\n"); 
			}
			else if(new_ndis_state == NDIS_EVENT_REPORT_DEND)
			{ 
			    ndis_state = NDIS_STATE_DEATIVED;
			   	deconfigure_interface();
			   	if(RIL_DEBUG) ALOGD( "netdevice ndisi_state:NDIS_STATE_ATIVED , ndis_event:NDIS_EVENT_REPORT_DEND\n"); 
			}
			else //NDIS_EVENT_REQUEST_PDP_ACTIVE
			{  
			    if(RIL_DEBUG) ALOGD( "ndisi_state:NDIS_STATE_ATIVED ,ndis_event:%d\n", new_ndis_state);
				RIL_onRequestComplete(Token, RIL_E_SUCCESS, rmnet0_response, sizeof(rmnet0_response));
			}
			break;
		case NDIS_STATE_ATIVING:
		    if(new_ndis_state ==  NDIS_EVENT_REPORT_DCONN)
			{  
				delet_timer();
				/*if((start_ip_check_pthread(Token)) > 0)
				{
					if(RIL_DEBUG) LOGE("SUCCESS!");
					
				}*/
				if(dhcp_up_handle() >= 0)
				{
			    	if(RIL_DEBUG) ALOGD(  "ndisi_state:NDIS_STATE_ATIVING ,ndis_event:NDIS_EVENT_REPORT_DCONN\n");   
			      	ndis_state = NDIS_STATE_ATIVED;
				  	
			       	//call_tbl.apn = rmnet_st->apn;
			       	//call_tbl.address = rmnet0_response[2];
			       	//call_tbl.active = 1; 
			       	//call_tbl.cid = 1;
	                //call_tbl.type = "ip";	
	                
					
				#ifdef ANDROID_3
				    rmnet0_response_v6.status = PDP_FAIL_NONE;
				#ifdef ANDROID_4
				    rmnet0_response_v6.suggestedRetryTime = -1;
				#endif
				    rmnet0_response_v6.cid = 1;
				    rmnet0_response_v6.active = 2;     
				    rmnet0_response_v6.type = g_pdptype;      
				    //rmnet0_response_v6.ifname = "rmnet0";
				    //rmnet0_response_v6.ifname = "eth1";
				    rmnet0_response_v6.ifname = PORT_ECM_KEYNAME;
				    rmnet0_response_v6.addresses = rmnet0_response[2]; 
				    rmnet0_response_v6.dnses = dns_address;     
				    rmnet0_response_v6.gateways = gateway_address;  
				    RIL_onRequestComplete(Token, RIL_E_SUCCESS, &rmnet0_response_v6, sizeof(rmnet0_response_v6));
				#else	
				    RIL_onRequestComplete(Token, RIL_E_SUCCESS, rmnet0_response, sizeof(rmnet0_response));
				#endif
						   
			      	//request_or_send_pdp_context_list(NULL);
			   	}
			   	else
			   	{ 	if(RIL_DEBUG) ALOGE("%s: start_ip_check_pthread() failed: %s", __FUNCTION__, strerror(errno));
			   		if(RIL_DEBUG) ALOGE(  "ndisi_state:NDIS_STATE_ATIVING ,ndis_event:NDIS_EVENT_REPORT_DCONN");    
			        ndis_state = NDIS_STATE_DEATIVED;
					at_send_command("AT^NDISDUP=1,0", NULL);
			        RIL_onRequestComplete(Token, RIL_E_GENERIC_FAILURE, NULL, 0);
			   	}
		    }
		    else if(new_ndis_state == NDIS_EVENT_REPORT_DEND)
		    {
				ndis_state = NDIS_STATE_DEATIVED;
				delet_timer();
				RIL_onRequestComplete(Token, RIL_E_GENERIC_FAILURE, NULL, 0);
			}
		    else
		    {  
		    	ndis_state = NDIS_STATE_DEATIVED;
			    delet_timer();
		       	RIL_onRequestComplete(Token, RIL_E_GENERIC_FAILURE, NULL, 0);
				if(RIL_DEBUG) ALOGE(  " ndisi_state:NDIS_STATE_ATIVING ,ndis_event:%d\n",new_ndis_state);
			}
			break;
			
		case NDIS_STATE_DEATIVING:
			if(new_ndis_state ==  NDIS_EVENT_REPORT_DEND)
			{
			    if(RIL_DEBUG) ALOGD("ndisi_state:NDIS_STATE_DEATIVING ,ndis_event:NDIS_EVENT_REPORT_DEND\n"); 
                ndis_state = NDIS_STATE_DEATIVED;
				delet_timer();
			    RIL_onRequestComplete(Token, RIL_E_SUCCESS, NULL, 0);
			}
			else
			{ 
			    if(RIL_DEBUG) ALOGE("ndisi_state:NDIS_STATE_DEATIVING ,ndis_event:%d\n",new_ndis_state);
			    ndis_state = NDIS_STATE_DEATIVED;
			    delet_timer();
                RIL_onRequestComplete(Token, RIL_E_GENERIC_FAILURE, NULL, 0);
			}
			break;
			
		default:
			break;
		}
	pthread_mutex_unlock(&s_ndis_mutex);
	return ;
}
