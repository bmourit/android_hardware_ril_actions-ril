#ifndef __HUAWEI__H__
#define __HUAWEI__H__  1

#include <telephony/ril.h>
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
#include <utils/Log.h>
#include <android/log.h>

#include "atchannel.h"
#include "at_tok.h"
#include "misc.h"
#include "ril-ps-sm.h"
//begin to add by h81003427 for sar feature 20120321
#include "sar_feature.h"
//begin to add by h81003427 for sar feature 20120321

#ifdef LOG_NDEBUG
#undef LOG_NDEBUG
#define LOG_NDEBUG 0
#endif
#ifdef LOG_TAG
#undef LOG_TAG
#define LOG_TAG "RIL"
#endif

/*要查看AT命令，则要定义成1；发布产品要定义成0*/
#define RIL_DEBUG  0  //1,0
/* Begin added by x84000798 for android4.1 Jelly Bean DTS2012101601022 2012-10-16 */	
#ifndef LOGI
#define LOGI(fmt, args...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, fmt, ##args)
#endif
#ifndef LOGD
#define LOGD(fmt, args...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, fmt, ##args)
#endif
#ifndef LOGE
#define LOGE(fmt, args...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##args) 
#endif
#ifndef LOGW
#define LOGW(fmt, args...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, fmt, ##args)
#endif
/* End added by x84000798 for android4.1 Jelly Bean DTS2012101601022 2012-10-16 */	

#define MAX_PREFIX_LENGTH   32 
#define MAX_AT_LENGTH     512 
#define MAX_PDU_LENGTH    400 

typedef enum {
	NETWORK_CDMA = 1 ,/*电信*/
	NETWORK_WCDMA = 2,/*联通*/
	NETWORK_TDSCDMA = 4,/*移动*/
	NETWORK_UNKNOWN = 0XFFFF
}Module_Type;

//Begin to be added by alic for vendor
typedef enum {
	MODEM_VENDOR_HAUWEI = 0 ,
	MODEM_VENDOR_ZTE = 1,
	MODEM_VENDOR_SCV = 2,
	MODEM_VENDOR_ALCATEL =3,
	MODEM_VENDOR_UNKNOWN = 0XFFFF
}MODEM_Vendor;
//end being added by alic for vendor

//Begin to be added by c00221986 for ndis
typedef enum {         
	NDIS_PROCESS  = 1,          
	MODEM_PROCESS= 2
} HWRIL_NDIS_MODEM;
//End being added by c00221986 for nd
//Begin add by fKF34305 20110827  for DTS2011071904295
typedef enum {
	NO_CARRIER= 0 ,
	UNS_NO_CARRIER = 1,
}CARRIER_Type;
//End add by fKF34305 20110827  for DTS2011071904295

typedef enum {
	CDMA_RUIM_TYPE = 0 ,//机卡分离
	CDMA_NV_TYPE = 1,//机卡合一
	CDMA_UNKNOWN_TYPE= 0XFFFF
}CDMA_SubType;


typedef enum {
	PID_OLD,//旧PID 方案
	PID_NEW,//新PID 方案
	PID_UNKNOWN
}PID_Project;

/* Begin added by x84000798 */
typedef enum
{
	STK_OLD,	//旧STK方案(raw data模式)
	STK_NEW,	//新STK方案(协议标准模式)
	STK_UNKNOW
}STK_Project;
/* End added by x84000798 */

/* Begin modified by x84000798 for DTS2012110300583  2012-11-03*/
typedef enum
{
	SMS_OLD = 0,	//旧短信方案
	SMS_NEW = 1,	//新短信方案
	SMS_UNKNOW = 0xFFFF
}SMS_Project;
/* Begin modified by x84000798 for DTS2012110300583  2012-11-03*/

//begin add by alic for vendor
struct MODEMP
{
	char number[10];
	char index[10];
};
struct DIAGP
{
 	char number[10];
 	char index[10];
};
struct PCUIP
{
	char number[10];
	char index[10];
};
struct DEVICEPORTS
{
	char VID[10];
	char PID[10];
	unsigned int networktype;
	struct MODEMP modem;
	struct DIAGP diag;
	struct PCUIP pcui;
	};
	
static const struct timeval TIMEVAL_SIMREADY = {0,5000000};
static const struct timeval TIMEVAL_SIMPOLL = {1,0};
static const struct timeval TIMEVAL_CALLSTATEPOLL = {0,500000};    
static const struct timeval TIMEVAL_IMMEDIATE= {0,0};
//static const struct timeval TIMEVAL_LIVE= {15,0};//by alic
/* Begin added by x84000798 2012-06-01 DTS2012060806329 */
static const struct timeval TIMEVAL_AUTOPOLL = {1,0};
/* End added by x84000798 2012-06-01 DTS2012060806329 */
const struct RIL_Env *s_rilenv;

#define RIL_onRequestComplete(t, e, response, responselen) s_rilenv->OnRequestComplete(t,e, response, responselen)
#define RIL_onUnsolicitedResponse(a,b,c) s_rilenv->OnUnsolicitedResponse(a,b,c)
#define RIL_requestTimedCallback(a,b,c) s_rilenv->RequestTimedCallback(a,b,c)

/*** Static Variables ***/
typedef enum {
    SIM_ABSENT = 0,
    SIM_NOT_READY = 1,
    SIM_READY = 2, /* SIM_READY means the radio state is RADIO_STATE_SIM_READY */
    SIM_PIN = 3,
    SIM_PUK = 4,
    SIM_NETWORK_PERSONALIZATION = 5,
    //add by wkf32792 for android 3.x begin
    RUIM_ABSENT = 6,
    RUIM_NOT_READY = 7,
    RUIM_READY = 8,
    RUIM_PIN = 9,
    RUIM_PUK = 10,
    RUIM_NETWORK_PERSONALIZATION = 11
    //add by wkf32792 for android 3.x end
} SIM_Status; 

typedef enum {
    SERVICE_CC,
    SERVICE_DEV,
    SERVICE_MM,
    SERVICE_SMS,
    SERVICE_PS,
    SERVICE_SIM,
    SERVICE_SS,
    SERVICE_UNSOL,
    SERVICE_TOTAL
} ServiceType;

//begin to add by hexiaokong kf39947 20120216 for multi-platform
typedef enum
{
    QUALCOMM,
    HUAWEIB,
    T3G,
    DATANG,
    STE,
    NXP,
    ICERA,
    MARVELL,
    HUAWEIH,
    INFINEON
} provider_info;
//end to add by hexiaokong kf39947 20120216 for multi-platform
typedef struct RegState_s {
    int stat;
    int lac;
    int cid;
} RegState; 

typedef struct OperInfo_s {
    int stat;
    char operLongStr[20];
    char operShortStr[10];
    char operNumStr[10];
    int act;
} OperInfo;

typedef struct
{
  char * mcc;                          /*  digit numeric code  (MCC + MNC)*/                       /* Mobile Country Code */
  char *short_name_ptr;                /* Pointer to a null terminated string containing the network's short name */
  char *full_name_ptr;                 /* Pointer to a null terminated string containing the network's full name */
} Oper_memory_entry_type;

int is_radio_on(void);
 
void set_radio_state(RIL_RadioState newState);
RIL_RadioState getRadioState();
void on_at_timeout(void);
#endif
