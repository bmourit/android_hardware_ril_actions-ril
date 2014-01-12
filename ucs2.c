/********************************************************
 Copyright (C), 1988-2012, Huawei Tech. Co., Ltd.
 File Name: ucs2.c
 Version: 
 Description:
    Handle ucs2 80, 81, 82 format string in SIM card.       
 Function List:
 	1. str_cut_ffff
 	2. str_cut_ff
 	3.ucs2_8182_to_unicode
 	4.UCS2_to_UTF8
 History:
 <author>				<time>			<version>		<desc>
 x00134282/x84000798    2012/06/29      1.0       
*******************************************************/

#include "gsm_7bit.h" 
//#include <utils/Log.h>
#include "huawei-ril.h"

/************************************************
 Function:  str_cut_ffff
 Description:  
    For ucs2 80 format, cutting off the "FFFF"tail and get the useable string.
 Input:
 	src - the hexstring from sim
 	len - length of the useable string
 Output:
 	dst - the useable string
 Return: none
 Others: 
**************************************************/
void str_cut_ffff( char* dst,char *src ,int len)
{
    int i=0;
	char tmp[5] = {0};
	char *p = src;
    while(i < len)
    {   
    	strncpy(tmp, p, 4);
		/* "FFFF" is unuseable */
        if(strncmp(tmp, "FFFF",4) == 0)
        {
            break;
        }
        i += 4;
        p += 4;
    }
	strncpy(dst,src,i);
    dst[i] = '\0';
    LOGD("dst=%s",dst);
	return;
}
/************************************************
 Function:  str_cut_ff
 Description:  
    For ucs2 81, 82 format, cutting off the "FF"tail and get the useable string.
 Input:
 	src - the hexstring from sim
 	len - length of the useable string
 Output:
 	dst - the useable string
 Return: none
 Others: 
**************************************************/
void str_cut_ff(char* dst,char *src ,int len)
{
	int i=0;
	char tmp[3] = {0};
	char *p = src;
	while(i < len)
	{   
		strncpy(tmp, p, 2);
		/* "FF" is unuseable */
	    if(strncmp(tmp, "FF",2) == 0)
	    {
	        break;
	    }
	    i += 2;
	    p += 2;
	}
	strncpy(dst,src,i);
	dst[i] = '\0';
	LOGD("dst=%s",dst);
	return ;
}

/***************************************************
 Function:  ucs2_8182_to_unicode
 Description:  
    1. Transform the ucs2 81,82 format to unicode,
    2. After transformation, original char format is also changed to wide char.
 Input:
    base - the ucs2 base.
    src  - actual characters
    len  - actual characters length
 Output:
    dst  - standard unicode string
 Return: none
 Others:
**************************************************/
void ucs2_8182_to_unicode(WCHAR base,WCHAR *dst, char *src, int len)
{
	int i =0;
	WCHAR tmp;
	while(i < len)
	{
		if((src[i] & 0x80) != 0)
		{
		 	tmp = src[i] & 0x7f;
			tmp = base + tmp;
		}
		else
		{
		 	tmp = src[i];
		}
		dst[i] = tmp;
		i++;
	}
}

/***************************************************
 Function:  UCS2_to_UTF8
 Description:  
    Transform the ucs2 string, 80, 81 and 82 format, in SIM card to UTF-8. 
 Input:
    src - String input.
 Output:
    dst - String output.
 Return:
    int - success: 0, fail: -1.
 Others:
**************************************************/
int UCS2_to_UTF8(char *src, char *dst)
{
	char c_ucs_format[3] = {0};	//ucs2 format: 80, 81, 82

	char * p_str_start = NULL; 	//Mark where the actual string characters start.
	char str_code[128] = {0};   //store actual characters.
	char str_tmp[128] = {0};   //store actual characters.

	WCHAR base = 0;
	WCHAR wsc_code[128] = {0};

	if ((NULL == src) || (NULL == dst))
	{
		return -1;
	}
	strncpy(c_ucs_format, src, 2);
	c_ucs_format[2] = '\0';

	if (0 == strcmp(c_ucs_format, "80"))
	{
		p_str_start = src + 2;
		//cut off additional tail "FFFF"
		str_cut_ffff(str_code , p_str_start, strlen(p_str_start));
		int len = hexStringToBytes(str_code, str_tmp, 128);
		len = BytesToWidechar(str_tmp, wsc_code, len);	//to Unicode
		len = UnicodeToUtf8(wsc_code, dst, len);
		dst[len] = '\0';
		
	}
	else if(0 == strcmp(c_ucs_format, "81"))
	{ 
		p_str_start = src + 4;
		//cut off additional tail "FF"
		str_cut_ff(str_code, p_str_start, strlen(p_str_start));
		int len = hexStringToBytes(str_code, str_tmp, 128);
		//get ucs2 base code
		base = ((WCHAR)str_tmp[0] << 7) & 0x7ffe;

		ucs2_8182_to_unicode(base, wsc_code, &(str_tmp[1]), len-1);
		len = UnicodeToUtf8(wsc_code, dst, len - 1);
		dst[len] = '\0';
	}
	else if(0 == strcmp(c_ucs_format, "82"))
	{
		p_str_start = src + 4;
		//cut off additional tail "FF"
		str_cut_ff(str_code, p_str_start, strlen(p_str_start));
		int len = hexStringToBytes(str_code, str_tmp, 128);
		//get ucs2 base code
		base = ((WCHAR)str_tmp[0] << 8) | (WCHAR)str_tmp[1];

		ucs2_8182_to_unicode(base, wsc_code, &(str_tmp[2]), len-2);
		len = UnicodeToUtf8(wsc_code, dst, len - 2);
		dst[len] = '\0';
	}
	else 
	{
		return -1; //not ucs2 
	}
	return 0;
}

