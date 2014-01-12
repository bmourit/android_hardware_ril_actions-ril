/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-ps-sm.h
 Author: fangyuchuang/00140058
 Version: V1.0
 Date: 2010/04/07
 Description:
            The function transfer sting  between network type and decimalist. 
            And some functions Setting de network attributes of Android and Linux.
 Function List:
 History:
 <author>         <time>        <version>        <desc>
 f00140058        10/04/08        1.0
 fangyuchuang     10/07/28        1.1
*******************************************************/
#ifndef   __PDP_SM__H__
#define   __PDP_SM__H__
#include <telephony/ril.h>

typedef enum {         
	NDIS_EVENT_REQUEST_PDP_ACTIVE  = 1,          
	NDIS_EVENT_REQUEST_PDP_DEACTIVE = 2,
	NDIS_EVENT_REPORT_DEND=3 ,
	NDIS_EVENT_REPORT_DCONN=4 ,
	NDIS_EVENT_UNKNOWN=0XFF
} HWRIL_NDIS_State;

#define APN_LEN 128
#define PPP_USERNAME_LEN 64
#define PPP_PASSWORD_LEN 128

typedef struct
{	
	char apn[APN_LEN+1];
	char user[PPP_USERNAME_LEN+1];
	char passwd[PPP_PASSWORD_LEN+1];
	int authpref;
	RIL_Token token;
	HWRIL_NDIS_State ndis_state;
}HWRIL_RMNET_PARAMETERS_ST;

#ifdef ANDROID_3
	RIL_Data_Call_Response_v6 rmnet0_response_v6;
#endif

#define  NDIS_STATE_DEATIVED		0
#define  NDIS_STATE_ATIVED          1
#define  NDIS_STATE_ATIVING         2
#define  NDIS_STATE_DEATIVING     	3



static int active_ndis(HWRIL_RMNET_PARAMETERS_ST *rmnet_st);

static int deactive_ndis(void );

static void active_pdp_timeout(void);

static void deactive_pdp_timeout(void);

static int  dhcp_up_handle();

//void ndis_sm(void *data, int ndis_event);
void ndis_sm(HWRIL_RMNET_PARAMETERS_ST * rmnet_st);

void DEND_report(void*);

void  DCONN_report(void*);

void reset_ndis_sm(void);

void init_sigaction(void);

void start_timer(void);

void delet_timer(void);

void init_sigaction(void);

#endif

