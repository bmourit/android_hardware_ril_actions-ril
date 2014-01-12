/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-ss.c
 Version: 
 Description:
    handle decode and encode for ussd        
 Function List:
 History:
 <author>             <time>          <version>                    <desc>
 wkf32792             2011/11/10      RIL-V100R005B002D03SP00C03       
*******************************************************/

#include "gsm_7bit.h"

/************************************************
 Function:  GSM_7DefaultAlphabet
 Description:  
    �ܷ���gsm-7bitȱʡ���ַ���ʾ
 Calls: 
 Called By: 
    GSM_7BitAlphabet
 Input:
    ĳ�����ַ�
 Output:
    none
 Return:
    1:�ܣ� 0:��
 Others:    
**************************************************/
static int GSM_7DefaultAlphabet(WCHAR wChar)
{
    int n = 0;
    for (n = 0; n < 128; n++)
    {
        if (wChar == GSM_7DefaultTable[n])
        {
            return 1;
        }
    }
    return 0;
}

/************************************************
 Function:  GSM_7ExtAlphabet
 Description:  
    �ܷ���gsm-7bit��չ���ַ���ʾ
 Calls: 
 Called By: 
    GSM_7BitAlphabet
 Input:
    ĳ�����ַ�
 Output:
    none
 Return:
    1:�ܣ� 0:��
 Others:    
**************************************************/
static int GSM_7ExtAlphabet(WCHAR wChar)
{
    int n = 0;
    for (n = 0; n < 12; n++)
    {
        if (wChar == Simple_GSM_7ExtTable[n].value)
        {
            return 1;
        }
    }
    return 0;
}

/************************************************
 Function:  GSM_7BitAlphabet
 Description:  
    �ܷ���gsm-7bit���ַ���ʾ
 Calls: 
    GSM_7DefaultAlphabet��GSM_7ExtAlphabet
 Called By: 
    GSM_7BitAlphabet
 Input:
    ĳ�����ַ�
 Output:
    none
 Return:
    1:�ܣ� 0:��
 Others:    
**************************************************/
int GSM_7BitAlphabet(WCHAR wChar)
{
    if (!GSM_7DefaultAlphabet(wChar))
    {
        return GSM_7ExtAlphabet(wChar);
    }
    return 1;
}

/************************************************
 Function:  UnicodeToGsm7bit
 Description:  
    Unicode����ת����Gsm7bit����
 Calls: 
 Called By: 
    request_send_ussd
 Input:
    Unicode���ݣ�����
 Output:
    gsm7bit����
 Return:
    gsm7bit���ݳ���
 Others:    
**************************************************/
int UnicodeToGsm7bit(WCHAR* src, char* dest, int len)
{
	int i = 0, n = 0;
	int iLen = 0;
	for (i = 0; i < len; i++)
	{  
	    for (n = 0; n < 128; n++)
	    {
	        if (src[i] == GSM_7DefaultTable[n])
	        {
	        	dest[iLen++] = n;
	        	break;
	        }	        
	    }
	    if (n == 128)
	    {
	        for (n = 0; n < 128; n++)
	        {
	            if (src[i] == GSM_7ExtTable[n])
	            {
	                dest[iLen++] = ESCAPE_CHAR;
	                dest[iLen++] = n;
	                break;
	            }
	        }
	    }
    }
	
	return iLen;
}

/************************************************
 Function:  Gsm7bitToUnicode
 Description:  
    Gsm7bit����ת����Unicode����
 Calls: 
 Called By: 
    hwril_unsolicited_ss
 Input:
    gsm7bit����, ����,
 Output:
    Unicode����
 Return:
    Unicode���ݳ���
 Others:    
**************************************************/
int Gsm7bitToUnicode(char* src, WCHAR* dest, unsigned int len)
{
    unsigned int i = 0, iLen = 0;
    unsigned char temp = 0;
    unsigned short result = 0;
    const WCHAR* pDfTable = GSM_7DefaultTable;
    const WCHAR* pExtTable = GSM_7ExtTable;       	
    while (i < len) 
    {            
        temp = src[i];
        if (temp != ESCAPE_CHAR) {
            result = pDfTable[temp];            
        } else {                              
            temp = src[++i];   
            result = pExtTable[temp];            
        } 
        if (0xFFFF == result) {
            result = ' ';            
        }
        dest[iLen++] = result;
        i++;
    }
    return iLen;
}

/************************************************
 Function:  Gsm7bitToUnicodeExt
 Description:  
    Gsm7bit����ת����Unicode����
 Calls: 
 Called By: 
    hwril_unsolicited_ss
 Input:
    gsm7bit����, ����, �������Ա�־
 Output:
    Unicode����
 Return:
    Unicode���ݳ���
 Others:    
**************************************************/
int Gsm7bitToUnicodeExt(char* src, WCHAR* dest, unsigned int len, LANGUAGEID LanID)
{
    unsigned int i = 0, iLen = 0;
    unsigned char temp = 0;
    unsigned short result = 0;
    const WCHAR* pDfTable = GSM_7DefaultTable;
    const WCHAR* pExtTable = GSM_7ExtTable;   
    if (SPANISH == LanID)
    {
        pExtTable = SpanishExtAlphabet_7bit;
    }
    else if (PORTUGUESE == LanID)
    {
        pDfTable = PortugueseAlphabet_7bit;
        pExtTable = PortugueseExtAlphabet_7bit;
    }
    else if (TURKISH == LanID)
    {
        pDfTable = TurkishAlphabet_7bit;
        pExtTable = TurkishExtAlphabet_7bit;
    }
    	
    while (i < len) 
    {            
        temp = src[i];
        if (temp != ESCAPE_CHAR) {
            result = pDfTable[temp];            
        } else {                              
            temp = src[++i];   
            result = pExtTable[temp];            
        } 
        if (0xFFFF == result) {
            result = ' ';            
        }
        dest[iLen++] = result;
        i++;
    }
    return iLen;
}

/************************************************
 Function:  CompressGsm7bit
 Description:  
    ѹ��Gsm7bit����
 Calls: 
 Called By: 
    request_send_ussd
 Input:
    gsm7bit���ݣ�����
 Output:
    ѹ�����gsm7bit����
 Return:
    ѹ�����gsm7bit���ݳ���
 Others:    
**************************************************/
int CompressGsm7bit(const char* pszSrc, unsigned char* puszDst, int iSrcLength)
{
    int nSrc;        
    int nDst;        
    int nChar;       
    unsigned char nLeft = 0;   
    nSrc = 0;
    nDst = 0;

    if (pszSrc == NULL || puszDst == NULL || iSrcLength == 0)
    {
        return 0;
    }

    while ( nSrc < iSrcLength)
    {
        nChar = nSrc & 7;

        if(nChar == 0)
        {
            nLeft = *pszSrc;
        }
        else
        {
            // ���������ֽڣ������ұ߲��������������ӣ��õ�һ��Ŀ������ֽ�
            *puszDst = (*pszSrc << (8 - nChar)) | nLeft;

            // �����ֽ�ʣ�µ���߲��֣���Ϊ�������ݱ�������
            nLeft = *pszSrc >> nChar;

            puszDst++;
            nDst++; 
        } 

        pszSrc++; 
        nSrc++;
    }


    if ((iSrcLength % 8) != 0)
    {
        *puszDst = nLeft;
        nDst++;

        // USSD���룬����ַ�����Ϊ8N-1�������һ���ֽ�ǰ��7λ���0x0D.
        if( iSrcLength % 8 == 7 )
        {
            *puszDst = (*puszDst & 0x1) | (0x0D << 1);
        }
        puszDst++;
    }
    else
    {
        // ���USSD��������Ϊ8�ı����������һ���ַ�ʱ0x0D������Ҫ����һ��0x0D, TS 23.038, 6.1.2.3.1
        pszSrc--;
        if(*pszSrc == 0x0D)
        {
            *puszDst++ = 0x0D;
            nDst++;
        }
    }
    
    puszDst = 0;
    return nDst; 
}

/************************************************
 Function:  UncompressGsm7bit
 Description:  
    ��ѹ��Gsm7bit����
 Calls: 
 Called By: 
    hwril_unsolicited_ss
 Input:
    ѹ����gsm7bit���ݣ�����
 Output:
    gsm7bit����
 Return:
    gsm7bit���ݳ���
 Others:    
**************************************************/
int UncompressGsm7bit(const char* puszSrc, char* pszDst, int iSrcLength)
{
    
    int nSrc = 0;        
    int nDst = 0;        
    int nByte = 0;       
    unsigned char nLeft = 0;   

    if (puszSrc == NULL || pszDst == NULL || iSrcLength == 0)
    {
        return 0;
    }

    while (nSrc < iSrcLength)
    {
        // ��Դ�ֽ��ұ߲��������������ӣ�ȥ�����λ���õ�һ��Ŀ������ֽ�
        *pszDst = ((*puszSrc << nByte) | nLeft) & 0x7f;
        // �����ֽ�ʣ�µ���߲��֣���Ϊ�������ݱ�������
        nLeft = *puszSrc >> (7 - nByte);

        pszDst++;
        nDst++;

        nByte++;

        if(nByte == 7)
        {
            *pszDst = nLeft;
            pszDst++;
            nDst++;

            nByte = 0;
            nLeft = 0;
        }

        puszSrc++;
        nSrc++;
    }

    *pszDst = 0;

    return nDst;
}

/************************************************
 Function:  gsmDecode8bit
 Description:  
    gsm8bit����ת����unicode����
 Calls: 
 Called By: 
    hwril_unsolicited_ss
 Input:
    gsm8bit���ݣ�����
 Output:
    unicode����
 Return:
    unicode���ݳ���
 Others:    
**************************************************/
int gsmDecode8bit(char* pSrc, WCHAR* pDst, int nSrcLength)
{

    if (NULL == pDst || NULL == pSrc)
    {
        return 0;
    }
 
    int i = 0;
    for (i = 0; i < nSrcLength; i++)
	{
		pDst[i] = pSrc[i];
	}
	pDst[i] = 0;

	return nSrcLength;
}

int hexCharToInt(char c)
{
	if (c >= '0' && c <= '9') 
		return (c - '0');
	if (c >= 'A' && c <= 'F') 
		return (c - 'A' + 10);
	if (c >= 'a' && c <= 'f') 
		return (c - 'a' + 10);
	return 0;
}

/************************************************
 Function:  hexStringToBytes
 Description:  
    16�����ַ�����ת��Ϊ�ֽ�����, "ABCD" ---> {0xAB,0xCD}
 Calls: 
 Called By: 
    request_send_ussd, request_sim_io, cdma_pdu_decode
 Input:
   16�����ַ�����,16�����ַ����鳤��
 Output:
    �ֽ�����
 Return:
    �ֽ����鳤��
 Others:    
**************************************************/
int hexStringToBytes(char * s,char *hs ,int len)
{	
	    int i;
        if (s == 0) return -1;

        int sz = strlen(s);

        for (i=0 ; i <sz ; i+=2) {
			if(i/2 >= len)
				return -1;
	 		hs[i/2] = (char) ((hexCharToInt(s[i]) << 4) 
	                                | hexCharToInt(s[i+1]));
        }
        return (sz+1) / 2;
}

/************************************************
 Function:  BytesToHexstring
 Description:  
    ת���ֽ�����Ϊ16�����ַ�����, {0xAB,0xCD} ---> "ABCD"
 Calls: 
 Called By: 
    request_send_ussd
 Input:
   �ֽ�����,�ֽ����鳤��
 Output:
    16�����ַ�����
 Return:
    16�����ַ����鳤��
 Others:    
**************************************************/
int BytesToHexstring(const unsigned char *src, char *dest, int SrcLen)
{
    const char tab[] = "0123456789ABCDEF";    /* Character table */
    int i = 0, iLen = 0;
	for(i = 0; i < SrcLen; i++)
    {
        dest[iLen++] = tab[*src >> 4];      /* High 4 bits of 'src' */
        dest[iLen++] = tab[*src & 0x0f];   /* low 4 bits of 'src' */
        src++;
    }

    dest[iLen] = '\0';    
    return iLen;
}

/************************************************
 Function:  IsLittleEndian
 Description:  
    �жϵ�ǰ�����ֽ�˳��
 Calls: 
 Called By: 
    WidecharToByte
 Input:
    none
 Output:
    none
 Return:
    1:С���ֽ�˳�� 0:����ֽ�˳��
 Others:    
**************************************************/
int IsLittleEndian()
{
    long testData = 1;
    char* psz = (char*)&testData;
    return (1 == *psz);
}

/************************************************
 Function:  Utf8ToUnicode
 Description:  
    utf-8����ת����unicode����
 Calls: 
 Called By: 
    request_send_ussd
 Input:
    utf-8�ַ���������
 Output:
    unicode����
 Return:
    unicode���ݳ���
 Others:    
**************************************************/
int Utf8ToUnicode(const char* src, WCHAR* dest, int SrcLen)
{
    int i = 0, ilen = 0;
    WCHAR tmp = 0;
    while(i < SrcLen)
    {
        tmp = 0;
        if ((*src & 0x80) == 0)//one byte
        {           
            dest[ilen++] = *src++;
        }
        else if((*src & 0xe0) == 0xc0) //two bytes
        {
            tmp |= ((*src++) & 0x1f) << 6; 
            if((*src != '\0') && (*src & 0xc0) == 0x80)
            {       
                    tmp |= (*src++) & 0x3f;
            }
            dest[ilen++] = tmp;
    
        }
        else if((*src & 0xf0) == 0xe0)//three bytes
        {
            tmp |= ((*src ++ ) & 0x0f) << 12;
            if ((*src != '\0') && (*src & 0xc0) == 0x80)
            {
                tmp |= ((*src ++ ) & 0x3f) << 6;
            }
            if ((*src != '\0') && (*src & 0xc0) == 0x80)
            {
                tmp |= (*src ++ ) & 0x3f;
            }
            dest[ilen++] = tmp;            
        }
        i++;
    }
    return ilen;  
}

/************************************************
 Function:  UnicodeToUtf8
 Description:  
    unicode����ת����utf-8����
 Calls: 
 Called By: 
    hwril_unsolicited_ss
 Input:
    unicode�ַ���������
 Output:
    utf-8����
 Return:
    utf-8���ݳ���
 Others:    
**************************************************/
int UnicodeToUtf8(const WCHAR* src, char* dest, int len)
{
    int i = 0, iLen = 0;
    WCHAR tmp = 0;
	unsigned char a,b,c;
	while(i < len)
	{     
        tmp = *src++;
        if(tmp > 0x7ff)//to three bytes
        {
            a = 0xe0;
            b = 0x80;
            c = 0x80;
            c |= tmp & 0x3f;
            b |= (tmp >> 6) & 0x3f;
            a |= (tmp >> 12) & 0x0f;
            dest[iLen++] = a;
            dest[iLen++] = b;
            dest[iLen++] = c;
        }
        else if(tmp > 0x7f)//to two bytes
        {
            a = 0xc0;
            b = 0x80;
            b |= tmp & 0x3f;
            a |= (tmp >> 6) & 0x1f;
            dest[iLen++] = a;
            dest[iLen++] = b;
        }
        else //to one byte
        {
        	dest[iLen++] = tmp & 0xff;
        }
        i++;
	}  
	return iLen;
}

/************************************************
 Function:  WidecharToByte
 Description:  
    ���ַ�ת�����ֽ�����
 Calls: 
 Called By: 
    request_send_ussd
 Input:
    ���ַ���������
 Output:
    �ֽ�����
 Return:
    �ֽ����ݳ���
 Others:    
**************************************************/
int WidecharToByte(const WCHAR* src, unsigned char* dst, unsigned long srcLen)
{
    unsigned int i = 0;
    WCHAR* pTemp = NULL;     
    pTemp = (WCHAR*) dst;    
    for(i = 0; i < srcLen; i++)
    {       
        if (!IsLittleEndian())  
        {
            /* ����ֽ�˳�� */
            *(pTemp+i) = *(src+i);
        }
        else  
        {
            /* С���ֽ�˳�� */
            *(pTemp+i) = *(src+i) << 8;
            *(unsigned char*)(pTemp+i) = *(src+i) >> 8;           
        }
    }
    return srcLen*2;     
}

/************************************************
 Function:  BytesToWidechar
 Description:  
    �ֽ�����ת���ɿ��ַ�
 Calls: 
 Called By: 
    hwril_unsolicited_ss
 Input:
    �ֽ����ݣ�����
 Output:
    ���ַ�����
 Return:
    ���ַ����ݳ���
 Others:    
**************************************************/
int BytesToWidechar(const char* src, WCHAR* dest, int len)
{
    int i = 0, iLen = 0;
    while (i < len/2)
    {
        dest[iLen] = *src++ << 8;
        dest[iLen] |= *src++;
        iLen++;
        i++;
    }
    return iLen;
}
