#ifndef __HUAWEI__SAR__FEATURE__H
#define __HUAWEI__SAR__FEATURE__H 1


#include <pthread.h>
#include <stdio.h>


//begin to modify by h81003427 for sar feature 20120414
#define FILENAME_SYS_SAR_CONTROL "/sys/power/sar/state"
#define STRING_SAR_CONTROL_ON  "SAR_CONTROL_ON" /* SAR Back-off control ON */
#define STRING_SAR_CONTROL_OFF "SAR_CONTROL_OFF" /* SAR Back-off control OFF */
#define STRING_SAR_CONTROL_SUSPEND "SAR_CONTROL_SUSPEND" /*SAR Back-off control SUSPEND*/
#define STRING_SAR_CONTINUE    "SAR_CONTINUE" /* Ack from ril */
//#define STRING_SAR_SIGNAL      "SAR_SIGNAL" /* Get ril PID */
#define STRING_SAR_UNKNOWN     "PM_UNKNOWN" /* Unknown Notification*/

#define STRING_SAR_CREATE_PTHREAD "SAR_CREATE_PTHREAD"  /*This is a notification from framework to create pthread*/
//for testing
#define FILENAME_SYS_SAR_CONTROLTEST "/sys/power/sar/controltest"
#define STRING_SARON_TEST      "saron"
#define STRING_SAROFF_TEST     "saroff"
#define STRING_SARSUSPEND_TEST "sarsuspend"
#define STRING_UNKNOWN         "unknown"
/*
typedef enum
{
        No_service      = 0,
        AMPS            = 1,     //not supported
        CDMA            = 2,     //not suported
        GSM850          = 31,
        GSM900          = 32,
        GSM1800         = 33,
        GSM1900         = 34,
        WCDMA2100       = 80,
        WCDMA1900       = 81,
        WCDMA850        = 84,
        WCDMA900        = 87

}BAND_VALUE;
*/
#define TIMEOUT_SEARCH_FOR_CONTROL 10

/*#define SAR_FEATURE_SWITCH 1*/


void* processSarCtrlNotification(void *_args);
//int getpid();

pthread_t s_tid_processSarCtrlNotification;
//end    to modify by h81003427 for sar feature 20120414
#endif


