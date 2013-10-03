/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-ps-api.h
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
*******************************************************/

int ifc_init(void);
int ifc_close(void);
int ifc_set_addr(const char *name, unsigned addr);
int ifc_create_default_route(const char *name, unsigned addr);
int ifc_remove_default_route(const char *name);
int ifc_up(const char *name);
int ifc_down(const char *name);

int char_to_int(char *ch);
int hex_address_string_to_int(char *address);
char *int_to_netwrok_string(int address);
char *hex_address_sting_to_network_string(char* address);
void set_dns(int i,char *dns);
int set_gateway(char *gateway);
int configure_interface(char *ip, char *gateway, char *dns1, char *dns2);
void deconfigure_interface(void);
int enable_ppp_interface( RIL_Token token,int cid, const char* user, const char* passwd, const char *authoption);
void disable_ppp_Interface(void);       // DTS2011111400455  lkf52201 2011-12-7 modified
int get_interface_addr(int cid, const char* ifname, char* ipaddress);
void shut_down_ttyUSB0(void);
void hwril_kill_pppd(void);
int  get_pid(char * Name);
