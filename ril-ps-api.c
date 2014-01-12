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
 fangyuchuang              10/04/07                      1.0   
 fangyuchuang              10/07/28                      1.1   
 hushanshan                10/08/20                      1.2
 hushanshan                10/08/30                      1.3

*******************************************************/
#include "ioctl.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <cutils/properties.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

#include "huawei-ril.h"
#include "ril-ps-api.h"

/*-------------------------------------------------------
		 Macro for constant
  -------------------------------------------------------*/
int ifc_init(void);
int ifc_close(void);
int ifc_set_addr(const char *name, unsigned addr);
int ifc_create_default_route(const char *name, unsigned addr);
int ifc_remove_default_route(const char *name);
int ifc_up(const char *name);
int ifc_down(const char *name);
extern void onunsol_data_call_list_chanage();

char shell[1024];
extern char ip_address[64];
extern char dns_address[64];
extern char gateway_address[64];
extern char *rmnet0_response[3];
extern char SERIAL_PORT_MODEM_KEYNAME[60]; /*Added by z128629 DTS2011032203061:obtain tty by read usb description file after usb reset */

//Bebin to be added by c00221986 for ndis
extern char PORT_ECM_KEYNAME[60];
//End being added by c00221986 for ndis

extern int g_pspackage; //Added by x84000798 for PS package test
extern MODEM_Vendor g_modem_vendor;
/* BEGIN DTS2011111806466  lkf52201 2011-12-03 added */
/* ip_main_loop()函数的while循环里面会判断terminal_connect变量是否为1，如果为1，则
 * goto error，结束获取ip的这个过程；当pppd退出时，ppp_main_loop()函数将该变量设为1.
 */
int terminal_connect = 0;
/* END DTS2011111806466  lkf52201 2011-12-03 added */

extern char g_apn[65];
extern char g_pdptype[65];//add by wkf32792 20110817 for android3.x

extern Module_Type g_module_type;

/***************************************************
 Function:  char_to_int
 Description:  
 	change a char type to an int type
 Calls: 
 	none
 Called By:
	none
 Input:
	ch - pointer to char
 Output:
      no
 Return:
	int type
 Others:
	none
**************************************************/
int char_to_int(char *ch)
{
	int ret;
	if((*ch >= '0')&&(*ch <= '9')){
		ret = *ch - '0';
	}else if((*ch >= 'a')&&(*ch <= 'z')){
		ret = *ch - 'a' + 10;	
	}else if((*ch >= 'A')&&(*ch <= 'Z')){
		ret = *ch - 'A' + 10;
	}else{
		ret = 0;
	}
	return ret;    	
}

/***************************************************
 Function:  hex_address_string_to_int
 Description:  
 	transfer a hex string to an int ,for example: transfer sting "6789abcd" to int 0x6789abcd 
 Calls: 
 	char_to_int
 Called By:
	set_ip
	hex_address_sting_to_network_string
	set_gateway
 Input:
	address - pointer to hex string
 Output:
      no
 Return:
	int type
 Others:
	none
**************************************************/
int hex_address_string_to_int(char *address)
{  
	int tmp[4];
	int len,i;
	char dns_string[8];
	int ret = 0;

	len = strlen(address);
	memset(dns_string,'0',8);
	memset(tmp,0,4*sizeof(tmp[0]));

	memcpy(dns_string+8-len, address, len);

	for(i=0; i<4; i++){
		tmp[i] = char_to_int(dns_string+i*2)*16 + char_to_int(dns_string+i*2+1);
	}

	//ret = tmp[0] | (tmp[1]<< 8) | (tmp[2]<<16) | (tmp[3] << 24); 

	ret = tmp[3] | (tmp[2]<< 8) | (tmp[1]<<16) | (tmp[0] << 24);
	return ret; 	
}

/***************************************************
 Function:  int_to_netwrok_string
 Description:  
 	transfer int type to network string. for example: transfer 0x0a0b2284 to "10.11.34.116"
 Calls: 
 	none
 Called By:
	hex_address_sting_to_network_string
 Input:
	address - int
 Output:
      no
 Return:
       pointer to a network string 
 Others:
	none
**************************************************/
char *int_to_netwrok_string(int address)
{
	char *ret;
	int tmp[4],i;

	for(i=0; i<4; i++)
	{
		tmp[i]=(address >> (i*8))&0xFF;
	}
	asprintf(&ret,"%d.%d.%d.%d",tmp[0],tmp[1],tmp[2],tmp[3]);
	return ret;   
}

/***************************************************
 Function:  hex_address_sting_to_network_string
 Description:  
 	transfer a hex string  to a network string. for example: transfer "0a0b2284" to "10.11.34.116"
 Calls: 
 	hex_address_string_to_int
 	int_to_netwrok_string
 Called By:
	set_ip
	set_gateway
 Input:
	address - pointer to hex string
 Output:
      no
 Return:
	pointer to a network string 
 Others:
	none
**************************************************/
char *hex_address_sting_to_network_string(char* address)
{
	char *ret;

	int net_address_int;
	net_address_int = hex_address_string_to_int(address);
	ret = int_to_netwrok_string(net_address_int);
	return ret;
}

/***************************************************
 Function:  set_dns
 Description:  
 	set the android and Linux dns.
 Calls: 
 	hex_address_sting_to_network_string
 Called By:
	none
 Input:
       i - index which dns to be set. for example dns1\dns2 
	dns - pointer to hex string
 Output:
      no
 Return:
	pointer to a network string 
 Others:
	none
**************************************************/
void set_dns(int i, char *dns)
{
	char *str = NULL;	
	char dns1[64];
	char dns2[64];

	if(i == 1){
		str = hex_address_sting_to_network_string(dns);
		sprintf(dns1, "net.%s.dns1",PORT_ECM_KEYNAME);
		//property_set("net.eth1.dns1",str);
		if(RIL_DEBUG)ALOGD("dns1 is:%s -- %s \n",dns1,str);
		property_set(dns1,str);
		strncpy(dns_address, str, strlen(str));
		if(str != NULL)
			free(str);
	}else if(i == 2){
		str =hex_address_sting_to_network_string(dns);
		sprintf(dns2, "net.%s.dns2",PORT_ECM_KEYNAME);
		//property_set("net.eth1.dns2",str);
		if(RIL_DEBUG)ALOGD("dns2 is:%s -- %s \n",dns2,str);
		property_set(dns2,str);
		strncpy(dns_address, str, strlen(str));
		if(str != NULL)
			free(str);
	}
	return ;
}

/***************************************************
 Function:  set_ip
 Description:  
 	set the android and Linux ip address.
 Calls: 
 	hex_address_string_to_int
 	hex_address_sting_to_network_string
 Called By:
	none
 Input:
	ip  - pointer to hex string
 Output:
      no
 Return:
	0   set ip successful
	-1 set ip fail
 Others:
	none
**************************************************/
int set_ip(char *ip)
{   
	int ip_int;
	char *ip_string;

	ip_int = hex_address_string_to_int(ip);
	if(RIL_DEBUG) ALOGD( "**ip_int is: %d",ip_int);
	//if (ifc_set_addr("eth1", ip_int)) {
	if (ifc_set_addr(PORT_ECM_KEYNAME, ip_int)) {
		if(RIL_DEBUG) ALOGD( "failed to set set_ip");
		return -1;
	}

	ip_string = hex_address_sting_to_network_string(ip);
	if(RIL_DEBUG) ALOGD( "**ip_string is: %s",ip_string);
	strncpy(ip_address, ip_string, strlen(ip_string));
	rmnet0_response[2] = ip_address;

	return 0;
}

/***************************************************
 Function:  set_gateway
 Description:  
 	set the android and Linux gateway.
 Calls: 
 	hex_address_sting_to_network_string
 Called By:
	none
 Input:
	address - pointer to hex string
 Output:
      no
 Return:
	0  set gateway successufu
      -1 set gateway fail 
 Others:
	none
**************************************************/
int set_gateway(char *gateway)
{
	int gw_int;
	char *gw_string;
	char net_gateway[64];

	gw_string = hex_address_sting_to_network_string(gateway);
	if(RIL_DEBUG) ALOGD( "***gateway is :%s \n",gateway);
	strncpy(gateway_address, gw_string, strlen(gw_string));
	sprintf(net_gateway, "net.%s.gw",PORT_ECM_KEYNAME);
	if(RIL_DEBUG) ALOGD( "net_gateway is :%s -- %s \n",net_gateway,gw_string);
	//property_set("net.eth1.gw", gw_string);
	property_set(net_gateway, gw_string);
	free(gw_string);

	gw_int = hex_address_string_to_int(gateway);
	//if (ifc_create_default_route("eth1", gw_int)) {
	if (ifc_create_default_route(PORT_ECM_KEYNAME, gw_int)) {
		if(RIL_DEBUG) ALOGD("ifc_create_default_route error: %s",strerror(errno));
		if(RIL_DEBUG) ALOGD( "failed to set default route");
		if(RIL_DEBUG) ALOGD( "gateway %x \n",gw_int);
		return -1;
	}
	return 0;	
}

/***************************************************
 Function:  set_dns
 Description:  
 	configure linux netwok device and setting android netwok attribute 
 Calls: 
 	hex_address_sting_to_network_string
 Called By:
	none
 Input:
            ip    - pointer to hex string
	gateway - pointer to hex string
	   dns1 - pointer to hex string
	   dns2 - pointer to hex string
 Output:
      no
 Return:
	0  configure netwok successful
	-1 configure network fail
 Others:
	none
**************************************************/
int configure_interface(char *ip, char *gateway, char *dns1, char *dns2)
{   
#ifdef IFC

	//if (ifc_up("eth1")) {
	if (ifc_up(PORT_ECM_KEYNAME)) {
		if(RIL_DEBUG) ALOGD("Failed to turn on interface: %s",strerror(errno));
		if(RIL_DEBUG) ALOGD( "failed to turn on interface");
		ifc_close();
		return -1;
	}

	if(set_ip(ip) <0){
		if(RIL_DEBUG) ALOGD( "set_ip error");
		ifc_close();
		return -1; 	
	}

	if(set_gateway(gateway) <0){
		if(RIL_DEBUG) ALOGD("set_gateway error: %s",strerror(errno));
		if(RIL_DEBUG) ALOGD( "set_gateway error");
		ifc_close();
		return -1; 	
	}  

	set_dns(1, dns1);
	set_dns(2, dns2);

	ifc_close();
		
#else

	char *ip_string;
	char *gw_string;
	char *dns1_string;
	char *dns2_string;
	int dhcpstatus = -1;
	unsigned int count = 3;
	in_addr_t myaddr = 0;
	char ipaddress[64]="";
	char netcfg_up[64];
	char netcfg_dhcp[64];
	
	ip_string = hex_address_sting_to_network_string(ip);
	if(RIL_DEBUG) ALOGD( "DHCP_IP is: %s \n",ip_string);	

	gw_string = hex_address_sting_to_network_string(gateway);
	if(RIL_DEBUG) ALOGD( "DHCP_gateway is :%s \n",gw_string);
	strncpy(gateway_address, gw_string, strlen(gw_string));
		
	dns1_string = hex_address_sting_to_network_string(dns1);
	if(RIL_DEBUG) ALOGD( "DHCP_dns1 is :%s \n",dns1_string);
		
	dns2_string = hex_address_sting_to_network_string(dns2);
	if(RIL_DEBUG) ALOGD( "DHCP_dns2 is :%s \n",dns2_string);
	strncpy(dns_address, dns2_string, strlen(dns2_string));
	
	sprintf(netcfg_up, "netcfg %s up",PORT_ECM_KEYNAME);
	sprintf(netcfg_dhcp, "netcfg %s dhcp",PORT_ECM_KEYNAME);
	dhcpstatus = system(netcfg_up);
    if(RIL_DEBUG) ALOGD( "netcfg %s dhcp up: status %d\n", PORT_ECM_KEYNAME,dhcpstatus);  
    dhcpstatus = system(netcfg_dhcp);
    if(RIL_DEBUG) ALOGD( "netcfg %s dhcp dhcp: status %d\n", PORT_ECM_KEYNAME,dhcpstatus);
	
	if(ifc_init() < 0){
		if(RIL_DEBUG) ALOGD( "ifc_init false!");
	}

	while( count > 0 )
		{
			sleep(1);
			//ifc_get_info("eth1", &myaddr, NULL, NULL);
			ifc_get_info(PORT_ECM_KEYNAME, &myaddr, NULL, NULL);
			if ( myaddr )
			{
				sprintf(ipaddress, "%d.%d.%d.%d", myaddr & 0xFF, (myaddr >> 8) & 0xFF, (myaddr >> 16) & 0xFF, (myaddr >> 24) & 0xFF);
				if(RIL_DEBUG) ALOGD ( "IP address is: %s\n", ipaddress);  
				break;
			}
			--count;
		}
	ifc_close();

	if(0 < count)
	{
		strncpy(ip_address, ipaddress, strlen(ipaddress));
		strncpy(rmnet0_response[2], ipaddress, strlen(ipaddress));
	}
	else
	{
		if(RIL_DEBUG) ALOGD( "get IP address failed\n" );
		return -1;
	}
	
#endif	

	return 0; 
}

/***************************************************
 Function:  deconfigure_interface
 Description:  
 	deconfigure linux netwok device
 Calls: 
 	hex_address_sting_to_network_string
 Called By:
	none
 Input:
	address - pointer to hex string
 Output:
      no
 Return:
	pointer to a network string 
 Others:
	none
**************************************************/
void deconfigure_interface(void)
{   
#ifdef IFC
	char net_gw[64];
	sprintf(net_gw, "net.%s.gw",PORT_ECM_KEYNAME);
	//property_set("net.rmnet0.gw","");
	//property_set("net.eth1.gw","");
	property_set(net_gw,"");

	//if (ifc_remove_default_route("rmnet0")) {
	//if (ifc_remove_default_route("eth1")) {
	if (ifc_remove_default_route(PORT_ECM_KEYNAME)) {
		ALOGE("failed to remove default route");
		ifc_close();
		return;
	}
#else
	ifc_init();
#endif
	//if (ifc_down("rmnet0")) {
	//if (ifc_down("eth1")) {
	if (ifc_down(PORT_ECM_KEYNAME)) {
		ALOGE( "failed to turn on interface");
		ifc_close();
		return;
	}
	ifc_close();
}
/***************************************************
 Function:  ppp_main_loop
 Description:  
 	the thread loop for execute the pppd, if it return show the pppd had exited. so inform the framework by 
 	onUnsolited RIL_UNSOL_DATA_CALL_LIST_CHANGED.
 Calls: 
 Called By:
	start_ppp_pthread
 Input:
	
 Output:
      no
 Return:
	
 Others:
	none
**************************************************/
static void *ppp_main_loop(void *arg)
{	
	int ret;

	ret = system(shell);
	if(RIL_DEBUG)ALOGD("ppp_main_loop return ret %d!\n", ret);

    /* BEGIN DTS2011111806466  lkf52201 2011-12-03 added */
    /*
	if ( ret == -1)
	{
		if(RIL_DEBUG)LOGE("Launch: %s failed! \n", shell);
		goto error;
	}
error:
    */
    /* END DTS2011111806466  lkf52201 2011-12-03 added */
	
	terminal_connect = 1;   /* DTS2011111806466  lkf52201 2011-12-03 added */
	g_apn[0]= '\0';
	onunsol_data_call_list_chanage();
	return ((void *)0);
}

/***************************************************
 Function:  start_ppp_pthread
 Description:  
 	create a new thread, the ppp_main_loop will run the pppd, and if
 	the pppd exit because of some unknown reasions, it will imform the 
 	framework that the pppd is out of running
 Calls: 
 Called By:
	enable_ppp_interface
 Input:
	
 Output:
      no
 Return:
	
 Others:
	none
**************************************************/
pthread_t s_ppp_mainloop;

int start_ppp_pthread()
{
	int ret = -1;
	pthread_attr_t attr;
	pthread_attr_init (&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&s_ppp_mainloop, &attr, ppp_main_loop, NULL);
	return ret;
}

/***************************************************
 Function: ip_main_loop
 Description:  
 Calls: 
 Called By:
	start_ip_pthread
 Input:
	
 Output:
      no
 Return:
	
 Others:
	add by lifei 2011.2.15
**************************************************/
static void *ip_main_loop(void *arg)	
{	
//begin modified by wkf32792 for android 3.x 20110815
	struct in_addr myaddr;
	myaddr.s_addr = 0;
	struct in_addr mygateway;	
#ifdef ANDROID_3
	RIL_Data_Call_Response_v6 response_V6;
#endif

	char *response[3] = { "1", "ppp0",""};
	char ipaddress[64]="";	
    char gateway[64]="";    //add by wkf32792 20110817 for android 3.x
    char dns[128]={0};
    
	RIL_Token t = (RIL_Token *)arg;
	
	unsigned int count = 60;
	ifc_init();
	terminal_connect = 0;   /* DTS2011111806466  lkf52201 2011-12-03 added */
	while( count > 0 )
	{
		/* BEGIN DTS2011111806466  lkf52201 2011-12-03 added */
		if (1 == terminal_connect)
		{
		    goto error;
		}
		/* END DTS2011111806466  lkf52201 2011-12-03 added */
		
		ifc_get_info("ppp0",(in_addr_t)&myaddr, NULL, NULL);   
		if ( 0 != myaddr.s_addr )
		{	
		    strcpy(ipaddress, inet_ntoa(myaddr));
			if(RIL_DEBUG)ALOGD("ppp0 IP address is: %s!\n", ipaddress);			
			#if 1
			sleep(1);
			
      		property_get("net.ppp0.dns1", dns, "");
       		if(RIL_DEBUG)ALOGD("dns1=%s",dns);
			if(strstr(dns,"10.11.12"))
			{
				ALOGD("error dns!!!!!!!");
				goto error;
			}
			#endif
			//add by wkf32792 20110817 for android 3.x begin
			mygateway.s_addr = ifc_get_default_route("ppp0");			
			strcpy(gateway, inet_ntoa(mygateway));
			if(RIL_DEBUG)ALOGD("ppp0 gate way is: %s!\n", gateway);
			//add by wkf32792 20110817 for android 3.x end
			break;
		}
		--count;
		if(RIL_DEBUG)ALOGD("count is %d\n",count);
		/* BEGIN DTS2011110902480  lkf52201 2011-11-09 added */
		sleep(1);
		/* END DTS2011110902480  lkf52201 2011-11-09 added */
	}
	if(0 == count)
	{
		if(RIL_DEBUG)ALOGD("ppp get address timeout!\n");
		goto error;	
    }
	ifc_close();

	/*Begin added by x84000798 for test PS package*/
	if (1 == g_pspackage)
	{
		RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);
		g_pspackage = 0;
		return ((void *)0);
	}	
	/*End added by x84000798 for test PS package*/	
	
	response[2] = ipaddress;
	
#ifdef ANDROID_3
    response_V6.status = PDP_FAIL_NONE;
/* BEGIN DTS2012010602643 lkf52201 2012-01-07 added for android 4.0 */
#ifdef ANDROID_4
    response_V6.suggestedRetryTime = -1;
#endif
/* END DTS2012010602643 lkf52201 2012-01-07 added for android 4.0 */
    response_V6.cid = 1;
    response_V6.active = 2;     
    response_V6.type = g_pdptype;      
    response_V6.ifname = "ppp0";
    response_V6.addresses = ipaddress; 
    response_V6.dnses = dns;     
    response_V6.gateways = gateway;  
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response_V6, sizeof(response_V6));
#else	
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
#endif
//end modified by wkf32792 for android 3.x 20110815

	return ((void *)0);
error:
	ifc_close();
	disable_ppp_Interface();  // DTS2011111400455  lkf52201 2011-12-7 modified
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	return ((void *)0);
}

/***************************************************
 Function:  start_ip_check_pthread
 Description:  
 Calls: 
 Called By:
	check_ip_addr
 Input:
	
 Output:
      no
 Return:
	
 Others:
	add by lifei 2011.2.15
**************************************************/
pthread_t s_ip_mainloop;

int start_ip_check_pthread(RIL_Token  token)
{	
	int ret = -1;
	pthread_attr_t attr;
	pthread_attr_init (&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create(&s_ip_mainloop, &attr, ip_main_loop, (void *)token);
	return ret;
}

/***************************************************
 Function:  enable_ppp_interface
 Description:  
 	enable the ppp interface 
 Calls: 
 Called By:
	setup_ppp_connection
 Input:
	
 Output:
      no
 Return:
	pointer to a network string 
 Others:
	none
**************************************************/
int enable_ppp_interface(RIL_Token token,int cid, const char* user, const char* passwd, const char *authoption)
{
	int ret = 0;
	
	if (cid < 1)
	{
		ALOGE("%s: invalid cid:%d\n", __FUNCTION__, cid);
		return -1;
	}
	/* Begin modified by x84000798 for DTS2012080605618 2012-8-6*/
  	if((user[0]=='\0')||(passwd[0]=='\0'))
  	{
  		if(RIL_DEBUG)ALOGD("user & passwd is NULL,pppd");
			if(NETWORK_CDMA == g_module_type) {
		    sprintf(shell, "/system/bin/pppd  %s 115200 mru 1280 nodetach debug dump defaultroute usepeerdns novj %s  novjccomp noipdefault ipcp-accept-local ipcp-accept-remote connect-delay 5000 ipcp-max-failure 60 ipcp-max-configure 60",SERIAL_PORT_MODEM_KEYNAME, authoption);
		}
		else {
	        sprintf(shell, "/system/bin/pppd  %s 115200 mru 1280 nodetach debug dump defaultroute usepeerdns novj %s  novjccomp noipdefault ipcp-accept-local ipcp-accept-remote connect-delay 5000 ipcp-max-failure 60 ipcp-max-configure 60 -am",SERIAL_PORT_MODEM_KEYNAME, authoption);
		}
    }
    else
    {
    	/*start modify by fKF34305 20101211  增加了C 卡模式*/
    	//modified by hkf39947  20120109 begin for DTS2012042001242 20120420
    	//Begin to be modified by c00221986 for DTS2013040707115
        if(NETWORK_CDMA == g_module_type)
        {
             if(RIL_DEBUG)ALOGD("CDMA pppd");
			 sprintf(shell, "/system/bin/pppd %s 115200 crtscts debug  dump %s nodetach modem noipdefault  defaultroute usepeerdns user '%s' password '%s'",SERIAL_PORT_MODEM_KEYNAME,authoption, user,passwd);
        	 //sprintf(shell, "/system/bin/pppd %s 115200 crtscts debug  dump %s nodetach modem noipdefault  defaultroute usepeerdns user \"%s\" password \"%s\"",SERIAL_PORT_MODEM_KEYNAME,authoption, user,passwd);  //crtscts ,user,passwd
        }
        else
        {
             if(RIL_DEBUG)ALOGD("WCDMA pppd");
			 sprintf(shell, "/system/bin/pppd  %s  115200 mru 1280 nodetach debug dump %s defaultroute usepeerdns novj  user '%s' password '%s'  novjccomp noipdefault ipcp-accept-local ipcp-accept-remote connect-delay 5000 ipcp-max-failure 60 ipcp-max-configure 60 -am",SERIAL_PORT_MODEM_KEYNAME,authoption, user,passwd);
    		 //sprintf(shell, "/system/bin/pppd  %s  115200 mru 1280 nodetach debug dump %s defaultroute usepeerdns novj  user \"%s\" password \"%s\"  novjccomp noipdefault ipcp-accept-local ipcp-accept-remote connect-delay 5000 ipcp-max-failure 60 ipcp-max-configure 60 -am",SERIAL_PORT_MODEM_KEYNAME,authoption, user,passwd);
        }
		//End being modified by c00221986 for DTS2013040707115
        //modified by hkf39947  20120109 end for DTS2012042001242 20120420
		/*end modify by fKF34305 20101211  增加了C 卡模式*/
	}
	/* End modified by x84000798 for DTS2012080605618 2012-8-6*/
	if((ret = start_ppp_pthread()) < 0)
	{
		ALOGE("%s: start_ppp_pthread() failed: %s", __FUNCTION__, strerror(errno));
		return -1;
	}
	if(RIL_DEBUG)ALOGD("shell=%s",shell);
	/* BEGIN DTS2011110902480  lkf52201 2011-11-09 modified */
    //sleep(1);
    /* END DTS2011110902480  lkf52201 2011-11-09 modified */
	if((ret = start_ip_check_pthread(token))<0)
	{
		ALOGE("%s: start_ip_check_pthread() failed: %s", __FUNCTION__, strerror(errno));
		return -1;
	}

	return 1;
}

/***************************************************
 Function:  shut_down_ttyUSB0
 Description:  
 	for adator the huawei modem for pppd
 Calls: 
 	
 Called By:
	
 Input:
	no
 Output:
      no
 Return:
 Others:
	none
	2011-05-14:增加TD制式挂断命令 V3B02D01
**************************************************/
void shut_down_ttyUSB0(void)
{
	if(NETWORK_CDMA == g_module_type)
    {	
    	//don't user echo, it will add \n to the tail of the send buf
    	dial_at_modem("ath");// atd and ath must send to the same port
    }
    else if ( NETWORK_WCDMA == g_module_type )
    {
    	dial_at_modem("+++");
    }
    else
    {
		at_send_command("ATH",NULL);
    }
}

/*BEGIN Added by qkf33988 2010-12-03 for kill pppd,get pid by name*/
int get_pid(char *Name)
{
    /* BEGIN DTS2011110902480   lkf52201 2011-11-09 modified */
    char name[14] = {0};
    strncpy(name,Name,14);
	char szBuf[256] = {0}; 
	char cmd[20] = {0};
	char *p_pid = NULL;
	FILE *pFile = NULL;
	int  pid = 0;

	sprintf(cmd, "ps %s", name);
	pFile = popen(cmd, "r");
	if (pFile != NULL)
	{
		while (fgets(szBuf, sizeof(szBuf), pFile))  
		{  
			if(strstr(szBuf, name))
			{
				//sleep(1);   //  DTS2011110902480   lkf52201 2011-11-09 modified 
				p_pid = strstr(szBuf, " "); 
				//while(*(++p_pid) == ' ');    //  DTS2011110902480   lkf52201 2011-11-09 modified 
				pid = strtoul(p_pid, NULL, 10);
				if(RIL_DEBUG) ALOGD("--- PPPD PID = %d ---",pid);
				break;
			}
		}  
	}
  /* END DTS2011110902480   lkf52201 2011-11-09 modified */

    pclose(pFile); 
    return pid;
}

void hwril_kill_pppd()
{
    int pppd_pid = get_pid("pppd");
    //char cmd[256] = {0};   //  DTS2011110902480   lkf52201 2011-11-09 modified 
    int count = 5;

    if(pppd_pid)
	{
		/* BEGIN DTS2011110902480   lkf52201 2011-11-09 modified */
		//sprintf(cmd, "kill %d", pppd_pid);
		//system(cmd);
		kill(pppd_pid, SIGKILL);   // DTS2011111400455 lkf52201 2011-12-7 modified
		while (--count >= 0)
		{
			kill(pppd_pid, 0);
			if (errno == ESRCH)
			{
				break;
			}
			sleep(1);
		}
		//sleep(5); //Wait pppd exit
		/* END DTS2011110902480   lkf52201 2011-11-09 modified */
	}
}
/*BEGIN Added by qkf33988 2010-12-03 for kill pppd*/
/***************************************************
 Function:  disable_ppp_Interface
 Description:  
 	disable the ppp interface
 Calls: 
 	
 Called By:
	
 Input:
	  no
 Output:
      no
 Return:
 Others:
	none
**************************************************/
void disable_ppp_Interface(void)            // DTS2011111400455  lkf52201 2011-12-7 modified
{
    hwril_kill_pppd();						// DTS2012042600987 x84000798 2012-5-3 modified
    if(NETWORK_WCDMA == g_module_type)
    {
       at_send_command("AT+CGACT=0", NULL); // DTS2011111400455  lkf52201 2011-12-7 modified
    }
    at_send_command("ATH", NULL);           // DTS2011111400455  lkf52201 2011-12-7 modified
    
    shut_down_ttyUSB0();					// DTS2012042600987 x84000798 2012-5-3 modified
	property_set("net.ppp0.dns1","");
}

/***************************************************
 Function:  deconfigure_interface
 Description:  
 	deconfigure linux netwok device
 Calls: 
 	hex_address_sting_to_network_string
 Called By:
	none
 Input:
	address - pointer to hex string
 Output:
      no
 Return:
	pointer to a network string 
 Others:
	none
**************************************************/
int get_interface_addr(int cid, const char* ifname, char* ipaddress)
{
	int ret = -1;
	unsigned myaddr = 0;
	
    if(NULL == ipaddress)
    {
		return ret;
	}
	
	ifc_init();
	ifc_get_info(ifname, &myaddr, NULL, NULL);
	if ( myaddr )
	{
		ret = 0;
		sprintf(ipaddress, "%d.%d.%d.%d", myaddr & 0xFF, (myaddr >> 8) & 0xFF, (myaddr >> 16) & 0xFF, (myaddr >> 24) & 0xFF);
		if(RIL_DEBUG)ALOGD("ppp0 IP address is: %s!\n", ipaddress);
	}
	ifc_close();
	return ret;
}




