
/*
 * main.c
 *
 *  Created on: Aug 23, 2019
 *      Author: dave
 * Modified on: Nov 13, 2019
 *      Author: Team 3
 */

//AXI GPIO driver
#include "xgpio.h"
//#include "sleep.h"
//send data over UART
#include "xil_printf.h"

//information about AXI peripherals
#include "xparameters.h"
//#include "platform.h"
#include "xsysmon.h"
#define RX_BUFFER_SIZE 4 // defines how many XADC channels are read
#define xadc XPAR_SYSMON_0_DEVICE_ID
XSysMon xadc_inst;
int xsts;

//VAUX14 = temp.sensor, VAUX06 = PIS
char *channel[] ={"VAUX14","VAUX07","VAUX15","VAUX06"}; //for RawData printf
int sample[4] = {XSM_CH_AUX_MIN + 14,XSM_CH_AUX_MIN + 7,XSM_CH_AUX_MIN + 15,XSM_CH_AUX_MIN + 6};
//sample array used to specify channel when reading ADC data into XADC_Buf
u32 toBCD(u32 x) {
	//converts a number to bcd so it can be displayed on the basys 3
	//if the number is printed in hexadecimal it should look the same as the original decimal input
	return x<10 ? x : (x%10)+16*toBCD(x/10);
}
int main()
{
	XGpio gpio;
	XGpio gpio1;
	XGpio gpio2;
	XGpio gpio3;
	XGpio gpio4;
	u32 btn, led;
	u32 sw;
	u32 jbin, digin, rpiin;
	u32 jcout;
	u32 input_from_rpi;
	u32 output;
    
	XGpio_Initialize(&gpio, 0); //buttons and leds
	XGpio_Initialize(&gpio1, 1); //switches
	XGpio_Initialize(&gpio2, 2); //JB connector
	XGpio_Initialize(&gpio3, 3); //JC connector
	XGpio_Initialize(&gpio4, 4); //Seven Segment Display
	
	XGpio_SetDataDirection(&gpio, 2, 0x00000000); // set led GPIO channel tristates to All Output
	XGpio_SetDataDirection(&gpio, 1, 0xFFFFFFFF); // set BTN GPIO channel tristates to All Input
	XGpio_SetDataDirection(&gpio1, 1, 0xFFFFFFFF); // set sw GPIO channel tristates to All Input
	XGpio_SetDataDirection(&gpio2, 1, 0xFFFFFFFF); // set JB GPIO channel tristates to All Input
	XGpio_SetDataDirection(&gpio3, 1, 0x00000000); // set JC GPIO channel tristates to All Output
	XGpio_SetDataDirection(&gpio4, 1, 0x00000000); //set bcdout channel to ALL Outputs
	int Index;
	XSysMon *xadc_inst_ptr = &xadc_inst;
	u32 XADC_Buf[RX_BUFFER_SIZE];


	XSysMon_Config *xadc_config;

//	init_platform();

	xil_printf("XADC Example\n\r");

	xadc_config = XSysMon_LookupConfig(xadc);
	if (NULL == xadc_config){
		xil_printf("XSysMon_LookupConfig failed\n");
	}

	XSysMon_CfgInitialize(xadc_inst_ptr,xadc_config,xadc_config->BaseAddress);

	xsts=XSysMon_SelfTest(xadc_inst_ptr);
	if (XST_SUCCESS != xsts){
		xil_printf("ADC self test failed\n");
	}
	XSysMon_SetSequencerMode(xadc_inst_ptr,XSM_SEQ_MODE_SAFE);
	XSysMon_SetAlarmEnables(xadc_inst_ptr, 0x00000000);
	XSysMon_SetSequencerMode(xadc_inst_ptr,XSM_SEQ_MODE_SAFE);
	XSysMon_SetAlarmEnables(xadc_inst_ptr, 0x0);
	xsts = XSysMon_SetSeqChEnables(xadc_inst_ptr,
				XSM_SEQ_CH_AUX14 |
				XSM_SEQ_CH_AUX07 |
				XSM_SEQ_CH_AUX15 |
				XSM_SEQ_CH_AUX06);
	if (XST_SUCCESS != xsts){
			xil_printf("Failed to configure XSysMon_SetSeqChEnables\n");
	}
	xsts=XSysMon_SetSeqInputMode(xadc_inst_ptr,0);
	if (XST_SUCCESS != xsts){
		xil_printf("Failed to configure all XADC channels as unipolar\n");
	}
	XSysMon_SetSequencerMode(xadc_inst_ptr,XSM_SEQ_MODE_CONTINPASS);

	while(1){
 		/*********PushButton, Switch, and Led Section*/
		btn = XGpio_DiscreteRead(&gpio, 1);
		sw  = XGpio_DiscreteRead(&gpio1,1);
		jbin  = XGpio_DiscreteRead(&gpio2,1);
		jcout = 1;//set to 0 for silence, 3 for high frequency buzzer
		input_from_rpi = jbin&1; //input = 1, this = 1
		digin = jbin & 0xF; //jb[3..0]
		rpiin = (jbin & 0xF0)>>4; //jb[7..4];
		
		XGpio_DiscreteWrite(&gpio, 2, led);
		XGpio_DiscreteWrite(&gpio3, 1, jcout);
		XGpio_DiscreteWrite(&gpio2, 1, input_from_rpi); //input reading	
		//xil_printf("\rbutton state: %08x\n",btn);
		//xil_printf("\r jbin: %08x, digin: %08x, rpiin: %08x\n",jbin,digin,rpiin);
		/******************end of section*************/

		 /*****************************XADC section**************/
		//getting the raw data 
		XSysMon_GetStatus(xadc_inst_ptr); //Clear the old status
		for (Index = 0;Index<RX_BUFFER_SIZE;Index++){
			while((XSysMon_GetStatus(xadc_inst_ptr)& XSM_SR_EOS_MASK) != XSM_SR_EOS_MASK){
				XADC_Buf[Index] = XSysMon_GetAdcData(xadc_inst_ptr,sample[Index]);
			}
		}
		//at the point, the raw value of each vaux has been assigned.

		//Output to Rpi, python script
		//PIS = vaux06
		int raw_pis = (int)(XADC_Buf[3]>>4);

		//temperature sensor = vaux14
		double raw_temp = (double)(XADC_Buf[0]>>4);
		double temp_F = raw_temp*0.07984 - 10.0;
		double temp_C = (temp_F - 32.0)/ 1.8;
		int temp_F_in = (int)temp_F;
		int temp_C_in = (int)temp_C;
		
		//1 = C, F = 0
		//1 = user wants to see in C,
		if (input_from_rpi == 1){
		  output = temp_C_in;
		}else{
		  output = temp_F_in;
		}
		if(raw_pis == 4095){
			XGpio_DiscreteWrite(&gpio4, 1, toBCD(output)); //displays temperature
		}
		else {
			XGpio_DiscreteWrite(&gpio4, 1, 0xaaaa); //displays dashes
		}

		//print temp. sensor 
		//xil_printf("Temp in F: %d \n",temp_F_in);
		xil_printf("%d",temp_C_in);
		xil_printf("   \n");

		sleep(1);

		/****************end of section*************/
	} //end of while(1)

	return 0;
}
