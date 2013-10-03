/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-dev.c
 Author: liubing/00141886
 Version: V1.0
 Description:
 
    Handle request and unsolicited about dev.include below interface:
    
	RIL_REQUEST_RADIO_POWER
    RIL_REQUEST_GET_IMEI
    RIL_REQUEST_GET_IMEISV
    RIL_REQUEST_BASEBAND_VERSION
    RIL_REQUEST_OEM_HOOK_RAW
    RIL_REQUEST_OEM_HOOK_STRINGS
    RIL_REQUEST_SET_BAND_MODE
    RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE
    RIL_REQUEST_DEVICE_IDENTITY
    RIL_REQUEST_CDMA_SUBSCRIPTION
    RIL_REQUEST_CDMA_SET_SUBSCRIPTION
    RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE
	RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE

NOTICE:tdscdma module does not support RIL_REQUEST_SET_BAND_MODE && RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE
 
*******************************************************/
/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

  when        who      version    why 
 -------    -------    -------   --------------------------------------------
2010/09/14  l141886    V1B01D01  add wcdma dev interface
2010/12/14  fkf34035   V2B01D01  add cdma dev interface
2011/05/14  fkf34035   V3B02D01  add tdscdma dev interface

===========================================================================*/

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/
#include "huawei-ril.h"
/*Begin added by x84000798 2012-05-25 DTS2012060806394 */
//#include <cutils/properties.h>
/*End added by x84000798 2012-05-25 DTS2012060806394 */


/*===========================================================================

                   INTERNAL DEFINITIONS AND TYPES

===========================================================================*/
static int sim_sc=0;       // lkf52201 2011-12-26 将该变量的定义从ril-ss.c 移到 ril-dev.c
/* Begin added by x84000798 2012-05-29 DTS2012060806513 */
int g_airplanemode = 0;
int sarfeature = 0;
extern int g_sarctrl;
/* End added by x84000798 2012-05-29 DTS2012060806513 */
extern int getNumRetries (int request);  // lkf52201 2011-12-26 将该变量的定义从ril-ss.c 移到 ril-dev.c
extern RIL_RadioState sState;
extern Module_Type g_module_type;
extern MODEM_Vendor g_modem_vendor;
extern CDMA_SubType g_cdma_subtype;
extern int has_set_radio_on;  // DTS2011120501094 lkf52201 2011-12-5 modified

/*===========================================================================

                                FUNCTIONS

===========================================================================*/
/***************************************************
 Function:  request_radio_power
 Description:  
    reference ril.h
 Calls: 
    RIL_onUnsolicitedResponse
    is_radio_on
 Called By:
    hwril_request_dev
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    Modeify by fangyuchuang/00141886 for sim locked
**************************************************/
static void request_radio_power(void *data, size_t datalen, RIL_Token t)
{
    int onOff;

    int err;
    ATResponse *p_response = NULL;

    assert (datalen >= sizeof(int *));
    onOff = ((int *)data)[0];
	
    /* DTS2010101801935 不插SIM的时候，飞行模式开启以后，需要重启系统 */
	#if 0
    if(sState == RADIO_STATE_SIM_LOCKED_OR_ABSENT){
           RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_RADIO_STATE_CHANGED,NULL, 0);
          RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
          return ;
    }
    #endif
	/*Begin modified by x84000798 2012-05-25 DTS2012060806394 */
	//char value[PROPERTY_VALUE_MAX];	
    if (0 == onOff) { // DTS2011120501094 lkf52201 2011-12-6 modified
         err = at_send_command("AT+CFUN=0", &p_response);
         if (err < 0 || p_response->success == 0) goto  error;
         set_radio_state(RADIO_STATE_OFF);
		 /* 由于很多模块不支持AT^SIMPWR=0命令，当前主线版本去掉此功能
		 if (property_get("sys.shutdown.requested", value, NULL))
		 {
			set_radio_state(RADIO_STATE_UNAVAILABLE);
			err = at_send_command("AT^SIMPWR=0", &p_response);
			if (err < 0 || p_response->success == 0) goto  error;
		 }
		 */
		 g_airplanemode = 1;	//Added by x84000798 2012-05-29 DTS2012060806513
	/*End modified by x84000798 2012-05-25 DTS2012060806394 */
    } else if (onOff > 0) {  // DTS2011120501094 lkf52201 2011-12-6 modified
        err = at_send_command("AT+CFUN=1", &p_response);
        if (err < 0|| p_response->success == 0) {
            // Some stacks return an error when there is no SIM,
            // but they really turn the RF portion on
            // So, if we get an error, let's check to see if it
            // turned on anyway

            if (is_radio_on() != 1) {
                goto error;
            }
        }

	    has_set_radio_on = 1;  	// DTS2011120501094 lkf52201 2011-12-5 added
		/* Begin added by x84000798 2012-05-29 DTS2012060806513 */
		g_airplanemode = 0;
		if (1 == sarfeature)
		{
			if (1 == g_sarctrl)
			{
				ALOGD("**** Airplane Mode Off, SARCTRL = 1 ****");
				at_send_command("AT^SARCTRL=1",NULL);
			}
			else if (0 == g_sarctrl)
			{
				ALOGD("**** Airplane Mode Off, SARCTRL = 0 ****");
				at_send_command("AT^SARCTRL=0",NULL);
			}
		}		
		/* End added by x84000798 2012-05-29 DTS2012060806513 */
        if ( NETWORK_CDMA == g_module_type ) {
			
			if( CDMA_NV_TYPE == g_cdma_subtype) {
				set_radio_state(RADIO_STATE_NV_NOT_READY);
			}else{
				set_radio_state(RADIO_STATE_RUIM_NOT_READY);
			}
		}else{
			set_radio_state(RADIO_STATE_SIM_NOT_READY);
		}
		
    }

    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;

error:
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
}

/* Begin modified by xiaopinghua x84000798 for DTS2012071606193  2012-07-30*/
/* Begin to modify by hexiaokong h81003427 for DTS2012050301434  2012-05-03*/
void request_get_imeisv(void *data, size_t datalen, RIL_Token t)
{
    int err; 
    int i_svnlen = 0;
    ATResponse *p_response;
    char *line = NULL;
	char *svn = NULL;
	/*请求厂商序列。这个函数新的和旧的是不同的，可能造成差异*/
	err = at_send_command_singleline("AT^IMEISV?", "^IMEISV:", &p_response);
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
	err = at_tok_nextstr(&line, &svn);
	if (0 > err)
	{
		goto error;
	}
	i_svnlen = strlen(svn);
	svn += (i_svnlen - 2);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, svn, sizeof(char *));
    at_response_free(p_response);	
    return;
	
	#if 0
    //err = at_send_command_numeric("AT+CGSN", &p_response);
    err = at_send_command_numeric("AT^IMEISV?", &p_response);
    if (err < 0 || p_response->success == 0) 
    {
        goto error;
    }

    line = p_response->p_intermediates->line;   
    i_linelen = strlen(line); 
    line += (i_linelen - 2);
   
    //RIL_onRequestComplete(t, RIL_E_SUCCESS,p_response->p_intermediates->line, sizeof(char *));
    RIL_onRequestComplete(t, RIL_E_SUCCESS,line, sizeof(char *));
    at_response_free(p_response);
	return;
	#endif
error: 
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}
/* End to modify by hexiaokong h81003427 for DTS2012050301434  2012-05-03*/
/* End modified by xiaopinghua x84000798 for DTS2012071606193  2012-07-30*/

void request_get_imei(void *data, size_t datalen, RIL_Token t)
{
    int err; 
    ATResponse *p_response;
    
    err = at_send_command_numeric("AT+CGSN", &p_response);
    if (err < 0 || p_response->success == 0) goto error;
    

    RIL_onRequestComplete(t, RIL_E_SUCCESS,p_response->p_intermediates->line, sizeof(char *));
    at_response_free(p_response);
    return;
error: 
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

void request_get_baseband_version(void *data, size_t datalen, RIL_Token t)
{
    int err; 
    char *line;
    ATResponse *p_response;
	if( NETWORK_CDMA == g_module_type )
	{
		err = at_send_command_singleline("AT+CGMR","+CGMR:", &p_response);
		if (err >= 0 && p_response->success != 0) 
		{
			line = p_response->p_intermediates->line;
			err = at_tok_start(&line);
			if (err < 0) goto error;
		}
		else
		{
		    /* BEGIN DTS2011123101074 lkf52201 2011-12-31 added */
		    at_response_free(p_response);
		    p_response = NULL;
		    /* END   DTS2011123101074 lkf52201 2011-12-31 added */
			err = at_send_command_numeric("AT+CGMR", &p_response);
			if (err < 0 || p_response->success == 0) goto error;
			line = p_response->p_intermediates->line;
		}
	}
	else
	{
	    err = at_send_command_numeric("AT+CGMR", &p_response);
	    if (err < 0 || p_response->success == 0) goto error;
	    line = p_response->p_intermediates->line;
	}
    RIL_onRequestComplete(t, RIL_E_SUCCESS,line, sizeof(char *));
    at_response_free(p_response);
    return;
error: 
    //free(line);
    at_response_free(p_response);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return;
}

void request_oem_hook_raw(void *data, size_t datalen, RIL_Token t)
{
	RIL_onRequestComplete(t, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
	return;
}

void request_oem_hook_string(void *data, size_t datalen, RIL_Token t)
{
    //begin to modify by h81003427 for sar feature 20120414
    /* BEGIN DTS2012022004973  lkf52201 2012-02-21 modified */
    int i = 0;
    int err = 0;
    const char **cur = NULL;
    ATResponse *p_response = NULL;
    ATLine *p_cur = NULL;
    char *response[256] = {0};
    static int sarctrl_thread_created = 0;

    /* Only take the first string in the array for now */
    cur = (const char **)data;
    
    ALOGD(">>>>>>>>>>>>the oem_hook_string is:%s",*cur);
	//Begin to be modified by c00221986 to delete Sar_feature
	if(0){
		if(!strncmp(*cur,STRING_SAR_CREATE_PTHREAD,strlen(STRING_SAR_CREATE_PTHREAD)))
	    {
	        pthread_attr_t attr_sar;
	        pthread_attr_init (&attr_sar);
	        pthread_attr_setdetachstate(&attr_sar, PTHREAD_CREATE_DETACHED);
	        if (!sarctrl_thread_created)
	        {
	            if (!pthread_create(&s_tid_processSarCtrlNotification, &attr_sar,processSarCtrlNotification,NULL))
	            {
	                sarctrl_thread_created = 1;
	                RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
					sarfeature = 1;	// Added by x84000798 2012-05-29 DTS2012060806513
	                return ;
	            }
	            else
	            {
	                RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	                return ;
	            }
	        }
	    }
	    //end to modify by h81003427 for sar feature 20120414
	}
    //End being modified by c00221986 to delete Sar_feature
    err = at_send_command_oem_string(*cur, &p_response);

    if (err < 0 || 0 == p_response->success)
    {
	    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    else /* answer with OK or ERROR without intermediate responses for now */
    {
	    for (p_cur = p_response->p_intermediates, i = 0; NULL != p_cur; 
	         p_cur = p_cur->p_next, ++i)
	    {
	        asprintf(&response[i], "%s", p_cur->line);
	    }
	    // 保存AT命令执行状态,如OK,ERROR等
	    asprintf(&response[i++], "%s", p_response->finalResponse);
	    
	    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(char *) * i);
	    while(--i >= 0)
	    {
	        free(response[i]);
	    }
    }
    
    at_response_free(p_response);
    /* END   DTS2012022004973  lkf52201 2012-02-21 modified */
    return;
}







//modified by lkf52201 20110826 begin for DTS2011081800127
/* 全局变量s_band_mode 用于存放频带值，其数组下标对应频带类型。
  * 频带类型的取值范围是0-17，所以下面数组的大小设置18.
  * s_band_mode 在request_query_available_band_mode() 函数中被赋值；
  * 在request_set_band_mode() 函数中用于设置频带模式。
  */
#define  MODE_KINDS  18
static char s_band_mode[MODE_KINDS][16];
//modified by lkf52201 20110826 end for DTS2011081800127


/**
 * RIL_REQUEST_SET_BAND_MODE
 *
 * Assign a specified band for RF configuration.
 *
 * "data" is int *
 * ((int *)data)[0] is == 0 for "unspecified" (selected by baseband automatically)
 * ((int *)data)[0] is == 1 for "EURO band" (GSM-900 / DCS-1800 / WCDMA-IMT-2000)
 * ((int *)data)[0] is == 2 for "US band" (GSM-850 / PCS-1900 / WCDMA-850 / WCDMA-PCS-1900)
 * ((int *)data)[0] is == 3 for "JPN band" (WCDMA-800 / WCDMA-IMT-2000)
 * ((int *)data)[0] is == 4 for "AUS band" (GSM-900 / DCS-1800 / WCDMA-850 / WCDMA-IMT-2000)
 * ((int *)data)[0] is == 5 for "AUS band 2" (GSM-900 / DCS-1800 / WCDMA-850)
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */

/*need modify for different products, this request should not be used*/
static void request_set_band_mode(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err, ret = SIM_ABSENT;
    const int *oper;
    oper = (const int *)data; 
    char bands_preferred[16]={0};
    char*  cmd = NULL;

    if(RIL_DEBUG) ALOGD( "syscfg =  band: %d", *oper);

    //modified by lkf52201 20110826 begin for DTS2011081800127
    strncpy (bands_preferred, s_band_mode[*oper], 16);
    //modified by lkf52201 20110826 end for DTS2011081800127

    // BEGIN DTS2011092800261 wkf32792 20110930 added 
    if (0 == s_band_mode[*oper][0])
    	goto error;
    // END DTS2011092800261 wkf32792 20110930 added 
    
    /*acqorder:w prefer 
    *  band : no change
    */
    //asprintf(&cmd, "AT^SYSCFG=2,3,%s,1,2",bands_preferred);
    asprintf(&cmd, "AT^SYSCFG=16,3,%s,2,4",bands_preferred);//Modify by fKF34305 for no change except bands_preferred 20110829
    err = at_send_command(cmd, &p_response); 
    free(cmd);
	if( err<0 )  goto error;
		
    /*need add more CME error type need modify*/
    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
	 	case CME_SIM_NOT_VALID:
            if(RIL_DEBUG) ALOGD( "syscfg= error 1: %d", err);
            ret = SIM_ABSENT;
            goto error;

        default:
            if(RIL_DEBUG) ALOGD( "syscfg= error 2: %d", err);
            ret = SIM_NOT_READY;
            goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);   
    return;
    
error:
    if(RIL_DEBUG) ALOGE( "%s: Format error in this AT response %d", __FUNCTION__,ret);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    return;
}


/***************************************************
 Function:  request_radio_power
 Description:  
    Get radio state.
 Calls: 
    at_send_command_singleline
 Called By:
    initialize_callback
    request_radio_power
 Input:
    none
 Output:
    none
 Return:
    1 if on, 0 if off, and -1 on error
 Others:
    none
**************************************************/
int is_radio_on()
{
    ATResponse *p_response = NULL;
    int err;
    char *line;
    char ret;

    err = at_send_command_singleline("AT+CFUN?", "+CFUN:", &p_response);

    if (err < 0 || p_response->success == 0) {
        // assume radio is off
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextbool(&line, &ret);
    if (err < 0) goto error;

    at_response_free(p_response);

    return (int)ret;

error:

    at_response_free(p_response);
    return -1;
}
  /*begin  add  by lifei 2010.11.10*/
  /***************************************************
 Function:  request_query_available_band_mode
 Description:  
    Handle the request about dev. 
 Calls: 
    null
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
void request_query_available_band_mode( RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err, ret, availableOptNumber,commas=0;
    char *line, *p, *status;
    int band[20] ,i=0;

    memset(s_band_mode,0,sizeof(s_band_mode));
	
    err = at_send_command_singleline("AT^SYSCFG=?", "^SYSCFG:", &p_response);
    if( err < 0 )  
  		goto error;
     /*need add more CME error type need modify*/
    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
	    case CME_SIM_NOT_VALID:
            if(RIL_DEBUG) ALOGD( "SYSCFG=? error 1: %d", err);
            ret = SIM_ABSENT;
            goto error;

        default:
            if(RIL_DEBUG) ALOGD( "SYSCFG=? error 2: %d", err);
            ret = SIM_NOT_READY;
            goto error;    
    }
	
    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;
    if(RIL_DEBUG) ALOGD( "  line1  is %s", line);
    for ( ;commas<5;line++) 
    {
        if (*line== ',') commas++;
    } 
    commas = 0;
    for (p = line ; *p != '\0' ;p++) 
    {
        if (*p == ',') commas++;
    }

    availableOptNumber = (commas - 1)/2;   /*be careful*/
    band[0] = availableOptNumber + 1;

    for(i=0;i<availableOptNumber && i<19;i++)
    {
		line++;
		line++;

		err = at_tok_nextstr(&line, &status);

		if (err < 0) {
			if(RIL_DEBUG) ALOGD( "SYSCFG=? error 3: %d  band  is %s", err,status);
			goto error;
		}
		if(RIL_DEBUG) ALOGD( "  status  is %s", status);
	    //modified by hkf39947  20120109 begin for DTS2011122904739 	
        //modified by wkf32792 20110625 begin for DTS2011062003717
		//if( 0 == strcasecmp(status,"3fffffff") )            //selected by baseband automatically
			//band[i+1] = 0;
		//else if( 0 == strcasecmp(status,"680380"))  
		if(( 0 == strcasecmp(status,"3fffffff") )  || ( 0 == strcasecmp(status,"680380"))) 
		    band[i+1] = 0; 
		/* 1 for "EURO band" */
		else if( 0 == strcasecmp(status, "80"))     //GSM1800
			band[i+1] = 1;	
		else if( (0 == strcasecmp(status, "300")) || (0 == strcasecmp(status, "100")) || (0 == strcasecmp(status, "200")))            //GSM900
			band[i+1] = 1;	
		else if( 0 == strcasecmp(status, "2000000000000"))         //WCDMA900
		    band[i+1] = 1;
		else if( 0 == strcasecmp(status, "400000"))         //WCDMA2100
		    band[i+1] = 1;	
		else if( 0 == strcasecmp(status, "380") || (0 == strcasecmp(status, "180")) || (0 == strcasecmp(status, "280")))            //GSM900/GSM1800
			band[i+1] = 1;
		else if( 0 == strcasecmp(status, "2000000000080"))         //GSM1800/WCDMA900
		    band[i+1] = 1;
		else if( 0 == strcasecmp(status, "400080"))         //GSM1800/WCDMA2100
		    band[i+1] = 1;
		else if(( 0 == strcasecmp(status, "2000000000100")) || ( 0 == strcasecmp(status, "2000000000200"))|| ( 0 == strcasecmp(status, "2000000000300")))         //GSM900/WCDMA900
		   band[i+1] = 1;
		else if(( 0 == strcasecmp(status, "400100")) || ( 0 == strcasecmp(status, "400200"))|| ( 0 == strcasecmp(status, "400300")))         //GSM900/WCDMA2100
		   band[i+1] = 1;
              else if( 0 == strcasecmp(status, "2000000400000"))         //WCDMA900/WCDMA2100
		    band[i+1] = 1;
		else if(( 0 == strcasecmp(status, "2000000000180")) ||( 0 == strcasecmp(status, "2000000000280")) ||( 0 == strcasecmp(status, "2000000000380")))         //GSM900/GSM1800/WCDMA900
		    band[i+1] = 1;	
		else if(( 0 == strcasecmp(status, "400180")) ||( 0 == strcasecmp(status, "400280")) ||( 0 == strcasecmp(status, "400380")))         //GSM900/GSM1800/WCDMA2100
		    band[i+1] = 1;	
		
		else if (( 0 == strcasecmp(status, "2000000400180")) || ( 0 == strcasecmp(status, "2000000400280")) || ( 0 == strcasecmp(status, "2000000400380")))  //GSM900/GSM1800/WCDMA900/WCDMA2100
		    band[i+1] = 1;	
			
		/* 2 for "US band" */		
		else if( 0 == strcasecmp(status,"280000"))          //GSM850/GSM1900(GSM PCS)
		    band[i+1] = 2; 	  
		else if( 0 == strcasecmp(status,"4280000"))         //GSM850/GSM1900/WCDMA850
		    band[i+1] = 2; 	  
		else if( 0 == strcasecmp(status,"a80000"))          //GSM850/GSM1900/WCDMA-PCS-1900
		    band[i+1] = 2; 	 
		else if( 0 == strcasecmp(status,"4a80000"))         //GSM850/GSM1900/WCDMA850/WCDMA1900
		    band[i+1] = 2; 	 
		else if(( 0 == strcasecmp(status,"6a80000")) || ( 0 == strcasecmp(status,"4000004a80000")))         //GSM850/GSM1900/WCDMA850/AWS/WCDMA1900
		    band[i+1] = 2; 	 
		/* 3 for "JPN band" */
		else if( 0 == strcasecmp(status,"8400000"))         //WCDMA800/WCDMA2100
		    band[i+1] = 3; 
		else if(( 0 == strcasecmp(status,"4000000000000")) || ( 0 == strcasecmp(status,"2000000")))   //WCDMA1700、AWS
			band[i+1] = 3; 
		/* 4 for "AUS band" */
		else if(( 0 == strcasecmp(status,"4400180")) || ( 0 == strcasecmp(status,"4400280")) || ( 0 == strcasecmp(status,"4400380")))         //GSM900/GSM1800/WCDMA850/WCDMA2100
		    band[i+1] = 4; 
		/* 5 for "AUS band 2" */
		else if(( 0 == strcasecmp(status,"4000180")) || ( 0 == strcasecmp(status,"4000280")) || ( 0 == strcasecmp(status,"4000380")))         //GSM900/GSM1800/WCDMA850
		    band[i+1] = 5; 	
		/* CDMA band */
		else if( 0 == strcasecmp(status,"1"))               //for "Cellular (800-MHz Band)"
		    band[i+1] = 6; 
		else if( 0 == strcasecmp(status,"2"))               //for "Cellular (800-MHz Band)"
		    band[i+1] = 6; 
		else if( 0 == strcasecmp(status,"4"))               //for "PCS (1900-MHz Band)"
		    band[i+1] = 7; 		
		else if( 0 == strcasecmp(status,"10"))              //Band Class 3 (JTACS Band)
		    band[i+1] = 8; 	 
		else if( 0 == strcasecmp(status,"20"))              //Band Class 4 (Korean PCS Band)
		    band[i+1] = 9; 	
		else if( 0 == strcasecmp(status,"40"))              //Band Class 5 (450-MHz Band)
		    band[i+1] = 10; 	 
		else if( 0 == strcasecmp(status,"400"))             //Band Class 6 (2-GMHz IMT2000 Band)
		    band[i+1] = 11; 	 
		else if( 0 == strcasecmp(status,"800"))             //Band Class 7 (Upper 700-MHz Band)
		    band[i+1] = 12; 	 
		else if( 0 == strcasecmp(status,"1000"))            //Band Class 8 (1800-MHz Band)
		    band[i+1] = 13; 	
		else if( 0 == strcasecmp(status,"2000"))            //Band Class 9 (900-MHz Band)
		    band[i+1] = 14; 	 
		else if( 0 == strcasecmp(status,"4000"))            //Band Class 10 (Secondary 800-MHz Band)
		    band[i+1] = 15; 	 
		else if( 0 == strcasecmp(status,"8000"))            //Band Class 11 (400-MHz European PAMR Band)
		    band[i+1] = 16; 	 
		else if( 0 == strcasecmp(status,"80000000"))        //Band Class 15 (AWS Band)
		    band[i+1] = 17; 	
		else band[i+1] = 0;
		
		//modified by wkf32792 20110625 end for DTS2011062003717
		//modified by hkf39947  20120109 end for DTS2011122904739 	
		//modified by lkf52201 20110826 begin for DTS2011081800127
		if(RIL_DEBUG)ALOGD("request_query_available_band_mode : %s", status);
		 strncpy(s_band_mode[band[i+1]], status, sizeof(s_band_mode[band[i+1]]) - 1);
		*(s_band_mode[band[i+1]] + sizeof(s_band_mode[band[i+1]])-1) = '\0';
		//modified by lkf52201 20110826 end for DTS2011081800127
		do
		{
		    line++;
		}while(*line !=  ',');
     }

     RIL_onRequestComplete(t, RIL_E_SUCCESS, band, sizeof(band));
     at_response_free(p_response);
     return;  
error:
    /*temp return not support, need modify!!!*/
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);    
  	return;
}

static void request_query_device_identity(RIL_Token t)
{
    int err;
    char *line;
    char *response[4];
    char *temp_line = NULL;     // DTS2012010602711 lkf52201 2012-01-07 added

    memset(response, 0, sizeof(response));
    ATResponse *p_response = NULL;

	if(RIL_DEBUG)ALOGD("request_query_device_identity : %d", g_module_type);
    /* BEGIN DTS2012010602711 lkf52201 2012-01-07 modified */
    if (NETWORK_CDMA == g_module_type)
    {
    	if(RIL_DEBUG)ALOGD("request_query_device_identity NETWORK_CDMA");
        /*get ESN*/
        err = at_send_command_singleline("AT+GSN", "+GSN:", &p_response);
        if (err < 0 || p_response->success == 0)
        {
            goto error;   
        }
        
        line = p_response->p_intermediates->line;
        err = at_tok_start (&line);

        if (err < 0) 
        {
            goto error;   
        }
        
        /*ESN is 8 byte string,HEX string, "32AB3C13" is 0x32AB3C13*/
        /* BEGIN DTS2011123101074 lkf52201 2011-12-31 added */
        err = at_tok_nextstr(&line, &temp_line);
        if (err < 0) {
            goto error;   
        }
        response[2] = (char*)alloca(strlen(temp_line) + 1);
        strcpy(response[2], temp_line);
        
        at_response_free(p_response);
        p_response = NULL;
        /* END   DTS2011123101074 lkf52201 2011-12-31 added */

        /*get MEID*/
        err = at_send_command_singleline("AT^MEID", "^MEID:", &p_response);
        if (err < 0 || p_response->success == 0)
        {
            goto error;   
        }
        
        line = p_response->p_intermediates->line;
        err = at_tok_start (&line);

        if (err < 0) 
        {
            goto error;   
        }

        /*MEID is 14 byte string,HEX string, "32AB3C134D3E56" is 0x32AB3C134D3E56*/
        err = at_tok_nextstr(&line, &(response[3]));

        if (err < 0) {
            goto error;   
        }

        response[0] = NULL;     /* 当前是CDMA模块,将GSM模式下的返回值设为NULL */
        response[1] = NULL;
    }
    else                        // 非CDMA模块
    {
        /* get IMEI */
        if(RIL_DEBUG)ALOGD("request_query_device_identity ! NETWORK_CDMA");
        err = at_send_command_numeric("AT+CGSN", &p_response);
        if (err < 0 || p_response->success == 0)
        {
            goto error;
        }
        
        response[0] = p_response->p_intermediates->line;
        response[1] = response[0];  /* SVN not support now */
        response[2] = NULL;         /* 当前是非CDMA模块,将CDMA模式下的返回值设为NULL */
        response[3] = NULL;
    }
    /* END DTS2012010602711 lkf52201 2012-01-07 modified */

    #ifdef RIL_DEBUG 
    if(RIL_DEBUG)ALOGD( "this is request Device Identity's success\n");
    #endif
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    at_response_free(p_response);
    return;
    
error:
    #ifdef RIL_DEBUG 
        ALOGE( "this is request Device Identity err \n");  
    #endif
    response[0] = NULL;
    response[1] = NULL;
    response[2] = NULL;
    response[3] = NULL;
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0); 
    at_response_free(p_response);
    return ;
}


static void request_query_cdma_subscription(RIL_Token t)
{
    int err;
    char *line;
    char *response[5];
    char *temp_line = NULL;      // DTS2011123101074 lkf52201 2011-12-31 added
    memset(response, 0, 5*sizeof(char*));
    ATResponse *p_response = NULL;

	if( ((MODEM_VENDOR_ZTE == g_modem_vendor) || (MODEM_VENDOR_SCV == g_modem_vendor) || (MODEM_VENDOR_ALCATEL == g_modem_vendor))
	//if( ((MODEM_VENDOR_ZTE == g_modem_vendor) || (MODEM_VENDOR_SCV == g_modem_vendor))
		&&(NETWORK_CDMA == g_module_type))
	{
		char *tmpresponse[5] = {"","13840","15","6301529652","0"};
		RIL_onRequestComplete(t, RIL_E_SUCCESS, tmpresponse, 5*sizeof(char*));
		return;
	}
	
    /*get MDN*/
    err = at_send_command_singleline("AT^MDN", "^MDN:", &p_response);
    if (err < 0 || p_response->success == 0)
    { 
        goto error;
    }
    
    line = p_response->p_intermediates->line;
    err = at_tok_start (&line);
    if (err < 0) 
    {
        goto error;
    }
    
    err = at_tok_nextstr(&line, &temp_line);
    if (err < 0) 
    {
        goto error;   
    }
    response[0] = (char*)alloca(strlen(temp_line) + 1);
    strcpy(response[0], temp_line);
    
    at_response_free(p_response);  // DTS2011123101074 lkf52201 2011-12-31 added
    p_response = NULL;
	
    /* Begin add by fKF34305 20111031 for CDMA NID & SID*/
    /*get NID & SID*/
    err = at_send_command_singleline("AT^CURRSID", "^CURRSID:", &p_response);
    if (err < 0 || p_response->success == 0)
    { 
        //goto error;//by alic
        response[1] = (char*)alloca(2);
        strcpy(response[1], "0");
        response[2] = (char*)alloca(2);
        strcpy(response[2], "0");
    }else{
    
    line = p_response->p_intermediates->line;
    err = at_tok_start (&line);
    if (err < 0) 
    {
        goto error;
    }
    
    err = at_tok_nextstr(&line, &temp_line);   
    if (err < 0) 
    {
        goto error;   
    }
    response[1] = (char*)alloca(strlen(temp_line) + 1);
    strcpy(response[1], temp_line);

    err = at_tok_nextstr(&line, &temp_line);   
    if (err < 0) 
    {
        goto error;   
    }
    response[2] = (char*)alloca(strlen(temp_line) + 1);
    strcpy(response[2], temp_line);
   }
    at_response_free(p_response);  // DTS2011123101074 lkf52201 2011-12-31 added
    p_response = NULL;

    /* End add by fKF34305 20111031 for CDMA NID & SID*/

    /*get MIN*/
    err = at_send_command_numeric("AT+CIMI", &p_response);
    if (err < 0 || p_response->success == 0)
    {
        goto error;
    } 
    if(strlen(line = p_response->p_intermediates->line))
    {
    	if((response[3] = (char *)alloca(sizeof(char)*11)) != NULL)
	    strcpy(response[3],line+strlen(line)-10);
	else
	    goto error;
    }
    at_response_free(p_response);  // DTS2011123101074 lkf52201 2011-12-31 added
    p_response = NULL;
    
    /*get PRL Ver*/
    err = at_send_command_singleline("AT^PRLVER?", "^PRLVER:", &p_response);
    if (err < 0 || p_response->success == 0)
    { 
        goto error;
    }

    line = p_response->p_intermediates->line;
    err = at_tok_start (&line);
    if (err < 0) 
    {
        goto error;
    }
    
    err = at_tok_nextstr(&line, &(response[4]));
    if (err < 0) 
    {
        goto error;   
    }
    // p_response = NULL; // DTS2011123101074 lkf52201 2011-12-31 deleted

    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, 5*sizeof(char*));
    at_response_free(p_response);    
    return;
    
error:
    ALOGE("requestCDMASubscription error");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    return;
}


static void request_cdma_set_subscription(RIL_Token t)
{
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, sizeof(char *));
    return;
}

/***************************************************
 * RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE
 *
 * Request to set the roaming preferences in CDMA
 *
 * "data" is int *
 * ((int *)data)[0] is == 0 for Home Networks only, as defined in PRL
 * ((int *)data)[0] is == 1 for Roaming on Affiliated networks, as defined in PRL
 * ((int *)data)[0] is == 2 for Roaming on Any Network, as defined in the PRL
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
**************************************************/
static void request_cdma_set_roaming(void *data, size_t datalen, RIL_Token t)
{  
    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    return;
}

/* BEGIN lkf52201 2011-12-26 将以下两个函数从ril-ss.c 移到 ril-dev.c */
static void request_query_facility_lock(void *data, size_t datalen, RIL_Token t)
{
	int result = 0;
	ATLine *p_cur;
	int err, status, classNo;
	ATResponse *p_response;
	char *facility;
	char *service_bit;
	char *cmd;

	facility = ((char **)data)[0];
	service_bit = ((char**)data)[2];

	if(CDMA_NV_TYPE == g_cdma_subtype)
	{
		sim_sc = 1;
		RIL_onRequestComplete(t, RIL_E_SUCCESS,&sim_sc , sizeof(result));
		return;
	}

	asprintf(&cmd,"AT+CLCK=\"%s\",2",facility); 
	err = at_send_command_multiline(cmd, "+CLCK:",&p_response);   
	if (err < 0 || p_response->success == 0)  goto error;
	
	for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next ) 
	{
		char *line = p_cur->line;
		if(RIL_DEBUG) ALOGE( "1 line : %s",line);
		err = at_tok_start(&line);
		if (err < 0) goto error;

		err = at_tok_nextint(&line, &status);
		if(RIL_DEBUG) ALOGE( "2 line : %s",line);
		if (err < 0) goto error;

		if (!at_tok_hasmore(&line)) 
		{
			if(status == 1)
				result += 7;
			continue;
		}

		err = at_tok_nextint(&line, &classNo);
		if(RIL_DEBUG) ALOGE( "3 line : %s",line);
		if (err < 0) goto error;

		if(status == 1)
			result += classNo;
	}
	
	if(!strncmp(facility,"SC",2))
	{
		sim_sc = result;
	}  
	RIL_onRequestComplete(t, RIL_E_SUCCESS,&sim_sc , sizeof(result));
	return ;

error:
	if(RIL_DEBUG) ALOGE( "ERROR \n");
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ;	
}
/************************************************
 Function:  request_set_facility_lock
 Description:  
    开启PIN码功能
 Calls: 
 Called By: hwril_request_ss
 Input:
 Output:
    none
 Return:
    none
 Others:
    Modified by z128629 2011-04-20 三次输入PIN码错误后，重新polling SIM卡状态
**************************************************/
static void request_set_facility_lock(void *data, size_t datalen, RIL_Token t,int request)
{
	char *cmd;
	char *tc_code;
	char *lock;
	char *pwssward;
	int  times = -1;
	ATResponse *p_response;
	int err;
	tc_code = ((char**)data)[0];
	lock = ((char**)data)[1];
	pwssward = ((char**)data)[2];
  /*BEGIN Added by lkf34826  DTS2011032203169 V3D01:增加返回剩余PIN码输入次数 */
	asprintf(&cmd,"AT+CLCK=\"%s\",%s,\"%s\"",tc_code,lock,pwssward); 
	err = at_send_command(cmd, &p_response);
	
	times = getNumRetries(request);
	if(RIL_DEBUG) ALOGD("remain times: %d ",times);
	if(err == 0)
	{
		if(p_response->success == 0)
		{
			RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, &times, sizeof(int*));
			if(times == 0)
		    {
		     	if(RIL_DEBUG) ALOGE("remain times: %d ",times);
		     	
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
		}
		else
		{
			RIL_onRequestComplete(t, RIL_E_SUCCESS,&times, sizeof(int*));
		}
	}
	else 
	{
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, &times, sizeof(int*));
	}
	/*END Added by lkf34826  DTS2011032203169 V3D01:增加返回剩余PIN码输入次数 */
	at_response_free(p_response);
	free(cmd);
	return ;
}
/* END lkf52201 2011-12-26 将以上两个函数从ril-ss.c 移到 ril-dev.c */

 /*begin  add  by lifei 2010.11.10*/
/***************************************************
 Function:  hwril_request_dev
 Description:  
    Handle the request about dev. 
 Calls: 
    request_radio_power
    request_get_imeisv
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
void  hwril_request_dev(int request, void *data, size_t datalen, RIL_Token token)
{    
    
    switch (request) 
    {
		case RIL_REQUEST_RADIO_POWER:
		{
			request_radio_power(data, datalen, token);
			break;
		}

		case RIL_REQUEST_GET_IMEI:
		{
			request_get_imei(data,datalen,token);
			break;
		}

		case RIL_REQUEST_GET_IMEISV:
		{   
			request_get_imeisv(data,datalen,token);
			break;
		}

		case RIL_REQUEST_BASEBAND_VERSION:
		{
			request_get_baseband_version(data,datalen,token);
			break;
		}
        
		case RIL_REQUEST_OEM_HOOK_RAW:
		{
			// echo back data
			request_oem_hook_raw(data,datalen,token);
			break;
		}
        
	        case RIL_REQUEST_OEM_HOOK_STRINGS:         
	        {
			request_oem_hook_string(data,datalen,token);
			break;
	        }
        
		case RIL_REQUEST_SET_BAND_MODE:
		{
			request_set_band_mode( data, datalen,token);
			break;
		}	
		
		case RIL_REQUEST_QUERY_AVAILABLE_BAND_MODE:
		{
			request_query_available_band_mode(token);
			break;
		}
		
		case RIL_REQUEST_DEVICE_IDENTITY:
		{
	        request_query_device_identity(token);
			break;
		}

		case RIL_REQUEST_CDMA_SUBSCRIPTION:
		{
			if(RIL_DEBUG) ALOGD( "RIL_REQUEST_CDMA_SUBSCRIPTION 2 \n");	
	        request_query_cdma_subscription(token);
			break;
		}
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3	     
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION_SOURCE: 
#else	
        case RIL_REQUEST_CDMA_SET_SUBSCRIPTION:
#endif
//end modified by wkf32792 for android 3.x 20110815
		{
	        request_cdma_set_subscription(token);
		  	break;
		}

		case RIL_REQUEST_CDMA_SET_ROAMING_PREFERENCE:
		{
	        request_cdma_set_roaming( data, datalen,token);
		  	break;
		}

		case RIL_REQUEST_CDMA_QUERY_ROAMING_PREFERENCE:
		{
			//add by hexiaokong 20111205 for DTS2011120206619 
			RIL_onRequestComplete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			//add by hexiaokong 20111205 for DTS2011120206619 
		  	break;
		}
		case RIL_REQUEST_QUERY_FACILITY_LOCK:
		{
			request_query_facility_lock(data,datalen,token);
			break;   
		}

		case RIL_REQUEST_SET_FACILITY_LOCK:
		{
			request_set_facility_lock(data, datalen, token, request);
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
 Function:  hwril_unsolicited_dev
 Description:  
    Handle the unsolicited about dev. 
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

void hwril_unsolicited_dev (const char *s)
{
    return; 
}    


