/********************************************************
 *  Copyright (C), 1988-2009, Huawei Tech. Co., Ltd.
 *File Name: sar_feature.c
 *Author: hexiaokong/h81003427 
 *Version: V1.0
 * Date: 2012/03/15
 * Description:
 * pthread function to implement sar feature.
 * Function List:
 *History:
 * <author>                           <time>                    <version>               <desc>
 * hexiaokong/h81003427       2012/03/13                  1.0       
 *
 ********************************************************/
#include "huawei-ril.h"

//begin to modify by h81003427 for sar feature 20120414
/* Table of pre-tuned power (values are tentative) */
//int PowerTableWcdma[4] = {23, 22, 21, 20}; /* Band1, 2, 5, 8*/
//int PowerTableGsm[][4] = {{31, 27, 23, 19}, /* GM850 x 4 slot */
	//{30, 26, 22, 18}, /* GSM900 x 4 slot*/
	//{29, 25, 21, 17}, /* GSM1800 x 4slot */
	//{28, 24, 20, 16}}; /* GSM1900 x 4slot */

/* Begin added by x84000798 2012-05-29 DTS2012060806513 */
int g_sarctrl = -1;
extern int g_airplanemode;
/* End added by x84000798 2012-05-29 DTS2012060806513 */
void* processSarCtrlNotification(void *_args)
{
	LOGD("************create thread*****************");
	for(; ; )
	{
		FILE *sys_file;
		char buf[50];
		//static int i_pid = 0;
		//int i_slot_band_valid = 0;

		/*if(!i_pid)
		{
			sys_file = fopen(FILENAME_SYS_SAR_CONTROL,"w");
			if(sys_file)
			{
				LOGD("**********************RIL is ready,and pid is:%d",s_tid_processSarCtrlNotification);
				//write pid to PRC driver through control node
				fprintf(sys_file,"%s %d",STRING_SAR_SIGNAL,s_tid_processSarCtrlNotification);
				fclose(sys_file);
				i_pid = 1;
			}
			else
			{
				continue ;
			}
		}*/
		/*read one line from sysfs*/
		memset (buf,0,sizeof(buf));
		sys_file = fopen(FILENAME_SYS_SAR_CONTROL,"r");
		if(!sys_file)
		{
			sleep(TIMEOUT_SEARCH_FOR_CONTROL);
			continue;
		}

		//read notification from PRC driver through control node
		if(fgets(buf,sizeof(buf),sys_file))
		{
			fclose(sys_file);
		}
		else
		{
			LOGD("***************************read NULL***************");
			fclose(sys_file);
		}

		//replace the last character '\n' to '\0' after reading successfully
		if(strlen(buf)>0 && buf[strlen(buf)-1] == '\n')
		{
			buf[strlen(buf)-1] = '\0';
		}
		//if the notification is STRING_SAR_CONTROL_ON
		if(!strncmp(buf,STRING_SAR_CONTROL_ON,strlen(STRING_SAR_CONTROL_ON)))
		{
			LOGD("***************%s:%s\n",__func__,STRING_SAR_CONTROL_ON);
			//there is no need to get slot and band now. Such work is been done in FW. Just send AT command to enable SAR back off
			/*
			int err = -1;
                     ATResponse *p_response = NULL;
			char *cmd = NULL;
			char *line = NULL;
			int band = 0;
			int uplink_slot = 0;
			int downlink_slot = 0;

			//get band
			err = at_send_command_singleline("AT^GETBAND", "^GETBAND:", &p_response);

			if (err < 0 || p_response->success == 0) 
			{ 	
				LOGD("***********************************AT COMMAND RETURN ERROR**************");
				goto error;
			}

			line = p_response->p_intermediates->line;
			err = at_tok_start (&line);    
			LOGD("*************************line is:%s",line);
			if (err < 0) 
			{
				goto error;
			}

			err = at_tok_nextint(&line, &band);
			LOGD("*************************band is:%d",band);
			if (err < 0) 
			{
				goto error;
			}
			at_response_free(p_response);
			p_response = NULL;

			//if GSM
			if ((band == GSM850) || (band == GSM900) || (band == GSM1800) || (band == GSM1900))
			{
				//get slot
				err = at_send_command_singleline("AT^GETSLOT", "^GETSLOT:", &p_response);             
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
				err = at_tok_nextint(&line, &uplink_slot);
				LOGD("**********************uplink_slot is:%d",uplink_slot);
				if(err < 0)
				{
					goto error;
				}
				err = at_tok_nextint(&line, &downlink_slot);
				LOGD("**********************downlink_slot is:%d,but we don't use this value********",downlink_slot);
				if(err < 0)
				{
					goto error;
				}
				//get non-tuned power value from table according to uplink_slot to decide the power.
				switch(uplink_slot)
				{
					case 1:	
						asprintf(&cmd,"AT^BODYSARGSM=%d,%d,%d,%d",PowerTableGsm[0][0],PowerTableGsm[1][0],PowerTableGsm[2][0],PowerTableGsm[3][0]);
						LOGD("********************cmd is:%s*************",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					case 2:
						asprintf(&cmd,"AT^BODYSARGSM=%d,%d,%d,%d",PowerTableGsm[0][1],PowerTableGsm[1][1],PowerTableGsm[2][1],PowerTableGsm[3][1]);
						LOGD("********************cmd is:%s*************",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					case 3:
						asprintf(&cmd,"AT^BODYSARGSM=%d,%d,%d,%d",PowerTableGsm[0][2],PowerTableGsm[1][2],PowerTableGsm[2][2],PowerTableGsm[3][2]);
						LOGD("********************cmd is:%s*************",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					case 4:
						asprintf(&cmd,"AT^BODYSARGSM=%d,%d,%d,%d",PowerTableGsm[0][3],PowerTableGsm[1][3],PowerTableGsm[2][3],PowerTableGsm[3][3]);
						LOGD("********************cmd is:%s*************",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					case 0:
					default :
						LOGD("***************************no uplink timeslot*******************************");
						i_slot_band_valid = 1;
						break;

				}
			}

			//if WCDMA
			else if ((band == WCDMA850) || (band == WCDMA900) || (band == WCDMA1900) || (band == WCDMA2100))
			{
				switch(band)
				{
					case WCDMA2100:
						asprintf(&cmd,"AT^BODYSARWCDMA=%d",PowerTableWcdma[0]);
						LOGD("***********************cmd is:%s**********",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					case WCDMA1900:
						asprintf(&cmd,"AT^BODYSARWCDMA=%d",PowerTableWcdma[1]);
						LOGD("***********************cmd is:%s**********",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					case WCDMA850:
						asprintf(&cmd,"AT^BODYSARWCDMA=%d",PowerTableWcdma[2]);
						LOGD("***********************cmd is:%s**********",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					case WCDMA900:
						asprintf(&cmd,"AT^BODYSARWCDMA=%d",PowerTableWcdma[3]);
						LOGD("***********************cmd is:%s**********",cmd);
						err = at_send_command(cmd,NULL);
						free(cmd);
						break;
					default:
						LOGD("******************no wcdma band to match******");
						i_slot_band_valid = 1;
						break;
				}
			}
			else
			{
				LOGD("*************************no such band****************");
				goto error;
			}*/
			/* Begin added by x84000798 2012-05-29 DTS2012060806513 */
			//at_send_command("AT^SARCTRL=1",NULL);
			if (0 == g_airplanemode)
			{
				LOGD("Airplane Mode OFF, send AT command");
				at_send_command("AT^SARCTRL=1",NULL);
			}
			g_sarctrl = 1;
			/* Begin added by x84000798 2012-05-29 DTS2012060806513 */

		} 
		else if(!strncmp(buf,STRING_SAR_CONTROL_OFF,strlen(STRING_SAR_CONTROL_OFF)))
		{
			LOGD("***************%s:%s\n",__func__,STRING_SAR_CONTROL_OFF);
			/* Begin added by x84000798 2012-05-29 DTS2012060806513 */
			//at_send_command("AT^SARCTRL=0",NULL);
			if (0 == g_airplanemode)
			{
				LOGD("Airplane Mode OFF, send AT command");
				at_send_command("AT^SARCTRL=0",NULL);
			}
			g_sarctrl = 0;
			/* End added by x84000798 2012-05-29 DTS2012060806513 */
		}
		else if(!strncmp(buf,STRING_SAR_CONTROL_SUSPEND,strlen(STRING_SAR_CONTROL_SUSPEND)))
		{
			LOGD("***************%s:%s\n",__func__,STRING_SAR_CONTROL_SUSPEND);
			/* Begin added by x84000798 2012-05-29 DTS2012060806513 */
			//at_send_command("AT^SARCTRL=1",NULL);
			if (0 == g_airplanemode)
			{
				LOGD("Airplane Mode OFF, send AT command");
				at_send_command("AT^SARCTRL=1",NULL);
			}
			g_sarctrl = 1;
			/* End added by x84000798 2012-05-29 DTS2012060806513 */

			//ACK return
		       sys_file = fopen(FILENAME_SYS_SAR_CONTROL,"w");
		       
		       fputs(STRING_SAR_CONTINUE,sys_file);
		       fclose(sys_file); 
		}
		else
		{
			LOGD("**************%s:string not match!",__func__);
			continue;
		}
              /*
		if(i_slot_band_valid)
		{
			goto error;
		}
		//ACK return
		sys_file = fopen(FILENAME_SYS_SAR_CONTROL,"w");
		fputs(STRING_SAR_CONTINUE,sys_file);
		fclose(sys_file); 
		continue;
error:
		LOGD("**************************continue when error occurs or no slot or band cannot match******");
		continue;*/
	} 
	//return (void*) 0;//modify by x84000798 for PC_Lint warning
}

//end to modify by h81003427 for sar feature 20120414


