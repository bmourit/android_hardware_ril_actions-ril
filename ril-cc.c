/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-cc.c
 Author: liubing/00141886
 Version: V1.1
 Date: 2010/04/07
 Description:
         Handle request and unsolicited about curent call.
 Function List:
 History:
 <author>                      <time>                    <version>               <desc>
 liubing                      10/04/07                      1.0             
 fangyuchuang           10/04/28                      1.1  
*******************************************************/

#include <telephony/ril.h>
#include "huawei-ril.h"
#include "at_tok.h"
#include "misc.h"

// Max number of times we'll try to repoll when we think
// we have a AT+CLCC race condition
#define REPOLL_CALLS_COUNT_MAX 4
// Line index that was incoming or waiting at last poll, or -1 for none
static int s_incomingOrWaitingLine = -1;
// Number of times we've asked for a repoll of AT+CLCC
static int s_repollCallsCount = 0;
// Should we expect a call to be answered in the next CLCC?
static int s_expectAnswer = 0;

/* begin support DTMF y46886 20090507 */
// Current ative line index
static unsigned int s_curAtiveLine = 99;
extern Module_Type g_module_type ;
extern MODEM_Vendor g_modem_vendor;

#define DEFAULT_DTMF_BURST_LENGTH  150
#define DEFAULT_DTMF_BURST_INTERVAL 60
/* end support DTMF y46886 20090507 */
/* begin y46886 20090508 */

typedef enum {
	 CALL_END_LL_END = 100,
	 CALL_END_NETWORK_END = 104,
	 CALL_END_UNKNOWN = 0xffff
} RIL_CallEndCause;

int cc_lastCallFailCause =  CALL_END_UNKNOWN;
extern int g_network_indicator;
/*Begin to add by h81003427 20120526 for ps domain incoming packet call*/
extern pthread_cond_t s_cmd_cond;
int g_pspackage = 0;
int g_csd = 0;
/*End to add by h81003427 20120526 for ps domain incoming packet call*/
extern RIL_RadioState sState;
#define WORKAROUND_CLCCERROR   //add by fKF34305 20110408
static int p_chld=0;   //add by fKF34305 20110408
static void request_last_call_fail_cause(void *data, size_t datalen, RIL_Token token)
{
	int response[1];
	response[0] = cc_lastCallFailCause;

	if (response[0] == CALL_END_UNKNOWN) {
		response[0] = CALL_FAIL_ERROR_UNSPECIFIED;
	}
	RIL_onRequestComplete(token, RIL_E_SUCCESS, response, sizeof(response));
	return;
}

static int clcc_state_to_ril_state(int state, RIL_CallState *p_state)
{
	switch(state) {
		case 0: *p_state = RIL_CALL_ACTIVE;   return 0;
		case 1: *p_state = RIL_CALL_HOLDING;  return 0;
		case 2: *p_state = RIL_CALL_DIALING;  return 0;
		case 3: *p_state = RIL_CALL_ALERTING; return 0;
		case 4: *p_state = RIL_CALL_INCOMING; return 0;
		case 5: *p_state = RIL_CALL_WAITING;  return 0;
		default: return -1;
	}
}

/***************************************************
 Function:  call_from_clcc_line
 Description:  
    Lists the current call information.
 Calls: 
    at_tok_start
    at_tok_nextint
 Called By:
    request_get_current_calls
 Input:
    line    - pointer to hex string 
    p_call  - pointer to sturct RIL_Call
 Output:
    none
 Return:
    return 0 on success, otherwise return -1
 Others:
    none
**************************************************/
static int call_from_clcc_line(char *line, RIL_Call *p_call)
{
	int err;
	int state;
	int mode;

	err = at_tok_start(&line);
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &(p_call->index));
	if (err < 0) goto error;

	err = at_tok_nextbool(&line, &(p_call->isMT));
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &state);
	if (err < 0) goto error;

	err = clcc_state_to_ril_state(state, &(p_call->state));
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &mode);
	if (err < 0) goto error;

	p_call->isVoice = (mode == 0);

	err = at_tok_nextbool(&line, &(p_call->isMpty));
	if (err < 0) goto error;

	if (at_tok_hasmore(&line)) {
	err = at_tok_nextstr(&line, &(p_call->number));

	/* tolerate null here */
	if (err < 0) return 0;

	// Some lame implementations return strings
	// like "NOT AVAILABLE" in the CLCC line
	if (p_call->number != NULL
	&& 0 == (int)strspn(p_call->number, "+0123456789")
	) {
		p_call->number = NULL;
	}

	err = at_tok_nextint(&line, &p_call->toa);
	if (err < 0) goto error;
	}

	return 0;

error:
	if(RIL_DEBUG) ALOGE("invalid CLCC line\n");
	return -1;
}

/***************************************************
 Function:  parse_call_end_response
 Description:  
    Change cc_lastCallFailCause according to content of parsing line 
 Calls: 
    at_tok_start
    at_tok_nextint
 Called By:
    hwril_unsolicited_cc
 Input:
    line    - pointer to hex string 
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
static void parse_call_end_response(char * line)
{
	int err;
	int call_id;
	int call_duration;
	int end_status = -1; 
	int cc_cause = -1;

	err = at_tok_start(&line);
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &call_id);
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &call_duration);
	if (err < 0) goto error;

	err = at_tok_nextint(&line, &end_status);
	if (err < 0) goto error;

	if (at_tok_hasmore(&line)) {
		err = at_tok_nextint(&line, &cc_cause);
		if (err < 0) goto error; 
	}

	if (end_status == CALL_END_NETWORK_END || end_status == CALL_END_LL_END) {
		cc_lastCallFailCause = (cc_cause == -1) ? CALL_END_UNKNOWN : cc_cause;
	}else {
		cc_lastCallFailCause = (end_status == -1)? CALL_END_UNKNOWN : end_status;
	}
	return;

	error:
	cc_lastCallFailCause = CALL_END_UNKNOWN;
	return;	 
}

/***************************************************
 Function:  send_call_state_changed
 Description:  
    Unsolicited call state changed
 Calls: 
    RIL_onUnsolicitedResponse
 Called By:
    request_get_current_calls
 Input:
    param    - pointer to hex string 
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
static void send_call_state_changed(void *param)
{
	RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
}

/***************************************************
 Function:  request_get_current_calls
 Description:  
    reference ril.h
 Calls: 
    at_send_command_multiline
    RIL_requestTimedCallback
 Called By:
    hwril_request_cc
 Input:
    data    - pointer to void
    datalen - data len
    t       - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    Modeify by fangyuchuang/0014188
**************************************************/
static void request_get_current_calls(void *data, size_t datalen, RIL_Token token)
{
	int err;
	ATResponse *p_response;
	ATLine *p_cur;
	int countCalls;
	int countValidCalls;
	RIL_Call *p_calls;
	RIL_Call **pp_calls;
	int i;
	int needRepoll = 0;
	static int p_index = 0;

#ifdef WORKAROUND_ERRONEOUS_ANSWER
	int prevIncomingOrWaitingLine;

	prevIncomingOrWaitingLine = s_incomingOrWaitingLine;
	s_incomingOrWaitingLine = -1;
#endif /*WORKAROUND_ERRONEOUS_ANSWER*/
	//if((NETWORK_CDMA == g_module_type)
		//&&(MODEM_VENDOR_ZTE == g_modem_vendor))
	//{
		//RIL_requestTimedCallback (send_call_state_changed, NULL, &TIMEVAL_CALLSTATEPOLL);
		//return;
	//}
	
	err = at_send_command_multiline ("AT+CLCC", "+CLCC:", &p_response);

	/*Begin modify by fKF34305 20110403  DTS2011032905472*/
	if (err < 0 || p_response->success == 0)                                   
		goto error;  
  
	/*End modify by fKF34305 20110403  DTS2011032905472*/              

/* count the calls */
	for (countCalls = 0, p_cur = p_response->p_intermediates
	; p_cur != NULL
	; p_cur = p_cur->p_next
	) {
		countCalls++;
	}

/* yes, there's an array of pointers and then an array of structures */

	pp_calls = (RIL_Call **)alloca(countCalls * sizeof(RIL_Call *));
	p_calls = (RIL_Call *)alloca(countCalls * sizeof(RIL_Call));
	memset (p_calls, 0, countCalls * sizeof(RIL_Call));

/* init the pointer array */
	for(i = 0; i < countCalls ; i++) {
		pp_calls[i] = &(p_calls[i]);
	}

	for (countValidCalls = 0, p_cur = p_response->p_intermediates
	; p_cur != NULL
	; p_cur = p_cur->p_next
	) {
		err = call_from_clcc_line(p_cur->line, p_calls + countValidCalls);

		if (err != 0) {
			continue;
		}

		/*BEGIN add by fKF34305 规避单板下发CHLD=1之后首次查询呼叫状态出错*/
		#ifdef WORKAROUND_CLCCERROR
		if(RIL_DEBUG)ALOGD("state=%d,conut=%d",p_calls[countValidCalls].state,p_index);
		if((1 == countCalls) && (p_calls[countValidCalls].state == RIL_CALL_HOLDING)
		&& (p_index < REPOLL_CALLS_COUNT_MAX) && p_chld)
		{
			ALOGD("error!");
			p_index++;
			goto error;	
		}
		p_index = 0;
		p_chld=0;
		#endif
		/*END add by fKF34305 规避单板下发CHLD=1之后首次查询呼叫状态出错*/
	#ifdef WORKAROUND_ERRONEOUS_ANSWER
		if (p_calls[countValidCalls].state == RIL_CALL_INCOMING
		|| p_calls[countValidCalls].state == RIL_CALL_WAITING
		) {
			s_incomingOrWaitingLine = p_calls[countValidCalls].index;
		}
	#endif /*WORKAROUND_ERRONEOUS_ANSWER*/

		if (p_calls[countValidCalls].state != RIL_CALL_ACTIVE
		&& p_calls[countValidCalls].state != RIL_CALL_HOLDING
		) {
			needRepoll = 1;
		}

		if (p_calls[countValidCalls].state == RIL_CALL_ACTIVE)
		{
			if (p_calls[countValidCalls].state != s_curAtiveLine)
			{
				s_curAtiveLine = p_calls[countValidCalls].index;
				if(RIL_DEBUG) ALOGD( "update current ative call,  index = %d", s_curAtiveLine);
			}
		}
		countValidCalls++;
	}

#ifdef WORKAROUND_ERRONEOUS_ANSWER
// Basically:
// A call was incoming or waiting
// Now it's marked as active
// But we never answered it
//
// This is probably a bug, and the call will probably
// disappear from the call list in the next poll
	if (prevIncomingOrWaitingLine >= 0
	&& s_incomingOrWaitingLine < 0
	&& s_expectAnswer == 0
	) {
		for (i = 0; i < countValidCalls ; i++) 
		{
			if (p_calls[i].index == prevIncomingOrWaitingLine
			&& p_calls[i].state == RIL_CALL_ACTIVE
			&& s_repollCallsCount < REPOLL_CALLS_COUNT_MAX
			) {
				if(RIL_DEBUG) ALOGD( 
				"Hit WORKAROUND_ERRONOUS_ANSWER case."
				" Repoll count: %d\n", s_repollCallsCount);
				s_repollCallsCount++;
				goto error;
			}
		}
	}

	s_expectAnswer = 0;
	s_repollCallsCount = 0;
	#endif /*WORKAROUND_ERRONEOUS_ANSWER*/

	RIL_onRequestComplete(token, RIL_E_SUCCESS, pp_calls,
	countValidCalls * sizeof (RIL_Call *));

	at_response_free(p_response);

#ifdef POLL_CALL_STATE
	if (countValidCalls) {  // We don't seem to get a "NO CARRIER" message from
	// smd, so we're forced to poll until the call ends.
#else
	if (needRepoll) {
#endif
		RIL_requestTimedCallback (send_call_state_changed, NULL, &TIMEVAL_CALLSTATEPOLL);          
		//RIL_requestTimedCallback (Callringing, NULL, &TIMEVAL_CALLSTATEPOLL);           
	}
	return;
	
error:
	if(RIL_DEBUG) ALOGE( "get current call error!\n");
	//RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);   
	RIL_onRequestComplete(token, RIL_E_RADIO_NOT_AVAILABLE, NULL, 0);
	at_response_free(p_response);
	return ;                                                                      
}



static void request_dial(void *data, size_t datalen, RIL_Token token)
{
	RIL_Dial *p_dial;
	char *cmd;
	const char *clir;
	int ret;

	p_dial = (RIL_Dial *)data;

	switch (p_dial->clir) {
		case 1: clir = "I"; break;  /*invocation*/
		case 2: clir = "i"; break;  /*suppression*/
		
		default:
		case 0: clir = ""; break;   /*subscription default*/
	}

	if(NETWORK_CDMA == g_module_type)
	{
		asprintf(&cmd, "AT+CDV%s", p_dial->address);
	}
	else
	{
		asprintf(&cmd, "ATD%s%s;", p_dial->address, clir);
	}
	
	ret = at_send_command(cmd, NULL);
	free(cmd);

	/* success or failure is ignored by the upper layer here.
	it will call GET_CURRENT_CALLS and determine success that way */
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
}

static void request_hangup(void *data, size_t datalen, RIL_Token token)
{
	int *p_line;

	int ret;
	char *cmd;

	p_line = (int *)data;

	// 3GPP 22.030 6.5.5
	// "Releases a specific active call X"
    /*BEGIN Modify by fKF34305 20110420 问题单: DTS2011032805555 由于 MU509不支持CHLD=1X挂断未激活的通话
	，所以用CHUP代替。*/
	#if 0
		asprintf(&cmd, "AT+CHLD=1%d", p_line[0]);
	#else
		if(NETWORK_CDMA ==g_module_type) 
		{
			asprintf(&cmd, "AT+CHV");
		}
		else
		{
			asprintf(&cmd, "AT+CHUP");
		}
		
	#endif
	/*END Modify by fKF34305 20110420 问题单: DTS2011032805555 */
	ret = at_send_command(cmd, NULL);

	free(cmd);

	/* success or failure is ignored by the upper layer here.
	it will call GET_CURRENT_CALLS and determine success that way */
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
}


/***************************************************
 Function:  request_hangup_waiting_or_backgroud
 Description:  
    reference ril.h
 Calls: 
    at_send_command_multiline
    RIL_requestTimedCallback
 Called By:
    hwril_request_cc
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    ADD by fanglei/KF34305
**************************************************/
static void request_hangup_waiting_or_backgroud(void *data, size_t datalen, RIL_Token token)
{
	ATResponse   *p_response = NULL;
	int  err;
	char *cmd = NULL;
	// 3GPP 22.030 6.5.5
	// "Releases all held calls or sets User Determined User Busy
	//  (UDUB) for a waiting call."
	

	if(NETWORK_CDMA ==g_module_type) 
	{
		asprintf(&cmd, "AT+CHV");
	}
	else
	{
		asprintf(&cmd, "AT+CHLD=0");
	}
	
	err=at_send_command (cmd, &p_response); 
	free(cmd); 
	if (err < 0 || p_response->success == 0)
	{                                
    	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	else
	{
		RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);  
	}
	
	at_response_free(p_response);
	return; 
}


/***************************************************
 Function:  request_answer
 Description:  
    reference ril.h
 Calls: 
    at_send_command_multiline
    RIL_requestTimedCallback
 Called By:
    hwril_request_cc
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    ADD by fanglei/KF34305
**************************************************/
static void request_answer(void *data, size_t datalen, RIL_Token token)
{
	at_send_command("ATA", NULL);

	#ifdef WORKAROUND_ERRONEOUS_ANSWER
		s_expectAnswer = 1;
	#endif /* WORKAROUND_ERRONEOUS_ANSWER */

	/* success or failure is ignored by the upper layer here.
	it will call GET_CURRENT_CALLS and determine success that way */
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
}

/***************************************************
 Function:  request_hangup_foreground_resume_backgroud
 Description:  
    reference ril.h
 Calls: 
    at_send_command_multiline
    RIL_requestTimedCallback
 Called By:
    hwril_request_cc
 Input:
    data    - pointer to void
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    ADD by fanglei/KF34305
**************************************************/
static void request_hangup_foreground_resume_backgroud(void *data, size_t datalen, RIL_Token token)
{
	ATResponse   *p_response = NULL;
	int  err;
	char *cmd = NULL;

	/*BEGIN add by fKF34305 只在下发CHLD=1 之后才多查几次呼叫状态*/
	#ifdef WORKAROUND_CLCCERROR
		p_chld=1;
	#endif
	/*END add by fKF34305 只在下发CHLD=1 之后才多查几次呼叫状态*/
	

    if((RADIO_STATE_SIM_READY==sState)||((RADIO_STATE_RUIM_READY==sState))||((RADIO_STATE_NV_READY==sState))) 
	{
		if(NETWORK_CDMA == g_module_type) 
		{
			asprintf(&cmd, "AT+CHV");
		}
		else
		{
			asprintf(&cmd, "AT+CHLD=1");
		}
	}else
	{
		//modify by KF34305 20111203 for   DTS2011111902320 
		if(NETWORK_CDMA == g_module_type) 
		{
			asprintf(&cmd, "AT+CHV");
		}
		else
		{
			asprintf(&cmd, "AT+CHUP");
		}
	}
	//modify by KF34305 20111203 for   DTS2011111902320 
	
	err=at_send_command (cmd, &p_response); 
	free(cmd); 
	if (err < 0 || p_response->success == 0)
	{                                
    	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	else
	{
		RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);  
	}
	
	at_response_free(p_response);
	return; 
}



/***************************************************
 Function:  request_switch_waiting_or_holding_and_active
 * RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE
 *
 * Switch waiting or holding call and active call (like AT+CHLD=2)
 *
 * State transitions should be is follows:
 *
 * If call 1 is waiting and call 2 is active, then if this re
 *
 *   BEFORE                               AFTER
 * Call 1   Call 2                 Call 1       Call 2
 * ACTIVE   HOLDING                HOLDING     ACTIVE
 * ACTIVE   WAITING                HOLDING     ACTIVE
 * HOLDING  WAITING                HOLDING     ACTIVE
 * ACTIVE   IDLE                   HOLDING     IDLE
 * IDLE     IDLE                   IDLE        IDLE
 *
 * "data" is NULL
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
**************************************************/
static void request_switch_waiting_or_holding_and_active(void *data, size_t datalen, RIL_Token token)
{
	ATResponse   *p_response = NULL;
	int  err;
	char *cmd = NULL;
	// 3GPP 22.030 6.5.5
	// "Releases all held calls or sets User Determined User Busy
	//  (UDUB) for a waiting call."
	
	#ifdef WORKAROUND_ERRONEOUS_ANSWER
		s_expectAnswer = 1;
	#endif /* WORKAROUND_ERRONEOUS_ANSWER */

	asprintf(&cmd, "AT+CHLD=2");
	err=at_send_command (cmd, &p_response); 
	free(cmd); 
	if (err < 0 || p_response->success == 0)
	{                                
    	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	else
	{
		RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);  
	}
	
	at_response_free(p_response);
	return; 
}


/***************************************************
 Function:  request_dtmf
 * RIL_REQUEST_DTMF
 *
 * Send a DTMF tone
 *
 * If the implementation is currently playing a tone requested via
 * RIL_REQUEST_DTMF_START, that tone should be cancelled and the new tone
 * should be played instead
 *
 * "data" is a char * containing a single character with one of 12 values: 0-9,*,#
 * "response" is NULL
 *
 * FIXME should this block/mute microphone?
 * How does this interact with local DTMF feedback?
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 * See also: RIL_REQUEST_DTMF_STOP, RIL_REQUEST_DTMF_START

 
    ADD by fanglei/KF34305
**************************************************/
static void request_dtmf(void *data, size_t datalen, RIL_Token token)
{
	char c = ((char *)data)[0];
	char *cmd;
	int err;
	ATResponse   *p_response = NULL;  //Added by c00221986 for DTS2013040706898
	/* Begin modified by x84000798 DTS2012060708964 2012-05-30 */
	//Begin to be modified by c00221986 for DTS2013040706898
	int call_id = s_curAtiveLine;
	//int burst_length = DEFAULT_DTMF_BURST_LENGTH;
	//int burst_intervel = DEFAULT_DTMF_BURST_INTERVAL;
	if(RIL_DEBUG)ALOGD("request dtmf >> char=%c", c); 

	//asprintf(&cmd,"AT^DTMF=%d,%c,%d,%d", call_id, c, burst_length, burst_intervel);
	asprintf(&cmd,"AT^DTMF=%d,%c", call_id, c);//为了同时支持MU509和EM770W，只填写两个参数,V3D02
	err=at_send_command (cmd, &p_response);
	//err = at_send_command(cmd, NULL);
	free(cmd);
	if (err < 0 || p_response->success == 0){                                
    	goto vts;  
	}
	at_response_free(p_response);
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	return;
vts:
	ALOGD("request dtmf >> char=%c", c);
	asprintf(&cmd, "AT+VTS=%c", c);
	err = at_send_command(cmd, NULL);
	free(cmd);
	if (0 > err)
	{
		goto error;
	}
	//End being modified by c00221986 for DTS2013040706898
	/* End modified by x84000798 DTS2012060708964 2012-05-30 */
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	return;
error:
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return;
}


/***************************************************
 Function:  request_cdma_dtmf
 * RIL_REQUEST_CDMA_BURST_DTMF
 *
 * Send DTMF string
 *
 * "data" is const char **
 * ((const char **)data)[0] is a DTMF string
 * ((const char **)data)[1] is the DTMF ON length in milliseconds, or 0 to use
 *                          default
 * ((const char **)data)[2] is the DTMF OFF length in milliseconds, or 0 to use
 *                          default
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305 20110826
**************************************************/
static void request_cdma_dtmf(void *data, size_t datalen, RIL_Token token)
{
	const char *c  = ((const char **)data)[0];
	
	const char *on_length = strcmp(((const char **)data)[1],"0") ? ((const char **)data)[1]:"150"; //modify by KF34305 20111205 for DTS2011120504509
	const char *off_length = strcmp(((const char **)data)[2],"0") ? ((const char **)data)[2]:"150";
	char *cmd;
	int call_id = s_curAtiveLine;

	asprintf(&cmd,"AT^DTMF=%d,%s,%s,%s", call_id, c,on_length,off_length);
	at_send_command(cmd, NULL);
	free(cmd);
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
}

/***************************************************
 Function:  request_conference
 * RIL_REQUEST_CONFERENCE
 *
 * Conference holding and active (like AT+CHLD=3)

 * "data" is NULL
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
**************************************************/
static void request_conference(void *data, size_t datalen, RIL_Token token)
{
	// 3GPP 22.030 6.5.5
	// "Adds a held call to the conversation"

	at_send_command("AT+CHLD=3", NULL);
	/* success or failure is ignored by the upper layer here.
	it will call GET_CURRENT_CALLS and determine success that way */
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
}



/***************************************************
 Function:  request_udub
 * RIL_REQUEST_UDUB
 *
 * Send UDUB (user determined used busy) to ringing or
 * waiting call answer)(RIL_BasicRequest r);
 *
 * "data" is NULL
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
**************************************************/
static void request_udub(void *data, size_t datalen, RIL_Token token)
{
	/* user determined user busy */
	/* sometimes used: ATH */
	at_send_command("ATH", NULL);

	/* success or failure is ignored by the upper layer here.
	it will call GET_CURRENT_CALLS and determine success that way */
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
}




/***************************************************
 Function:  request_separate_connection
 * Separate a party from a multiparty call placing the multiparty call
 * (less the specified party) on hold and leaving the specified party
 * as the only other member of the current (active) call
 *
 * Like AT+CHLD=2x
 *
 * See TS 22.084 1.3.8.2 (iii)
 * TS 22.030 6.5.5 "Entering "2X followed by send"
 * TS 27.007 "AT+CHLD=2x"
 *
 * "data" is an int *
 * (int *)data)[0] contains Connection index (value of 'x' in CHLD above) "response" is NULL
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
**************************************************/
static void request_separate_connection(void *data, size_t datalen, RIL_Token token)
{
	char  cmd[12];
	int   party = ((int*)data)[0];
	// Make sure that party is in a valid range.
	// (Note: The Telephony middle layer imposes a range of 1 to 7.
	// It's sufficient for us to just make sure it's single digit.)
	if (party > 0 && party < 10) {
		sprintf(cmd, "AT+CHLD=2%d", party);
		at_send_command(cmd, NULL);
		RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	}else{
		RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
}


/***************************************************
 Function:  request_dtmf_start
 * RIL_REQUEST_DTMF_START
 *
 * Start playing a DTMF tone. Continue playing DTMF tone until
 * RIL_REQUEST_DTMF_STOP is received
 *
 * If a RIL_REQUEST_DTMF_START is received while a tone is currently playing,
 * it should cancel the previous tone and play the new one.
 *
 * "data" is a char *
 * ((char *)data)[0] is a single character with one of 12 values: 0-9,*,#
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 * See also: RIL_REQUEST_DTMF, RIL_REQUEST_DTMF_STOP

 
    ADD by fanglei/KF34305
**************************************************/
static void request_dtmf_start(void *data, size_t datalen, RIL_Token token)
{
	char c = ((char *)data)[0];
	char *cmd;
	int err;
	ATResponse   *p_response = NULL;  //Added by c00221986 for DTS2013040706898
	/* Begin modified by x84000798 DTS2012060708964 2012-05-30 */
	//Begin to be modified by c00221986 for DTS2013040706898
	int call_id = s_curAtiveLine;
	//int burst_length = DEFAULT_DTMF_BURST_LENGTH;
	//int burst_intervel = DEFAULT_DTMF_BURST_INTERVAL;
	if(RIL_DEBUG)ALOGD("request dtmf >> char=%c", c); 

	//asprintf(&cmd,"AT^DTMF=%d,%c,%d,%d", call_id, c, burst_length, burst_intervel);
	asprintf(&cmd,"AT^DTMF=%d,%c", call_id, c);//为了同时支持MU509和EM770W，只填写两个参数,V3D02
	err=at_send_command (cmd, &p_response); 
	//at_send_command(cmd, NULL);
	free(cmd);
	if (err < 0 || p_response->success == 0)
	{
		goto vts;
	}
	at_response_free(p_response);
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	return;
vts:
	ALOGD("request dtmf >> char=%c", c);
	asprintf(&cmd, "AT+VTS=%c", c);
	err = at_send_command(cmd, NULL);
	free(cmd);	
	if (0 > err)
	{
		goto error;
	}
	//End being modified by c00221986 for DTS2013040706898
	/* End modified by x84000798 DTS2012060708964 2012-05-30 */
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	return;
error:
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return;
}

/***************************************************
 Function:  request_dtmf_stop
 * RIL_REQUEST_DTMF_STOP
 *
 * Stop playing a currently playing DTMF tone.
 *
 * "data" is NULL
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 *
 * See also: RIL_REQUEST_DTMF, RIL_REQUEST_DTMF_START

 
    ADD by fanglei/KF34305
**************************************************/
static void request_dtmf_stop(void *data, size_t datalen, RIL_Token token)
{
	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
}

/***************************************************
 * RIL_REQUEST_QUERY_CLIP
 *
 * Queries the status of the CLIP supplementary service
 *
 * (for MMI code "*#30#")
 *
 * "data" is NULL
 * "response" is an int *
 * (int *)response)[0] is 1 for "CLIP provisioned"
 *                           and 0 for "CLIP not provisioned"
 *                           and 2 for "unknown, e.g. no network etc"
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
    Modify by fKF34305 20110820 for DTS2011082200108 
**************************************************/

static void request_query_clip(void *data, size_t datalen, RIL_Token token)
{
	int err;
	ATResponse *p_response = NULL;
	int response = 0;
	char *line;
	int n = 0;  

	err = at_send_command_singleline("AT+CLIP?", "+CLIP:", &p_response);

	if (err < 0 || p_response->success == 0){                                
    	goto error;  
	}
	line = p_response->p_intermediates->line;

	err = at_tok_start(&line);

	if (err < 0) {
    	goto error;
	}

	err = at_tok_nextint(&line, &n);
	if (err < 0) {
    	goto error;
	}
	
	err = at_tok_nextint(&line, &response);
	if (err < 0) {
    	goto error;
	}
	
	if(RIL_DEBUG) ALOGD( "request_query_clip response = %d\n",response);
	RIL_onRequestComplete(token, RIL_E_SUCCESS, &response, sizeof(int));  
	at_response_free(p_response);
	return; 
error:
	at_response_free(p_response);
	if(RIL_DEBUG) ALOGE( "request_query_clip must never return error");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ;    
	
}


/***************************************************
 * RIL_REQUEST_GET_CLIR
 *
 * Gets current CLIR status
 * "data" is NULL
 * "response" is int *
 * ((int *)data)[0] is "n" parameter from TS 27.007 7.7
 * ((int *)data)[1] is "m" parameter from TS 27.007 7.7
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
**************************************************/
static void request_get_clir(void *data, size_t datalen, RIL_Token token)
{
	int err;
	ATResponse *p_response = NULL;
	int response[2] = {0};
	char *line;

	err = at_send_command_singleline("AT+CLIR?", "+CLIR:", &p_response);

	if(err <0 || p_response->success==0){
		goto error;
	}
	line = p_response->p_intermediates->line;

	err = at_tok_start(&line);
	if (err < 0) goto error;
	err = at_tok_nextint(&line, &(response[0]));
	if (err < 0) goto error;
	err = at_tok_nextint(&line, &(response[1]));
	if (err < 0) goto error;
		
	RIL_onRequestComplete(token, RIL_E_SUCCESS, response, 2*sizeof(int *));
	at_response_free(p_response);
	return; 
error:
	at_response_free(p_response);
	if(RIL_DEBUG) ALOGE( "request_get_mute must never return error");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ;    
}

/***************************************************
 * RIL_REQUEST_SET_CLIR
 *
 * "data" is int *
 * ((int *)data)[0] is "n" parameter from TS 27.007 7.7
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
**************************************************/
static void request_set_clir(void *data, size_t datalen, RIL_Token token)
{
	ATResponse *p_response = NULL;
	int err, ret=0;
	int *oper;
	oper = (int *)data;
	char *cmd = NULL;

	if(RIL_DEBUG) ALOGD( "CLIR= : %d", *oper);

	asprintf(&cmd, "AT+CLIR=%d",*oper);
	err = at_send_command(cmd, &p_response); 
    if( err < 0 ) goto error;

	switch (at_get_cme_error(p_response)) {
		case CME_SUCCESS:
		break;

		case CME_SIM_NOT_INSERTED:
		case CME_SIM_NOT_VALID:
			if(RIL_DEBUG) ALOGD( "CLIR=1,2 error 1: %d", err);
			ret = SIM_ABSENT;
			goto error;
		default:
			if(RIL_DEBUG) ALOGD( "CLIR=1,2 error 2: %d", err);
			ret = SIM_NOT_READY;
			goto error;
	}

	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	at_response_free(p_response);
	free(cmd);
	return;
    
error:
    if(RIL_DEBUG) ALOGE( "%s: Format error in this AT response %d", __FUNCTION__,ret);
    RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(cmd);
    return;
 }

/***************************************************
 * RIL_REQUEST_QUERY_CALL_FORWARD_STATUS
 *
 * "data" is const RIL_CallForwardInfo *
 *
 * "response" is const RIL_CallForwardInfo **
 * "response" points to an array of RIL_CallForwardInfo *'s, one for
 * each distinct registered phone number.
 *
 * For example, if data is forwarded to +18005551212 and voice is forwarded
 * to +18005559999, then two separate RIL_CallForwardInfo's should be returned
 *
 * If, however, both data and voice are forwarded to +18005551212, then
 * a single RIL_CallForwardInfo can be returned with the service class
 * set to "data + voice = 3")
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE


 
    ADD by fanglei/KF34305
**************************************************/
static void request_query_call_forward_status(void *data, size_t datalen, RIL_Token token)
{
	ATLine *p_cur;
	int err,i;
	int countCalls = 0;
	ATResponse *p_response = NULL;
	RIL_CallForwardInfo *response;
	response=(RIL_CallForwardInfo *)data;
	RIL_CallForwardInfo *response_rc=NULL;
	RIL_CallForwardInfo **response_rc_temp=NULL;
	char  *cmd = NULL;
	

	asprintf(&cmd, "AT+CCFC=%d,%d",response->reason,response->status);
		
	err = at_send_command_multiline(cmd,"+CCFC:", &p_response); 
	free(cmd);
	if (err < 0 || p_response->success == 0)  goto error;
	
	for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next ) 
	{
		countCalls++;
	}

	response_rc = (RIL_CallForwardInfo *)alloca(countCalls * sizeof(RIL_CallForwardInfo));
	response_rc_temp = (RIL_CallForwardInfo **)alloca(countCalls * sizeof(RIL_CallForwardInfo*));
	
	memset (response_rc, 0, countCalls * sizeof(RIL_CallForwardInfo));

	for(i = 0; i < countCalls ; i++) {
		response_rc_temp[i] = &(response_rc[i]);
	}

	
	
	
	for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next) 
	{
		char *line = p_cur->line;
		
		err = at_tok_start(&line);
		if (err < 0) goto error;
                                
		err = at_tok_nextint(&line, &(response_rc->status));
		err = at_tok_nextint(&line, &(response_rc->serviceClass));
		err = at_tok_nextstr(&line, &(response_rc->number));
		err = at_tok_nextint(&line, &(response_rc->toa));
		err = at_tok_nextint(&line, &(response_rc->timeSeconds));
		response_rc->reason=response->reason;

		if(response_rc->status)
		{
			response_rc++;
		}
	}

	RIL_onRequestComplete(token, RIL_E_SUCCESS, response_rc_temp, countCalls * sizeof (RIL_CallForwardInfo *));
	
	at_response_free(p_response);
	return ;

error:
	if(RIL_DEBUG) ALOGE( "ERROR \n");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return ;	
}

/***************************************************
 * RIL_REQUEST_SET_CALL_FORWARD
 *
 * Configure call forward rule
 *
 * "data" is const RIL_CallForwardInfo *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE

 
    ADD by fanglei/KF34305
**************************************************/
static void request_set_call_forward(void *data, size_t datalen, RIL_Token token)
{
	int err,ret;
    ATResponse *p_response = NULL;
	RIL_CallForwardInfo *response;
	response=(RIL_CallForwardInfo *)data;
	char  *cmd = NULL;

	/*Start Modify by fKF34305 20110602为0的时候MU509不识别，在此加了判断，如果上层下发的时间为0，则向单板下发AT的时候将时间置为空*/
	if(response->timeSeconds)
	{
		asprintf(&cmd, "AT+CCFC=%d,%d,%s,%d,%d,,,%d",response->reason,response->status,response->number,
											response->toa,response->serviceClass,response->timeSeconds);
	}
	else
	{
		asprintf(&cmd, "AT+CCFC=%d,%d,%s,%d,%d,,,",response->reason,response->status,response->number,
											response->toa,response->serviceClass);
	}

	/*End Modify by fKF34305 20110602*/

	err = at_send_command(cmd, &p_response); 

	if (err < 0 )  goto error;
	
	switch (at_get_cme_error(p_response)) 
	{
		case CME_SUCCESS:
			break;

		case CME_SIM_NOT_INSERTED:
		case CME_SIM_NOT_VALID:
			if(RIL_DEBUG) ALOGD( "CCFC=1,2 error 1: %d", err);
			ret = SIM_ABSENT;
			goto error;
		default:
			if(RIL_DEBUG) ALOGD( "CCFC=1,2 error 2: %d", err);
			ret = SIM_NOT_READY;
			goto error;
    }

    RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	at_response_free(p_response);
	free(cmd);
	return;
error:
	at_response_free(p_response);
	if(RIL_DEBUG) ALOGE( "request_set_call_forward must never return error");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ;    
}

/***************************************************
* RIL_REQUEST_QUERY_CALL_WAITING
 *
 * Query current call waiting state
 *
 * "data" is const int *
 * ((const int *)data)[0] is the TS 27.007 service class to query.
 * "response" is a const int *
 * ((const int *)response)[0] is 0 for "disabled" and 1 for "enabled"
 *
 * If ((const int *)response)[0] is = 1, then ((const int *)response)[1]
 * must follow, with the TS 27.007 service class bit vector of services
 * for which call waiting is enabled.
 *
 * For example, if ((const int *)response)[0]  is 1 and
 * ((const int *)response)[1] is 3, then call waiting is enabled for data
 * and voice and disabled for everything else
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 
    ADD by fanglei/KF34305
**************************************************/
static void request_query_call_waiting(void *data, size_t datalen, RIL_Token token)
{
	ATLine *p_cur;
	int err;
	ATResponse *p_response;
	char  *cmd = NULL;
	int *tdata = (int *)data; //Modified by x84000798 for DTS2012082108723 2012-09-03
	int serviceClass=7;//serviceClass default is 7;
	int response[2]={0};

	/* Begin added by x84000798 for DTS2012082108723 2012-09-03 */
	if (0 == tdata[0])
	{
		tdata[0] = 1;	//规避cardhu4.0系统下发0导致单板返回错误
	}
	/* End added by x84000798 for DTS2012082108723 2012-09-03 */
	asprintf(&cmd, "AT+CCWA=%d,%d,%d",1,2,tdata[0]);
	err = at_send_command_multiline(cmd, "+CCWA:", &p_response); 
	free(cmd);
	if (err < 0 || p_response->success == 0)  goto error;
	/*Begin modify by fKF34305 解决问题单DTS2011032805355 为了规避单板上报类型过多,在ril中做了判断，
	如果单板上报开通了等待业务的类型过多 ，那么就在将所有开通了等待业务的类型求或之后
	与上上层下发的参数，如果结果仍为上层下发的参数，则证明在所有开通了的等待业务中，
	有需要查询的业务20110403*/
	for (p_cur = p_response->p_intermediates; p_cur != NULL; p_cur = p_cur->p_next ) 
	{
		char *line = p_cur->line;
		err = at_tok_start(&line);
		if (err < 0) goto error;
		err = at_tok_nextint(&line, &(response[0]));
		if (err < 0) goto error;

		if (at_tok_hasmore(&line)) /*如果单板上报的参数为一个的时候添加判断，
									防止出现失败的情况。*/
		{
			err = at_tok_nextint(&line, &serviceClass);
			if (err < 0) goto error;
        	}
		if(response[0])
		{
			response[1] |= serviceClass;
		}

	}
	
	if(tdata[0] != (response[1] & tdata[0]))
	{
		response[0] = 0;
	}
	else
	{
		response[0] = 1;
	}

		
	/*End modify by fKF34305 解决问题单DTS2011032805355 为了规避单板上报类型过多20110403*/
	
	RIL_onRequestComplete(token, RIL_E_SUCCESS,response , 2*sizeof(int *));
	at_response_free(p_response);
	return ;

error:
	if(RIL_DEBUG) ALOGE( "ERROR \n");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	return ;	
}


/***************************************************
 * RIL_REQUEST_SET_CALL_WAITING
 *
 * Configure current call waiting state
 *
 * "data" is const int *
 * ((const int *)data)[0] is 0 for "disabled" and 1 for "enabled"
 * ((const int *)data)[1] is the TS 27.007 service class bit vector of
 *                           services to modify
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 
    ADD by fanglei/KF34305
**************************************************/
static void request_set_call_waiting(void *data, size_t datalen, RIL_Token token)
{
	int err,ret;
    	ATResponse *p_response = NULL;
	char *cmd = NULL;
	int *tdata;
	tdata=(int *)data;

	asprintf(&cmd, "AT+CCWA=%d,%d,%d",1,tdata[0],tdata[1]);
	err = at_send_command(cmd, &p_response); 
	if (err < 0 )  goto error;

	switch (at_get_cme_error(p_response)) 
	{
		case CME_SUCCESS:
		 	break;

		case CME_SIM_NOT_INSERTED:
		case CME_SIM_NOT_VALID:
			if(RIL_DEBUG) ALOGD( "CCWA=1,2 error 1: %d", err);
			ret = SIM_ABSENT;
			goto error;
		default:
			if(RIL_DEBUG) ALOGD( "CCWA=1,2 error 2: %d", err);
			ret = SIM_NOT_READY;
			goto error;
    }

    	RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	at_response_free(p_response);
	free(cmd);
	return;

error:
	at_response_free(p_response);
	if(RIL_DEBUG) ALOGE( "request_set_call_waiting must never return error");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ;    
}

/***************************************************
 Function:  request_set_mute
 * RIL_REQUEST_SET_MUTE
 *
 * Turn on or off uplink (microphone) mute.
 *
 * Will only be sent while voice call is active.
 * Will always be reset to "disable mute" when a new voice call is initiated
 *
 * "data" is an int *
 * (int *)data)[0] is 1 for "enable mute" and 0 for "disable mute"
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE
 
    ADD by fanglei/KF34305
**************************************************/
static void request_set_mute(void *data, size_t datalen, RIL_Token token)
{
	int err,ret;
    ATResponse *p_response = NULL;
	char*  cmd = NULL;
	int *tdata;
	tdata=(int *)data;

	asprintf(&cmd, "AT+CMUT=%d",tdata[0]);
	err = at_send_command(cmd, &p_response); 
	if (err < 0 )  goto error;

	switch (at_get_cme_error(p_response)) 
	{
    	case CME_SUCCESS:
       	 	break;

    	case CME_SIM_NOT_INSERTED:
 		case CME_SIM_NOT_VALID:
    		if(RIL_DEBUG) ALOGD( "CMUT=1,2 error 1: %d", err);
			ret = SIM_ABSENT;
			goto error;
    	default:
    		if(RIL_DEBUG) ALOGD( "CMUT=1,2 error 2: %d", err);
			ret = SIM_NOT_READY;
			goto error;
    }

    RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);
	at_response_free(p_response);
	free(cmd);
	return;

error:
	at_response_free(p_response);
	if(RIL_DEBUG) ALOGE( "request_set_mute must never return error");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ;    
}

/***************************************************
 Function:  request_get_mute
**
 * RIL_REQUEST_GET_MUTE
 *
 * Queries the current state of the uplink mute setting
 *
 * "data" is NULL
 * "response" is an int *
 * (int *)response)[0] is 1 for "mute enabled" and 0 for "mute disabled"
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE (radio resetting)
 *  GENERIC_FAILURE

    ADD by fanglei/KF34305 
**************************************************/
static void request_get_mute(void *data, size_t datalen, RIL_Token token)
{
	int err;
	ATResponse *p_response = NULL;
	int response = 0;
	char *line;

	err = at_send_command_singleline("AT+CMUT?", "+CMUT:", &p_response);

	if(err < 0 || p_response->success==0 ){                                      
    	goto error;  
	}
	
	line = p_response->p_intermediates->line;

	err = at_tok_start(&line);

	if (err < 0) 
	{
    		goto error;
	}
	err = at_tok_nextint(&line, &response);
	if (err < 0) goto error;
		
	if(RIL_DEBUG) ALOGD( "request_get_mute response = %d\n",response);
	RIL_onRequestComplete(token, RIL_E_SUCCESS, &response, sizeof(int));  
	at_response_free(p_response);
	return; 
error:
	at_response_free(p_response);
	if(RIL_DEBUG) ALOGE( "request_get_mute must never return error");
	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ;    
	
}

// Begin add by fkf34305 20111221 for DTS2012010907760
static void request_cdma_flash(void *data, size_t datalen, RIL_Token token)
{
	ATResponse   *p_response = NULL;
	int  err;
	char *cmd = NULL;
	const char *flastr ;
	flastr = ((const char *)data);


	if(0 == flastr[0])
	{
		asprintf(&cmd, "AT^HFLASH");
	}
	else
	{
		asprintf(&cmd, "AT^HFLASH=%s",flastr);
	}

	err=at_send_command (cmd, &p_response); 
	free(cmd); 
	if (err < 0 || p_response->success == 0)
	{                                
    	RIL_onRequestComplete(token, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	else
	{
		RIL_onRequestComplete(token, RIL_E_SUCCESS, NULL, 0);  
	}
	
	at_response_free(p_response);
	return; 
}
// End add by fkf34305 20111221 for DTS2012010907760
/***************************************************
 Function:  hwril_request_cc
 Description:  
    Handle the request about curent call. 
 Calls: 
    request_get_current_calls
    request_last_call_fail_cause
    request_dial
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
void  hwril_request_cc (int request, void *data, size_t datalen, RIL_Token token)
{
	switch (request) 
	{
		case RIL_REQUEST_GET_CURRENT_CALLS:
		{
			request_get_current_calls(data,  datalen, token);
			break;
		}

		case RIL_REQUEST_LAST_CALL_FAIL_CAUSE:
		{
			request_last_call_fail_cause(data,datalen,token);
			break;	    	
		}

		case RIL_REQUEST_DIAL:
		{
			request_dial(data, datalen, token);
			break;
		}

		case RIL_REQUEST_ANSWER:
		{
			request_answer(data, datalen, token);
			break;
		}

		case RIL_REQUEST_HANGUP:
		{
			request_hangup(data, datalen, token);
			break;
		}

		case RIL_REQUEST_HANGUP_WAITING_OR_BACKGROUND:
		{
			request_hangup_waiting_or_backgroud(data, datalen, token);
			break;
		}

		case RIL_REQUEST_HANGUP_FOREGROUND_RESUME_BACKGROUND:
		{
			request_hangup_foreground_resume_backgroud(data, datalen, token);
			break;
		}

		case RIL_REQUEST_SWITCH_WAITING_OR_HOLDING_AND_ACTIVE:
		{
			request_switch_waiting_or_holding_and_active(data, datalen, token);
			break;
		}

		case RIL_REQUEST_CONFERENCE:
		{
			request_conference(data, datalen, token);
			break;
		}

		case RIL_REQUEST_UDUB:
		{
			request_udub(data, datalen, token);
			break;
		}  
		case RIL_REQUEST_DTMF: 
		{
			request_dtmf(data, datalen, token);
			break;
		}

		//Begin add by fKF34305 20110826 for CDMA DTMF
		case RIL_REQUEST_CDMA_BURST_DTMF: 
		{
			request_cdma_dtmf(data, datalen, token);
			break;
		}
		//End add by fKF34305 20110826 for CDMA DTMF
		case RIL_REQUEST_DTMF_START:
		{
			request_dtmf_start(data, datalen, token);
			break;
		}
		case RIL_REQUEST_DTMF_STOP:
		{
			request_dtmf_stop(data, datalen, token);
			break;
		}
		case RIL_REQUEST_SEPARATE_CONNECTION:
		{
			request_separate_connection(data, datalen, token);
			break;
		}

		case RIL_REQUEST_QUERY_CLIP:
		{
			request_query_clip(data, datalen, token);
			break;
		}
		case RIL_REQUEST_GET_CLIR:
		{
			request_get_clir(data, datalen, token);
			break;
		}
		case RIL_REQUEST_SET_CLIR:
		{
			request_set_clir(data, datalen, token);
			break;
		}
		case RIL_REQUEST_QUERY_CALL_FORWARD_STATUS:
		{
			request_query_call_forward_status(data, datalen, token);
			break;
		}
		case RIL_REQUEST_SET_CALL_FORWARD:
		{
			request_set_call_forward(data, datalen, token);
			break;
		}
		case RIL_REQUEST_QUERY_CALL_WAITING:
		{
			request_query_call_waiting(data, datalen, token);
			break;
		}
		case RIL_REQUEST_SET_CALL_WAITING:
		{
			request_set_call_waiting(data, datalen, token);
			break;
		}
		case RIL_REQUEST_SET_MUTE:
		{
			request_set_mute(data, datalen, token);
			break;
		}
		case RIL_REQUEST_GET_MUTE:
		{
			request_get_mute(data, datalen, token);
			break;
		}
		//Begin add by fkf34305 20111221 for DTS2012010907760
		case RIL_REQUEST_CDMA_FLASH:
		{
			request_cdma_flash(data, datalen, token);
			break;
		}
		//End add by fkf34305 20111221 for DTS2012010907760

		default:
		{
			RIL_onRequestComplete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
			break;
		}
	}
}

/***************************************************
 Function:  parse_cring_response
 Description:  
 	Distinguish Voice call or PS incoming packet call
 Calls: 
    none
 Called By:
    hwril_unsolicited_cc
 Input:
    s - pointer to unsolicited intent(+CRING)
 Output:
    none
 Return:
    int: 0 - other  1 - voice call  2 -  csd call  3 - PS incoming data call
**************************************************/
int parse_cring_response(char *line)
{
	char *mode;
	int err = 0;

	err = at_tok_start(&line);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextstr(&line, &mode);
	if (0 > err)
	{
		goto error;
	}
	if (strStartsWith(mode, "VOICE"))
	{
		return 1;
	}
	else if (strStartsWith(mode, "ASYNC") || strStartsWith(mode, "SYNC")
				||strStartsWith(mode, "REL ASYNC") || strStartsWith(mode, "REL SYNC"))
	{
		return 2;
	}
	else if (strStartsWith(mode, "GPRS"))
	{
		return 3;
	}
	else
	{
		return 0;
	}

error:
	return 0;
}

/***************************************************
 Function:  hwril_unsolicited_cc
 Description:  
    Handle the unsolicited about curent call. 
    This is called on atchannel's reader thread. 
 Calls: 
    RIL_onUnsolicitedResponse
 Called By:
    on_unsolicited
 Input:
    s - pointer to unsolicited intent(+CRING)
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/

void hwril_unsolicited_cc (const char *s)
{
	char  *line;
	if (strStartsWith(s,"+CRING:") || strStartsWith(s,"RING")
	|| strStartsWith(s,"NO CARRIER") || strStartsWith(s,"+CCWA")
	|| strStartsWith(s,"^CEND:") || strStartsWith(s,"^CONN:") 
	|| strStartsWith(s,"HANGUP:")) {
		if (strStartsWith(s,"^CEND:")){
			line = strdup(s);
			parse_call_end_response(line);
			free(line);
		}
	/*Begin modified by x84000798 for DTS2012073007745 2012-8-11*/
		//Begin to be modified by c00221986 to delete g_pspackage
		else if (strStartsWith(s,"+CRING:"))
		{
			line = strdup(s);
			int ret = parse_cring_response(line);
			free(line);
		#ifdef PS_PACKAGE
				switch (ret)
				{
					case 1:		//voice call
						break;
					case 2:		//csd call
						ALOGD("---->>>>signal here to unblock, CSD call !!!");
						g_csd  = 1;
						pthread_cond_signal(&s_cmd_cond);
						return;
					case 3:		//ps incoming call
						ALOGD("---->>>>signal here to unblock, PS incoming data call !!!");
						g_pspackage = 1;
						pthread_cond_signal(&s_cmd_cond);
						return;				
					case 0:
						return;
				}
		#else
				switch (ret)
				{
					case 1:		//voice call
						break;			
					case 0:
						return;
				}
	    #endif
		}
		//End being modified by c00221986 to delete g_pspackage
	/*End modified by x84000798 for DTS2012073007745 2012-8-11*/
		RIL_onUnsolicitedResponse ( RIL_UNSOL_RESPONSE_CALL_STATE_CHANGED, NULL, 0);
	}
	return ;
}
