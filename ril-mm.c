/********************************************************
 Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 File Name: ril-mm.c
 Author: liubing/00141886
 Version: V1.0
 Description:
 
    Handle request and unsolicited about mm.include below interface:
    
	RIL_REQUEST_SIGNAL_STRENGTH
	RIL_REQUEST_REGISTRATION_STATE
	RIL_REQUEST_GPRS_REGISTRATION_STATE
	RIL_REQUEST_OPERATOR
	RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE
	RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC
	RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL
	RIL_REQUEST_QUERY_AVAILABLE_NETWORKS
	RIL_REQUEST_SCREEN_STATE
	RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE
	RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE
	RIL_REQUEST_SET_LOCATION_UPDATES
	
	RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED
	RIL_UNSOL_NITZ_TIME_RECEIVED
 
*******************************************************/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.

  when        who      version    why 
 -------    -------    -------   --------------------------------------------
2010/09/14  l141886    V1B01D01  add wcdma module network service function
2010/12/14  fkf34035   V2B01D01  add cdma module network service function
2011/05/14  lkf34826   V3B02D01  add tdscdma module network service function

===========================================================================*/

/*===========================================================================

                           INCLUDE FILES

===========================================================================*/
#include "huawei-ril.h"
#include <ctype.h>
#include <math.h>

/*===========================================================================

                   INTERNAL DEFINITIONS AND TYPES

===========================================================================*/
#define SEARCH_NETWORK_TIME 120   //DTS2011091606706 added by wkf32792 20110920
extern RIL_RadioState sState;
extern Module_Type g_module_type;
extern MODEM_Vendor g_modem_vendor;//by alic
int g_IsDialSucess = 0;//by alic
extern int s_MaxError;//by alic

//add by lifei 2010.12.31
static const Oper_memory_entry_type oper_memory_list[] =
{
  /***********************
   **** Test PLMN 1-1 ****
   ***********************/
  { "00101", "Test1-1", "Test PLMN 1-1" },

  /***********************
   **** Test PLMN 1-2 ****
   ***********************/
  { "00102", "Test1-2", "Test PLMN 1-2" },

  /***********************
   **** Test PLMN 2-1 ****
   ***********************/
  { "00201", "Test2-1", "Test PLMN 2-1" },


  /****************
   **** Greece ****
   ****************/
  { "20201", "Cosmote",  "COSMOTE - Mobile Telecommunications S.A." },
  { "20205", "Vodafone", "Vodafone Greece" },
  { "20209", "Wind",     "Wind Hella telecommunications S.A." },
  { "20210", "Wind",     "Wind Hella telecommunications S.A." },

  /*********************
   **** Netherlands ****
   *********************/
  { "20403", "Rabo Mobiel",             "KPN" },
  { "20404", "Vodafone",                "Vodafone Netherlands" },
  { "20408", "KPN",                     "KPN" },
  { "20412", "Telfort",                 "KPN" },
  { "20416", "T-Mobile / Ben",          "T-Mobile Netherlands B.V" },
  { "20420", "Orange Nederland",        "T-Mobile Netherlands B.V" },
  { "20421", "NS Railinfrabeheer B.V.", "NS Railinfrabeheer B.V." },

  /*****************
   **** Belgium ****
   *****************/
  { "20601", "Proximus", "Belgacom Mobile" },
  { "20610", "Mobistar", "France Telecom" },
  { "20620", "BASE",     "KPN" },

  /****************
   **** France ****
   ****************/
  { "20800", "Orange",                "Orange" },
  { "20801", "France Telecom Mobile", "France Orange" },
  { "20802", "Orange",                "Orange" },
  { "20805", "Globalstar Europe",     "Globalstar Europe" },
  { "20806", "Globalstar Europe",     "Globalstar Europe" },
  { "20807", "Globalstar Europe",     "Globalstar Europe" },
  { "20810", "SFR",                   "SFR" },
  { "20811", "SFR",                   "SFR" },
  { "20820", "Bouygues",              "Bouygues Telecom" },
  { "20821", "Bouygues",              "Bouygues Telecom" },

  /*****************
   **** Andorra ****
   *****************/
  { "21303", "Mobiland", "Servei De Tele. DAndorra" },

  /***************
   **** Spain ****
   ***************/
  { "21401", "Vodafone",  "Vodafone Spain" },
  { "21403", "Orange",    "France Telcom Espana SA" },
  { "21404", "Yoigo",     "Xfera Moviles SA" },
  { "21405", "TME",       "Telefonica Moviles Espana" },
  { "21406", "Vodafone",  "Vodafone Spain" },
  { "21407", "movistar",  "Telefonica Moviles Espana" },
  { "21409", "Orange",    "France Telcom Espana SA" },

  /*****************
   **** Hungary ****
   *****************/
  { "21620", "Pannon",   "Pannon GSM Tavkozlesi Zrt." },
  { "21630", "T-Mobile", "Magyar Telkom Plc" },
  { "21670", "Vodafone", "Vodafonei Magyarorszag Zrt." },

  /********************************
   **** Bosnia and Herzegovina ****
   ********************************/
  { "21803", "ERONET",    "Public Enterprise Croatian telecom Ltd." },
  { "21805", "m:tel",     "RS Telecommunications JSC Banja Luka" },
  { "21890", "BH Mobile", "BH Telecom" },

  /*****************
   **** Croatia ****
   *****************/
  { "21901", "T-Mobile", "T-Mobile Croatia" },
  { "21902", "Tele2",    "Tele2" },
  { "21910", "VIPnet",   "Vipnet" },

  /****************
   **** Serbia ****
   ****************/
  { "22001", "Telenor",         "Telenor Serbia" },
  { "22003", "Telekom Sribija", "Telekom Srbija" },
  { "22005", "VIP Mobile",      "VIP Mobile" },

  /***************
   **** Italy ****
   ***************/
  { "22201", "TIM",      "Telecom Italiz SpA" },
  { "22202", "Elsacom",  "Elsacom" },
  { "22210", "Vodafone", "Vodafone Omnitel N.V." },
  { "22230", "RRI",      "Rete  Ferroviaria Italiana" },
  { "22288", "Wind",     "Wind Telecomunicazioni SpA" },
  { "22299", "3 Italia", "Hutchison 3G" },

  /*****************
   **** Romania ****
   *****************/
  { "22601", "Vodafone",   "Vodafone Romania" },
  { "22603", "Cosmote",    "Cosmote Romania" },
  { "22605", "DIGI.mobil", "RCS&RDS" },
  { "22610", "Orange",     "Orange Romania" },

  /*********************
   **** Switzerland ****
   *********************/
  { "22801", "Swisscom",     "Swisscom Ltd" },
  { "22802", "Sunrise",      "Sunrise Communications AG" },
  { "22803", "Orange",       "Orange Communications SA" },
  { "22806", "SBB AG",       "SBB AG" },
  { "22807", "IN&Phone",     "IN&Phone SA" },
  { "22808", "Tele2",        "Tele2 Telecommunications AG" },

  /************************
   **** Czech Republic ****
   ************************/
  { "23001", "T-Mobile",      "T-Mobile Czech Republic" },
  { "23002", "EUROTEL PRAHA", "Telefonica O2 Czech Republic" },
  { "23003", "OSKAR",         "Vodafone Czech Republic" },
  { "23098", "CZDC s.o.",     "CZDC s.o." },

  /******************
   **** Slovakia ****
   ******************/
  { "23101", "Orange",   "Orange Slovensko" },
  { "23102", "T-Mobile", "T-Mobile Slovensko" },
  { "23104", "T-Mobile", "T-Mobile Slovensko" },
  { "23106", "O2",       "Telefonica O2 Slovakia" },

  /*****************
   **** Austria ****
   *****************/
  { "23201", "A1",       "Mobilkom Austria" },
  { "23203", "T-Mobile", "T-Mobile Austria" },
  { "23205", "Orange",   "Orange Austria" },
  { "23207", "T-Mobile", "T-Mobile Austria" },
  { "23210", "3",        "Hutchison 3G" },

  /************************
   **** United Kingdom ****
   ************************/
  { "23400", "BT",                                               "British Telecom" },
  { "23401", "UK01",                                             "Mapesbury Communications Ltd." },
  { "23402", "O2",                                               "O2" },
  { "23403", "Jersey Telenet",                                   "Jersey Telnet" },
  { "23410", "O2",                                               "Telefonica O2 UK Limited" },
  { "23411", "O2",                                               "Telefonica Europe" },
  { "23412", "Railtrack",                                        "Network Rail Infrastructure Ltd" },
  { "23415", "Vodafone",                                         "Vodafone United Kingdom" },
  { "23416", "Opal Telecom Ltd",                                 "Opal Telecom Ltd" },
  { "23418", "Cloud9",                                           "Wire9 Telecom plc" },
  { "23419", "Telaware",                                         "Telaware plc" },
  { "23420", "3",                                                "Hutchison 3G UK Ltd" },
  { "23430", "T-Mobile",                                         "T-Mobile" },
  { "23431", "Virgin",                                           "Virgin Mobile" },
  { "23432", "Virgin",                                           "Virgin Mobile" },
  { "23433", "Orange",                                           "Orange PCS Ltd" },
  { "23434", "Orange",                                           "Orange PCS Ltd" },
  { "23450", "JT-Wave",                                          "Jersey Telecoms" },
  { "23455", "Cable & Wireless Guernsey / Sure Mobile (Jersey)", "Cable & Wireless Guernsey / Sure Mobile (Jersey)" },
  { "23458", "Manx Telecom",                                     "Manx Telecom" },
  { "23475", "Inquam",                                           "Inquam Telecom (Holdings) Ltd" },

  /*****************
   **** Denmark ****
   *****************/
  { "23801", "TDC",                 "TDC A/S" },
  { "23802", "Sonofon",             "Telenor" },
  { "23806", "3",                   "Hi3G Denmark ApS" },
  {" 23830", "Telia",               "Telia Nattjanster Norden AB" },
  { "23870", "Tele2",               "Telenor" },

  /****************
   **** Sweden ****
   ****************/
  { "24001", "Telia",                              "TeliaSonera Mobile Networks" },
  { "24002", "3",                                  "3" },
  { "24004", "3G Infrastructure Services",         "3G Infrastructure Services" },
  { "24005", "Sweden 3G",                          "Sweden 3G" },
  { "24006", "Telenor",                            "Telenor" },
  { "24007", "Tele2",                              "Tele2 AB" },
  { "24008", "Telenor",                            "Telenor" },
  { "24021", "Banverket",                          "Banverket" },

  /****************
   **** Norway ****
   ****************/
  { "24201", "Telenor",           "Telenor" },
  { "24202", "NetCom",            "NetCom GSM" },
  { "24205", "Network Norway",    "Network Norway" },
  { "24220", "Jernbaneverket AS", "Jernbaneverket AS" },

  /*****************
   **** Finland ****
   *****************/
  { "24403", "DNA",        "DNA Oy" },
  { "24405", "Elisa",      "Elisa Oyj" },
  { "24412", "DNA Oy",     "DNA Oy" },
  { "24414", "AMT",        "Alands Mobiltelefon" },
  { "24491", "Sonera",     "TeliaSonera Finland Oyj" },

  /*******************
   **** Lithuania ****
   *******************/
  { "24601", "Omnitel", "Omnitel" },
  { "24602", "BITE",    "UAB Bite Lietuva" },
  { "24603", "Tele 2",  "Tele 2" },

  /****************
   **** Latvia ****
   ****************/
  { "24701", "LMT",   "Latvian Mobile Telephone" },
  { "24702", "Tele2", "Tele2" },
  { "24705", "Bite",  "Bite Latvija" },

  /*****************
   **** Estonia ****
   *****************/
  { "24801", "EMT",    "Estonian Mobile Telecom" },
  { "24802", "Elisa",  "Elisa Eesti" },
  { "24803", "Tele 2", "Tele 2 Eesti" },

  /***************************
   **** Russia Federation ****
   ***************************/
  { "25001", "MTS",                   "Mobile Telesystems" },
  { "25002", "MegaFon",               "MegaFon OJSC" },
  { "25003", "NCC",                   "Nizhegorodskaya Cellular Communications" },
  { "25005", "ETK",                   "Yeniseytelecom" },
  { "25007", "SMARTS",                "Zao SMARTS" },
  { "25012", "Baykalwstern",          "Baykal Westcom/New Telephone Company/Far Eastern Cellular" },
  { "25014", "SMARTS",                "SMARTS Ufa" },
  { "25016", "NTC",                   "New Telephone Company" },
  { "25017", "Utel",                  "JSC Uralsvyazinform" },
  { "25019", "INDIGO",                "INDIGO" },
  { "25020", "Tele2",                 "Tele2" },
  { "25023", "Mobicom - Novosibirsk", "Mobicom - Novosibirsk" },
  { "25039", "Utel",                  "Uralsvyazinform" },
  { "25099", "Beeline",               "OJSC VimpelCom" },

  /*****************
   **** Ukraine ****
   *****************/
  { "25501", "MTS", "Ukrainian Mobile Communications" },
  { "25502", "Beeline", "Ukrainian Radio Systems" },
  { "25503", "Kyivstar", "Kyivstar GSM JSC" },
  { "25505", "Golden Telecom", "Golden Telecom" },
  {" 25506", "life:)", "Astelit" },
  { "25507", "Utel", "Ukrtelecom" },

  /*****************
   **** Belarus ****
   *****************/
  { "25701", "Velcom", "Velcom" },
  {" 25702", "MTS",    "JLLC Mobile TeleSystems" },
  { "25704", "life:)", "Belarussian Telecommunications Network" },

  /*****************
   **** Moldova ****
   *****************/
  { "25901", "Orange",   "Orange Moldova" },
  { "25902", "Moldcell", "Moldcell" },
  { "25904", "Eventis",  "Eventis Telecom" },

  /****************
   **** Poland ****
   ****************/
  { "26001", "Plus",           "Polkomtel" },
  { "26002", "Era",            "Polska Telefonia Cyfrowa (PTC)" },
  { "26003", "Orange",         "PTK Centertel" },
  { "26006", "Play",           "P4 Sp. zo.o" },
  { "26012", "Cyfrowy Polsat", "Cyfrowy Polsat" },
  { "26014", "Sferia",         "Sferia S.A." },

  /*****************
   **** Germany ****
   *****************/
  { "26201", "T-Mobile",      "T-Mobile" },
  { "26202", "Vodafone",      "Vodafone D2 GmbH" },
  { "26203", "E-Plus",        "E-Plus Mobilfunk" },
  { "26204", "Vodafone",      "Vodafone" },
  { "26205", "E-Plus",        "E-Plus Mobilfunk" },
  { "26206", "T-Mobile",      "T-Mobile" },
  { "26207", "O2",            "O2 (Germany) GmbH & Co. OHG" },
  { "26208", "O2",            "O2" },
  { "26209", "Vodafone",      "Vodafone" },
  {" 26210", "Arcor AG & Co", "Arcor AG * Co" },
  {" 26211", "O2",            "O2" },
  { "26215", "Airdata",       "Airdata" },
  { "26260", "DB Telematik",  "DB Telematik" },
  { "26276", "Siemens AG",    "Siemens AG" },
  { "26277", "E-Plus",        "E-Plus" },

  /*******************
   **** Gibraltar ****
   *******************/
  { "26601", "GibTel", "Gibraltar Telecoms" },

  /******************
   **** Portugal ****
   ******************/
  { "26801", "Vodafone", "Vodafone Portugal" },
  { "26803", "Optimus",  "Sonaecom - Servicos de Comunicacoes, S.A." },
  { "26806", "TMN",      "Telecomunicacoes Moveis Nacionais" },

  /********************
   **** Luxembourg ****
   ********************/
  { "27001", "LuxGSM",    "P&T Luxembourg" },
  { "27077", "Tango",     "Tango SA" },
  { "27099", "Voxmobile", "VOXmobile S.A" },

  /*****************
   **** Ireland ****
   *****************/
  { "27201", "Vodafone", "Vodafone Ireland" },
  { "27202", "O2",       "O2 Ireland" },
  { "27203", "Meteor",   "Meteor" },
  {" 27205", "3",        "Hutchison 3G IReland limited" },

  /*****************
   **** Iceland ****
   *****************/
  { "27401", "Siminn",   "Iceland Telecom" },
  { "27402", "Vodafone", "iOg fjarskipti hf" },
  { "27404", "Viking",   "IMC Island ehf" },
  { "27407", "IceCell",  "IceCell ehf" },
  { "27411", "Nova",     "Nova ehf" },

  /*****************
   **** Albania ****
   *****************/
  { "27601", "AMC",          "Albanian Mobile Communications" },
  {"27602", "Vodafone",     "Vodafone Albania" },
  { "27603", "Eagle Mobile", "Eagle Mobile" },

  /***************
   **** Malta ****
   ***************/
  { "27801", "Vodafone", "Vodafone Malta" },
  { "27821", "GO",       "Mobisle Communications Limited" },
  { "27877", "Melita",   "Melita Mobile Ltd. (3G Telecommunictaions Limited" },

  /****************
   **** Cyprus ****
   ****************/
  { "28001", "Cytamobile-Vodafone", "Cyprus Telcommunications Auth" },
  { "28010", "MTN",                 "Areeba Ltde" },

  /*****************
   **** Georgia ****
   *****************/
  { "28201", "Geocell",  "Geocell Limited" },
  { "28202", "Magti",    "Magticom GSM" },
  { "28204", "Beeline",  "Mobitel LLC" },
  { "28267", "Aquafon",  "Aquafon" },
  { "28288", "A-Mobile", "A-Mobile" },

  /*****************
   **** Armenia ****
   *****************/
  { "28301", "Beeline",      "ArmenTel" },
  { "28305", "VivaCell-MTS", "K Telecom CJSC" },

  /******************
   **** Bulgaria ****
   ******************/
  { "28401", "M-TEL",   "Mobiltel" },
  { "28403", "Vivatel", "BTC" },
  { "28405", "GLOBUL",  "Cosmo Bulgaria Mobile" },

  /****************
   **** Turkey ****
   ****************/
  { "28601", "Turkcell", "Turkcell lletisim Hizmetleri A.S." },
  { "28602", "Vodafone", "Vodafone Turkey" },
  { "28603", "Avea",     "Avea" },

  /********************************
   **** Faroe Islands (Demark) ****
   ********************************/
  { "28801", "Faroese",  "Faroese Telecom" },
  { "28802", "Vodafone", "Vodafone Faroe Islands" },

  /*******************
   **** Greenland ****
   *******************/
  { "29001", "TELE Greenland A/S", "Tele Greenland A/S" },

  /********************
   **** San Marino ****
   ********************/
  { "29201", "PRIMA", "San Marino Telecom" },

  /******************
   **** Slovenia ****
   ******************/
  { "29340", "Si.mobil", "SI.MOBIL d.d" },
  { "29341", "Si.mobil", "Mobitel D.D." },
  { "29364", "T-2",      "T-2 d.o.o." },
  { "29370", "Tusmobil", "Tusmobil d.o.o." },

  /*******************************
   **** Republic of Macedonia ****
   *******************************/
  { "29401", "T-Mobile",     "T-Mobile Makedonija" },
  { "29402", "Cosmofon",     "Cosmofon" },
  { "29402", "VIP Operator", "VIP Operator" },

  /***********************
   **** Liechtenstein ****
   ***********************/
  { "29501", "Swisscom", "Swisscom Schweiz AG" },
  { "29502", "Orange",   "Orange Liechtenstein AG" },
  { "29505", "FL1",      "Mobilkom Liechtenstein AG" },
  { "29577", "Tele 2",   "Belgacom" },

  /********************
   **** Montenegro ****
   ********************/
  { "29701", "ProMonte", "ProMonte GSM" },
  { "29702", "T-Mobile", "T-Mobile Montenegro LLC" },
  { "29703", "m:tel CG", "MTEL CG" },

  /****************
   **** Canada ****
   ****************/
  { "302370", "Fido",                    "Fido" },
  { "302620", "ICE Wireless",            "ICE Wireless" },
  { "302720", "Rogers Wireless",         "Rogers Wireless" },

  /********************************************
   **** Saint Pierre and Miquelon (France) ****
   ********************************************/
  { "30801", "Ameris", "St. Pierre-et-Miquelon Telecom" },

  /****************************************
   **** United States of America, Guam ****
   ****************************************/
  { "31020", "Union Telephony Company",          "Union Telephony Company" },
  { "31026", "T-Mobile",                         "T-Mobile" },
  { "31030", "Centennial",                       "Centennial Communications" },
  { "3108", "AT&T",                             "AT&T Mobility" },
  { "31040", "Concho",                           "Concho Cellular Telephony Co., Inc." },
  { "31046", "SIMMETRY",                         "TMP Corp" },
  { "31070", "AT&T",                             "AT&T" },
  { "31080", "Corr",                             "Corr Wireless Communications LLC" },
  { "31090", "AT&T",                             "AT&T" },
  { "310100", "Plateau Wireless",                 "New Mexico RSA 4 East Ltd. Partnership" },
  { "310110", "PTI Pacifica",                     "PTI Pacifica Inc." },
  { "310150", "AT&T",                             "AT&T" },
  { "310170", "AT&T",                             "AT&T" },
  { "310180", "West Cen",                         "West Central" },
  { "310190", "Dutch Harbor",                     "Alaska Wireless Communications, LLC" },
  { "310260", "T-Mobile",                         "T-Mobile" },
  { "310300", "Get Mobile Inc",                   "Get Mobile Inc" },
  { "310311", "Farmers Wireless",                 "Farmers Wireless" },
  { "310330", "Cell One",                         "Cellular One" },
  { "310340", "Westlink",                         "Westlink Communications" },
  { "310380", "AT&T",                             "AT&T" },
  { "310400", "i CAN_GSM",                        "Wave runner LLC (Guam)" },
  { "310410", "AT&T",                             "AT&T" },
  { "310420", "Cincinnati Bell",                  "Cincinnati Bell Wireless" },
  { "310430", "Alaska Digitel",                   "Alaska Digitel" },
  { "310450", "Viaero",                           "Viaero Wireless" },
  { "310460", "Simmetry",                         "TMP Corporation" },
  { "310540", "Oklahoma Western",                 "Oklahoma Western Telephone Company" },
  { "310560", "AT&T",                             "AT&T" },
  { "310570", "Cellular One",                     "MTPCS, LLC" },
  { "310590", "Alltel",                           "Alltel Communications Inc" },
  { "310610", "Epic Touch",                       "Elkhart Telephone Co." },
  { "310620", "Coleman County Telecom",           "Coleman County Telecommunications" },
  { "310640", "Airadigim",                        "Airadigim Communications" },
  { "310650", "Jasper",                           "Jasper wireless, inc" },
  { "310680", "AT&T",                             "AT&T" },
  { "310770", "i wireless",                       "lows Wireless Services" },
  { "310790", "PinPoint",                         "PinPoint Communications" },
  { "310830", "Caprock",                          "Caprock Cellular" },
  { "310850", "Aeris",                            "Aeris Communications, Inc." },
  { "310870", "PACE",                             "Kaplan Telephone Company" },
  { "310880", "Advantage",                        "Advantage Cellular Systems" },
  { "310890", "Unicel",                           "Rural cellular Corporation" },
  { "310900", "Taylor",                           "Taylor Telecommunications" },
  { "310910", "First Cellular",                   "First Cellular of Southern Illinois" },
  { "310950", "XIT Wireless",                     "Texas RSA 1 dba XIT Cellular" },
  { "310970", "Globalstar",                       "Globalstar" },
  { "310980", "AT&T",                             "AT&T" },
  { "31110", "Chariton Valley",                  "Chariton Valley Communications" },
  { "31120", "Missouri RSA 5 Partnership",       "Missouri RSA 5 Partnership" },
  { "31130", "Indigo Wireless",                  "Indigo Wireless" },
  { "31140", "Commnet Wireless",                 "Commnet Wireless" },
  { "31150", "Wikes Cellular",                   "Wikes Cellular" },
  { "31160", "Farmers Cellular",                 "Farmers Cellular Telephone" },
  { "31170", "Easterbrooke",                     "Easterbrooke Cellular Corporation" },
  { "31180", "Pine Cellular",                    "Pine Telephone Company" },
  { "31190", "Long Lines Wireless",              "Long Lines Wireless LLC" },
  { "311100", "High Plains Wireless",             "High Plains Wireless" },
  { "311110", "High Plains Wireless",             "High Plains Wireless" },
  { "311130", "Cell One Amarillo",                "Cell One Amarillo" },
  { "311150", "Wilkes Cellular",                  "Wilkes Cellular" },
  { "311170", "PetroCom",                         "Broadpoint Inc" },
  { "311180", "AT&T",                             "AT&T" },
  { "311210", "Farmers Cellular",                 "Farmers Cellular Telephone" },

  /*********************
   **** Puerto Rico ****
   *********************/
  { "33011", "Claro", "Puerto Rico Telephony Company" },

  /****************
   **** Mexico ****
   ****************/
  { "33402", "Telcel",   "America Movil" },
  { "33403", "movistar", "Pegaso Comunicaciones y Sistemas" },

  /*****************
   **** Jamaica ****
   *****************/
  { "33820", "Cable & Wireless", "Cable & Wireless" },
  { "33850", "Digicel",          "Digicel (Jamaica) Limited" },
  { "33870", "Claro",            "Oceanic Digital Jamaica Limited" },

  /*****************************
   **** Guadeloupe (France) ****
   *****************************/
  { "34001", "Orange",   "Orange Caraibe Mobiles" },
  { "34002", "Outremer", "Outremer Telecom" },
  { "34003", "Teleceli", "Saint Martin et Saint Barthelemy Telcell Sarl" },
  { "34008", "MIO GSM",  "Dauphin Telecom" },
  { "34020", "Digicel",  "DIGICEL Antilles Franccaise Guyane" },

  /******************
   **** Barbados ****
   ******************/
  { "342600", "bmobile", "cable &Wireless Barbados Ltd." },
  { "342750", "Digicel", "Digicel (Jamaica) Limited" },

  /*****************************
   **** Antigua and Barbuda ****
   *****************************/
  { "34430", "APUA",    "Antigua Public Utilities Authority" },
  { "344920", "bmobile", "Cable & Wireless Caribbean Cellular (Antigua) Limited" },
  { "344930", "Digicel", "Antigua Wireless Ventures Limited" },

  /*****************************************
   **** Cayman Islands (United Kingdom) ****
   *****************************************/
  { "34650", "Digicel",          "Digicel Cayman Ltd." },
  { "346140", "Cable & Wireless", "Cable & Wireless (Caymand Islands) Limited" },

  /*************************************************
   **** British Virgin Islands (United Kingdom) ****
   *************************************************/
  { "348170", "Cable & Wireless",             "Cable & Wireless (West Indies)" },
  { "348570", "Caribbean Cellular Telephone", "Caribbean Cellular Telephone" },

  /*****************
   **** Bermuda ****
   *****************/
  { "35001", "Digicel Bermuda", "Telecommunications (Bermuda & West Indies) Ltd" },
  { "35002", "Mobility",        "M3 wireless" },
  { "35038", "Digicel",         "Digicel" },

  /*****************
   **** Grenada ****
   *****************/
  { "35230", "Digicel",          "Digicel Grenada Ltd." },
  { "352110", "Cable & Wireless", "Cable & Wireless Grenada Ltd." },

  /******************************
   **** Netherlands Antilles ****
   ******************************/
  { "36251", "Telcell", "Telcell N.V." },
  { "36269", "Digicel", "Curacao Telecom N.V." },
  {" 36291", "UTS",     "Setel NV" },

  /********************************************
   **** Aruba (Kingdom of the Netherlands) ****
   ********************************************/
  { "36301", "SETAR",    "SETAR (Servicio di Telecommunication diAruba" },
  { "36320", "Digicell", "Digicell" },

  /*****************
   **** Bahamas ****
   *****************/
  { "364390", "BaTelCo", "The Bahamas Telecommunications Company Ltd" },

  /***********************************
   **** Anguilla (United Kingdom) ****
   ***********************************/
  { "36510", "Weblinks Limited", "Weblinks Limited" },

  /**************
   **** Cuba ****
   **************/
  { "36801", "ETECSA", "Empresa de Telecomunicaciones de Cuba, SA" },

  /****************************
   **** Dominican Republic ****
   ****************************/
  { "37001", "Orange", "Orange Dominicana" },
  { "37002", "Claro",  "Compania Dominicana de Telefonos, C por" },
  { "37004", "ViVa",   "Centennial Dominicana" },

  /***************
   **** Haiti ****
   ***************/
  { "37210", "Comcel / Voila", "Comcel / Voila" },
  { "37250", "Digicel",        "Digicel" },

  /*****************************
   **** Trinidad and Tobaga ****
   *****************************/
  { "37412", "bmobile", "TSTT" },
  { "37413", "Digicel", "Digicel" },

  /********************
   **** Azerbaijan ****
   ********************/
  { "40001", "Azercell",   "Azercell" },
  { "40002", "Bakcell",    "Bakcell" },
  { "40004", "Nar Mobile", "Azerfon" },

  /********************
   **** Kazakhstan ****
   ********************/
  { "40101", "Beeline",                "KaR-TeL LLP" },
  { "40102", "K'Cell",                 "GSM Kazakhstan Ltdx." },
  { "40177", "Mobile Telecom Service", "Mobile Telecom Service LLP" },

  /****************
   **** Bhutan ****
   ****************/
  { "40211", "B-Mobile",  "B-Mobile" },
  { "40277", "TashiCell", "Tashi InfoComm Limited" },

  /***************
   **** India ****
   ***************/
  { "40401", "Vodafone - Haryana",            "Vodafone Essar" },
  { "40402", "Airtel - Punjab",               "Bharti Airtel" },
  { "40403", "Airtel",                        "Bharti Airtel" },
  { "40404", "Idea - Delhi",                  "Idea cellular" },
  { "40405", "Vodafone - Gujarat",            "Vodafone Essar - Gujarat Limited" },
  { "40407", "Idea Cellular",                 "Idea Cellular" },
  { "40412", "Idea Haryana",                  "IDEA Cellular Limited" },
  {"40513", "Reliance - Maharashtra",        "Reliance GSM" },
  { "40420", "Vodafone",                      "Vodafone Mumbai" },
  { "40421", "BPL Mobile Mumbai",             "BPL Mobile Mumbai" },
  { "40422", "IDEA Cellular - Maharashtra",   "Idea cellular" },
  { "40424", "IDEA Cellular - Gujarat",       "Idea cellular" },
  { "40427", "Vodafone - Maharashtra",        "Vodafone Essar" },
  { "40428", "Aircel - Orissa",               "Dishnet Wireless/Aircel" },
  { "40430", "Vodafone - Kolkata",            "Vodafone Essar East Limited" },
  { "40431", "Airtel - Kolkata",              "Bharti Airtel" },
  { "40444", "Spice Telecom - Karnataka",     "Spice Communications Limited" },
  { "40450", "Reliance",                      "Reliance GSM" },
  { "40452", "Reliance - Orissa",             "Reliance Telecom Private" },
  { "40456", "Idea - UP West",                "IDEA Cellular Limited" },
  { "40466", "BSNL - Maharashtra",            "BSNL Maharashtra & Goa" },
  { "40467", "Vodafone - West Bengal",        "Vodafone Essar East" },
  { "40468", "MTNL - Delhi",                  "Mahanagar Telephone Nigam Ltd" },
  { "40469", "MTNL - Mumbai",                 "Mahanagar Telephone Nigam Ltd" },
  { "40472", "BSNL - Kerala",                 "Bharti Sanchar Nigam Limited" },
  { "40474", "BSNL - West Bengal",            "Bharti Sanchar Nigam Limited" },
  { "40575", "Vodafone-Bihar",                "Vodafone Essar" },
  { "40481", "BSNL - Kolkata",                "Bharti Sanchar Nigam Limited" },
  { "40483", "Reliance",                      "Reliance GSM - West Bengal" },
  { "40485", "Reliance",                      "Reliance GSM - West Bengal" },
  { "40490", "Airtel - Maharashtra",          "Airtel - Maharashtra & Goa" },
  { "40491", "Airtel - Kolkata Metro Circle", "Dishnet Wireless/Aircel" },
  { "40492", "Airtel Mumbai",                 "Airtel Mumbai" },
  { "40495", "Airtel",                        "Bharti Airtel" },
  { "40496", "Airtel - Haryana",              "Bharti Airtel" },

  /******************
   **** Pakistan ****
   ******************/
  { "41001", "Mobilink", "Mobilink-PMCL" },
  { "41003", "Ufone",    "Pakistan Telecommunication Mobile Ltd" },
  { "41004", "Zong",     "China Mobile" },
  { "41006", "Telenor",  "Telenor Pakistan" },
  { "41007", "Warid",    "WaridTel" },

  /*******************
   *** Afghanistan ***
   ********************/
  { "41201", "AWCC",     "Afghan wireless Communication Company" },
  { "41220", "Roshan",   "Telecom Development Company Afghanistan Ltd." },
  { "41240", "Areeba",   "MTN Afghanistan" },
  { "41250", "Etisalat", "Etisalat Afghanistan" },

  /*******************
   **** Sri Lanka ****
   *******************/
  { "41301", "Mobitel",         "Mobitel Lanka Ltd." },
  { "41302", "Dialog",          "Dialog Telekom PLC." },
  { "41303", "Tigo",            "Celtel Lanka Ltd" },
  { "41308", "Hutch Sri Lanka", "Hutch Sri Lanka" },

  /*****************
   **** Myanmar ****
   *****************/
  { "41401", "MPT", "Myanmar Post and Telecommunication" },

  /*****************
   **** Lebanon ****
   *****************/
  { "41501", "Alfa",      "Alfa" },
  { "41503", "MTC-Touch", "MIC 2" },

  /****************
   **** Jordan ****
   ****************/
  { "41601", "Zain",   "Jordan Mobile Teelphone Services" },
  { "41603", "Umniah", "Umniah" },
  { "41677", "Orange", "Oetra Jordanian Mobile Telecommunications Company (MobileCom)" },

  /***************
   **** Syria ****
   ***************/
  { "41701", "SyriaTel", "SyriaTel" },
  { "41702", "MTN Syria", "MTN Syria (JSC)" },

  /****************
   **** Iraq ****
   ****************/
  { "41820", "Zain Iraq", "Zain Iraq" },
  { "41830", "Zain Iraq", "Zain Iraq" },
  { "41850", "Asia Cell", "Asia Cell Telecommunications Company" },
  {"41840", "Korek",     "Korel Telecom Ltd" },

  /****************
   **** Kuwait ****
   ****************/
  { "41902", "Zain",     "Mobile Telecommunications Co." },
  { "41903", "Wataniya", "National Mobile Telecommunications" },
  { "41904", "Viva",     "Kuwait Telecommunication Company" },

  /**********************
   **** Saudi Arabia ****
   **********************/
  { "42001", "STC",     "Saudi Telecom Company" },
  { "42003", "Mobily",  "Etihad Etisalat Company" },
  { "42004", "Zain SA", "MTC Saudi Arabia" },

  /***************
   **** Yemen ****
   ***************/
  { "42101", "SabaFon", "SabaFon" },
  { "42102", "MTN",     "Spacetel" },

  /**************
   **** Oman ****
   **************/
  { "42202", "Oman Mobile", "Oman Telecommunications Company" },
  { "42203", "Nawras",      "Omani Qatari Telecommunications Company SAOC" },

  /******************************
   **** United Arab Emirates ****
   ******************************/
  { "42402", "Etisalat", "Emirates Telecom Corp" },
  { "42403", "du",       "Emirates Integrated Telecommunications Company" },

  /****************
   **** Israel ****
   ****************/
  { "42501", "Orange",    "Partner Communications Company Ltd" },
  { "42502", "Cellcom",   "Cellcom" },
  { "42503", "Pelephone", "Pelephone" },

  /*******************************
   **** Palestinian Authority ****
   *******************************/
  { "42505", "JAWWAL", "Palestine Cellular Communications, Ltd." },

  /***************
   **** Qatar ****
   ***************/
  { "42701", "Qatarnet", "Q-Tel" },

  /******************
   **** Mongolia ****
   ******************/
  { "42888", "Unitel", "Unitel LLC" },
  { "42899", "MobiCom", "MobiCom Corporation" },

  /***************
   **** Nepal ****
   ***************/
  { "42901", "Nepal Telecom", "Nepal Telecom" },
  { "42902", "Mero Mobile",   "Spice Nepal Private Ltd" },

  /**************
   **** Iran ****
   **************/
  { "43211", "MCI",      "Mobile Communications Company of Iran" },
  { "43214", "TKC",      "KFZO" },
  { "43219", "MTCE",     "Mobile Telecommunications Company of Esfahan" },
  { "43232", "Taliya",   "Taliya" },
  { "43235", "Irancell", "Irancell Telecommunications Services Company" },

  /********************
   **** Uzbekistan ****
   ********************/
  { "43404", "Beeline", "Unitel LLC" },
  { "43405", "Ucell",   "Coscom" },
  { "43407", "MTS",     "Mobile teleSystems (FE 'Uzdunrobita' Ltd)" },

  /********************
   **** Tajikistan ****
   ********************/
  { "43601", "Somoncom",   "JV Somoncom" },
  { "43602", "Indigo",     "Indigo Tajikistan" },
  { "43603", "MLT",        "TT Mobile, Closed joint-stock company" },
  { "43604", "Babilon-M",  "CJSC Babilon-Mobile" },
  { "43605", "Beeline TJ", "Co Ltd. Tacom" },

  /********************
   **** Kyrgyzstan ****
   ********************/
  { "43701", "Bitel",   "Sky Mobile LLC" },
  { "43705", "MegaCom", "BiMoCom Ltd" },
  { "43709", "O!",      "NurTelecom LLC" },

  /**********************
   **** Turkmenistan ****
   **********************/
  { "43801", "MTS",     "Barash Communication Technologies" },
  { "43802", "TM-Cell", "TM-Cell" },

  /***************
   **** Japan ****
   ***************/
  { "44000", "eMobile",  "eMobile, Ltd." },
  { "44001", "DoCoMo",   "NTT DoCoMo" },
  { "44002", "DoCoMo",   "NTT DoCoMo Kansai" },
  { "44003", "DoCoMo",   "NTT DoCoMo Hokuriku" },
  { "44004", "SoftBank", "SoftBank Mobile Corp" },
  { "44006", "SoftBank", "SoftBank Mobile Corp" },
  { "44010", "DoCoMo",   "NTT DoCoMo Kansai" },
  { "44020", "SoftBank", "SoftBank Mobile Corp" },

  /*********************
   **** South Korea ****
   *********************/
  { "45005", "SKT",      "SK Telecom" },
  { "45008", "KTF SHOW", "KTF" },

  /*****************
   **** Vietnam ****
   *****************/
  { "45201", "MobiFone",       "Vietnam Mobile Telecom Services Company" },
  { "45202", "Vinaphone",      "Vietnam Telecoms Services Company" },
  { "45204", "Viettel Mobile", "iViettel Corporation" },

  /*******************
   **** Hong Kong ****
   *******************/
  { "45400", "CSL",                   "Hong Kong CSL Limited" },
  { "45401", "CITIC Telecom 1616",    "CITIC Telecom 1616" },
  { "45402", "CSL 3G",                "Hong Kong CSL Limited" },
  { "45403", "3(3G)",                 "Hutchison Telecom" },
  { "45404", "3 DualBand (2G)",       "Hutchison Telecom" },
  { "45406", "Smartone-Vodafone",     "SmarTone Mobile Comms" },
  { "45407", "China Unicom",          "China Unicom" },
  { "45408", "Trident",               "Trident" },
  { "45409", "China Motion Telecom",  "China Motion Telecom" },
  { "45410", "New World",             "Hong Kong CSL Limited" },
  { "45411", "Chia-HongKong Telecom", "Chia-HongKong Telecom" },
  { "45412", "CMCC Peoples",          "China Mobile Hong Kong Company Limited" },
  { "45414", "Hutchison Telecom",     "Hutchison Telecom" },
  { "45415", "SmarTone Mobile Comms", "SmarTone Mobile Comms" },
  { "45416", "PCCW",                  "PCCW Mobile (PCCW Ltd)" },
  { "45417", "SmarTone Mobile Comms", "SmarTone Mobile Comms" },
  { "45418", "Hong Kong CSL Limited", "Hong Kong CSL Limited" },
  { "45419", "PCCW",                  "PCCW Mobile (PCCW Ltd)" },

  /***************
   **** Macau ****
   ***************/
  { "45500", "SmarTone", "SmarTone Macau" },
  { "45501", "CTM",      "C.T.M. Telemovel+" },
  { "45503", "3",        "Hutchison Telecom" },
  { "45504", "CTM",      "C.T.M. Telemovel+" },
  { "45505", "3",        "Hutchison Telecom" },

  /******************
   **** Cambodia ****
   ******************/
  { "45601", "Mobitel",    "CamGSM" },
  { "45602", "hello",      "Telekom Malaysia International (Cambodia) Co. Ltd" },
  { "45604", "qb",         "Cambodia Advance Communications Co. Ltd" },
  { "45605", "Star-Cell",  "APPLIFONE CO. LTD." },
  { "45618", "Shinawatra", "Shinawatra" },

  /**************
   **** Laos ****
   **************/
  { "45701", "LaoTel", "Lao Shinawatra Telecom" },
  { "45702", "ETL",    "Enterprise of Telecommunications Lao" },
  { "45703", "LAT",    "Lao Asia Telecommunication State Enterprise (LAT)" },
  { "45708", "Tigo",   "Millicom Lao Co Ltd" },

  /***************
   **** China ****
   ***************/
  { "46000", "China Mobile", "China Mobile" },
  { "46001", "China Unicom", "China Unicom" },
  { "46003", "China Telecom","China Telecom"},
    /***********************
   **** Test Huawei ****
   ***********************/
  { "46009", "HWTest", "HuaweiTest" },

  /****************
   **** Taiwan ****
   ****************/
  { "46601", "FarEasTone", "Far EasTone Telecommunications Co Ltd" },
  { "46606", "Tuntex", "Tuntex Telecom" },
  { "46688", "KG Telecom", "KG Telecom" },
  //begin to add by hexiaokong 20111205 for DTS2011110306164
  { "46689", "VIBO", "VIBO Telecom" },
  //end to add by hexiaokong 20111205 for DTS2011110306164
  { "46692", "Chungwa", "Chungwa" },
  { "46693", "MobiTai", "iMobitai Communications" },
  { "46697", "Taiwan Mobile", "Taiwan Mobile Co. Ltd" },
  { "46699", "TransAsia", "TransAsia Telecoms" },

  /*********************
   **** North Korea ****
   *********************/
  { "467193", "SUN NET", "Korea Posts and Telecommunications Corporation" },

  /********************
   **** Bangladesh ****
   ********************/
  { "47001", "Grameenphone", "GrameenPhone Ltd" },
  { "47002", "Aktel",        "Aktel" },
  { "47003", "Banglalink",   "Orascom telecom Bangladesh Limited" },
  { "47004", "TeleTalk",     "TeleTalk" },
  { "47006", "Citycell",     "Citycell" },
  { "47007", "Warid",        "Warid Telecom" },

  /******************
   **** Maldives ****
   ******************/
  { "47201", "Dhiraagu", "Dhivehi Raajjeyge Gulhun" },
  { "47202", "Wataniya", "Wataniya Telecom Maldives" },

  /******************
   **** Malaysia ****
   ******************/
  { "50212", "Maxis",    "Maxis Communications Berhad" },
  { "50213", "Celcom",   "Celcom Malaysia Sdn Bhd" },
  { "50216", "DiGi",     "DiGi Telecommunications" },
  { "50218", "U Mobile", "U Mobile Sdn Bhd" },
  { "50219", "Celcom",   "Celcom Malaysia Sdn Bhd" },

  /*******************
   **** Australia ****
   *******************/
  { "50501", "Telstra",               "Telstra Corp. Ltd." },
  { "50502", "YES OPTUS",             "Singtel Optus Ltd" },
  { "50503", "Vodafone",              "Vodafone Australia" },
  { "50506", "3",                     "Hutchison 3G" },
  { "50590", "YES OPTUS",             "Singtel Optus Ltd" },

  /*******************
   **** Indonesia ****
   *******************/
  { "51000", "PSN",          "PT Pasifik Satelit Nusantara (ACeS)" },
  { "51001", "INDOSAT",      "PT Indonesian Satellite Corporation Tbk (INDOSAT)" },
  { "51008", "AXIS",         "PT Natrindo Telepon Seluler" },
  { "51010", "Telkomsel",    "PT Telkomunikasi Selular" },
  { "51011", "XL",           "PT Excelcomindo Pratama" },
  { "51089", "3",            "PT Hutchison CP Telecommunications" },

  /********************
   **** East Timor ****
   ********************/
  { "51402", "Timor Telecom", "Timor Telecom" },

  /********************
   **** Philipines ****
   ********************/
  { "51501", "Islacom",    "Innove Communicatiobs Inc" },
  { "51502", "Globe",      "Globe Telecom" },
  { "51503", "Smart Gold", "Smart Communications Inc" },
  { "51505", "Digitel",    "Digital Telecommunications Philppines" },
  { "51518", "Red Mobile", "Connectivity Unlimited resource Enterprise" },

  /******************
   **** Thailand ****
   ******************/
  { "52001", "Advanced Info Service", "Advanced Info Service" },
  { "52015", "ACT Mobile",            "ACT Mobile" },
  { "52018", "DTAC",                  "Total Access Communication" },
  { "52023", "Advanced Info Service", "Advanced Info Service" },
  { "52099", "True Move",             "True Move" },

  /*******************
   **** Singapore ****
   *******************/
  { "52501", "SingTel",                       "Singapore Telecom" },
  { "52502", "SingTel-G18",                   "Singapore Telecom" },
  { "52503", "M1",                            "MobileOne Asia" },
  { "52505", "StarHub",                       "StarHub Mobile" },

  /****************
   **** Brunei ****
   ****************/
  { "52802", "B-Mobile", "B-Mobile Communications Sdn Bhd" },
  { "52811", "DTSCom",   "DataStream Technology" },

  /*********************
   **** New Zealand ****
   *********************/
  { "53001", "Vodafone", "Vodafone New Zealand" },
  { "53003", "Woosh",    "Woosh wireless New Zealand" },
  { "53005", "Telecom",  "Telecom New Zealand" },
  { "53024", "NZ Comms", "NZ Communications New Zealand" },

  /**************************
   **** Papua New Guinea ****
   **************************/
  { "53701", "B-Mobile", "Pacific Mobile Communications" },

  /*************************
   **** Solomon Islands ****
   *************************/
  { "54001", "BREEZE", "Solomon Telekom Co Ltd" },

  /*****************
   **** Vanuatu ****
   *****************/
  { "54101", "SMILE", "telecom Vanuatu Ltd" },

  /**************
   **** Fiji ****
   **************/
  { "54201", "Vodafone", "Vodafone Fiji" },

  /******************
   **** Kiribati ****
   ******************/
  { "54509", "Kiribati Frigate", "Telecom services Kiribati Ltd" },

  /***********************
   **** New Caledonia ****
   ***********************/
  { "54601", "Mobilis", "OPT New Caledonia" },

  /**************************
   **** French Polynesia ****
   **************************/
  { "54720", "VINI", "Tikiphone SA" },

  /************************************
   **** Cook Islands (New Zealand) ****
   ************************************/
  { "54801", "Telecom Cook", "Telecom Cook" },

  /***************
   **** Samoa ****
   ***************/
  { "54901", "Digicel",  "Digicel Pacific Ltd." },
  { "54927", "SamoaTel", "SamoaTel Ltd" },

  /********************
   **** Micronesia ****
   ********************/
  { "55001", "FSM Telecom", "FSM Telecom" },

  /***************
   **** Palau ****
   ***************/
  { "55201", "PNCC",         "Palau National Communications Corp." },
  { "55280", "Palau Mobile", "Palau Mobile Corporation" },

  /***************
   **** Egypt ****
   ***************/
  { "60201", "Mobinil",  "ECMS-Mobinil" },
  { "60202", "Vodafone", "Vodafone Egypt" },
  { "60203", "etisalat", "Etisalat Egypt" },

  /*****************
   **** Algeria ****
   *****************/
  { "60301", "Mobilis", "ATM Mobilis" },
  { "60302", "Djezzy", "Orascom Telecom Algerie Spa" },
  { "60303", "Nedjma", "Wataniya Telecom Algerie" },

  /*****************
   **** Morocco ****
   *****************/
  { "60400", "Meditel", "Medi Telecom" },
  { "60401", "IAM",     "Ittissalat Al Maghrib (Maroc Telecom)" },

  /*****************
   **** Tunisia ****
   *****************/
  { "60502", "Tunicell", "Tunisie Telecom" },
  { "60503", "Tunisiana", "Orascom Telecom Tunisie" },

  /***************
   **** Libya ****
   ***************/
  { "60600", "Libyana", "Libyana" },
  { "60601", "Madar",   "Al Madar" },

  /*******************
   **** Mauritius ****
   *******************/
  { "60901", "Mattel",   "Mattel" },
  { "60910", "Mauritel", "Mauritel Mobiles" },

  /**************
   **** Mali ****
   **************/
  { "61001", "Malitel", "Malitel SA" },
  { "61002", "Orange",  "Orange Mali SA" },

  /****************
   **** Guinea ****
   ****************/
  { "61102", "Lagui",          "Sotelgui Lagui" },
  { "61103", "Telecel Guinee", "INTERCEL Guinee" },
  { "61104", "MTN",            "Areeba Guinea" },

  /*********************
   **** Ivory Coast ****
   *********************/
  { "61202", "Moov",   "Moov" },
  { "61203", "Orange", "Orange" },
  { "61204", "KoZ",    "Comium Ivory Coast Inc" },
  { "61205", "MTN",    "MTN" },
  { "61206", "ORICEL", "ORICEL" },

  /**********************
   **** Burkina Faso ****
   **********************/
  { "61301", "Onatel",       "Onatel" },
  { "61302", "Zain",         "Celtel Burkina Faso" },
  { "61303", "Telecel Faso", "Telecel Faso SA" },

  /*****************
   **** Nigeria ****
   *****************/
  { "61401", "SahelCom", "SahelCom" },
  { "61402", "Zain",     "Celtel Niger" },
  { "61403", "Telecel",  "Telecel Niger SA" },
  { "61404", "Orange",   "Orange Niger" },

  /**************
   **** Togo ****
   **************/
  { "61501", "Togo Cell", "Togo Telecom" },
  { "61505", "Telecel",   "Telecel Togo" },

  /***************
   **** Benin ****
   ***************/
  { "61600", "BBCOM",   "Bell Benin Communications" },
  { "61602", "Telecel", "Telecel Benin Ltd" },
  { "61603", "Areeba",  "Spacetel Benin" },

  /*******************
   **** Mauritius ****
   *******************/
  { "61701", "Orange", "Cellplus Mobile Communications Ltd" },
  { "61710", "Emtel",  "Emtel Ltd" },

  /*****************
   **** Liberia ****
   *****************/
  { "61801", "LoneStar Cell", "Lonestar Communications Corporation" },

  /***************
   **** Ghana ****
   ***************/
  { "62001", "MTN",                   "ScanCom Ltd" },
  { "62002", "Ghana Telecomi Mobile", "Ghana Telecommunications Company Ltd" },
  { "62003", "tiGO",                  "Millicom Ghana Limited" },

  /*****************
   **** Nigeria ****
   *****************/
  { "62120", "Zain",  "Celtel Nigeria Ltd." },
  { "62130", "MTN",   "MTN Nigeria Communications Limited" },
  { "62140", "M-Tel", "Nigerian Mobile Telecommunications Limited" },
  { "62150", "Glo",   "Globacom Ltd" },

  /**************
   **** Chad ****
   **************/
  { "62201", "Zain",            "CelTel Tchad SA" },
  { "62203", "TIGO - Millicom", "TIGO - Millicom" },

  /**********************************
   **** Central African Republic ****
   **********************************/
  { "62301", "CTP", "Centrafrique Telecom Plus" },
  { "62302", "TC", "iTelecel Centrafrique" },
  { "62303", "Orange", "Orange RCA" },
  { "62304", "Nationlink", "Nationlink Telecom RCA" },

  /******************
   **** Cameroon ****
   ******************/
  { "62401", "MTN-Cameroon", "Mobile Telephone Network Cameroon Ltd" },
  { "62402", "Orange",       "Orange Cameroun S.A." },

  /********************
   **** Cabo Verde ****
   ********************/
  { "62501", "CMOVEL", "CVMovel, S.A." },

  /*******************************
   **** Sao Tome and Principe ****
   *******************************/
  { "62601", "CSTmovel", "Companhia Santomese de Telecomunicacoe" },

  /**************************
   *** Equatorial Guinea ****
   **************************/
  { "62701", "Orange GQ", "GETESA" },

  /***************
   **** Gabon ****
   ***************/
  { "62801", "Libertis",                  "Libertis S.A." },
  { "62802", "Moov (Telecel) Gabon S.A.", "Moov (Telecel) Gabon S.A." },
  { "62803", "Zain",                      "Celtel Gabon S.A." },

  /*******************************
   **** Republic of the Congo ****
   *******************************/
  { "62910", "Libertis Telecom", "MTN CONGO S.A" },

  /******************************************
   **** Democratic Republic of the Congo ****
   ******************************************/
  { "63001", "Vodacom",      "Vodacom Congo RDC sprl" },
  { "63002", "Zain",         "Celtel Congo" },
  { "63005", "Supercell",    "Supercell SPRL" },
  { "63086", "CCT",          "Congo-Chine Telecom s.a.r.l" },
  { "63089", "SAIT Telecom", "OASIS SPRL" },

  /*****************
   **** Angola ****
   *****************/
  { "63102", "UNITEL", "UNITEL S.a.r.l." },

  /***********************
   **** Guinea-Bissau ****
   ***********************/
  { "63202", "Areeba", "Spacetel Guine-Bissau S.A." },

  /********************
   **** Seychelles ****
   ********************/
  { "63302", "Mdeiatech International", "Mdeiatech International Ltd." },

  /***************
   **** Sudan ****
   ***************/
  { "63401", "Mobitel/Mobile Telephone Company", "Mobitel/Mobile Telephone Company" },
  { "63402", "MTN",                              "MTN Sudan" },

  /****************
   **** Rwanda ****
   ****************/
  { "63510", "MTN", "MTN Rwandacell SARL" },

  /******************
   **** Ethiopia ****
   ******************/
  { "63601", "ETMTN", "Ethiopian Telecommmunications Corporation" },

  /*****************
   **** Somalia ****
   *****************/
  { "63704", "Somafona",       "Somafona FZLLC" },
  { "63710", "Nationalink",    "Nationalink" },
  { "63719", "Hormuud",        "Hormuud Telecom Somalia Inc" },
  { "63730", "Golis",          "Golis Telecommunications Company" },
  { "63762", "Telcom Mobile",  "Telcom Mobile" },
  { "63765", "Telcom Mobile",  "Telcom Mobile" },
  { "63782", "Telcom Somalia", "Telcom Somalia" },

  /******************
   **** Djibouti ****
   ******************/
  { "63801", "Evatis", "Djibouti Telecom SA" },

  /***************
   **** Kenya ****
   ***************/
  { "63902", "Safaricom",    "Safaricom Limited" },
  { "63903", "Zain",         "Celtel Kenya Limited" },
  { "63907", "Orange Kenya", "Telkom Kemya" },

  /******************
   **** Tanzania ****
   ******************/
  { "64002", "Mobitel", "MIC Tanzania Limited" },
  { "64003", "Zantel",  "Zanzibar Telecom Ltd" },
  { "64004", "Vodacom", "Vodacom Tanzania Limited" },

  /****************
   **** Uganda ****
   ****************/
  { "64110", "MTN",                 "MTN Uganda" },
  { "64114", "Orange",              "Orange Uganda" },
  { "64122", "Warid Telecom",       "Warid Telecom" },

  /*****************
   **** Burundi ****
   *****************/
  { "64201", "Spacetel", "Econet Wireless Burundi PLC" },
  { "64202", "Aficell",  "Africell PLC" },
  { "64203", "Telecel",  "Telecel Burundi Company" },

  /********************
   **** Mozambique ****
   ********************/
  { "64301", "mCel",    "Mocambique Celular S.A.R.L." },

  /****************
   **** Zambia ****
   ****************/
  { "64501", "Zain",   "Zain" },
  { "64502", "MTN",    "MTN" },
  { "64503", "ZAMTEL", "ZAMTEL" },

  /********************
   **** Madagascar ****
   ********************/
  { "64601", "Zain",   "Celtel" },
  { "64602", "Orange", "Orange Madagascar S.A." },
  { "64604", "Telma",  "Telma Mobile S.A." },

  /**************************
   **** Reunion (France) ****
   **************************/
  { "64700", "Orange",      "Orange La Reunion" },
  { "64702", "Outremer",    "Outremer Telecom" },
  { "64710", "SFR Reunion", "Societe Reunionnaisei de Radiotelephone" },

  /******************
   **** Zimbabwe ****
   ******************/
  { "64801", "Net*One", "Net*One cellular (Pvt) Ltd" },
  { "64803", "Telecel", "Telecel Zimbabwe (PVT) Ltd" },
  { "64804", "Econet",  "Econet Wireless (Private) Limited" },

  /*****************
   **** Namibia ****
   *****************/
  { "64901", "MTC",      "MTC Namibia" },
  { "64903", "Cell One", "Telecel Globe (Orascom)" },

  /****************
   **** Malawi ****
   ****************/
  { "65001", "TNM",  "Telecom Network Malawi" },
  { "65010", "Zain", "Celtel Limited" },

  /*****************
   **** Lesotho ****
   *****************/
  { "65101", "Vodacom",          "Vodacom Lesotho (Pty) Ltd" },

  /******************
   **** Botswana ****
   ******************/
  { "65201", "Mascom",     "Mascom Wirelessi (Pty) Limited" },
  { "65202", "Orange",     "Orange (Botswans) Pty Limited" },
  { "65204", "BTC Mobile", "Botswana Telecommunications Corporation" },

  /**********************
   **** South Africa ****
   **********************/
  { "65501", "Vodacom",                          "Vodacom" },
  { "65502", "Telkom",                           "Telkom" },
  { "65507", "Cell C",                           "Cell C" },
  { "65510", "MTN",                              "MTN Group" },

  /*****************
   **** Eritrea ****
   *****************/
  { "65701", "Eritel", "Eritel Telecommunications Services Corporation" },

  /****************
   **** Belize ****
   ****************/
  { "70267", "Belize Telemedia",                      "Belize Telemedia" },
  { "70268", "International Telecommunications Ltd.", "International Telecommunications Ltd." },

  /*******************
   **** Guatemala ****
   *******************/
  { "70401", "Claro",         "Servicios de Comunicaciones Personales Inalambricas (SRECOM)" },
  { "70402", "Comcel / Tigo", "Millicom / Local partners" },
  { "70403", "movistar",      "Telefonica Moviles Guatemala (Telefonica)" },

  /*********************
   **** El Salvador ****
   *********************/
  { "70601", "CTE Telecom Personal",  "CTE Telecom Personal SA de CV" },
  { "70602", "digicel",               "Digicel Group" },
  { "70603", "Telemovil EL Salvador", "Telemovil EL Salvador S.A" },
  { "70604", "movistar",              "Telfonica Moviles El Salvador" },
  { "70610", "Claro",                 "America Movil" },

  /******************
   **** Honduras ****
   ******************/
  { "70801", "Claro",          "Servicios de Comunicaciones de Honduras S.A. de C.V." },
  { "70802", "Celtel / Tigo",  "Celtel / Tigo" },
  { "70804", "DIGICEL",        "Digicel de Honduras" },
  { "70830", "Hondutel",       "Empresa Hondurena de telecomunicaciones" },

  /*******************
   **** Nicaragua ****
   *******************/
  { "71021", "Claro",    "Empresa Nicaraguense de Telecomunicaciones,S.A." },
  { "71030", "movistar", "Telefonica Moviles de Nicaragua S.A." },
  { "71073", "SERCOM",   "Servicios de Comunicaciones S.A." },

  /*******************
   **** Cost Rica ****
   *******************/
  { "71201", "ICE", "Instituto Costarricense de Electricidad" },
  { "71202", "ICE", "Instituto Costarricense de Electricidad" },

  /****************
   **** Panama ****
   ****************/
  { "71401", "Cable & Wireless", "Cable & Wireless Panama S.A." },
  { "71402", "movistar",         "Telefonica Moviles Panama S.A" },
  { "71404", "Digicel",          "Digicel (Panama) S.A." },

  /**************
   **** Peru ****
   **************/
  { "71606", "movistar", "Telefonica Moviles Peru" },
  { "71610", "Claro",    "America Movil Peru" },

  /*******************
   **** Argentina ****
   *******************/
  { "72210", "Movistar", "Telefonica Moviles Argentina SA" },
  { "72270", "Movistar", "Telefonica Moviles Argentina SA" },
  { "722310", "Claro",    "AMX Argentina S.A" },
  { "722320", "Claro",    "AMX Argentina S.A" },
  { "722330", "Claro",    "AMX Argentina S.A" },
  { "722340", "Personal", "Teecom Personal SA" },

  /****************
   **** Brazil ****
   ****************/
  { "72402", "TIM",                   "Telecom Italia Mobile" },
  { "72403", "TIM",                   "Telecom Italia Mobile" },
  { "72404", "TIM",                   "Telecom Italia Mobile" },
  { "72405", "Claro",                 "Claro (America Movil)" },
  { "72406", "Vivo",                  "Vivo S.A." },
  { "72407", "CTBC Celular",           "CTBC Telecom" },
  { "72408", "TIM",                   "Telecom Italiz Mobile" },
  { "72410", "Vivo",                  "Vivo S.A." },
  { "72411", "Vivo",                  "Vivo S.A." },
  { "72415", "Sercomtel",             "Sercomtel Celular" },
  { "72416", "Oi / Brasil Telecom",   "Brasil Telecom Celular SA" },
  { "72423", "Vivo",                  "Vivo S.A." },
  { "72424", "Oi / Amazonia Celular", "Amazonia Celular S.A." },
  { "72431", "Oi",                    "TNL PCS" },
  { "72437", "aeiou",                 "Unicel do Brasil" },

  /***************
   **** Chile ****
   ***************/
  { "73001", "Entel",    "Entel Pcs" },
  { "73002", "movistar", "Movistar Chile" },
  { "73003", "Claro",    "Claro Chile"},
  { "73010", "Entel",    "Entel Telefonica Movil" },

  /******************
   **** Colombia ****
   ******************/
  { "732101", "Comcel",   "Comcel Colombia" },
  { "732102", "movistar", "Bellsouth Colombia" },
  { "732103", "Tigo",     "Colombia Movil" },
  { "732111", "Tigo",     "Colombia Movil" },
  { "732123", "movistar", "Telefonica Moviles Colombia" },

  /*******************
   **** Venezuela ****
   *******************/
  { "73401", "Digitel",  "Corporacion Digitel C.A." },
  { "73402", "Digitel",  "Corporacion Digitel C.A." },
  { "73403", "Digitel",  "Corporacion Digitel C.A." },
  { "73404", "movistar", "Telefonica Moviles Venezuela" },
  { "73406", "Movilnet", "Telecommunicaciones Movilnet" },

  /*****************
   **** Bolivia ****
   *****************/
  { "73601", "Nuevatel", "Nuevatel PCS De Bolivia SA" },
  { "73602", "Entel",    "Entel SA" },
  { "73603", "Tigo",     "Telefonica Celular De Bolivia S.A" },

  /****************
   **** Guyana ****
   ****************/
  { "73801", "Digicel", "U-Mobile (Cellular) Inc." },

  /*****************
   **** Ecuador ****
   *****************/
  { "74000", "Movistar", "Otecel S.A." },
  { "74001", "Porta",    "America Movil" },

  /******************
   **** Paraguay ****
   ******************/
  { "74401", "VOX",      "Hola Paraguay S.A." },
  { "74402", "Claro",    "AMX Paraguay S.A." },
  { "74404", "Tigo",     "Telefonica Celular Del Paraguay S.A. (Telecel)" },
  { "74405", "Personal", "Nucleo S.A." },

  /*****************
   **** Uruguay ****
   *****************/
  { "74801", "Ancel",    "Ancel" },
  { "74807", "Movistar", "Telefonica Moviles Uruguay" },
  { "74810", "Claro",    "AM Wireless Uruguay S.A." },

  /*******************
   **** Satellite ****
   *******************/
  { "90101", "ICO",                                          "ICO Satellite Management" },
  { "90102", "Sense Communications International",           "Sense Communications International" },
  { "90103", "Iridium",                                      "Iridium" },
  { "90104", "GlobalStar",                                   "Globalstar" },
  { "90105", "Thuraya RMSS Network",                         "Thuraya RMSS Network" },
  { "90106", "Thuraya Satellite telecommunications Company", "Thuraya Satellite Telecommunications Company" },
  { "90107", "Ellipso",                                      "Ellipso" },
  { "90109", "Tele1 Europe",                                 "Tele1 Europe" },
  { "90110", "ACeS",                                         "ACeS" },
  { "90111", "Immarsat",                                     "Immarsat" },

  /*************
   **** Sea ****
   *************/
  { "90112", "MCP",                                          "Maritime Communications Partner AS" },

  /****************
   **** Ground ****
   ****************/
  { "90113", "GSM.AQ",                                       "GSM.AQ" },

  /*************
   **** Air ****
   *************/
  { "90114", "AeroMobile AS",                                "AeroMobile AS" },
  { "90115", "OnAir Switzerland Sarl",                       "OnAir Switzerland Sarl" },

  /*******************
   **** Satellite ****
   *******************/
  { "90116", "Jasper Systems",                               "Jasper Systems" },
  { "90117", "Navitas",                                      "Navitas" },
  { "90118", "Cingular Wireless",                            "Cingular Wireless" },
  { "90119", "Vodafone Malta Maritime",                      "Vodafone Malta Maritime" }

};

/*===========================================================================

                                FUNCTIONS

===========================================================================*/

/***************************************************
 Function:  Operator_ons_lookup_memory_list
 Description:  
    lookup the Operator's  long alpha ONS or EONS and short alpha ONS 
    or EONS
 Calls: 
     NULL
 Called By:
    static void request_operator(void *data, size_t datalen, RIL_Token t)
     static void request_query_available_networks(RIL_Token  t)
 Input:
    long_ons_ptr   - long alpha operator name 
    short_ons_ptr  - short alpha operator name
    mcc_mnc_ptr - 5 or 6 digit numeric code (MCC + MNC) 
 Output:
    none
 Return:
    none
 Others:
    add by lifei kf34826 2010.12.31
**************************************************/

void Operator_ons_lookup_memory_list(char **long_ons_ptr, char **short_ons_ptr, char **mcc_mnc_ptr)
{
  const int number_of_entries = sizeof( oper_memory_list ) / sizeof( Oper_memory_entry_type );
  int i = 0;
  int continue_search = 1;
  const Oper_memory_entry_type *ons_mem_ptr = NULL;

  if(*mcc_mnc_ptr  != NULL)
  {
	/* Search the table for the MCC and MNC */
	while ( continue_search && ( i < number_of_entries ) )
	{  
		if (!strcmp(*mcc_mnc_ptr , oper_memory_list[ i ].mcc) )
		{
		   ons_mem_ptr = &oper_memory_list[ i ];
		   continue_search = 0;
		}
		i++;
	  } /* end while */
  }
  if ( ons_mem_ptr == NULL )
  {   
      *long_ons_ptr = NULL;
      *short_ons_ptr = NULL;
  }
  else
  {
   *long_ons_ptr = (ons_mem_ptr->full_name_ptr);
   *short_ons_ptr = (ons_mem_ptr->short_name_ptr);
  }

} /* operator_ons_lookup_memory_list */   
void search_automatic(void *param);
struct operatorPollParams
{
	RIL_Token t;
	int loopcount;
};
/* End added by x84000798 2012-06-01 DTS2012060806329 */
extern int UCS2_to_UTF8(char *src, char *dst);
/*===========================================================================

                                FUNCTIONS

===========================================================================*/


/**
 * Indicates the current state of the screen.  When the screen is off, the
 * RIL should notify the baseband to suppress certain notifications (eg,
 * signal strength and changes in LAC or CID) in an effort to conserve power.
 * These notifications should resume when the screen is on.
 *
 * "data" is int *
 * ((int *)data)[0] is == 1 for "Screen On"
 * ((int *)data)[0] is == 0 for "Screen Off"
 *
 * "response" is NULL
 *
 * Valid errors:
 *  SUCCESS
 *  GENERIC_FAILURE
 */
void request_screen_state(void *data, size_t datalen, RIL_Token t)
{	
    int err = 0; //modify by fKF34305 for PC-lint

    /* BEGIN DTS2011111400455  lkf52201 2011-12-7 added */
	ATResponse *p_response = NULL;
	char *line = NULL;
	int cid = -1;
	int active = -1;
	/* END DTS2011111400455  lkf52201 2011-12-7 added */

    if((*(int *)data)==0)
    {
        err = at_send_command("AT^CURC=0", NULL);
	 if(err < 0)  
        {
	     goto error;
        }
	 
        //begin add by hkf39947 20110802 DTS2011080205073, according to ril definition, disable LAC/CID when screen off
        err = at_send_command("AT+CREG=0", NULL);
        if(err < 0)  
        {
	     goto error;
        }

        err = at_send_command("AT+CGREG=0", NULL); 
        if(err < 0)  
        {
	     goto error;
        }
        //end add by hkf39947 20110802 DTS2011080205073, according to ril definition, disable LAC/CID when screen off
        
        RIL_onRequestComplete( t, RIL_E_SUCCESS,  NULL, 0);
    }
    else
    {
        if(RIL_DEBUG) ALOGD( "request_screen_state %d\n", (*(int *)data));
        /* BEGIN DTS2011111400455  lkf52201 2011-12-7 added */
		if (NETWORK_WCDMA == g_module_type)    // CDMA现在没有AT+CGACT命令
		{
		    int tryScreenOff = 1;
		    do
		    {
    		    /* 当定义了多个PDP上下文时，AT+CGACT?命令可能会有多个返回值，但是RIL程序中强制
    		     * 定义了CID为1，所以PDP上下文应该只有一个，下面使用了at_send_command_singleline()
    		     * 函数来获取AT+CGACT?的单行返回值，没有考虑AT+CGACT?返回多行的情况.
    		     *
    		     * 由于这段代码是客户需求引入,当前接口实际上不需要下发AT+CGACT?命令,则下发
    		     * 这个AT的过程中,如果出错,不会报错到上层,而是继续进行后面的处理;即这个AT的
    		     * 执行情况不影响当前接口的上报值.
    		     */
    			err = at_send_command_singleline("AT+CGACT?", "+CGACT:", &p_response);
    		    if (err < 0 || 0 == p_response->success)
    		    {
    		        err = at_send_command("AT^CURC=0", NULL);
            	    if(err < 0)  
                    {
            	        goto error;
                    }
            	 
                    //begin add by hkf39947 20110802 DTS2011080205073, according to ril definition, disable LAC/CID when screen off
                    err = at_send_command("AT+CREG=0", NULL);
                    if(err < 0)  
                    {
            	        goto error;
                    }
            
                    err = at_send_command("AT+CGREG=0", NULL); 
                    if(err < 0)  
                    {
            	        goto error;
                    }
                    
                    if (tryScreenOff > 0)
                    {
                        tryScreenOff--;
                        usleep(100000);
                        continue;
                    }
    		        break;
    		    }
    		    line = p_response->p_intermediates->line;
    			err = at_tok_start(&line);
    			if (err < 0) 
    			{
    				break;
    			}
    			err = at_tok_nextint(&line, &cid);
    			if (err < 0) 
    			{
    				break;
    			}
    			err = at_tok_nextint(&line, &active);
    			if (err < 0) 
    			{
    				break;
    			}
    			//if (0 == active) // by alic :按照我们修改过的，这段是屏蔽了的。但huawei源码没有屏蔽
    			//{
    				//RIL_onUnsolicitedResponse(RIL_UNSOL_DATA_CALL_LIST_CHANGED, NULL, 0);
    			//}
		    } while(0);
		}
		at_response_free(p_response);
		/* END DTS2011111400455  lkf52201 2011-12-7 added */
		
        at_send_command("AT^CURC=1", NULL);
        if(err < 0)  
        {
	     goto error;
        }
		
        //begin add by hkf39947 20110802 DTS2011080205073, according to ril definition, enable LAC/CID when screen on
        err = at_send_command("AT+CREG=2", NULL);
        if(err < 0)  
        {
	     goto error;
        }

        err = at_send_command("AT+CGREG=2", NULL); 
        if(err < 0)  
        {
	     goto error;
        }
        //end add by hkf39947 20110802 DTS2011080205073, according to ril definition, enable LAC/CID when screen on
        
        RIL_onRequestComplete( t, RIL_E_SUCCESS,  NULL, 0);
//begin modified by wkf32792 for android 3.x 20110815        
#ifdef ANDROID_3
        RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,NULL, 0);
#else
		RIL_onUnsolicitedResponse(RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED,NULL, 0);//Added by lkf34826  DTS2011032203112 V3D01:增加网络状态改变的主动上报 */ 
#endif
//end modified by wkf32792 for android 3.x 20110815		
   }
    return;

error:
    RIL_onRequestComplete( t, RIL_E_GENERIC_FAILURE,  NULL, 0);
    at_response_free(p_response);  // DTS2011111400455 lkf52201 2011-12-7 added
    return;
}

/**
 * Request current registration state
 *
 * "data" is NULL
 * "response" is a "char **"
 * ((const char **)response)[0] is registration state 0-5 from TS 27.007 7.2
 * ((const char **)response)[1] is LAC if registered or NULL if not
 * ((const char **)response)[2] is CID if registered or NULL if not
 *
 * LAC and CID are in hexadecimal format.
 * valid LAC are 0x0000 - 0xffff
 * valid CID are 0x00000000 - 0x0fffffff
 *     In GSM, CID is Cell ID (as described in TS 27.007) in 16 bits
 *     In UMTS, CID is UMTS Cell Identity (as described in TS 25.331) in 28 bits
 * 
 * Please note that registration state 4 ("unknown") is treated 
 * as "out of service" in the Android telephony system
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
static void request_cdma_registration_state(int request, void *data,
                                        size_t datalen, RIL_Token t)
                                     
{
    int err;
    int response[7];
    char * responseStr[14]={0};
    ATResponse *p_response = NULL;
    char *line;
    int srv_status, srv_domain, raom_status, sys_mode, sim_state;
    int curr_sid = 0;
    int curr_nid = 0;
    int i;
    
    if(RIL_DEBUG) ALOGD("----***********request_cdma_registration_state********---");
    if(RIL_DEBUG) ALOGD("registration cdma state");

    /*Just handle CDMA module, ignore UMTS module*/
    err = at_send_command_singleline("AT^SYSINFO", "^SYSINFO:", &p_response);
    if (err < 0 || p_response->success == 0)
    { 
        goto error;
    }
    
    line = p_response->p_intermediates->line;
    err = at_tok_start (&line);    
    if (err < 0) {
        goto error;
    }

	err = at_tok_nextint(&line, &srv_status);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &srv_domain);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &raom_status);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &sys_mode);
	if (err < 0) {
		goto error;
	}

	err = at_tok_nextint(&line, &sim_state);
	if (err < 0) {
		goto error;
	}

    switch(srv_status)
    {
        case 0: /* Not registered*/
            response[0] = 0;
            break;
            
        case 4: /* Save power status */
        case 2: /*Registered*/
            if(1 == raom_status)
            {
                response[0] = 5;
            }
            else
            {
                response[0] = 1;
            }
            break;
        default: /*No error is considerred*/
            response[0] = 0;
            break;
    }
    /* BEGIN DTS2011123101074 lkf52201 2011-12-31 added */
    at_response_free(p_response);
    p_response = NULL;
    /* END   DTS2011123101074 lkf52201 2011-12-31 added */

    /* BEGIN DTS2011113004936  lixianyi 2011-12-1 added */
	 if((g_module_type != NETWORK_CDMA)
		&&(g_modem_vendor != MODEM_VENDOR_ZTE))
	{
	    err = at_send_command_singleline("AT^CURRSID?", "^CURRSID:", &p_response);
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
	    
	    err = at_tok_nextint(&line, &curr_sid);   
	    if (err < 0) 
	    {
	        goto error;   
	    }

	    err = at_tok_nextint(&line, &curr_nid);   
	    if (err < 0) 
	    {
	        goto error;   
	    }
	}
    /* END DTS2011113004936  lixianyi 2011-12-1 added */

    /*LAC for UMTS, CDMA not available, set to 0x0000*/
    response[1] = 0;
    /*CID if registered on a * GSM/WCDMA or NULL if not. CDMA not available, set to 0x00000000*/
    response[2] = 0;

    switch(sys_mode)
    {
        case 0: /*0 - Unknown*/
            response[3] = 0;
            break;
        case 2: /*CDMA*/
            response[3] = 6;
            break;
        case 4:/*EVDO*/
            response[3] = 8;
            break;
        case 8:/*Hybird, no item for Hybird*/
            response[3] = 8;
            break;
        default: /*No error is considerred*/
            response[3] = 0;
            break;
    }

    asprintf(&responseStr[0], "%d", response[0]);
    responseStr[1]= NULL;
    responseStr[2]= NULL;
    //asprintf(&responseStr[1], "%x", response[1]);
    //asprintf(&responseStr[2], "%x", response[2]);
    asprintf(&responseStr[3], "%x", response[3]);
    asprintf(&responseStr[4], "%d", -1);        // modified by lkf52201 2011-12-1
    asprintf(&responseStr[5], "%d", INT_MAX);   // modified by lkf52201 2011-12-1
    asprintf(&responseStr[6], "%d", INT_MAX);   // modified by lkf52201 2011-12-1
    asprintf(&responseStr[7], "%x", 0);
    
    /* BEGIN DTS2011113004936  lixianyi 2011-12-1 modified */
    asprintf(&responseStr[8], "%d", curr_sid);
    asprintf(&responseStr[9], "%d", curr_nid);
    /* END DTS2011113004936  lixianyi 2011-12-1 modified */

    /* BEGIN DTS2011113002950  lixianyi 2011-12-1 modified */
    asprintf(&responseStr[10], "%d", 1);
    asprintf(&responseStr[11], "%d", 1);
    asprintf(&responseStr[12], "%d", 1);
    /* END DTS2011113002950  lixianyi 2011-12-1 modified */
    asprintf(&responseStr[13], "%x", 0);  
    

    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, 14*sizeof(char*));
    at_response_free(p_response);    

    /* BEGIN DTS2011113002950  lixianyi 2011-12-1 added, 释放由asprintf()函数分配的内存 */
    for (i = 0; i < 14; ++i)
    {
        free(responseStr[i]);
    }
    /* END DTS2011113002950  lixianyi 2011-12-1 added, 释放由asprintf()函数分配的内存 */
    return;
    
error:
    ALOGE("requestRegistrationState must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}


static void request_registration_state(int request, void *data,
                                        size_t datalen, RIL_Token t)
{
    int err;
    int response[4];
    char * responseStr[4];
    ATResponse *p_response = NULL;

    char *line, *p;
    int commas;
    int skip;
      
    if(RIL_DEBUG) ALOGD("registration state");
    err = at_send_command_singleline("AT+CREG?", "+CREG:", &p_response);

    if (err < 0 || p_response->success == 0) 
	{
	    if(RIL_DEBUG) ALOGE( "err %d,\n",err);
		goto error;
    }
    
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;
    
    /* Ok you have to be careful here
     * The solicited version of the CREG response is
     * +CREG: n, stat, [lac, cid]
     * and the unsolicited version is
     * +CREG: stat, [lac, cid]
     * The <n> parameter is basically "is unsolicited creg on?"
     * which it should always be
     *
     * Now we should normally get the solicited version here,
     * but the unsolicited version could have snuck in
     * so we have to handle both
     *
     * Also since the LAC and CID are only reported when registered,
     * we can have 1, 2, 3, or 4 arguments here
     *
     * finally, a +CGREG: answer may have a fifth value that corresponds
     * to the network type, as in;
     *
     *   +CGREG: n, stat [,lac, cid [,networkType]]
     */
    /* count number of commas */
    commas = 0;
    for (p = line ; *p != '\0' ;p++) 
    {
        if (*p == ',') commas++;
    } 

    switch (commas) {
        case 0: /* +CREG: <stat> */
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
        break;

        case 1: /* +CREG: <n>, <stat> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
            if (err < 0) goto error;
        break;

        case 2: /* +CREG: <stat>, <lac>, <cid> */
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
        break;
        case 3: /* +CREG: <n>, <stat>, <lac>, <cid> */
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
        break;
		case 4: /* +CREG: <n>, <stat>, <lac>, <cid> */
		case 5:
            err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
            if (err < 0) goto error;
        break;
        default:
            ALOGE( "CREG : Radio module is invalid!");
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
    }
   /*BEGIN Modify by lkf34826 DTS2011040803135: V3D01 上报的LAC、CID字符串格式不正确 2011-04-08*/
    asprintf(&responseStr[0], "%d", response[0]);
    asprintf(&responseStr[1], "%x", response[1]);
    asprintf(&responseStr[2], "%x", response[2]);
   /*END Modify by lkf34826 DTS2011040803135: V3D01 上报的LAC、CID字符串格式不正确 2011-04-08*/
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, 3*sizeof(char*));
    at_response_free(p_response);
    free(responseStr[0]);
	free(responseStr[1]);
	free(responseStr[2]);
    return;
    
error:
    if(RIL_DEBUG) ALOGE( "request_registration_state must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}

/**
 * RIL_REQUEST_GPRS_REGISTRATION_STATE
 *
 * Request current GPRS registration state
 *
 * "data" is NULL
 * "response" is a "char **"
 * ((const char **)response)[0] is registration state 0-5 from TS 27.007 10.1.20 AT+CGREG
 * ((const char **)response)[1] is LAC if registered or NULL if not
 * ((const char **)response)[2] is CID if registered or NULL if not
 * ((const char **)response)[3] indicates the available radio technology, where:
 *      0 == unknown
 *      1 == GPRS only
 *      2 == EDGE
 *      3 == UMTS
 *      9 == HSDPA
 *      10 == HSUPA
 *      11 == HSPA
 *
 * LAC and CID are in hexadecimal format.
 * valid LAC are 0x0000 - 0xffff
 * valid CID are 0x00000000 - 0x0fffffff
 *
 * Please note that registration state 4 ("unknown") is treated
 * as "out of service" in the Android telephony system
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 *  GENERIC_FAILURE
 */
static void request_gprs_registration_state(int request, void *data,
                                        size_t datalen, RIL_Token t)
{
    int err;
    int skip;
    int response[4]={0};
    int srv_status = 0, srv_domain = 0, raom_status, sys_mode, sim_state, lock_state, sys_submode;
	char *sysmode_name, *submode_name;	//added by c00221986 for DTS2013040706143
    char *line;
	char *cpinResult;
    char * responseStr[4]={0};
    ATResponse *p_response = NULL;
	char *p;
    int commas;
	
mRetry:
	if(RIL_DEBUG) ALOGD("registration gprs state");
    err = at_send_command_singleline("AT+CGREG?", "+CGREG:", &p_response);
    if (err < 0 || p_response->success == 0) 
    {
        goto error;
    }
    line = p_response->p_intermediates->line;
    err = at_tok_start(&line);
    if (err < 0) goto error;
    commas = 0;
    for (p = line ; *p != '\0' ;p++) 
    {
        if (*p == ',') commas++;
    } 
	switch(commas)
	{
		case 0:/* +CGREG: <stat> */
			err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
			response[3] = 0;
			break;
		case 1:/* +CGREG: <n>, <stat> */
			err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
			response[3] = 0;
			
			if ((response[0] == 0) && (sState == RADIO_STATE_SIM_READY))
			{
                at_response_free(p_response); 
                usleep(100000);
                goto mRetry;
			}
			
			break;
		case 2:/* +CGREG: <stat>, <lac>, <cid> */
			err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
			response[3] = 0;
			break;
		case 3:/* +CGREG: <n>, <stat>, <lac>, <cid> */
			err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
			break;
		case 4:/* +CGREG: <n>, <stat>, <lac>, <cid> */
		case 5:/*MT509*/
			err = at_tok_nextint(&line, &skip);
            if (err < 0) goto error;
            err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[1]);
            if (err < 0) goto error;
            err = at_tok_nexthexint(&line, &response[2]);
			break;
        default:
            /* 不会出现这种情况，除非单板出错 */ 
            ALOGE( "CGREG : Radio module is invalid!");
			err = at_tok_nextint(&line, &response[0]);
            if (err < 0) goto error;
            response[1] = -1;
            response[2] = -1;
			response[3] = 0;
	}
	if(RIL_DEBUG) ALOGD("+CGREG:%d rsp1:%d  rsp2:%d  rsp3:%d",commas, response[1], response[2], response[3]);
    at_response_free(p_response);  
	p_response = NULL;
	//if(3 == commas)
	/* Begin added by c00221986 for DTS2013040706143 */
	err = at_send_command_singleline("AT^SYSINFOEX", "^SYSINFOEX:", &p_response);
	if ((0 == err) && (1 == p_response->success))
	{
		if(RIL_DEBUG) ALOGD("----***********request_gprs_registration_state********---");
		//^SYSINFOEX:2,3,0,1,,3,"WCDMA",41,"WCDMA"
		line = p_response->p_intermediates->line;
		if (RIL_DEBUG) LOGD("sysinfoex:%s", line);
		err = at_tok_start(&line);
		if (0 > err)
		{
			goto end;
		}
		err = at_tok_nextint(&line, &srv_status);
		if (0 > err)
		{
			goto end;
		}
		err = at_tok_nextint(&line, &srv_domain);
		if (0 > err)
		{
			goto end;
		}
		err = at_tok_nextint(&line, &raom_status);
		if (0 > err)
		{
			goto end;
		}
		err = at_tok_nextint(&line, &sim_state);
		if (0 > err)
		{
			goto end;
		}
		/* reserved. always NULL */
		err = at_tok_nextint(&line, &lock_state);
		err = at_tok_nextint(&line, &sys_mode);
		if (0 > err)
		{
			goto end;
		}
		err = at_tok_nextstr(&line, &sysmode_name);
		if (0 > err)
		{
			goto end;
		}
		err = at_tok_nextint(&line, &sys_submode);
		if (0 > err)
		{
			goto end;
		}
		switch (sys_submode)
		{
	        case 0: 	/*0 - Unknown*/
	            response[3] = 0;
	            break;
	        case 2: 	/*2 - GPRS*/
	            response[3] = 1;
	            break;
	        case 3: 	/*3 - EDGE*/
	            response[3] = 2;
	            break;
			case 41:	/*41 - WCDMA*/
				response[3] = 3;
				break;
			case 42:	/*42 - HSDPA*/
				response[3] = 9;
				break;
			case 43:	/*43 - HSUPA*/
				response[3] = 10;
				break;
			case 44:	/*44 - HSPA*/
				response[3] = 11;
				break;
			case 45:	/*45 - HSPA+*/
#ifdef ANDROID_3
	            response[3] = 15;/*For Android 3.x and 4.x, there is HSPA+ mode*/
#else          
                response[3] = 11;/*However,for Android 2.x , there is no HSPA+ mode,so we use HSPA*/
#endif  	
				break;
             case 46:	/*46 -DC- HSPA+*/
#ifdef ANDROID_3
	            response[3] = 15;/*For Android 3.x and 4.x, there is HSPA+ mode*/
#else          
                response[3] = 11;/*However,for Android 2.x , there is no HSPA+ mode,so we use HSPA*/
#endif  	
				break;
			case 101:	/*101 - LTE*/
#ifdef ANDROID_3				
				response[3] = 14;
#else
				response[3] = 0; /* Android 2.x , there is no LTE sysmode */
#endif
				break;
			default:
				response[3] = 0;
				break;				
		}		
	}
	/* End   added by c00221986 for DTS2013040706143 */
	else
	{
    at_response_free(p_response);  
		p_response = NULL;
	    err = at_send_command_singleline("AT^SYSINFO", "^SYSINFO:", &p_response);
		if (err < 0 || p_response->success == 0) 
	  {//start by alic
    			at_response_free(p_response);  
			p_response = NULL;
			err = at_send_command_singleline("AT+ZPAS?", "+ZPAS:", &p_response);
			if (err < 0 || p_response->success == 0) 
			{
				if (MODEM_VENDOR_SCV == g_modem_vendor) 
				{
					err = at_send_command_singleline("AT+MDRAT", "+MDRAT:", &p_response);
					if (err < 0 || p_response->success == 0) 
					{
						goto end;
					}
					line = p_response->p_intermediates->line;
					if(RIL_DEBUG)ALOGD("MDRAT:%s",line);
					err = at_tok_start (&line);    
					if (err < 0) 
					{
						goto end;
					}
					cpinResult = line + 1;
				}
					else if (MODEM_VENDOR_ALCATEL == g_modem_vendor){
						err = at_send_command_singleline("AT+SSND?", "+SSND", &p_response);//AT+CGMM: nema; AT+SSND?
						if (err < 0 || p_response->success == 0) 
						{
							goto end;
						}
						line = p_response->p_intermediates->line;
						if(RIL_DEBUG)ALOGD("SSND:%s",line);
						err = at_tok_start (&line);    
						if (err < 0) 
						{
							goto end;
						}
						cpinResult = line + 1;
					}
				else if (g_IsDialSucess == 1)
				{
				    /* Some dongle are not understand 'sysinfo' and 'zpas', so we do this when dial sucess */
				    /* ZTE mf186*/
				    cpinResult = "\"UMTS\"";
				}
				else
				{
					goto end;
				}
			}
			else
			{
				line = p_response->p_intermediates->line;
				if(RIL_DEBUG)ALOGD("ZPAS:%s",line);
				err = at_tok_start (&line);    
				if (err < 0) 
				{
					goto end;
				}
				err = at_tok_nextstr(&line, &cpinResult);
				if (err < 0) 
				{
					goto end;
				}
			}
			
			if(0 == strcmp(cpinResult, "HSPA"))
			{
				response[3] = 9;
			}
			else if(0 == strcmp(cpinResult, "HSDPA"))
			{
				response[3] = 9;
			}
			else if(0 == strcmp(cpinResult, "EDGE"))
			{
				response[3] = 2;
			}
			else if(0 == strcmp(cpinResult, "UMTS"))
			{
				response[3] = 3;
			}
			else if(0 == strcmp(cpinResult, "GSM"))
			{
				response[3] = 1;
			}
			else if(0 == strcmp(cpinResult, "No Service"))
			{
				response[3] = 0;
			}
			else if(0 == strcmp(cpinResult, "Limited Service"))
			{
				response[3] = 0;
			}
			else
			{
				response[3] = 11;
			}
				//goto end;
	    }
	    //end by alic
		else
		{
	    line = p_response->p_intermediates->line;
		if(RIL_DEBUG)ALOGD("sysinfo:%s",line);
	    err = at_tok_start (&line);    
	    if (err < 0) 
	    {
	        goto end;
	    }
	    
		err = at_tok_nextint(&line, &srv_status);
		if (err < 0) 
		{
			goto end;
		}
		err = at_tok_nextint(&line, &srv_domain);
		if (err < 0) 
		{
			goto end;
		}
		err = at_tok_nextint(&line, &raom_status);
		if (err < 0) 
		{
			goto end;
		}
		err = at_tok_nextint(&line, &sys_mode);
		if (err < 0)
		{
			goto end;
		}
		err = at_tok_nextint(&line, &sim_state);
		if (err < 0) 
		{
			goto end;
		}    
		/* reserved. always NULL */
		err = at_tok_nextint(&line, &lock_state);

		err = at_tok_nextint(&line, &sys_submode);
		if (err < 0) 
		{
			goto end;
		}
	     
	    switch(sys_submode)
	    {
	        case 0: /*0 - Unknown*/
	            response[3] = 0;
	            break;
/* Begin added by x84000798 for android4.1 Jelly Bean DTS2012101601022 2012-10-16 */		
						case 1:	/*1 - GSM*/
#ifdef ANDROID_5
							response[3] = 16;
#else
							response[3] = 0;
#endif
							break;
/* End added by x84000798 for android4.1 Jelly Bean  DTS2012101601022 2012-10-16 */
	        case 2: /*GPRS*/
	            response[3] = 1;
	            break;
	        case 3: /*EDGE*/
	            response[3] = 2;
	            break;
	        case 4:/*WCDMA*/
	        case 15:
	            response[3] = 3;
	            break;
	        case 5:/*HSDPA*/	
				response[3] = 9;
	            break;
	        case 6:/*HSUPA*/
	            response[3] = 10;
	            break;
		            /* Begin to modify by hexiaokong h81003427 for DTS2012050301233  2012-05-03*/
						case 7:/*HSDPA + HSUPA,such mode should be HSPA*/
		            //response[3] = 9;
		            response[3] = 11;
				break;
	        case 9:/*HSPA+*/
	#ifdef ANDROID_3
		            response[3] = 15;/*For Android 3.x and 4.x, there is HSPA+ mode*/
	#else          
	              response[3] = 11;/*However,for Android 2.x , there is no HSPA+ mode,so we use HSPA*/
	#endif                   
	            break;            
		            /* End to modify by hexiaokong h81003427 for DTS2012050301233  2012-05-03*/
	        default: /*No error is considerred*/
	            response[3] = 0;
	            break;
	    }
		}
	}
end:
	/* Begin modified by x84000798 201-06-04 DTS2012060805497 */
	/*BEGIN Added by z128629 workaround EM820X,CGREG state incorrect V3D01 2011-03-21*/
	if((0 == response[0]) && (2 == srv_status) && ((2 == srv_domain) || (3 == srv_domain)))
	{
		response[0] = 1;
	}
	/*END Added by z128629 workaround EM820X,CGREG state incorrect V3D01 2011-03-21*/
	/* End modified by x84000798 201-06-04 DTS2012060805497 */

	/*BEGIN Modify by lkf34826 DTS2011040803135: V3D01 上报的LAC、CID字符串格式不正确 2011-04-08*/
    asprintf(&responseStr[0], "%d", response[0]);
    asprintf(&responseStr[1], "%x", response[1]);
    asprintf(&responseStr[2], "%x", response[2]);
    asprintf(&responseStr[3], "%d", response[3]);
    /*END Modify by lkf34826 DTS2011040803135: V3D01 上报的LAC、CID字符串格式不正确 2011-04-08*/
    if(RIL_DEBUG)
    {
        ALOGD("*********registration state : %d LAC : %d  CID : %d  available radio : %d ",
                           response[0],response[1],response[2],response[3]);
    }
    RIL_onRequestComplete(t, RIL_E_SUCCESS, responseStr, 4*sizeof(char*));
    at_response_free(p_response);    
	free(responseStr[0]);
	free(responseStr[1]);
	free(responseStr[2]);
	free(responseStr[3]);
    return;
    
error:
    if(RIL_DEBUG) ALOGE( "request_gprs_registration_state must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
	return;
}


/************************************************
 Function:  request_registration_state
 Description:  
    reference ril.h
 Calls: 
    at_send_command_multiline
    at_tok_start
 Called By:
    hwril_request_mm
 Input:
    data    - pointer to void
    datalen - data len
    t       - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
static void request_operator(void *data, size_t datalen, RIL_Token t)
{
    int err;
    int i;
    int skip;
    ATLine *p_cur;
    char *response[3];

    memset(response, 0, sizeof(response));

    ATResponse *p_response = NULL;
    
    err = at_send_command_multiline(  
        "AT+COPS=3,0;+COPS?;+COPS=3,1;+COPS?;+COPS=3,2;+COPS?",
        "+COPS:", &p_response);

    /* we expect 3 lines here:
     * +COPS: 0,0,"T - Mobile"
     * +COPS: 0,1,"TMO"
     * +COPS: 0,2,"310170"
     */
    if (err < 0 || p_response->success == 0) goto error;           


    for (i = 0, p_cur = p_response->p_intermediates
            ; p_cur != NULL
            ; p_cur = p_cur->p_next, i++
    ) {
        char *line = p_cur->line;

        err = at_tok_start(&line);
        if (err < 0) {
            if(RIL_DEBUG) ALOGD( "at_tok_start  line  = %p ",line);
            goto error;
        }

        err = at_tok_nextint(&line, &skip);
        if (err < 0) {
            if(RIL_DEBUG) ALOGD( "at_tok_nextint  line  = %p ",line);
            goto error;
        }

        // If we're unregistered, we may just get
        // a "+COPS: 0" response
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;  
        //    goto unregistered;
            continue;
        }

        err = at_tok_nextint(&line, &skip);
        if (err < 0) goto error;

        // a "+COPS: 0, n" response is also possible
        if (!at_tok_hasmore(&line)) {
            response[i] = NULL;
            continue;
        }

        err = at_tok_nextstr(&line, &(response[i]));
        if (err < 0) goto error;
    }

    if (i != 3) {
        /* expect 3 lines exactly */
        goto error;
    }
  //如果采用UCS2编码则需要ril解析
	/* Begin Modified by x84000798 2012-06-29 DTS2012063002019 */
	//if response string is UCS2,ril should transform it to UTF-8
   if((response[0] != NULL) && (response[1] != NULL))
		{
	   	char long_name[128] = {0};
			char short_name[128] = {0};
			if (response[0] == strstr(response[0],"80")
				|| response[0] == strstr(response[0],"81")
				|| response[0] == strstr(response[0],"82"))
			{
				err = UCS2_to_UTF8(response[0], long_name);
				if (-1 == err)
				{
					goto error;
				}
				response[0] = long_name;
			}
			if (response[1] == strstr(response[1],"80")
				|| response[1] == strstr(response[1],"81")
				|| response[1] == strstr(response[1],"82"))
			{
				err = UCS2_to_UTF8(response[1], short_name);
				if (-1 == err)
				{
					goto error;
				}
				response[1] = short_name;
			}
   	}
	/* Begin Modified by x84000798 2012-06-29 DTS2012063002019 */
	  Operator_ons_lookup_memory_list(&response[0], &response[1], &response[2]);
	 //}
   //}
    if(RIL_DEBUG) ALOGD( "this is request_operator's success\n");
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    at_response_free(p_response);
    return;
    
error:
    if(RIL_DEBUG) ALOGE( "this is requestOperator's err \n");   
    response[0] = NULL;
    response[1] = NULL;
    response[2] = NULL;
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0); 
    at_response_free(p_response);
    return ;
}

static void request_Operator_CDMA(void *data, size_t datalen, RIL_Token t)
{
    char *response[3];

    memset(response, 0, sizeof(response));
    /*has no at command , this value need obtain by app*/
    response[0] = "CDMA";
    response[1] = "CDMA";
    response[2] = "46003";
 
    if(RIL_DEBUG)ALOGD("this is requestOperator's success\n");
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));
    
    return; 
}
static void request_query_network_selection_mode(void *data, size_t datalen, RIL_Token t)
{
    int err;
    ATResponse *p_response = NULL;
    int response = 0;
    char *line;

    if( NETWORK_CDMA == g_module_type ) {
        RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));
        return ;                      
    }
    
    err = at_send_command_singleline("AT+COPS?", "+COPS:", &p_response);
                                  
    if (err < 0 || p_response->success == 0) {
         goto error;
    }
  
    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);

    if (err < 0) {
        goto error;
    }

    err = at_tok_nextint(&line, &response);

    if (err < 0) {
        goto error;
    }
    if(RIL_DEBUG) ALOGD( "request_query_network_selection_mode response = %d\n",response);
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(int));  
    at_response_free(p_response);
    return; 
error:
    at_response_free(p_response);
    if(RIL_DEBUG) ALOGE( "request_query_network_selection_mode must never return error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    return ;                      
}

int wcdma_rssi_to_real(int rssi)
{
	int ret = 99;
	if (7 >= rssi)
		ret = 0;
	if (8 == rssi)
		ret = 1;
	if ((8 < rssi) && (66 >= rssi))
	{
		ret = floor(rssi/2 -3);
	}
	if (66 < rssi)
		ret = 99;
	LOGD("the real rssi is %d ", ret);
	return ret;
}
int lte_rsrp_to_real(int rsrp)
{
	int ret = -140;
	if (0 == rsrp)
		ret = -140;
	if (1 <= rsrp && 96 >= rsrp)
		ret = -141 + rsrp;
	if (97 == rsrp)
		ret = -44;
	if (255 == rsrp)
		ret = -44;
	return ret;
}
int lte_rsrq_to_real(int rsrq)
{
	int ret = -20;
	int i = 0;

	i = rsrq%2;
	if (255 == rsrq)
		ret = -20;
	else if (0 <= rsrq && 34 >= rsrq)
	{
		if (0 == i)
			ret = rsrq/2 -20;
		if (1 == i)
			ret = (rsrq+1)/2 - 20;
	}
	else
	{
		ret = -20;
	}
	return ret;
}
static void request_signal_strength(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err;
    int response[12]={99, 99, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}; //modified by wkf32792 for invalid signal length
    char *line, *sysmode;
	int rssi, rsrp, rsrq;

	/* Begin added by c00221986 for DTS2013040706143 */
	err = at_send_command_singleline("AT^HCSQ?", "^HCSQ:", &p_response);
	if (0 == err && 1 == p_response->success)
	{
		//^HCSQ: <sysmode>,<value1>,<value2>,<value3>,<value4>,<value5>
		line = p_response->p_intermediates->line;
		err = at_tok_start(&line);
		if (0 > err)
		{
			goto error;
		}
		err = at_tok_nextstr(&line, &sysmode);
		if (0 > err)
		{
			goto error;
		}
		if (0 == strcmp("NOSERVICE", sysmode))
		{
			LOGD("NOSERVICE");
		}
		if (0 == strcmp("GSM", sysmode))
		{	
			err = at_tok_nextint(&line, &rssi);
			response[0] = wcdma_rssi_to_real(rssi);

		}
		if (0 == strcmp("WCDMA", sysmode))
		{
			err = at_tok_nextint(&line, &rssi);
			response[0] = wcdma_rssi_to_real(rssi);	
		}
#ifdef ANDROID_3		
		if (0 == strcmp("LTE", sysmode))
		{
			err = at_tok_nextint(&line, &rssi);
			response[7] = wcdma_rssi_to_real(rssi);	
			err = at_tok_nextint(&line, &rsrp);
			response[8] = lte_rsrp_to_real(rsrp);
			err = at_tok_nextint(&line, &rsrq);
			response[9] = lte_rsrq_to_real(rsrq);
		}
#endif		
		if (0 == strcmp("CDMA", sysmode))
		{
		}
		if (0 == strcmp("EVDO", sysmode))
		{
		}
	}
	/* End added by c00221986 for DTS2013040706143 */
	else
	{
		at_response_free(p_response);
		p_response = NULL;
    err = at_send_command_singleline("AT+CSQ", "+CSQ:", &p_response);

    if (err < 0 || p_response->success == 0) {
        goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response[0]));
    if (err < 0) goto error;

    err = at_tok_nextint(&line, &(response[1]));
    if (err < 0) goto error;

    /*BEGIN Added TDSCDMA  signal strength */
    if(response[0] > 99 && response[0] < 191) {
    	response[0] = ( response[0]-100 )*31/91 ;
    }
    else if (response[0] == 199){
		response[0] = 99;
    }
    /*END Added TDSCDMA  signal strength */
	}
    RIL_onRequestComplete(t, RIL_E_SUCCESS, response, sizeof(response));

    at_response_free(p_response);
    return;

error:
    if(RIL_DEBUG) ALOGE( "request_signal_strength must never return an error when radio is on");
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
}


/******************************
 * RIL_REQUEST_SIGNAL_STRENGTH
 *
 * Requests current signal strength and associated information
 *
 * Must succeed if radio is on.
 *
 * "data" is NULL
 *
 * "response" is a const RIL_SignalStrength *
 *
 * Valid errors:
 *  SUCCESS
 *  RADIO_NOT_AVAILABLE
 ****************************/
static void request_cdma_signal_strength(void *data, size_t datalen, RIL_Token t)
{
	ATResponse *p_response_1x = NULL;
	ATResponse *p_response_evdo = NULL;
	int err_1x;
	int err_evdo;
//#ifdef ANDROID_3
//    RIL_SignalStrength_v5 response;
//#else
//	RIL_SignalStrength response;
//#endif
	char *line_1x;
	char *line_evdo;
	int rssi,ber;
	int hdr_rssi;
	int response[12]={99, 99, -1, 40, 30, 10, 8, -1, -1, -1, -1, -1}; 
	
	if(MODEM_VENDOR_HAUWEI == g_modem_vendor)
	{
		err_1x= at_send_command_singleline("AT^CSQLVL", "^CSQLVL:", &p_response_1x);
		err_evdo = at_send_command_singleline("AT^HDRCSQLVL ", "^HDRCSQLVL:", &p_response_evdo);

		if (err_1x< 0 || p_response_1x->success == 0) 
		{
			if(err_evdo < 0 || p_response_evdo->success == 0)
			{
				goto error;
			}
		}

		line_1x= p_response_1x->p_intermediates->line;
		line_evdo= p_response_evdo->p_intermediates->line;

		err_1x= at_tok_start(&line_1x);
		err_evdo= at_tok_start(&line_evdo);
		if (err_1x< 0) 
		{
			if (err_evdo< 0) 
			{
				goto error;
			}
		}
		//err_1x= at_tok_nextint(&line_1x, &rssi);
		//err_1x= at_tok_nextint(&line_1x, &ber);
//		err_1x= at_tok_nextint(&line_1x, &(response.GW_SignalStrength.signalStrength));
//		err_1x= at_tok_nextint(&line_1x, &(response.GW_SignalStrength.bitErrorRate));
//		err_1x= at_tok_nextint(&line_1x, &(response.CDMA_SignalStrength.dbm));
//		err_1x= at_tok_nextint(&line_1x, &(response.CDMA_SignalStrength.ecio));
//		
//		err_evdo= at_tok_nextint(&line_evdo, &hdr_rssi);
//		err_evdo= at_tok_nextint(&line_evdo, &(response.EVDO_SignalStrength.dbm));
//		err_evdo= at_tok_nextint(&line_evdo, &(response.EVDO_SignalStrength.ecio));
//		err_evdo= at_tok_nextint(&line_evdo, &(response.EVDO_SignalStrength.signalNoiseRatio));
//        
//        ALOGE("signalStrength=%d, bitErrorRate=%d, dbm=%d, ecio=%d, hdr_rssi=%d, dbm=%d, ecio=%d, signalNoiseRatio=%d",
//         response.GW_SignalStrength.signalStrength,response.GW_SignalStrength.bitErrorRate,response.CDMA_SignalStrength.dbm,
//         response.CDMA_SignalStrength.ecio,hdr_rssi,response.EVDO_SignalStrength.dbm,response.EVDO_SignalStrength.ecio,
//         response.EVDO_SignalStrength.signalNoiseRatio);
        
		//response.CDMA_SignalStrength.dbm *= -1;
		//response.EVDO_SignalStrength.dbm *= -1;
        err_1x= at_tok_nextint(&line_evdo, &(response[2]));
        if (err_1x < 0) goto error;
        response[3] = response[2];
        response[4] = response[2];
        response[5] = response[2];
        if (response[2] == 80)
		{
			response[6] = 8;
		}
		else if (response[2] == 60)
		{
			response[6] = 6;
		}
		else if (response[2] == 40)
		{
			response[6] = 4;
		}
		else if (response[2] == 20)
		{
			response[6] = 2;
		}
		else
		{
			response[6] = 0;
		}
		RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));

		at_response_free(p_response_1x);
		at_response_free(p_response_evdo);
		return;

		error:
		ALOGE("request_cdma_signal_strength must never return an error when radio is on");
		RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
		at_response_free(p_response_1x);
		at_response_free(p_response_evdo);
	}
	else
	{
		 err_1x = at_send_command_singleline("AT+CSQ", "+CSQ:", &p_response_1x);

		 if (err_1x < 0 || p_response_1x->success == 0) {
		     goto error1;
		 }

		 line_1x = p_response_1x->p_intermediates->line;

		 err_1x = at_tok_start(&line_1x);
		 if (err_1x < 0) goto error1;
         err_1x= at_tok_nextint(&line_1x, &(response[2]));
		 //err_1x = at_tok_nextint(&line_1x, &(response.GW_SignalStrength.signalStrength));
		 if (err_1x < 0) goto error1;

		 //err_1x = at_tok_nextint(&line_1x, &(response.GW_SignalStrength.bitErrorRate));
		 //if (err_1x < 0) goto error1;

		 /*BEGIN Added TDSCDMA  signal strength */
		 //if(response[0] > 99 && response[0] < 191) {
		 	//response[0] = ( response[0]-100 )*31/91 ;
		 //}
		 //else if (response[0] == 199){
			//response[0] = 99;
		 //}
		 /*END Added TDSCDMA  signal strength */
		if((response[2] > 0)&&(response[2] <= 8))
		{
			response[2] = 20;
			response[6] = 2;
		}
		else if((response[2] > 8)&&(response[2] <= 16))
		{
			response[2] = 40;
			response[6] = 4;
		}
		else if((response[2] > 16)&&(response[2] <= 24))
		{
			response[2] = 60;
			response[6] = 6;
		}
		else if((response[2] > 24)&&(response[2] <= 31))
		{
			response[2] = 80;
			response[6] = 8;
		}
		else
		{
			response[2] = 0;
			response[6] = 0;
		}
		response[3] = response[2];
        response[4] = response[2];
        response[5] = response[2];
		 RIL_onRequestComplete(t, RIL_E_SUCCESS, &response, sizeof(response));

		 at_response_free(p_response_1x);
		 return;

		error1:
		 if(RIL_DEBUG) ALOGE( "request_cdma_signal_strength must never return an error when radio is on");
		 RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	}
	return;
}







/* Begin added by x84000798 2012-06-01 DTS2012060806329 */
void search_automatic(void *param)
{
	RIL_Token t;
	ATResponse *p_response = NULL;
	int err;
	struct operatorPollParams *poll_params;

	poll_params = (struct operatorPollParams *) param;
	t = poll_params->t;
	if (poll_params->loopcount >= SEARCH_NETWORK_TIME)
	{
		goto error;
	}
	err = at_send_command_singleline("AT+COPS?", "+COPS:", &p_response); 
	if (err < 0 || p_response->success == 0)
	{
		goto error;
	}
	char *line = p_response->p_intermediates->line;
	err = at_tok_start(&line);
	if (err < 0)
	{
		goto error;
	}
	int commas = 0;
	for (; *line != '\0' ;line++)
	{
		if (',' == *line)
		{
			commas++;
		}
	}
	if (commas > 0)
	{
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
		at_response_free(p_response);
		free(poll_params);
		return;
	}
	poll_params->loopcount++;
	RIL_requestTimedCallback (search_automatic, poll_params, &TIMEVAL_AUTOPOLL);
	at_response_free(p_response);
	return;

error:
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
	free(poll_params);
}
/* End added by x84000798 2012-06-01 DTS2012060806329 */

static void request_set_network_selection_automatic(RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err, ret = SIM_ABSENT;
	/* Begin added by x84000798 2012-06-01 DTS2012060806329 */
	struct operatorPollParams *poll_params = NULL;
	poll_params = (struct operatorPollParams *)malloc(sizeof(struct operatorPollParams));
	if (NULL == poll_params)
	{
		goto error;
	}
	/* End added by x84000798 2012-06-01 DTS2012060806329 */
	//if( NETWORK_CDMA == g_module_type ){
	if( (NETWORK_CDMA == g_module_type) || (MODEM_VENDOR_ALCATEL == g_modem_vendor )){// for alcatel
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
		return;
	}
    err = at_send_command("AT+COPS=0", &p_response); 

    if ( err < 0 ) {
        goto error;
    }
    
    switch (at_get_cme_error(p_response)) {
    	
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
	    case CME_SIM_NOT_VALID:
            if(RIL_DEBUG) ALOGD( "cops=0error 1: %d", err);
            ret = SIM_ABSENT;
            goto error;

        default:
            if(RIL_DEBUG) ALOGD( "cops=0 error 2: %d", err);
            ret = SIM_NOT_READY;
            goto error;
    }
    if(RIL_DEBUG) ALOGD( "cops=0 suc=0");
    /* BEGIN DTS2011123101074 lkf52201 2011-12-31 added */
    at_response_free(p_response);
    p_response = NULL;
    /* END   DTS2011123101074 lkf52201 2011-12-31 added */

	/* Begin added by x84000798 2012-06-01 DTS2012060806329 */
	poll_params->loopcount = 0;
	poll_params->t = t;
	RIL_requestTimedCallback (search_automatic, poll_params, &TIMEVAL_AUTOPOLL);
	return;
	
	#if 0
    //BEGIN DTS2011091606706 wkf32792 20110920 modified
    int i;
    for (i = 0; i < SEARCH_NETWORK_TIME; i++)
    {
    	err = at_send_command_singleline("AT+COPS?", "+COPS:", &p_response);                                  
	    if (err < 0 || p_response->success == 0) {
    	     goto error;
    	}
    	char *line = p_response->p_intermediates->line;
	    err = at_tok_start(&line);
    	if (err < 0) {
        	goto error;
	    }	
    	int commas = 0;
	    for (; *line != '\0' ;line++){	
    		if (',' == *line)
    		{
    	    	commas++;
    		}
    	}
    	//ALOGD("commas is %d", commas);
      
    	if (commas > 0) {
        	RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        	break;
    	}
    	else {
    		sleep(1);
    	}
    	/* BEGIN DTS2011123101074 lkf52201 2011-12-31 added */
    	at_response_free(p_response);
    	p_response = NULL;
    	/* END   DTS2011123101074 lkf52201 2011-12-31 added */
    }
    if (SEARCH_NETWORK_TIME == i) {
    	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    }
    //END DTS2011091606706 wkf32792 20110920 modified
      

    at_response_free(p_response);
    return;
	#endif
	/* End added by x84000798 2012-06-01 DTS2012060806329 */
	
error:
    if(RIL_DEBUG) ALOGE( "%s: Format error in this AT response %d", __FUNCTION__,ret);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
	free(poll_params); // Added by x84000798 2012-06-01 DTS2012060806329
}

static void request_set_network_selection_manual(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err, ret=SIM_ABSENT;
    const char *oper;
    oper = (const char *)data;  
    char* cmd = NULL;
	
	if( NETWORK_CDMA == g_module_type ){
		RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
		return;
	}

    /* BEGIN lkf52201 2011-12-24 deleted */
    /* If manual selected network has been registered, not necessary to send AT cmd again */
    /*
    if(strcmp(oper, "need add") == 0) {
        if(RIL_DEBUG) ALOGD( "%s: Current operator is already %s", __FUNCTION__, oper);
        RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
        return;
    }
    */
    /* END lkf52201 2011-12-24 deleted */
    
    asprintf(&cmd, "AT+COPS=1,2,\"%s\"",oper);  
    err = at_send_command(cmd, &p_response); 
   
    if (err < 0 ) {
        goto error;
    }

    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
	 	case CME_SIM_NOT_VALID:
            if(RIL_DEBUG) ALOGD( "cops=1,2 error 1: %d", err);
            ret = SIM_ABSENT;
            goto error;

        default:
            if(RIL_DEBUG) ALOGD( "cops=1,2 error 2: %d", err);
            ret = SIM_NOT_READY;
            goto error;
    }

    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
    at_response_free(p_response);
    free(cmd);
    return;
error:
    if(RIL_DEBUG) ALOGE( "%s: Format error in this AT response %d", __FUNCTION__,ret);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(cmd);
}

/* Begin to modify by hexiaokong kf39947 for DTS2012020605786 2012-02-18*/
static void request_query_available_networks(RIL_Token  t)
{
    ATResponse *p_response = NULL;
    int err, ret;
    char *line;
    int linelen = 0;
   int j = 0;//loop param for counting opernum
   char c;
   char *pstr = NULL;
   int opernum = 0;
   char** result=NULL;

    err = at_send_command_singleline("AT+COPS=?", "+COPS:", &p_response);
    
    if (err < 0 ) {
        goto error;
    }

    /*need add more CME error type need modify*/
    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
	    case CME_SIM_NOT_VALID:
            if(RIL_DEBUG) ALOGD( "cops=? error 1: %d", err);
            ret = SIM_ABSENT;
            goto error;

        default:
            if(RIL_DEBUG) ALOGD( "cops=? error 2: %d", err);
            ret = SIM_NOT_READY;
            goto error;
    }

    line = p_response->p_intermediates->line;

    err = at_tok_start(&line);
    if (err < 0) goto error;

   linelen = strlen(line);
   if (NULL != strstr(line,",,"))
   {
       linelen = strstr(line,",,") - line;
   }

  pstr = (char*)malloc(linelen * sizeof(char));
  if(NULL == pstr)
  {
     goto error;
  }

  strncpy(pstr,line,linelen); 

   for(j = 0; j < linelen; j++)
   {
       c = pstr[j];
       if('"' == c)
       {
	    opernum++;
       }
   }
   opernum /= 6;
    if(RIL_DEBUG) ALOGD( "%s: available operator number is ********:%d",__FUNCTION__, opernum);
    result = (char**)alloca(opernum*4*sizeof(char*));
    if(NULL == result){
		goto error;
    }
	
    int i = 0;
    while(i < opernum) 
    {
        /*need consider (), such as (3,"CHINA 09","09","46009",1)*/
	/* End to modify by hexiaokong kf39947 for DTS2012020605786 2012-02-18*/
        char* status,*skip;
        at_tok_nextstr(&line,&status);
        if(RIL_DEBUG) ALOGD( "status:%s",status);
        switch(status[1]) {
        case '1': 
            result[i*4+3] = "available";
            break;
        case '2': 
            result[i*4+3] = "current";
            break;
        case '3': 
            result[i*4+3] = "forbidden";
            break;
        default:  
            result[i*4+3] = "unknown";
        }
        if(RIL_DEBUG) ALOGD( "state:%s",result[i*4+3]);
        at_tok_nextstr(&line,&result[i*4]);
        if(RIL_DEBUG) ALOGD( "longname:%s",result[i*4]);
        at_tok_nextstr(&line,&result[i*4+1]);
        if(RIL_DEBUG) ALOGD( "shortname:%s",result[i*4+1]);
        at_tok_nextstr(&line,&result[i*4+2]);
        if(RIL_DEBUG) ALOGD( "numbername:%s",result[i*4+2]);
        at_tok_nextstr(&line,&skip); /*skip rat*/
        i++;
    }

	RIL_onRequestComplete(t, RIL_E_SUCCESS, result, sizeof(char*)*opernum*4);
	at_response_free(p_response);
        free(pstr);
	return; 
error:
	if(RIL_DEBUG) ALOGE( "%s: Format error in this AT response", __FUNCTION__);
	RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
	at_response_free(p_response);
}

static void request_get_prefered_network_type(RIL_Token  t)
{
    ATResponse *p_response = NULL;
    int err;
    int ret;
    int systemMode;
	char *line, *ch_acqorder;

    int preferedNetworkType;
    char *preferedNetworkTypeLine;

    if (sState == RADIO_STATE_OFF || sState == RADIO_STATE_UNAVAILABLE) {
        ret = SIM_NOT_READY;
        goto error;
    }
	/* Begin added by c00221986 for DTS2013040706143 */
	err = at_send_command_singleline("AT^SYSCFGEX?", "^SYSCFGEX:", &p_response);
	if ((0 == err) && (1 == p_response->success))
	{
		line = p_response->p_intermediates->line;
		err = at_tok_start(&line);
		if (0 > err)
		{
			goto error;
		}
		err = at_tok_nextstr(&line, &ch_acqorder);
		if (0 > err)
		{
			goto error;
		}
		if (0 == strcmp(ch_acqorder, "0201"))
		{
			preferedNetworkType = 0;
		}
		else if (0 == strcmp(ch_acqorder, "01"))
		{
			preferedNetworkType = 1;
		}
		else if (0 == strcmp(ch_acqorder, "02"))
		{
			preferedNetworkType = 2;
		}
		else if (0 == strcmp(ch_acqorder, "04"))
		{
			preferedNetworkType = 5;
		}
		else if (0 == strcmp(ch_acqorder, "07"))
		{
			preferedNetworkType = 6;
		}
		else if (0 == strcmp(ch_acqorder, "030407"))
		{
			preferedNetworkType = 8;
		}
		else if (0 == strcmp(ch_acqorder, "030201"))
		{
			preferedNetworkType = 9;
		}	
		else if (0 == strcmp(ch_acqorder, "0304070201"))
		{
			preferedNetworkType = 10;
		}
		else if (0 == strcmp(ch_acqorder, "03"))
		{
			preferedNetworkType = 11;
		}
		else if (0 == strcmp(ch_acqorder, "00"))
		{
			preferedNetworkType = 7;
		}
		else
		{
			preferedNetworkType = 7;
		}
	    RIL_onRequestComplete(t, RIL_E_SUCCESS, &preferedNetworkType, sizeof(preferedNetworkType));
	    at_response_free(p_response);
	    return;
	}
	/* End added by c00221986 for DTS2013040706143 */
	else
	{
		at_response_free(p_response);
		p_response = NULL;
//Begin modify by fKF34305 2011 08 26 for add CDMA prefmode 
    if(NETWORK_CDMA == g_module_type) 
    {
		err = at_send_command_singleline("AT^PREFMODE?", "^PREFMODE:", &p_response);
    }
    else
   {
	  err = at_send_command_singleline("AT^SYSCFG?", "^SYSCFG:", &p_response);
   }
	
	if (err < 0 ) {
        goto error;
    }
     /*need add more CME error type*/
    switch (at_get_cme_error(p_response)) {
        case CME_SUCCESS:
            break;

        case CME_SIM_NOT_INSERTED:
    	case CME_SIM_NOT_VALID:
            if(RIL_DEBUG) ALOGD( "syscfg error 2: %d", err);
            ret = SIM_ABSENT;
            goto error;

        default:
            if(RIL_DEBUG) ALOGD( "syscfg error 3: %d", err);
            ret = SIM_NOT_READY;
            goto error;
    }

    preferedNetworkTypeLine = p_response->p_intermediates->line;
    err = at_tok_start (&preferedNetworkTypeLine);

    if (err < 0) {
        if(RIL_DEBUG) ALOGD( "syscfg error 4: %d", err);
        ret = SIM_NOT_READY;
        goto error;
    }

    err = at_tok_nextint(&preferedNetworkTypeLine, &systemMode);

    if (err < 0) {
        if(RIL_DEBUG) ALOGD( "syscfg error 5: %d", err);
        ret = SIM_NOT_READY;
        goto error;
    }

    if(RIL_DEBUG) ALOGD( "syscfg systemMode1: %d", systemMode);
	
    if(NETWORK_CDMA == g_module_type) 
    {
	switch(systemMode) {
        case 2: /* CDMA only */
            preferedNetworkType= 5;
            break;
        case 4: /*HARD only*/
            preferedNetworkType = 6;
            break;
        case 8: /*CDMA and HARD*/
            preferedNetworkType = 4;
            break;
        default: /*No error is considerred*/
            preferedNetworkType = 7;
            break;
        }
    }
	else
    {
	switch(systemMode) {
        case 2: /* Auto */
            preferedNetworkType= 0;
            break;
        case 13: /*GSM only*/
            preferedNetworkType = 1;
            break;
        case 14: /*WCDMA only*/
        case 15: /*TDSCDMA only*/
            preferedNetworkType = 2;
            break;
        default: /*No error is considerred*/
            preferedNetworkType = 0;
            break;
        }
    }
    //Begin modify by fKF34305 2011 08 26 for add CDMA prefmode 

    if(RIL_DEBUG) ALOGD( "syscfg suc 1: %d", preferedNetworkType);
  
    RIL_onRequestComplete(t, RIL_E_SUCCESS, &preferedNetworkType, sizeof(preferedNetworkType));
   
    at_response_free(p_response);
    return;
	}
error:
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);    
    return;
}

static void request_set_preferred_network_type(void *data, size_t datalen, RIL_Token t)
{
    ATResponse *p_response = NULL;
    int err, ret=SIM_ABSENT;
    const int *oper;
    oper = (const int *)data; 
    int preferred_network_type;
	int acqorde = 2;
    char*  cmd = NULL;

	/* Begin added by c00221986 for DTS2013040706143 */
	char *ch_acqorder = NULL;
	switch (*oper)
	{
		case 0:	/* GSM/WCDMA (WCDMA preferred) */
			ch_acqorder = "0201";
			break;
		case 1:	/* GSM only */
			ch_acqorder = "01";
			break;
		case 2:	/* WCDMA  */
			ch_acqorder = "02";
			break;			
		case 3: /* GSM/WCDMA (auto mode, according to PRL) */
			//LOGD("Please notify: GSM auto prl treat as LTE auto for test");
			LOGD("Please notify: GSM auto prl not to be treat as LTE auto for test");
			//ch_acqorder = "0301";
			ch_acqorder = "00";
			break;
		case 5:	/* CDMA only */
			ch_acqorder = "04";
			break;
		case 6:	/* EvDo only */
			ch_acqorder = "07";
			break;
		case 8:	/* LTE, CDMA and EvDo */
			ch_acqorder = "030407";
			break;
		case 9:	/* LTE, GSM/WCDMA */
			ch_acqorder = "030201";
			break;	
		case 10:/* LTE, CDMA, EvDo, GSM/WCDMA */
			ch_acqorder = "0304070201";
			break;
		case 11:/* LTE only */
			ch_acqorder = "03";
			break;
		default:
			ch_acqorder = "00";
			break;
	}
	asprintf(&cmd, "AT^SYSCFGEX=\"%s\",40000000,2,4,7FFFFFFFFFFFFFFF,,", ch_acqorder);
    err = at_send_command(cmd, &p_response); 
	free(cmd);
	if ((0 == err) && (1 == p_response->success))
	{
	    RIL_onRequestComplete(t, RIL_E_SUCCESS, NULL, 0);
	    at_response_free(p_response);
	    return;
	}
	else
	{
    if(RIL_DEBUG) ALOGD( "syscfg= : %d", *oper);
	if(NETWORK_CDMA == g_module_type) {
		switch(*oper) {
	        case 4:
	            preferred_network_type = 8;
	            break;
	        case 5:
	            preferred_network_type = 2;
	            break;
	        case 6:
	            preferred_network_type = 4;
	            break;
	        default:
	            preferred_network_type = 8;
	            break;
	    }
    	asprintf(&cmd, "AT^PREFMODE=%d",preferred_network_type);
	}
	else if(NETWORK_TDSCDMA == g_module_type) {
		switch(*oper) {
			case 0:
			    preferred_network_type = 2;
			    break;
			case 1:
			    preferred_network_type = 13;
				acqorde = 1;
			    break;
			case 2:
			    preferred_network_type = 15;
			    break;
			default:
			    preferred_network_type =2;
			    break;
			}
			asprintf(&cmd, "AT^SYSCONFIG=%d,%d,2,4",preferred_network_type,acqorde);
	}
	else {
	    switch(*oper) {
	        case 0:
	            preferred_network_type = 2;
	            break;
	        case 1:
	            preferred_network_type = 13;
				acqorde = 1;
	            break;
	        case 2:
	            preferred_network_type = 14;
	            break;
	        default:
	            preferred_network_type =2;
	            break;
	    }
	    /*acqorder:w prefer 
	    *  band : no change
	    */
	    asprintf(&cmd, "AT^SYSCFG=%d,%d,40000000,2,4",preferred_network_type,acqorde);//Modify by fKF34305 20110829 for DTS2011081806216
	}
    err = at_send_command(cmd, &p_response); 
    if (err < 0 ) {
        goto error;
    }

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
    free(cmd);
    return;
    }    
   	/* End   added by c00221986 for DTS2013040706143 */
error:
    if(RIL_DEBUG) ALOGE( "%s: Format error in this AT response %d", __FUNCTION__,ret);
    RIL_onRequestComplete(t, RIL_E_GENERIC_FAILURE, NULL, 0);
    at_response_free(p_response);
    free(cmd);
}

/***************************************************
 Function:  request_set_location_update
 Description:  
    Handle the request about mm. 
 Calls: 
          null
 Called By:
    on_request
 Input:
   data: is int *
   ((int *)data)[0] is == 1 for updates enabled (+CREG=2)
    ((int *)data)[0] is == 0 for updates disabled (+CREG=1)
    datalen - data len
    token   - pointer to void struct
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/
void request_set_location_update(void *data, size_t datalen, RIL_Token t)
{  
    int err;
    if(RIL_DEBUG) 
    {
    if(RIL_DEBUG)  ALOGD( "request_set_location_update\n");
    }
    if((*(int *)data)==0)
    {
        err = at_send_command("AT+CREG=1", NULL);
        if(err < 0)  
	 	{
	    	goto error;
        }

		 err = at_send_command("AT+CGREG=1", NULL); //add by hkf39947 20110802 DTS2011080205073, we also need to close cgreg LAC/CID reports
		 if(err < 0)  
		 {
		    goto error;
	     }
		 
		 RIL_onRequestComplete( t, RIL_E_SUCCESS,  NULL, 0);
    }
    else if ((*(int *)data)==1)
    {
        err = at_send_command("AT+CREG=2", NULL);
	 if(err < 0)
	 {
	     goto error;
	 }
	 
        err = at_send_command("AT+CGREG=2", NULL);//add by hkf39947 20110802 DTS2011080205073, we also need to open cgreg LAC/CID reports
	 if(err < 0)
	 {
	     goto error;
	 }	 
        RIL_onRequestComplete( t, RIL_E_SUCCESS,  NULL, 0);
    }
    else
    {
        RIL_onRequestComplete( t, RIL_E_GENERIC_FAILURE,  NULL, 0);  
    }
    return ;
 error:
     RIL_onRequestComplete( t, RIL_E_GENERIC_FAILURE,  NULL, 0);
     return ;
}

/***************************************************
 Function:  hwril_request_mm
 Description:  
    Handle the request about mm. 
 Calls: 
    request_screen_state
    request_registration_state
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
void hwril_request_mm (int request, void *data, size_t datalen, RIL_Token token)
{    
	switch (request) 
	{
	    /* Local implementation of SCREEN_STATE by local varialble, which will decide whether report unsol msg of CSQ and CREG/CGREG */
	    case RIL_REQUEST_SCREEN_STATE:
	    {
	         request_screen_state(data,datalen,token);
	         break;
	    }

//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3	   
        case RIL_REQUEST_VOICE_REGISTRATION_STATE: 
#else	
        case RIL_REQUEST_REGISTRATION_STATE:
#endif 
//end modified by wkf32792 for android 3.x 20110815      
	    {
			if(NETWORK_CDMA == g_module_type) {
				request_cdma_registration_state(request, data, datalen, token);
			}else{
				request_registration_state(request, data, datalen, token);
			}
	        break;
	    }
    
#ifdef ANDROID_3	    
	    case RIL_REQUEST_DATA_REGISTRATION_STATE: 
#else
	    case RIL_REQUEST_GPRS_REGISTRATION_STATE:
#endif	    
	    {
	        if(NETWORK_CDMA == g_module_type) {
	            request_cdma_registration_state(request, data, datalen, token);
	        }
	        else{
	        	request_gprs_registration_state(request, data, datalen, token);
	        }
	        break;
	    }
	    
	    case RIL_REQUEST_OPERATOR:
	    {

	        if(NETWORK_CDMA == g_module_type) {
	            request_Operator_CDMA(data, datalen, token);
	        }
	        else{
	        	request_operator(data, datalen, token);
	        }
	        break;
	    }
	      
	    case RIL_REQUEST_QUERY_NETWORK_SELECTION_MODE:
	    {
	        request_query_network_selection_mode(data, datalen, token);
	        break;
	    }
	        
	    case RIL_REQUEST_SIGNAL_STRENGTH:
	    {
			if(NETWORK_CDMA == g_module_type)
			{
				request_cdma_signal_strength(data, datalen, token);
			}else{
				request_signal_strength(data, datalen, token);
			}
	        break;
	    }
	    
	    case RIL_REQUEST_SET_NETWORK_SELECTION_AUTOMATIC:
	    {
	        request_set_network_selection_automatic(token);
	        break;
	    }
	    
	    case RIL_REQUEST_SET_NETWORK_SELECTION_MANUAL:
	    {
	        request_set_network_selection_manual(data,datalen,token);
	        break;
	    }
	    
	    case RIL_REQUEST_QUERY_AVAILABLE_NETWORKS:
	    {
			request_query_available_networks(token);
			break;
	    }                 
	    case RIL_REQUEST_GET_PREFERRED_NETWORK_TYPE:
	    {
			request_get_prefered_network_type(token);
			break;
	    }
	    case RIL_REQUEST_SET_PREFERRED_NETWORK_TYPE:
	    {
	        request_set_preferred_network_type(data, datalen, token);
	        break;
	    }
	    case  RIL_REQUEST_SET_LOCATION_UPDATES:
	    {
	        request_set_location_update(data, datalen, token);
			break;
	    }
	    default:
	    {
	        RIL_onRequestComplete(token, RIL_E_REQUEST_NOT_SUPPORTED, NULL, 0);
	        break;
	    }
	}      
}

/*************************************************
 Function:  rssi_on_unsolv
 Description:  
    Rssi unsolicited
 Calls: 
    at_tok_start
    RIL_onUnsolicitedResponse
 Called By:
    hwril_unsolicited_mm
 Input:
    line   - pointer to hex string 
 Output:
    none
 Return:
    none
 Others:
    none 
**************************************************/
void  rssi_on_unsolv(char  *line)
{
	int err;
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3
    /*BEGIN DTS2011091606801 wkf32792 20110919 modified*/
    int response[12] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,};  
    /*END   DTS2011091606801 wkf32792 20110919 modified*/
#else
    int response[2];
#endif
//end modified by wkf32792 for android 3.x 20110815	

	at_tok_start(&line);
	err = at_tok_nextint(&line, &response[0]);            
	if (err < 0) goto error;  

	 /*BEGIN Added TDSCDMA  signal strength */
    if(response[0] > 99 && response[0] < 191) {
    	response[0] = ( response[0]-100 )*31/91 ;
    }
    else if (response[0] == 199){
		response[0] = 99;
    }
    /*END Added TDSCDMA  signal strength */
	response[1] = 99;
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3
    RIL_onUnsolicitedResponse (RIL_UNSOL_SIGNAL_STRENGTH,  response, sizeof(response));  
#else
    RIL_onUnsolicitedResponse (RIL_UNSOL_SIGNAL_STRENGTH,  response, sizeof(response));  
#endif
//end modified by wkf32792 for android 3.x 20110815
	return;
	
error:
	if(RIL_DEBUG) ALOGE( "%s: Error parameter in ind msg: %s", __FUNCTION__, line);     
	return;
}


/*************************************************
 Function:  crssi_on_unsolv
 Description:  
    Crssi unsolicited
 Calls: 
    at_tok_start
    RIL_onUnsolicitedResponse
 Called By:
    hwril_unsolicited_mm
 Input:
    line   - pointer to hex string 
 Output:
    none
 Return:
    none
 Others:
    none 
**************************************************/
void  crssi_on_unsolv(char  *line)
{
	int err;
#ifdef ANDROID_3
	RIL_SignalStrength_v5 response = {{99, 99},{99, 99},{99, 99, 99}};// Modified by x84000798 2012-06-11 DTS2012061209625
#else
	RIL_SignalStrength response = {{99, 99},{99, 99},{99, 99, 99}};// Modified by x84000798 2012-06-11 DTS2012061209625
#endif
	int ptemp = 0;
	int rssi_temp = 0;

	if(strStartsWith(line, "^CRSSI"))
		rssi_temp = 1;
	
	at_tok_start(&line);
	err = at_tok_nextint(&line, &ptemp);  
	if(0 == ptemp)
	{
		ptemp = 125;
	}
	else if(ptemp <= 30)
	{
		ptemp = 115-ptemp;
		ptemp *= 31;
		ptemp /= 50;
	}
	else if(31 == ptemp)
	{
		ptemp = 75;
	}
	//if(rssi_temp)
	//{
		response.CDMA_SignalStrength.dbm = ptemp; 
	//}
	//else
	//{
		response.EVDO_SignalStrength.dbm = ptemp;  
	//}
	if (err < 0) goto error;  


	RIL_onUnsolicitedResponse (RIL_UNSOL_SIGNAL_STRENGTH,  &response, sizeof(response));  
	return;
	
error:
	if(RIL_DEBUG) ALOGE( "%s: Error parameter crssi_on_unsolv: %s", __FUNCTION__, line);     
	return;
}

/*************************************************
 Function:  mode_on_unsolv
 Description:  
    ^MODE unsolicited
 Calls: 
    at_tok_start
    RIL_onUnsolicitedResponse
 Called By:
    hwril_unsolicited_mm
 Input:    	line   
 Output:    none
 Return:    none
 Others:    Added by x84000798 for android4.1 Jelly Bean DTS2012101601022 2012-10-16 
**************************************************/
void mode_on_unsolv(char *line)
{
	int err;
	int sys_mode;
	int sys_submode;
	int response[1];
	
	at_tok_start(&line);
	err = at_tok_nextint(&line, &sys_mode);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextint(&line, &sys_submode);
	if (0 > err)
	{
		goto error;
	}
	switch (sys_submode)
	{
        case 0: /*0 - Unknown*/
            response[0] = 0;
            break;
		case 1:	/*1 - GSM*/
#ifdef ANDROID_5
			response[0] = 16;
#else
			response[0] = 0;
#endif
			break;
		case 2: /*GPRS*/
            response[0] = 1;
            break;
        case 3: /*EDGE*/
            response[0] = 2;
            break;
        case 4:/*WCDMA*/
        case 15:
            response[0] = 3;
            break;
        case 5:/*HSDPA*/	
			response[0] = 9;
            break;
        case 6:/*HSUPA*/
            response[0] = 10;
            break;
		case 7:/*HSDPA + HSUPA,such mode should be HSPA*/
            //response[3] = 9;
            response[0] = 11;
			break;
        case 9:/*HSPA+*/
#ifdef ANDROID_3
            response[0] = 15;/*For Android 3.x and 4.x, there is HSPA+ mode*/
#else          
            response[0] = 11;/*However,for Android 2.x , there is no HSPA+ mode,so we use HSPA*/
#endif                   
            break;            
        default: /*No error is considerred*/
            response[0] = 0;
            break;
    }	
#ifdef ANDROID_5
	RIL_onUnsolicitedResponse(RIL_UNSOL_VOICE_RADIO_TECH_CHANGED, response, sizeof(response));
#else
#ifdef ANDROID_3		 
	RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,NULL, 0); 
#else
    RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);   
#endif
#endif
	return;
error:
	if(RIL_DEBUG) ALOGE( "%s: Error parameter in the AT command: %s", __FUNCTION__, line);     
	return;
}

/*************************************************
 Function:  hcsq_on_unsolv
 Description:  
    hcsq unsolicited
 Calls: 
    at_tok_start
    RIL_onUnsolicitedResponse
 Called By:
    hwril_unsolicited_mm
 Input:
    line   - pointer to hex string 
 Output: none
 Return: none
 Others: added by c00221986 for DTS2013040706143
**************************************************/
void hcsq_on_unsolv(char *line)
{
	int err, rssi, rsrp, rsrq;
	char *sysmode;
	
#ifdef ANDROID_3
    int response[12] = {99, 99, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,};  
#else
    int response[2];
#endif
	err = at_tok_start(&line);
	if (0 > err)
	{
		goto error;
	}
	err = at_tok_nextstr(&line, &sysmode);
	if (0 > err)
	{
		goto error;
	}
	if (0 == strcmp("NOSERVICE", sysmode))
	{
		ALOGD("NOSERVICE");
	}
	if (0 == strcmp("GSM", sysmode))
	{	
		err = at_tok_nextint(&line, &rssi);
		response[0] = wcdma_rssi_to_real(rssi);

	}
	if (0 == strcmp("WCDMA", sysmode))
	{
		err = at_tok_nextint(&line, &rssi);
		response[0] = wcdma_rssi_to_real(rssi);	
	}
#ifdef ANDROID_3		
	if (0 == strcmp("LTE", sysmode))
	{
		err = at_tok_nextint(&line, &rssi);
		response[7] = wcdma_rssi_to_real(rssi);	
		err = at_tok_nextint(&line, &rsrp);
		response[8] = lte_rsrp_to_real(rsrp);
		err = at_tok_nextint(&line, &rsrq);
		response[9] = lte_rsrq_to_real(rsrq);
	}
#endif		
	if (0 == strcmp("CDMA", sysmode))
	{
	}
	if (0 == strcmp("EVDO", sysmode))
	{
	}
    RIL_onUnsolicitedResponse (RIL_UNSOL_SIGNAL_STRENGTH,  response, sizeof(response));  
	return;
error:
	if(RIL_DEBUG) ALOGE( "%s: Error parameter hcsq_on_unsolv: %s", __FUNCTION__, line);     
	return;	
}

static void skipWhiteSpace(char **p_cur)
{
	if (*p_cur == NULL) return;

	while (**p_cur != '\0' && isspace(**p_cur)) {
		(*p_cur)++;
	}
}

/***************************************************
 Function:  hwril_unsolicited_mm
 Description:  
    Handle the unsolicited about curent mm. 
    This is called on atchannel's reader thread. 
 Calls: 
    RIL_onUnsolicitedResponse
 Called By:
    on_unsolicited
 Input:
    s - pointer to unsolicited intent(+CREG)
 Output:
    none
 Return:
    none
 Others:
    none
**************************************************/

void hwril_unsolicited_mm (const char *s)
{
	char  *line= NULL;
	
	char	*rssivaluestr = NULL;// by alic
	int rssivaluetmp;// by alic
	static int lastrssivalue = 0;
	int err;
	
	//ALOGE( "******aliC in hwril_unsolicited_mm");
	
	if (strStartsWith(s,"+CREG:") || strStartsWith(s,"+CGREG") ||strStartsWith(s,"^SRVST:")|| strStartsWith(s,"^MODE:") ){       	
/* Begin added by x84000798 for android4.1 Jelly Bean DTS2012101601022 2012-10-16 */	
		if (strStartsWith(s,"^MODE:"))
		{
			line = strdup(s);
			mode_on_unsolv(line);
			free(line);
			line = NULL;			
		}
/* End added by x84000798 for android4.1 Jelly Bean DTS2012101601022 2012-10-16 */	
//begin modified by wkf32792 for android 3.x 20110815
#ifdef ANDROID_3		 
		RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_VOICE_NETWORK_STATE_CHANGED,NULL, 0); 
#else
        RIL_onUnsolicitedResponse (RIL_UNSOL_RESPONSE_NETWORK_STATE_CHANGED, NULL, 0);   
#endif
//end modified by wkf32792 for android 3.x 20110815
        
	}else if(strStartsWith(s, "^RSSI")){
		line = strdup(s);
		rssi_on_unsolv(line);
		//by alic :第一个修改使之重新拨号的地方。
		rssivaluestr = strchr(line, ':');
		rssivaluestr++;
		rssivaluetmp = atoi(rssivaluestr);
		//if(RIL_DEBUG) ALOGE( "******aliC in hwril_unsolicited_mm, rssivaluetmp %s", rssivaluetmp);//害死人
		if(RIL_DEBUG) ALOGE( "******aliC in hwril_unsolicited_mm, rssivaluetmp %d", rssivaluetmp);
		if ((99 == rssivaluetmp) || 
			 ((0 == lastrssivalue) && rssivaluetmp > 20))//for e353s2:bucarse it doesn't ussolv RSSI:99. 20是随机值。自定义的
			s_MaxError++;//end by alic
		
		lastrssivalue = rssivaluetmp;//for e353s2:bucarse it doesn't ussolv RSSI:99.
		free(line);
		line = NULL;
	}else if((strStartsWith(s, "^CRSSI")) || (strStartsWith(s, "^HDRRSSI"))){
		line = strdup(s);
		crssi_on_unsolv(line);   
		
		/*by alic :第四个修改使之重新拨号的地方。
		rssivaluestr = strchr(line, ':');
		rssivaluestr++;
		rssivaluetmp = atoi(rssivaluestr);
		if(RIL_DEBUG) ALOGE( "******aliC in hwril_unsolicited_mm, crssivaluetmp %d", rssivaluetmp);
		if (99 == rssivaluetmp)
			s_MaxError++;//end by alic
		*/	
		free(line);
		line = NULL;
	}else if (strStartsWith(s, "^SMMEMFULL")){
		//begin modify by KF34305 20111203 for DTS2011111804270;
		if(NETWORK_CDMA == g_module_type)
			{
				RIL_onUnsolicitedResponse (RIL_UNSOL_CDMA_RUIM_SMS_STORAGE_FULL, NULL, 0);
			}
			else
			{
				RIL_onUnsolicitedResponse (RIL_UNSOL_SIM_SMS_STORAGE_FULL, NULL, 0);
			}
		//end modify by KF34305 20111203 for DTS2011111804270;
	}else if (strStartsWith(s, "+CMTI:")){  
		int i;
		char *nextresponse;
		line = strdup(s);
		at_tok_start(&line);
		err = at_tok_nextstr(&line, &nextresponse);
		if (err < 0) goto error;
		err = at_tok_nextint(&line, &i);            
		if (err < 0) goto error;  
		i++;       
		if(line != NULL) free(line);
		RIL_onUnsolicitedResponse ( RIL_UNSOL_RESPONSE_NEW_SMS_ON_SIM, &i, sizeof(i)); 
	}
	else if (strStartsWith(s, "^NWTIME")){ //support nitz
	    if(RIL_DEBUG)ALOGD("NITZ: %s",s);
		char *nitz = NULL;
		nitz = strdup(s);
		at_tok_start(&nitz);
		skipWhiteSpace(&nitz);
		if(RIL_DEBUG)ALOGD("After parsing NITZ: %s",nitz);
		if(nitz != NULL ){
        	RIL_onUnsolicitedResponse( RIL_UNSOL_NITZ_TIME_RECEIVED,  (void*)nitz, strlen( nitz) );
		}
	}
	/* Begin added by c00221986 for DTS2013040706143 */
	else if (strStartsWith(s, "^HCSQ:"))
	{
		line = strdup(s);
		hcsq_on_unsolv(line);   
		free(line);
		line = NULL;		
	}
	/* End added by c00221986 for DTS2013040706143 */
	return ;

error:
	if(line != NULL) free(line); 
	if(RIL_DEBUG) ALOGE( "%s: Error parameter in ind msg: %s", __FUNCTION__, s);  
	return;
}


