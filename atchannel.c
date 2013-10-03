/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-ps-api.c
 Author: fangyuchuang/00140058
 Version: V1.0
 Date: 2010/04/07
 Description:
            The function transfer sting  between network type and decimalist. 
            And some functions Setting de network attributes of Android and Linux.
 Function List:
 History:
 <author>                      <time>                    <version>               <desc>
 fangyuchuang              10/07/28                      1.1                  
*******************************************************/

#include "atchannel.h"
#include "huawei-ril.h"
#include "at_tok.h"
#include <telephony/ril.h>

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#ifdef HAVE_ANDROID_OS
/* for IOCTL's */
#include <linux/omap_csmi.h>
#endif /*HAVE_ANDROID_OS*/

#include "misc.h"

#define USE_NP 1

#define ok_error(num) ((num)==EIO)

#define NUM_ELEMS(x) (sizeof(x)/sizeof(x[0]))

#define MAX_AT_RESPONSE (8 * 1024)
#define HANDSHAKE_RETRY_COUNT 8
#define HANDSHAKE_TIMEOUT_MSEC 250

static pthread_t s_tid_reader;
static int s_fd = -1;    /* fd of the AT channel */
static ATUnsolHandler s_unsolHandler;

int at_request;
static int  AtErrorCount = 0;
extern MODEM_Vendor g_modem_vendor;
/* DTS2011121100581 kf32792 20111216 removed */
// static int iNeedSleep = 0; //DTS2011120206698 kf32792 20111206 add,规避cms error带OK问题

/* for input buffering */
static char s_ATBuffer[MAX_AT_RESPONSE+1];
static char *s_ATBufferCur = s_ATBuffer;

static int s_ackPowerIoctl; /* true if TTY has android byte-count
                                handshake for low power*/
static int s_readCount = 0;

Module_Type g_module_type = NETWORK_WCDMA;
CDMA_SubType g_cdma_subtype = CDMA_UNKNOWN_TYPE;
								
/*BEGIN Added by z128629 DTS2011032203061:obtain tty by read usb description file after usb reset */
extern char SERIAL_PORT_MODEM_KEYNAME[60];
extern char SERIAL_PORT_DIAG_KEYNAME[60];
/*END Added by z128629 DTS2011032203061:obtain tty by read usb description file after usb reset */
CARRIER_Type g_at_type;//Add by fKF34305 2011-8-26 DTS2011071904295
								
extern void set_radio_state(RIL_RadioState newState);

extern int terminal_connect;  /* DTS2011111806466  lkf52201 2011-12-03 added */
#ifdef   CMTI_ISSUE
pthread_mutex_t s_readdelmutex ;//added by qkf33988 DTS2010110103862 CMTI方式短信上报问题 2010-11-13
#endif

#if AT_DEBUG
void  AT_DUMP(const char*  prefix, const char*  buff, int  len)
{
    if (len < 0)
    {
        len = strlen(buff);
    }
    if(RIL_DEBUG) ALOGD( "%.*s", len, buff);
}
#endif

/*
 * for current pending command
 * these are protected by s_commandmutex
 */

static pthread_mutex_t s_commandmutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_commandcond = PTHREAD_COND_INITIALIZER;

static ATCommandType s_type;
static const char *s_responsePrefix = NULL;
static const char *s_smsPDU = NULL;
static ATResponse *sp_response = NULL;

static void (*s_onTimeout)(void) = NULL;
static void (*s_onReaderClosed)(void) = NULL;
static int s_readerClosed;
int s_MaxError = 0;

static void onReaderClosed();
static int writeCtrlZ (const char *s);
static int writeline (const char *s);

static void setTimespecRelative(struct timespec *p_ts, long long msec)
{
    struct timeval tv;

    gettimeofday(&tv, (struct timezone *) NULL);

    /* what's really funny about this is that I know
       pthread_cond_timedwait just turns around and makes this
       a relative time again */
    p_ts->tv_sec = tv.tv_sec + (msec / 1000);
    p_ts->tv_nsec = (tv.tv_usec + (msec % 1000) * 1000L ) * 1000L;
}

static void sleepMsec(long long msec)
{
    struct timespec ts;
    int err;

    ts.tv_sec = (msec / 1000);
    ts.tv_nsec = (msec % 1000) * 1000 * 1000;

    do {
        err = nanosleep (&ts, &ts);
    } while (err < 0 && errno == EINTR);
}



/** add an intermediate response to sp_response*/
static void addIntermediate(const char *line)
{
    ATLine *p_new;

    p_new = (ATLine  *) malloc(sizeof(ATLine));

    p_new->line = strdup(line);

    /* note: this adds to the head of the list, so the list
       will be in reverse order of lines received. the order is flipped
       again before passing on to the command issuer */
    p_new->p_next = sp_response->p_intermediates;
    sp_response->p_intermediates = p_new;
}


/**
 * returns 1 if line is a final response indicating error
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char * s_finalResponsesError[] = {
    "ERROR",
    "+CMS ERROR:",
    "+CME ERROR:",
    "NO CARRIER", /* sometimes! */
    "NO ANSWER",
    "NO DIALTONE",
    "COMMAND NOT SUPPORT"
};

static int isFinalResponseError(const char *line)
{
    size_t i;

    for (i = 0 ; i < NUM_ELEMS(s_finalResponsesError) ; i++) {
        if (strStartsWith(line, s_finalResponsesError[i])) {
            /* BEGIN DTS2011121100581 kf32792 20111216 removed */
            /*
            //BEGIN DTS2011120206698 kf32792 20111206 add, 规避cms error带OK问题
            if (NULL != strstr(line, "CMS ERROR"))
            {
                iNeedSleep = 1;
                ALOGD("CMS ERROR,need to sleep!");
            }
            //END DTS2011120206698 kf32792 20111206 add, 规避cms error带OK问题
            */
            /* END DTS2011121100581 kf32792 20111216 removed */

            return 1;
        }
    }

    return 0;
}

/**
 * returns 1 if line is a final response indicating success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static const char * s_finalResponsesSuccess[] = {
    "OK",
    "CONNECT"       /* some stacks start up data on another channel */
};

static int isFinalResponseSuccess(const char *line)
{
    size_t i;

    for (i = 0 ; i < NUM_ELEMS(s_finalResponsesSuccess) ; i++) {
        if (strStartsWith(line, s_finalResponsesSuccess[i])) {
            return 1;
        }
    }

    return 0;
}

/**
 * returns 1 if line is a final response, either  error or success
 * See 27.007 annex B
 * WARNING: NO CARRIER and others are sometimes unsolicited
 */
static int isFinalResponse(const char *line)
{
    return isFinalResponseSuccess(line) || isFinalResponseError(line);
}


/**
 * returns 1 if line is the first line in (what will be) a two-line
 * SMS unsolicited response
 */
static const char * s_smsUnsoliciteds[] = {
    "+CMT:",
    "+CDS:", 
    "+CBM:"//add by x84000798 
};
static int isSMSUnsolicited(const char *line)
{
    size_t i;

    for (i = 0 ; i < NUM_ELEMS(s_smsUnsoliciteds) ; i++) {
        if (strStartsWith(line, s_smsUnsoliciteds[i])) {
            return 1;
        }
    }

    return 0;
}


/** assumes s_commandmutex is held */
static void handleFinalResponse(const char *line)
{
    sp_response->finalResponse = strdup(line);

    pthread_cond_signal(&s_commandcond);
}

static void handleUnsolicited(const char *line)
{
    if (s_unsolHandler != NULL) {
        s_unsolHandler(line, NULL);
    }
}

/* BEGIN DTS2012022004973  lkf52201 2012-02-21 added */
/*************************************************
Function:    isUnsolicitedLine
Description: 该函数判断所给字符串是否属于主动上报,如果是,则返回1,否则返回0
Calls:       strStartsWith() 
Called By:   processLine()
Input:       const char *line; 要判断的字符串
Output:      
Return:      int类型的变量, 1 表示所给字符串是主动上报, 0 表示不是
Others:      
*************************************************/
static int isUnsolicitedLine(const char *line)
{
    // 这个数组参照huawei-ril.c的judge_service_on_unsolicited()函数的matchTable[]
    char *unsolicited_line[] = 
    {
        "+CREG:",
        "+CGREG:",
        "^MODE:",
        "^SRVST:",
        "^RSSI:",
        "RING",
        "NO CARRIER",
        "+CCWA",
        "+CCCM:",
        "+CLCC:",
        "^CONN:",
        "^CEND:",
        "^ORIG:",
        "HANGUP:"
        "+CMT:",
        "+CMTI:",
        "+CDS:",
        "+CDSI:",
        "+CBM:",//add by x84000798
        "^SMMEMFULL:",
        "+CSSU:",
        "+CUSD:",
        "^DCONN",
        "^DEND",
        "^SIMST",
        "^STIN",
        "^NWTIME:",
        "*PSSIMSTAT:"
    };
    int i = 0;
    int total = sizeof(unsolicited_line) / sizeof(unsolicited_line[0]);
    
    if (NULL == line)
    {
        return 0;   // 如果输入的字符指针是NULL,则返回0,认为不是主动上报
    }
    
    for ( ; i < total; ++i)
    {
        if (strStartsWith(line, unsolicited_line[i]))
        {
            return 1;
        }
    }

    return 0;
}
/* END   DTS2012022004973  lkf52201 2012-02-21 added */

static void processLine(const char *line)
{
		char	*line1 = NULL;// by alic
		char	*rssivaluestr = NULL;// by alic
		char	*rssivaluestr1 = NULL;// by alic
		char	*rssivaluestr2 = NULL;// by alic
		int n,rssivaluetmp;// by alic
    pthread_mutex_lock(&s_commandmutex);

    if (sp_response == NULL) {
        /* no command pending */
        handleUnsolicited(line);
    }
     //Begin add by fKF34305 2011-8-26 for DTS2011071904295
	else if((g_at_type) && (strstr(line,"NO CARRIER") != NULL)){
        handleUnsolicited(line);
    } 
     //End add by fKF34305 2011-8-26 for DTS2011071904295
	else if (isFinalResponseSuccess(line)) {
        sp_response->success = 1;
        handleFinalResponse(line);
    } else if (isFinalResponseError(line)) {
        sp_response->success = 0;
        handleFinalResponse(line);
    } else if (s_smsPDU != NULL && 0 == strcmp(line, "> ")) {
        // See eg. TS 27.005 4.3
        // Commands like AT+CMGS have a "> " prompt
        writeCtrlZ(s_smsPDU);
        s_smsPDU = NULL;
    } else switch (s_type) {
        case NO_RESULT:
            handleUnsolicited(line);
            break;
        case NUMERIC:
            if (sp_response->p_intermediates == NULL
                && isdigit(line[0])
            ) {
                addIntermediate(line);
            } else {
                /* either we already have an intermediate response or
                   the line doesn't begin with a digit */
                handleUnsolicited(line);
            }
            break;
        case SINGLELINE:
            if (sp_response->p_intermediates == NULL
                && strStartsWith (line, s_responsePrefix)
            ) {
            		line1 = strdup(line);
                addIntermediate(line);
            		if(strStartsWith(line1, "+CSQ")){//by alic :第三个修改使之重新拨号的地方。  
									rssivaluestr1 = strchr(line1, ':');
									rssivaluestr2 = strchr(line1, ',');
									n = rssivaluestr2 - rssivaluestr1;
									rssivaluestr1++;
									rssivaluestr = malloc(n - 1);//少了这句，折腾死我了。
									memset(rssivaluestr,'\0',(n - 1));
									strncpy(rssivaluestr, rssivaluestr1, (n - 1));
									rssivaluetmp = atoi(rssivaluestr);//取出两个字符
									//if(RIL_DEBUG) ALOGD( "******aliC in processLine, rssivaluetmp is %d", rssivaluetmp);
									if ((MODEM_VENDOR_SCV != g_modem_vendor) && (99 == rssivaluetmp)){/*实创兴的有点问题这个地方*/										
										s_MaxError++;		
										if(RIL_DEBUG) ALOGD( "******aliC in processLine, s_MaxError is %d", s_MaxError);												
									}			
									free(rssivaluestr);	
								}//end by alic
								
								free(line1);
								line1 = NULL;
            } else {
                /* we already have an intermediate response */
                handleUnsolicited(line);
            }
            break;
        case MULTILINE:
            if (strStartsWith (line, s_responsePrefix)  
#ifdef   CMTI_ISSUE
				||  isdigit(line[0])
#endif
							){
                addIntermediate(line);
            } else {
                handleUnsolicited(line);
            }
        break;

        /* BEGIN DTS2012022004973  lkf52201 2012-02-21 added */
        case OEM_STRING:
            // 调用isUnsolicitedLine(),判断所读取到的字符串是否对应一个主动上报
            if (1 == isUnsolicitedLine(line))
            {
                handleUnsolicited(line);
            }
            else
            {
                addIntermediate(line);
            }
            break;
        /* END   DTS2012022004973  lkf52201 2012-02-21 added */
        
        default: /* this should never be reached */
            if(RIL_DEBUG) ALOGE( "Unsupported AT command type %d\n", s_type);
            handleUnsolicited(line);
        break;
    }

    pthread_mutex_unlock(&s_commandmutex);
}


/**
 * Returns a pointer to the end of the next line
 * special-cases the "> " SMS prompt
 *
 * returns NULL if there is no complete line
 */
static char * findNextEOL(char *cur)
{
    if (cur[0] == '>' && cur[1] == ' ' && cur[2] == '\0') {
        cur[3] = '\0'; /*hkf32055 20101028 规避接收到字符，解析出错的问题单：DTS2010102803936 */
        /* SMS prompt character...not \r terminated */
        return cur+2;
    }

    // Find next newline
    while (*cur != '\0' && *cur != '\r' && *cur != '\n') cur++;

    return *cur == '\0' ? NULL : cur;
}

/**
 * Reads a line from the AT channel, returns NULL on timeout.
 * Assumes it has exclusive read access to the FD
 *
 * This line is valid only until the next call to readline
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */

static const char *readline()
{
    ssize_t count;

    char *p_read = NULL;
    char *p_eol = NULL;
    char *ret;

    /* this is a little odd. I use *s_ATBufferCur == 0 to
     * mean "buffer consumed completely". If it points to a character, than
     * the buffer continues until a \0
     */
    if (*s_ATBufferCur == '\0') {
        /* empty buffer */
        s_ATBufferCur = s_ATBuffer;
        *s_ATBufferCur = '\0';
        p_read = s_ATBuffer;
    } else {   /* *s_ATBufferCur != '\0' */
        /* there's data in the buffer from the last read */

        // skip over leading newlines
        while (*s_ATBufferCur == '\r' || *s_ATBufferCur == '\n')
            s_ATBufferCur++;

        p_eol = findNextEOL(s_ATBufferCur);

        if (p_eol == NULL) {
            /* a partial line. move it up and prepare to read more */
            size_t len;

            len = strlen(s_ATBufferCur);

            memmove(s_ATBuffer, s_ATBufferCur, len + 1);
            p_read = s_ATBuffer + len;
            s_ATBufferCur = s_ATBuffer;
        }
        /* Otherwise, (p_eol !- NULL) there is a complete line  */
        /* that will be returned the while () loop below        */
    }

    while (p_eol == NULL) {
        if (0 == MAX_AT_RESPONSE - (p_read - s_ATBuffer)) {
            if(RIL_DEBUG) ALOGE( "ERROR: Input line exceeded buffer\n");
            /* ditch buffer and start over again */
            s_ATBufferCur = s_ATBuffer;
            *s_ATBufferCur = '\0';
            p_read = s_ATBuffer;
        }

        do {
            count = read(s_fd, p_read,
                            MAX_AT_RESPONSE - (p_read - s_ATBuffer));
        } while (count < 0 && errno == EINTR);

        if (count > 0) {
            AT_DUMP( "<< ", p_read, count );
            s_readCount += (int)count;

            p_read[count] = '\0';

            // skip over leading newlines
            while (*s_ATBufferCur == '\r' || *s_ATBufferCur == '\n')
                s_ATBufferCur++;

            p_eol = findNextEOL(s_ATBufferCur);
            p_read += (int)count;
        } else if (count <= 0) {
            if(RIL_DEBUG) ALOGD( "errno: %d", errno); //modified by wkf32792 for debug
            /* read error encountered or EOF reached */
            if(count == 0) {
                if(RIL_DEBUG) ALOGD( "atchannel: EOF reached");
            } else {
                if(RIL_DEBUG) ALOGD( "atchannel: read error %s", strerror(errno));
            }
            return NULL;
        }
    }

    /* a full line in the buffer. Place a \0 over the \r and return */

    ret = s_ATBufferCur;
    *p_eol = '\0';
    s_ATBufferCur = p_eol + 1; /* this will always be <= p_read,    */
                              /* and there will be a \0 at *p_read */
  /*                            
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ALOGD(" tv.sec %u  :, tv.usec %u\n", tv.tv_sec, tv.tv_usec);
  */
    if(RIL_DEBUG) ALOGD("AT< %s\n", ret);
    return ret;
}

static void onReaderClosed()
{
    /* 如果此时还在获取ip，则将terminal_connect设为1，将停止获取ip */
	terminal_connect = 1;   /* DTS2011111806466  lkf52201 2011-12-03 added */
	
       if (s_onReaderClosed != NULL && s_readerClosed == 0) {

	    if(RIL_DEBUG)ALOGD("*****onReaderClosed is ******");
	
        pthread_mutex_lock(&s_commandmutex);

        s_readerClosed = 1;

        pthread_cond_signal(&s_commandcond);

        pthread_mutex_unlock(&s_commandmutex);

        s_onReaderClosed();

    }
}


static void *readerLoop(void *arg)
{
    for (;;) {
        const char * line;

        line = readline();

        if (line == NULL) {
            break;
        }
        
        
        /* there mybe something wrong,reset usb and dongle */
        if (s_MaxError >= 1)
        {
            const char cmd[] = "2";
            int fd;
            fd = open("/sys/monitor/usb_port/config/run", O_RDWR | O_NONBLOCK);  
            if (fd > 0)
            {
                if(RIL_DEBUG)ALOGD("/sys/monitor/usb_port/config/run sucess");
                write(fd, cmd, sizeof(cmd));
            }
            close(fd);
        }

        if(isSMSUnsolicited(line)) {
            char *line1;
            const char *line2;

            // The scope of string returned by 'readline()' is valid only
            // till next call to 'readline()' hence making a copy of line
            // before calling readline again.
            line1 = strdup(line);
            line2 = readline();

	      if (line2 == NULL) {
    			free(line1);
    			break;
	      }

		if (s_unsolHandler != NULL) {
    		s_unsolHandler (line1, line2);
		}
		
            free(line1);
        } else {
            processLine(line);
        }

#ifdef HAVE_ANDROID_OS
        if (s_ackPowerIoctl > 0) {
            /* acknowledge that bytes have been read and processed */
            ioctl(s_fd, OMAP_CSMI_TTY_ACK, &s_readCount);
            s_readCount = 0;
        }
#endif /*HAVE_ANDROID_OS*/
    }

    onReaderClosed();

    return NULL;
}

/**
 * Sends string s to the radio with a \r appended.
 * Returns AT_ERROR_* on error, 0 on success
 *
 * This function exists because as of writing, android libc does not
 * have buffered stdio.
 */
static int writeline (const char *s)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;



    if (s_fd < 0 || s_readerClosed > 0) {
        return AT_ERROR_CHANNEL_CLOSED;
    }
    /*   
    struct timeval tv;
    gettimeofday(&tv,NULL);
    ALOGD(" tv.sec %u :, tv.usec %u\n", tv.tv_sec,tv.tv_usec);
    */   
    if(RIL_DEBUG) ALOGD("AT> %s\n", s);
    AT_DUMP( ">> ", s, strlen(s) );

    /* the main string */
    while (cur < len) {
        do {
            written = write (s_fd, s + cur, len - cur);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return AT_ERROR_GENERIC;
        }

        cur += written;
    }

    /* the \r  */

    do {
        written = write (s_fd, "\r" , 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0) {
        return AT_ERROR_GENERIC;
    }

    return 0;
}
static int writeCtrlZ (const char *s)
{
    size_t cur = 0;
    size_t len = strlen(s);
    ssize_t written;

    if (s_fd < 0 || s_readerClosed > 0) {
        return AT_ERROR_CHANNEL_CLOSED;
    }

    if(RIL_DEBUG) ALOGD("AT> %s^Z\n", s);

    AT_DUMP( ">* ", s, strlen(s) );

    /* the main string */
    while (cur < len) {
        do {
            written = write (s_fd, s + cur, len - cur);
        } while (written < 0 && errno == EINTR);

        if (written < 0) {
            return AT_ERROR_GENERIC;
        }

        cur += written;
    }

    /* the ^Z  */

    do {
        written = write (s_fd, "\032" , 1);
    } while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0) {
        return AT_ERROR_GENERIC;
    }

    return 0;
}

static void clearPendingCommand()
{
    if (sp_response != NULL) {
        at_response_free(sp_response);
    }

    sp_response = NULL;
    s_responsePrefix = NULL;
    s_smsPDU = NULL;
}

/**
 * Starts AT handler on stream "fd'
 * returns 0 on success, -1 on error
 */
int at_open(int fd, ATUnsolHandler h)
{
    int ret;
    
    pthread_attr_t attr;

    s_fd = fd;
    s_unsolHandler = h;
    s_readerClosed = 0;

    s_responsePrefix = NULL;
    s_smsPDU = NULL;
    sp_response = NULL;

    /* Android power control ioctl */
#if 0
#ifdef HAVE_ANDROID_OS
#ifdef OMAP_CSMI_POWER_CONTROL
    ret = ioctl(fd, OMAP_CSMI_TTY_ENABLE_ACK);
    if(ret == 0) {
        int ack_count;
		int read_count;
        int old_flags;
        char sync_buf[256];
        old_flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, old_flags | O_NONBLOCK);
        do {
            ioctl(fd, OMAP_CSMI_TTY_READ_UNACKED, &ack_count);
			read_count = 0;
            do {
                ret = read(fd, sync_buf, sizeof(sync_buf));
				if(ret > 0)
					read_count += ret;
            } while(ret > 0 || (ret < 0 && errno == EINTR));
            ioctl(fd, OMAP_CSMI_TTY_ACK, &ack_count);
         } while(ack_count > 0 || read_count > 0);
        fcntl(fd, F_SETFL, old_flags);
        s_readCount = 0;
        s_ackPowerIoctl = 1;
    }
    else
        s_ackPowerIoctl = 0;

#else // OMAP_CSMI_POWER_CONTROL
    s_ackPowerIoctl = 0;

#endif // OMAP_CSMI_POWER_CONTROL
#endif /*HAVE_ANDROID_OS*/
#endif

    if(RIL_DEBUG)ALOGD(" at_open ");
    pthread_attr_init (&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    ret = pthread_create(&s_tid_reader, &attr, readerLoop, &attr);

    if (ret < 0) {
        perror ("pthread_create");
        return -1;
    }


    return 0;
}

/* FIXME is it ok to call this from the reader and the command thread? */
void at_close()
{
    if (s_fd >= 0) {
        close(s_fd);
    }
    s_fd = -1;

    pthread_mutex_lock(&s_commandmutex);

    s_readerClosed = 1;

    pthread_cond_signal(&s_commandcond);

    pthread_mutex_unlock(&s_commandmutex);

    /* the reader thread should eventually die */
}

static ATResponse * at_response_new()
{
    return (ATResponse *) calloc(1, sizeof(ATResponse));
}

void at_response_free(ATResponse *p_response)
{
    ATLine *p_line;

    if (p_response == NULL) return;

    p_line = p_response->p_intermediates;

    while (p_line != NULL) {
        ATLine *p_toFree;

        p_toFree = p_line;
        p_line = p_line->p_next;

        free(p_toFree->line);
        free(p_toFree);
    }

    free (p_response->finalResponse);
    free (p_response);
}

/**
 * The line reader places the intermediate responses in reverse order
 * here we flip them back
 */
static void reverseIntermediates(ATResponse *p_response)
{
    ATLine *pcur,*pnext;

    pcur = p_response->p_intermediates;
    p_response->p_intermediates = NULL;

    while (pcur != NULL) {
        pnext = pcur->p_next;
        pcur->p_next = p_response->p_intermediates;
        p_response->p_intermediates = pcur;
        pcur = pnext;
    }
}

static long long get_timeout_from_prefix()
{
    long long timeMsec;
    int i,total;
    TimeOutType timespacetype;
   
    struct { 
        int  atrequest;
        TimeOutType timeouttype;
    } matchTable[] = {
        /*you can designate the special AT command with the time out  lenth*/
        { RIL_REQUEST_SET_FACILITY_LOCK ,                              TIMEOUT_30000_MS   },
        { RIL_REQUEST_SEND_SMS,                                        TIMEOUT_180_S       },  //DTS2011122607120 hkf39947 2012-01-14
        { RIL_REQUEST_CDMA_SEND_SMS,                                   TIMEOUT_180_S       },  //DTS2011111804153 kf32792 20111205 added 
        { RIL_REQUEST_SETUP_DATA_CALL ,                                TIMEOUT_30000_MS   },
        { RIL_REQUEST_ENTER_SIM_PIN ,                                  TIMEOUT_30000_MS   },
        { RIL_REQUEST_ENTER_SIM_PUK ,                                  TIMEOUT_30000_MS   },
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3
        { RIL_REQUEST_VOICE_REGISTRATION_STATE,                        TIMEOUT_30000_MS   },        
        { RIL_REQUEST_DATA_REGISTRATION_STATE ,                        TIMEOUT_30000_MS   },
#else
        { RIL_REQUEST_REGISTRATION_STATE,                              TIMEOUT_30000_MS   },
        { RIL_REQUEST_GPRS_REGISTRATION_STATE ,                        TIMEOUT_30000_MS   },
#endif
//end modified by wkf32792 for android 3.x 20110815
        { RIL_REQUEST_QUERY_AVAILABLE_NETWORKS,                        TIMEOUT_180_S      },  //Modified by x84000798 2012-06-04 DTS2012060806174  
        { RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC,                 TIMEOUT_120_S      },
        { RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL,                    TIMEOUT_120_S      },
        { RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE,                      TIMEOUT_120_S      },
        { RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE,                      TIMEOUT_120_S      },
        { RIL_REQUEST_SMS_ACKNOWLEDGE,                                 TIMEOUT_10000_MS   },
    };

   if(at_request == 0){
         timeMsec = 30000; 
         return timeMsec;
   }
   	
    total  = sizeof(matchTable) / sizeof(matchTable[0]);
    for (i = 0 ; i < total ; i++) 
    {
        if(matchTable[i].atrequest == at_request) 
        {       
            timespacetype = matchTable[i].timeouttype;
            switch(timespacetype){
				
		        case TIMEOUT_500_MS:{
                	  timeMsec = 500;
                       goto done;
                }
				
                case TIMEOUT_5000_MS:{
                	  timeMsec = 5000;
                       goto done;
                }
                case TIMEOUT_10000_MS:{
                	  timeMsec = 10000;
                       goto done;
                } 
                case TIMEOUT_20000_MS:{
                	  timeMsec = 20000;
                      goto done;
                }
                case TIMEOUT_30000_MS:{
                	  timeMsec = 30000;
                       goto done;
                }
                case TIMEOUT_40000_MS:{
                	  timeMsec = 40000;
                       goto done;
                }
                case TIMEOUT_50000_MS:{
                	  timeMsec = 50000;
                       goto done;
                }
                case TIMEOUT_60_S:{
                	  timeMsec = 60000;
                       goto done;
                }
                case TIMEOUT_120_S:{
                	  timeMsec = 120000;
                       goto done;
                }
                case TIMEOUT_180_S:{
                	  timeMsec = 180000;
                       goto done;
                }
                default:
                	 timeMsec = 30000;  
                	 goto done;
            }
        }        
    }
    
    timeMsec = 30000;  
done:
    at_request = 0;
    return timeMsec; 
}

/**
 * Internal send_command implementation
 * Doesn't lock or call the timeout callback
 *
 * timeoutMsec == 0 means infinite timeout
 */
static int at_send_command_full_nolock (const char *command, ATCommandType type,
                    const char *responsePrefix, const char *smspdu,
                    long long timeoutMsec, ATResponse **pp_outResponse)
{
    int err = 0;

    long long timeMsec;
    struct timespec ts;

    if(sp_response != NULL) {
        err = AT_ERROR_COMMAND_PENDING;
        goto error;
    }

    //Begin add by fKF34305 2011-8-26 for DTS2011071904295
    if((strstr(command,"ATA") != NULL) || (strstr(command,"ATD") != NULL))
    {
        g_at_type=NO_CARRIER;
    }
    else
    {
	g_at_type=UNS_NO_CARRIER;
    }

      //End add by fKF34305 2011-8-26 for DTS2011071904295
     
	/* BEGIN DTS2011121100581 kf32792 20111216 removed */
    /*
    //BEGIN DTS2011120206698 kf32792 20111206 add, 规避cms error带OK问题
    if (1 == iNeedSleep)
    {
        ALOGD("sleep 0.1s");
    	usleep(100000);
        iNeedSleep = 0;
    }
    //END DTS2011120206698 kf32792 20111206 add, 规避cms error带OK问题
    */
    /* END DTS2011121100581 kf32792 20111216 removed */

    err = writeline (command);

    if (err < 0) {
        goto error;
    }

    s_type = type;
    s_responsePrefix = responsePrefix;
    s_smsPDU = smspdu;
    sp_response = at_response_new();

    /* BEGIN DTS2011121200398 kf32792 20111216 modified */
    if (NULL != strcasestr(command, "AT^HCMGS")) {
         timeMsec = 180000; 
         ALOGE("command is %s,timeout is 180s!", command);
    }
	else if (NULL != strcasestr(command, "AT+COPS=?")) {
         timeMsec = 180000; 
         ALOGE("**command is %s,timeout is 180s!**", command);
    }
    else {
         timeMsec = get_timeout_from_prefix();
    }
    /* END DTS2011121200398 kf32792 20111216 modified */
   
    setTimespecRelative(&ts, timeMsec);
    
    while (sp_response->finalResponse == NULL && s_readerClosed == 0) {
    	
          err = pthread_cond_timedwait(&s_commandcond, &s_commandmutex, &ts);
/*
*the code below: send the same AT command if the first has not got the responce, and if the second also has
*not got the responce,  goto the error and restart the rild.
*/
#if 0
          if ((err == ETIMEDOUT)&&(AtErrorCount >= 1)) {
                 err = AT_ERROR_TIMEOUT;
                 s_onTimeout();
                 goto error;
           }else if ((err == ETIMEDOUT)&&(AtErrorCount < 1)){ 
                 AtErrorCount++;
                 setTimespecRelative(&ts, timeMsec);
                 err = writeline (command);
                 if (err < 0) {
                        goto error;
                 }            
           }else{
                  AtErrorCount = 0;
           }
#endif   
/*
*ignore the lost AT command, go on if the timeout and do not restart rild
*/
        if(err == ETIMEDOUT)
        {
		if(RIL_DEBUG)ALOGD("*****The AT command is TIMEOUT, You  can extend the wait time of this AT command*****");
					s_MaxError++;//by alic：这是第二个我认为要重启dongle的地方，e1780有时候拨号发下去的at指令会超时，就会等待在这里.可是改了没用
					if (s_MaxError >= 1)/*在发送请求的时候根本不会打破读等待的阻塞*/
	        {
	            const char cmd[] = "2";
	            int fd;
	            fd = open("/sys/monitor/usb_port/config/run", O_RDWR | O_NONBLOCK);  
	            if (fd > 0)
	            {
	                if(RIL_DEBUG)ALOGD("/sys/monitor/usb_port/config/run sucess");
	                write(fd, cmd, sizeof(cmd));
	            }
	            close(fd);
	        }
	        if(RIL_DEBUG)ALOGD("*****The AT command is TIMEOUT, I think to restart.");
		break;
        }
   }
    
    if (pp_outResponse == NULL) {
        at_response_free(sp_response);
    } else {
        /* line reader stores intermediate responses in reverse order */
        reverseIntermediates(sp_response);
        *pp_outResponse = sp_response;
    }

    sp_response = NULL;

    if(s_readerClosed > 0) {
        err = AT_ERROR_CHANNEL_CLOSED;
        goto error;
    }

    err = 0;
error:
    clearPendingCommand();
    return err;
}

/**
 * Internal send_command implementation
 *
 * timeoutMsec == 0 means infinite timeout
 */
static int at_send_command_full (const char *command, ATCommandType type,
                    const char *responsePrefix, const char *smspdu,
                    long long timeoutMsec, ATResponse **pp_outResponse)
{
    int err;

    if (0 != pthread_equal(s_tid_reader, pthread_self())) {
        /* cannot be called from reader thread */
        return AT_ERROR_INVALID_THREAD;
    }

    /* 2011-12-27 lkf52201 commented.
     * 目前,RILD进程中有两个线程会下发AT命令,而下面的s_commmandmutex互斥锁变量在
     * at_send_command_full_nolock()的pthread_cond_timedwait()函数里面会被暂时解开,
     * 此时若另外一个线程要下发AT命令,则它可以获取到s_commandmutex锁,进而往下执行
     * at_send_command_full_nolock()函数,这会破坏函数中用到的一些全局变量的值,可能
     * 导致程序崩溃.所以下面在锁s_commandmutex变量之前,先对s_readdelmutex变量加锁,
     * 保证对下面资源的互斥访问,该锁是RIL在引进hwril_read_sms_then_delete_sms线程
     * 时(该线程在main_loop()里面创建),增加的一个锁,原先使用CMTI_ISSUE宏来作为预编
     * 译控制开关,现在将#ifdef CMTI_ISSUE语句去掉,默认使用s_readdelmutex互斥锁.
     */
    pthread_mutex_lock(&s_readdelmutex); 
    pthread_mutex_lock(&s_commandmutex);
   
    err = at_send_command_full_nolock(command, type,
                    responsePrefix, smspdu,
                    timeoutMsec, pp_outResponse);

    pthread_mutex_unlock(&s_commandmutex);
    pthread_mutex_unlock(&s_readdelmutex); 

    if(0 != err){
		ALOGD("***********8err=%d",err);
    }
    if (err == AT_ERROR_TIMEOUT && s_onTimeout != NULL) {
        s_onTimeout();
    }

    return err;
}

/**
 * Issue a single normal AT command with no intermediate response expected
 *
 * "command" should not include \r
 * pp_outResponse can be NULL
 *
 * if non-NULL, the resulting ATResponse * must be eventually freed with
 * at_response_free
 */
int at_send_command (const char *command, ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, NO_RESULT, NULL,
                                    NULL, 0, pp_outResponse);

    return err;
}


int at_send_command_singleline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, SINGLELINE, responsePrefix,
                                    NULL, 0, pp_outResponse);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_numeric (const char *command,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, NUMERIC, NULL,
                                    NULL, 0, pp_outResponse);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_sms (const char *command,
                                const char *pdu,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, SINGLELINE, responsePrefix,
                                    pdu, 0, pp_outResponse);

    if (err == 0 && pp_outResponse != NULL
        && (*pp_outResponse)->success > 0
        && (*pp_outResponse)->p_intermediates == NULL
    ) {
        /* successful command must have an intermediate response */
        at_response_free(*pp_outResponse);
        *pp_outResponse = NULL;
        return AT_ERROR_INVALID_RESPONSE;
    }

    return err;
}


int at_send_command_multiline (const char *command,
                                const char *responsePrefix,
                                 ATResponse **pp_outResponse)
{
    int err;

    err = at_send_command_full (command, MULTILINE, responsePrefix,
                                    NULL, 0, pp_outResponse);

    return err;
}

/* BEGIN DTS2012022004973  lkf52201 2012-02-21 added */
/*************************************************
Function:    at_send_command_oem_string
Description: RIL_REQUEST_OEM_HOOK_STRINGS请求调用该函数下发AT命令,并获取AT返回值
Calls:       at_send_command_full() 
Called By:   request_oem_hook_string()
Input:       const char *command; 要下发的AT命令字符串
Output:      ATResponse **pp_outResponse; 保存AT命令的返回值
Return:      int类型的变量, 0 表示该函数执行无误, 小于0 表示该函数执行有误
Others:      
*************************************************/
int at_send_command_oem_string(const char *command, ATResponse **pp_outResponse)
{
    int err = 0;

    err = at_send_command_full(command, OEM_STRING, NULL,
                                    NULL, 0, pp_outResponse);

    return err;
}
/* END   DTS2012022004973  lkf52201 2012-02-21 added */

/** This callback is invoked on the command thread */
void at_set_on_timeout(void (*onTimeout)(void))
{
    s_onTimeout = onTimeout;
}

/**
 *  This callback is invoked on the reader thread (like ATUnsolHandler)
 *  when the input stream closes before you call at_close
 *  (not when you call at_close())
 *  You should still call at_close()
 */

void at_set_on_reader_closed(void (*onClose)(void))
{
    s_onReaderClosed = onClose;
}


/**
 * Periodically issue an AT command and wait for a response.
 * Used to ensure channel has start up and is active
 */

int at_handshake()
{
    int i;
    int err = 0;

    if (0 != pthread_equal(s_tid_reader, pthread_self())) {
        /* cannot be called from reader thread */
        return AT_ERROR_INVALID_THREAD;
    }

    pthread_mutex_lock(&s_commandmutex);

    for (i = 0 ; i < HANDSHAKE_RETRY_COUNT ; i++) {
        /* some stacks start with verbose off */
        // err = at_send_command_full_nolock ("ATE0V1", NO_RESULT,
        err = at_send_command_full_nolock ("ATE0V1", NO_RESULT,
                    NULL, NULL, HANDSHAKE_TIMEOUT_MSEC, NULL);

        if (err == 0) {
            break;
        }
    }
    if (err == 0) {
        /* pause for a bit to let the input buffer drain any unmatched OK's
           (they will appear as extraneous unsolicited responses) */

        sleepMsec(HANDSHAKE_TIMEOUT_MSEC);
    }

    pthread_mutex_unlock(&s_commandmutex);

    return err;
}

/**
 * Returns error code from response
 * Assumes AT+CMEE=1 (numeric) mode
 */
AT_CME_Error at_get_cme_error(const ATResponse *p_response)
{
    int ret;
    int err;
    char *p_cur;
    if (p_response->success > 0) {
        return CME_SUCCESS;
    }
    if (p_response->finalResponse == NULL
        || !strStartsWith(p_response->finalResponse, "+CME ERROR:")
    ) {
        return CME_ERROR_NON_CME;
    }

    p_cur = p_response->finalResponse;
	err = at_tok_start(&p_cur);
    if (err < 0) {

        return CME_ERROR_NON_CME;
    }

    err = at_tok_nextint(&p_cur, &ret);

    if (err < 0) {

        return CME_ERROR_NON_CME;
    }
    return (AT_CME_Error) ret;
}

/**
 * dial at modem port
 * 
 */
int dial_at_modem(const char *cmd)
{
	/* BEGIN DTS2011110902480  lkf52201 2011-11-09 modified */
	int readlen = 0;
	char pch[25] = {0};
	time_t readtime;
	const char *match_msg = "CONNECT";
	const char *match_err_msg = "NO CARRIER";
	const char *match_mode_msg = "MODE:";
	/* END DTS2011110902480  lkf52201 2011-11-09 modified */
	time_t gettime; // l81004851 2012-3-12 added for pclint Warning
	
    int fd,len,written,cur,i;//修改pclint警告 KF36756 2011-04-21
    i = 0;
    cur = 0;
	/*delete check result*/
	#if 0
	char   ch[512]={0}, *pch;    // 用来接收数据的缓冲区，大小为512字节
    const char *match_msg = NULL;          // 上报匹配字符串
    const char *match_err_msg = NULL;
    fd_set read_set;
    struct timeval tvSelect;
	#endif
	
    if(cmd == NULL)
    {
        return AT_ERROR_GENERIC;
    }
	
    len = strlen(cmd);
    
   /*BEGIN Modified by z128629 DTS2011032203061:obtain tty by read usb description file after usb reset */
    if(SERIAL_PORT_MODEM_KEYNAME[0] == '\0')
    {
		sprintf(SERIAL_PORT_MODEM_KEYNAME,"/dev/ttyUSB0");
    }
    fd = open (SERIAL_PORT_MODEM_KEYNAME, O_RDWR | O_NONBLOCK);  // DTS2011110902480  lkf52201 2011-11-09 modified 
    if(RIL_DEBUG)ALOGD("dial_at_modem : open %s\n",SERIAL_PORT_MODEM_KEYNAME);
    if( fd < 0 )
    {
		ALOGD("dial_at_modem : open %s error! %s", SERIAL_PORT_MODEM_KEYNAME,strerror(errno));
		sprintf(SERIAL_PORT_MODEM_KEYNAME,"/dev/ttyUSB1");
		return AT_ERROR_GENERIC;
    }
   /*END Modified by z128629 DTS2011032203061:obtain tty by read usb description file after usb reset */

		
    struct termios tios;
    if (tcgetattr(fd, &tios) < 0) {
	    close(fd);
		if(RIL_DEBUG)ALOGD("modem : tcgetattr error %s",strerror(errno));
        return AT_ERROR_GENERIC;
	}

    tios.c_cflag     &= ~(CSIZE | CSTOPB | PARENB | CLOCAL);
    tios.c_cflag     |= CS8 | CREAD | HUPCL;
    tios.c_iflag      = IGNBRK | IGNPAR;
    tios.c_oflag      = 0;
    tios.c_lflag      = 0;
    tios.c_cc[VMIN]   = 1;
    tios.c_cc[VTIME]  = 0;
    tios.c_cflag ^= (CLOCAL | HUPCL);

    while (tcsetattr(fd, TCSAFLUSH, &tios) < 0 && !ok_error(errno))
    if (errno != EINTR)ALOGD("tcsetattr: (line %d)",__LINE__);

    tcflush(fd, TCIOFLUSH);	
    if(RIL_DEBUG)ALOGD("modem : write %s ",cmd);
    /* the main string */
    while (cur < len) 
    {
        do 
        {
            written = write (fd, cmd + cur, len - cur);
        } 
        while (written < 0 && errno == EINTR);

        if (written < 0) 
        {
        	close(fd);
			if(RIL_DEBUG)ALOGD("modem : write modem error %s",strerror(errno));
            return AT_ERROR_GENERIC;
        }
        cur += written;
    }
	
    /*BEGIN Added by z128629 +++ command can not add \r */
	if(!strcmp(cmd,"+++"))
	{
		close(fd);
        return AT_ERROR_GENERIC;  // 2011-12-09 lkf52201 modified
	}
	/*END Added by z128629 +++ command can not add \r */
	
    /* the \r  */
    len = 0;
    do 
    {
        written = write (fd, "\r" , 1);
    } 
    while ((written < 0 && errno == EINTR) || (written == 0));

    if (written < 0)
    {
    	close(fd);
		if(RIL_DEBUG)ALOGD("modem : write modem1 error");
        return AT_ERROR_GENERIC;
    }

    /* BEGIN DTS2011110902480  lkf52201 2011-11-09 modified */
	readtime = time(&gettime);
    // DTS2011111806466 lkf52201 2011-12-03 modified. 将下面while循环的延时从3s改为20s
    // 改完DTS2011111806466问题单后,再次修改延时时间,改为30s; 2011-12-09 lkf52201 modified
	while ((time(&gettime) - readtime) <= 50)
	{ 
		readlen = read(fd, pch, 20);
		if (readlen < 0 && errno == EINTR)
		{
			continue;
		}
		else if (readlen > 0)
		{
			pch[readlen] = '\0';
			if (RIL_DEBUG)ALOGD("====== read from modem:%s=======", pch); // 2011-12-09 lkf52201 modified
			if (strstr(pch, match_msg))
			{
				close(fd);
				return 0;
			}
			else if (strstr(pch, match_err_msg))
			{
				close(fd);
				return AT_ERROR_GENERIC;
			}
			else if (strstr(pch, match_mode_msg))
			{
			    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,NULL, 0);
			}
		}
	}
	/* END DTS2011110902480  lkf52201 2011-11-09 modified */
	close(fd);
	// 30s读取不到CONNECT或者NO CARRIER,则返回AT超时,将重新发起拨号; 2011-12-09 lkf52201 modified
	return AT_ERROR_TIMEOUT; 
	/*delete check result*/
	#if 0
	if(strstr(cmd,"AT+CGDATA"))
	{
		match_msg = "CONNECT";
        match_err_msg = "NO CARRIER";
	}
      else if(strstr(cmd,"atd#777")) //added by kf32055 20101210
	{
		match_msg = "CONNECT";
    	match_err_msg = "NO CARRIER";
	}
	else
	{
		match_msg = "\r\nOK\r\n";
    	match_err_msg = "NO CARRIER";
	}
	
    tvSelect.tv_sec  = 15;   //Modified by kf32055 20101210 5
    tvSelect.tv_usec = 0;
    pch = &ch[0];
	
    while (1)
    {
        while (1)
        {
            FD_ZERO(&read_set);
            FD_SET(fd, &read_set);

            ret = select(fd + 1, &read_set, NULL, NULL, &tvSelect);
            if (-1 == ret)
            {
                if (EINTR == errno)
                {
                    continue;
                }
                if(RIL_DEBUG)ALOGD("modem : select modem error");
				close(fd);
                return AT_ERROR_GENERIC;
            }
            else if (!ret)
            {            
            	close(fd);
				if(RIL_DEBUG)ALOGD("modem : select timeout");
                return AT_ERROR_GENERIC;
            }
            break;
        }
  
        if(RIL_DEBUG) ALOGD("select time is : sec = %d, ms = %d, us = %d \n",
        (int)tvSelect.tv_sec, (int)(tvSelect.tv_usec / 1000), (int)(tvSelect.tv_usec % 1000));

        while (1)
        {
            ret = read(fd, pch, 64); // 每次最多读出64字节
            if (-1 == ret)
            {           
                if (EINTR == errno)
                {
                    continue;
                }
                if(RIL_DEBUG)ALOGD("read modem_fd error\n");
				close(fd);
                return AT_ERROR_GENERIC;
            }
			else if(0 == ret)/*add case: module reset*/
			{
				close(fd);
				if(RIL_DEBUG)ALOGD("read eof error\n");
                return AT_ERROR_GENERIC;
			}
            break;
        }

        pch[ret] = '\0';
        if(RIL_DEBUG)ALOGD("modem read %s len=%d",pch,ret);
        pch += ret;
        if (pch > &ch[512-64]) 
        {
        	close(fd);
			if(RIL_DEBUG)ALOGD("pch > &ch error\n");
            return AT_ERROR_GENERIC;
        }
        if(match_err_msg == NULL )
        {
            if(RIL_DEBUG)ALOGD("match_err_msg == NULL\n");
        }
        else
        {
            if(RIL_DEBUG)ALOGD("match_err_msg =%s\n",match_err_msg);
        }

        if(match_msg == NULL )
        {
            if(RIL_DEBUG)ALOGD("match_msg == NULL\n");
        }
        else
        {
            if(RIL_DEBUG)ALOGD("match_msg =%s\n",match_msg);
        }
        if (strstr(ch, match_msg))             /* 如果上报了正确匹配字符串 */
        {
            if(RIL_DEBUG)ALOGD("return MODEM_OK of match_msg");
			close(fd);
            return 0;
        }
        else if (strstr(ch, match_err_msg))  /* 如果上报了错误匹配字符串 */
        {
            if(RIL_DEBUG)ALOGD("receive at result %s\n", match_err_msg);
			close(fd);
            return AT_ERROR_GENERIC;
        }
        else
        {
            if (pch > &ch[512-64]) 
            {
                if(RIL_DEBUG)ALOGD("return no match message at all !!");
				close(fd);
                return AT_ERROR_GENERIC;
            }
        }
	}
	#endif
}
