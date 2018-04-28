#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h" 		 	 
#include "lcd.h"  
#include "key.h"     
#include "usmart.h" 
#include "malloc.h"
//#include "sdio_sdcard.h"  
#include "w25qxx.h"    
#include "ff.h"  
#include "exfuns.h"   
#include "text.h"
#include "piclib.h"	
#include "string.h"		
#include "math.h"	 
//#include "ov7670.h"
#include "beep.h" 
#include "timer.h" 
#include "exti.h"
#include "ov7725.h"
#include "sram.h" 
#include "MMC_SD.h"
#include "usart3.h"
#include "common.h" 


#define  OV7725 1
#define  OV7670 2
#define key_down PEin(3)
#define key_left PEin(2)
#define key_up PAin(0)
#define key_right PEin(4)

extern u8 ov_sta;	//在exit.c里面定义

u8 mode_vga;
u16 count = 0;
u8 pix_x[240];
u8 pix_y[320];

u16 x1 = 0;//标定框的四个坐标
u16 x2 = 0;
u16 y1 = 0;
u16 y2 = 0;

u8 flag_start = 0;
u8 center_x = 120;
u8 center_y = 160;
u8 center_x_last = 120;
u8 center_y_last = 160;
u8 temp_r,temp_g,temp_b;//RGB的临时变量
u8 temp_gray;
u16 color;
u8 sd_color;
u8 flag_reflash = 0;
float feature_aspectRatio = 0;//宽高比----第一个特征
u8 isFall = 0;//是否摔倒
u8 isPerson = 0;//是否有人

u32 N = 0;//图像的和(N)
int sum_pix;//计算和的临时变量
int Ex;//均值
int Ey;
float cov[2][2];//协方差矩阵
const float tanTable[90] = {0.009,0.026,0.044,0.061,0.079,0.096,0.114,
0.132,0.149,0.167,0.185,0.203,0.222,0.240,0.259,0.277,0.296,0.315,
0.335,0.354,0.374,0.394,0.414,0.435,0.456,0.477,0.499,0.521,0.543,
0.566,0.589,0.613,0.637,0.662,0.687,0.713,0.740,0.767,0.795,0.824,
0.854,0.885,0.916,0.949,0.983,1.018,1.054,1.091,1.130,1.171,1.213,
1.257,1.303,1.351,1.402,1.455,1.511,1.570,1.632,1.698,1.767,1.842,
1.921,2.006,2.097,2.194,2.300,2.414,2.539,2.675,2.824,2.989,3.172,
3.376,3.606,3.867,4.165,4.511,4.915,5.396,5.976,6.691,7.596,8.777,
10.385,12.706,16.350,22.904,38.188,114.589};
//用于my_tan()中查表计算arctan

int16_t angle = 0;//中间变量
u8 feature_angle = 0;//倾斜角度-----第二个特征

u32 sd_size;
u32 cnt = 0;
u8 mode = 0;
u8 sd_send_temp[512];
const u8 frame_flag[6] = {0xff,0x00,0xff,0x00,0xff,0x00};//用于同步帧
u8 isESP = 0;

	
/***********以下是储存在片外RAM的变量***************/
u8 pixel_back[240][320][3] __attribute__((at(0X680ff000)));//储存背景图片的所有像素点
u8 pixel[240][320][3]  __attribute__((at(0X68137400)));
u8 pixel_gray[240][320] __attribute__((at(0X6816f800)));//灰度图片
u8 sd_pixel[153600] __attribute__((at(0X68182400)));//灰度图片
//u32 testsram[250000] __attribute__((at(0X68100000)));//测试用数组

void menu_display()
{
	LCD_ShowString(30,190,200,16,16,(u8*)"Choose Mode");
	LCD_ShowString(30,210,200,16,16,(u8*)"Mode 0 Start");
	LCD_ShowString(30,230,200,16,16,(u8*)"Mode 1 SD Save");
	LCD_ShowString(30,250,200,16,16,(u8*)"Mode 2 Display Camera");
	LCD_ShowString(30,270,200,16,16,(u8*)"Mode 3 SD Send Pic");
	LCD_ShowString(30,290,200,16,16,(u8*)"Mode 4 SD Send fct");
	LCD_ShowString(30,310,200,16,16,(u8*)"Mode 5 WIFI");
	while(1)
	{
		if(key_down == 0)
		{
			LCD_ShowString(10,210 + mode * 20,20,16,16,(u8*)" ");
			delay_ms(200);
			if(mode == 5)  mode = 0;
			else  mode++;
		}
		LCD_ShowString(10,210 + mode * 20,20,16,16,(u8*)">");
		if(key_right == 0)
		{
			delay_ms(200);
			LCD_Clear(WHITE);
			break;
		}
	}
}

u32 my_abs(int a )
{
	return a>0 ? a : -a;
}
float my_abs_f(float a )
{
	return a>0 ? a : -a;
}

/*********************/
u8 get_value(u8 left,u8 right,float value)
{
    u8 half = (left + right) >> 1;
    if(left == half && half + 1 == right)  return right;
    if(left + 1 == half && half + 1 == right)
    {
        if(value < tanTable[half]) return half;
        else  return right;
    }
    if(value <= tanTable[half])
    {
        return get_value(left,half,value);
    }
    else
    {
        return get_value(half,right,value);
    }
}
//使用二分法查找
int16_t my_arctan(float value)
{
	  u8 flag = 0;
	  int16_t result = 0;
	  u8 left = 0,right = 89;
	
	
	  if(value < - 115.000) {BEEP = 1;return -90;}
	  else if(value > - 0.009 && value < 0.009) return 0;
	  else if(value > 115.000) return 90;
	
	  if(value <= 0)
		{
			flag = 1;
			value = 0.000 - value;
		}
			
    result = get_value(left,right,value);
		
		if(flag)
			return -result;
		else
			return result;
}
/*************************/

//更新LCD显示(OV7725)
void OV7725_camera_refresh(void)
{
	u16 i,j;
	
	if(ov_sta)//有帧中断更新
	{
		LCD_Scan_Dir(U2D_L2R);//从上到下,从左到右
		LCD_Set_Window((lcddev.width-320)/2,(lcddev.height-240)/2,320,240);//将显示区域设置到屏幕中央 480*320
		LCD_WriteRAM_Prepare();     //开始写入GRAM	
		OV7725_RRST=0;				//开始复位读指针 
		OV7725_RCK_L;
		OV7725_RCK_H;
		OV7725_RCK_L;
		OV7725_RRST=1;				//复位读指针结束 
		OV7725_RCK_H; 
		for(i=0;i<240;i++)
		{
			for(j=0;j<320;j++)
			{
				OV7725_RCK_L;
				color=GPIOC->IDR&0XFF;	//读数据
				OV7725_RCK_H;
				color<<=8;
				OV7725_RCK_L;
				color|=GPIOC->IDR&0XFF;	//读数据
				OV7725_RCK_H;
				pixel[i][j][0] = ((color&0xf800)>>11);
				pixel[i][j][1] = ((color&0x07e0)>>5);
				pixel[i][j][2] = ((color&0x001f));
				if(pixel_gray[i][j])
				{
					color = 0xffff;
				}
				
				if(j == y1 && i >= x1 && i <= x2)
					color = 0xf800;
				if(j == y2 && i >= x1 && i <= x2)
					color = 0xf800;
				if(i == x1 && j >= y1 && j <= y2)
					color = 0xf800;
				if(i == x2 && j >= y1 && j <= y2)
					color = 0xf800;
				LCD->LCD_RAM=color;
			}
		}
		ov_sta=0;					//清零帧中断标记
		LCD_Scan_Dir(DFT_SCAN_DIR);	//恢复默认扫描方向 
			
	}
}

//更新LCD显示(OV7725)
void OV7725_camera_refresh_2(void)
{
	u32 i;
	u32 j = 0;
	if(ov_sta)//有帧中断更新
	{
		LCD_Scan_Dir(U2D_L2R);//从上到下,从左到右
		LCD_Set_Window((lcddev.width-320)/2,(lcddev.height-240)/2,320,240);//将显示区域设置到屏幕中央 480*320
		LCD_WriteRAM_Prepare();     //开始写入GRAM	
		OV7725_RRST=0;				//开始复位读指针 
		OV7725_RCK_L;
		OV7725_RCK_H;
		OV7725_RCK_L;
		OV7725_RRST=1;				//复位读指针结束 
		OV7725_RCK_H; 
		for(i=0;i<76800;i++)
		{
				OV7725_RCK_L;
				color=GPIOC->IDR&0XFF;	//读数据
				sd_pixel[j] = color;
				OV7725_RCK_H;
				color<<=8;
				OV7725_RCK_L;
				sd_color = GPIOC->IDR&0XFF;//读数据
				color|=sd_color&0XFF;	
				
				OV7725_RCK_H;
				LCD->LCD_RAM=color;
				sd_pixel[j + 1] = sd_color;
			  j += 2;
		}
		//这里可以优化 可以512字节写一块 防止先去外部ram储存的时间浪费
		SD_WriteDisk_(sd_pixel,cnt,300);
		ov_sta=0;					//清零帧中断标记
		LCD_Scan_Dir(DFT_SCAN_DIR);	//恢复默认扫描方向 
	}
}

//更新LCD显示(OV7725)
void OV7725_camera_refresh_3(void)
{
	u16 i,j;
	
	if(ov_sta)//有帧中断更新
	{
		LCD_Scan_Dir(U2D_L2R);//从上到下,从左到右
		LCD_Set_Window((lcddev.width-320)/2,(lcddev.height-240)/2,320,240);//将显示区域设置到屏幕中央 480*320
		LCD_WriteRAM_Prepare();     //开始写入GRAM	
		OV7725_RRST=0;				//开始复位读指针 
		OV7725_RCK_L;
		OV7725_RCK_H;
		OV7725_RCK_L;
		OV7725_RRST=1;				//复位读指针结束 
		OV7725_RCK_H; 
		for(i=0;i<240;i++)
		{
			for(j=0;j<320;j++)
			{
				OV7725_RCK_L;
				color=GPIOC->IDR&0XFF;	//读数据
				OV7725_RCK_H;
				color<<=8;
				OV7725_RCK_L;
				color|=GPIOC->IDR&0XFF;	//读数据
				OV7725_RCK_H;
				LCD->LCD_RAM=color;
			}
		}
		ov_sta=0;					//清零帧中断标记
		LCD_Scan_Dir(DFT_SCAN_DIR);	//恢复默认扫描方向 
			
	}
}
/*******************************************
图像处理程序
********************************************/
void image_process()
{
	int i,j;
	//int16_t intj;
	/*****初始化*********/
	N = 0;
	sum_pix = 0;
	Ex = 0;
	Ey = 0;
	/********************/
	
	center_x_last = center_x;
	center_y_last = center_y;
	++count;
	if(count > 30)//reflash the background
	{
		count = 0;
	}

	if(count == 30 && isPerson == 0)//背景中没有人时更新背景
	{
		for(i=0;i<240;i++)
		{
			for(j=0;j<320;j++)
			{
				pixel_back[i][j][0] = pixel[i][j][0];
				pixel_back[i][j][1] = pixel[i][j][1];
				pixel_back[i][j][2] = pixel[i][j][2]; 
			}
		}
	}
	else if(flag_reflash == 1 && count >= 10)//刚开机时需更新背景
	{
		flag_reflash = 0;
		for(i=0;i<240;i++)
		{
			for(j=0;j<320;j++)
			{
				pixel_back[i][j][0] = pixel[i][j][0];
				pixel_back[i][j][1] = pixel[i][j][1];
				pixel_back[i][j][2] = pixel[i][j][2]; 
			}
		}
	}
	else
	{
		for(i=0;i<240;i++)//背景差分
		{
			for(j=0;j<320;j++)
			{
				temp_r = my_abs(pixel_back[i][j][0] - pixel[i][j][0]);
				temp_g = my_abs(pixel_back[i][j][1] - pixel[i][j][1]);
				temp_b = my_abs(pixel_back[i][j][2] - pixel[i][j][2]); 
				temp_gray = (temp_r + temp_g + temp_b);
				if(temp_gray >= 18)//灰度二值化
				{
					pixel_gray[i][j] = 1;
					pix_x[i]++;//记录横坐标和纵坐标的个数
					pix_y[j]++;
				}
				else
					pixel_gray[i][j] = 0;
			}
		}
	}
	/*****************四周向中间寻找目标*****************/
	for(i = 0;i < 239;i++)
	{
		
		if(pix_x[i] >= 5 && pix_x[i + 1] >= 5)
		{
			x1 = i;
			break;
		}
		else
			x1 = 0;
			
	}
	for(i = 239;i > 0;i--)
	{
		if(pix_x[i] >= 5 && pix_x[i - 1] >= 5)
		{
			x2 = i;
			break;
		}
		else
			x2 = 0;
	}
	
	for(i = 0;i < 319;i++)
	{
		if(pix_y[i] >= 5 && pix_y[i + 1] >= 5)
		{
			y1 = i;
			break;
		}
		else
			y1 = 0;
	}
	for(i = 319;i > 0;i--)
	{
		
		if(pix_y[i] >= 5 && pix_y[i - 1] >= 5)
		{
			y2 = i;
			break;
		}
		else
			y2 = 0;
			
	}
	center_x = (x1 + x2) >> 1;
	center_y = (y1 + y2) >> 1;
	
	/**************判断是否有人****************/
	
	if(x1 == 0 && x2 == 0 && y1 == 0 && y2 == 0)
	{
		isPerson = 0;
	}
	else
	{
		isPerson = 1;
	}
	
	if(x1 == x2 || y1 == y2)
	{
		isPerson = 0;
	}
	if(x2 - x1 < 10 || y2 - y1 < 10)
	{
		isPerson = 0;
	}
	
	/***宽高比***/
	if(isPerson == 1)
	{
		feature_aspectRatio = (x2 - x1) / (y2 - y1);
	}
	else
		feature_aspectRatio = 0;
	
	/***********协方差算姿态角***********/
	if(isPerson == 1)
	{
		for(i = x1;i <= x2;i++)//算出个数N
		{
			N += pix_x[i];
		}
		
		/***********计算均值 Ex Ey******/
		sum_pix = 0;
		for(i = x1;i <= x2;i++)
		{
			sum_pix += (pix_x[i] * (i - x1));
		}
		Ex = x1 + sum_pix / N;
		
		sum_pix = 0;
		for(i = y1;i <= y2;i++)
		{
			sum_pix += (pix_y[i] * (i - y1));
		}
		Ey = y1 + sum_pix / N;
		
		/***********************************
		计算四个方差
		其中:cov[0][0] = cov(x,x)
		     cov[0][1] = cov(x,y)
		               = cov(y,x) = cov[1][0]
		     cov[1][1] = cov(y,y)
		************************************/
		sum_pix = 0;
		for(i = x1;i <= x2;i++)
		{
			sum_pix += ((Ex - i) * (Ex - i) * pix_x[i]);
		}
		cov[0][0] = sum_pix / (N - 1);
		
		sum_pix = 0;
		for(i = y1;i <= y2;i++)
		{
			sum_pix += ((Ey - i) * (Ey - i) * pix_y[i]);
		}
		cov[1][1] = sum_pix / (N - 1);
		
		sum_pix = 0;
		for(i = x1;i <= x2;i++)
		{
			for(j = y1;j <= y2;j++)
			{
				if(pixel_gray[i][j])
				{
					sum_pix += ((Ex - i) * (Ey - j));
				}
			}
		}
		if(sum_pix < 0)
		{
			cov[0][1] = my_abs(sum_pix) / (N - 1);
			cov[0][1] = 0 - cov[0][1];
		}
		else
			cov[0][1] = my_abs(sum_pix) / (N - 1);
		
		cov[1][0] = cov[0][1];
			
		//计算角度
		angle = 0.5 * (my_arctan(2.00 * cov[1][0] / (cov[0][0] - cov[1][1])));
		
		//归一化成特征
		if(cov[0][0] < cov[1][1])
		{
			if(cov[0][1] < 0)
			{
			  feature_angle = 90 - angle;
			}
			else
			{
				feature_angle = 90 + angle;
			}
		}
		else
		{
			if(cov[0][1] < 0)
			{
				feature_angle = my_abs(angle);
			}
			else
			{
				feature_angle = my_abs(angle);
			}
		}
	}
	else
		feature_angle = 0;
	
	LCD_ShowxNum(100,60,feature_angle,2,16,0);
	LCD_ShowxNum(150,60,cnt,3,16,0);
	
	sd_send_temp[0] = x2 - x1;
	sd_send_temp[1] = y2 - y1;
	sd_send_temp[2] = feature_angle;
	SD_WriteDisk_(sd_send_temp,cnt,1);
	cnt++;
	if(feature_aspectRatio >= 1.5)
		BEEP = 1;
	else
		BEEP = 0;
		
	
/*************清空***************/
	for(i = 0;i < 240;i++)
	{
		pix_x[i] = 0;
	}
	for(i = 0;i < 320;i++)
	{
		pix_y[i] = 0;
	}
	/*****************/
	
}

/**************验收程序******************/
void start_mode()
{
	while(SD_Initialize())//检测不到SD卡
	{
		LCD_ShowString(60,150,200,16,16,(u8*)"SD Card Error!");
		delay_ms(500);					
		LCD_ShowString(60,150,200,16,16,(u8*)"Please Check! ");
		delay_ms(500);
		LED0=!LED0;//DS0闪烁
	}
	LCD_ShowString(50,60,200,16,16,(u8*)"Angle: ");
 	while(1)
	{	
		OV7725_camera_refresh();//更新显示
		image_process();
		if(key_down == 0)
		{
			flag_reflash = 1;
			count = 0;
		}
	}
}

/*****************sd卡模式*********************/
void SD_Save_mode()
{
	
	while(SD_Initialize())//检测不到SD卡
	{
		LCD_ShowString(60,150,200,16,16,(u8*)"SD Card Error!");
		delay_ms(500);					
		LCD_ShowString(60,150,200,16,16,(u8*)"Please Check! ");
		delay_ms(500);
		LED0=!LED0;//DS0闪烁
	}
 	POINT_COLOR=BLUE;//设置字体为蓝色 
	//检测SD卡成功 											    
	LCD_ShowString(60,150,200,16,16,(u8*)"SD Card OK    ");
	LCD_ShowString(60,170,200,16,16,(u8*)"SD Card Size:     MB");
	sd_size=SD_GetSectorCount();//得到扇区数
	LCD_ShowNum(164,170,sd_size>>11,5,16);//显示SD卡容量
	delay_ms(20000);
	while(1)
	{	
		OV7725_camera_refresh_2();//更新显示并存储数据
	}
}
/*********************SD卡发送***********************/
void SD_Send_PIC_mode()
{
	u16 i = 0;
	while(SD_Initialize())//检测不到SD卡
	{
		LCD_ShowString(60,150,200,16,16,(u8*)"SD Card Error!");
		delay_ms(500);					
		LCD_ShowString(60,150,200,16,16,(u8*)"Please Check! ");
		delay_ms(500);
		LED0=!LED0;//DS0闪烁
	}
 	POINT_COLOR=BLUE;//设置字体为蓝色
	LCD_ShowString(60,150,200,16,16,(u8*)"SD Card OK");
	LCD_ShowString(60,170,200,16,16,(u8*)"SD Sending...");
	while(1)
	{
		SD_ReadDisk_(sd_send_temp,cnt,1);
		if(cnt % 300 == 0)//发送帧的开始信号 用于同步
		{
			for(i = 0;i < 6;i++)
			{
				USART_SendData(USART1, frame_flag[i]);
				delay_ms(1);
			}
		}
		for(i = 0;i < 512;i++)
		{
			USART_SendData(USART1, sd_send_temp[i]);
			delay_ms(1);
		}
		LCD_ShowNum(60,190,cnt,5,16);//显示发送SD的扇区号
		LCD_ShowNum(60,210,sd_send_temp[0],5,16);
		
		delay_ms(10);
		cnt++;
		
	}
}
void SD_Send_feature_mode()
{
	while(SD_Initialize())//检测不到SD卡
	{
		LCD_ShowString(60,150,200,16,16,(u8*)"SD Card Error!");
		delay_ms(500);					
		LCD_ShowString(60,150,200,16,16,(u8*)"Please Check! ");
		delay_ms(500);
		LED0=!LED0;//DS0闪烁
	}
 	POINT_COLOR=BLUE;//设置字体为蓝色
	LCD_ShowString(60,150,200,16,16,(u8*)"SD Card OK");
	LCD_ShowString(60,170,200,16,16,(u8*)"SD Sending...");
	while(1)
	{
		SD_ReadDisk_(sd_send_temp,cnt,1);
		LCD_ShowNum(60,190,cnt,5,16);//显示发送SD的扇区号
		
		LCD_ShowNum(60,210,sd_send_temp[0],5,16);
		USART_SendData(USART1,sd_send_temp[0]);
		delay_ms(10);
		
		LCD_ShowNum(60,230,sd_send_temp[1],5,16);
		USART_SendData(USART1,sd_send_temp[1]);
		delay_ms(10);
		
		LCD_ShowNum(60,250,sd_send_temp[2],5,16);
		USART_SendData(USART1,sd_send_temp[2]);
		
		cnt++;
		delay_ms(50);
	}
}
/******************只显示摄像头图像*********************/
void Display_mode()
{
	while(1)
	{	
		OV7725_camera_refresh_3();//更新显示
	}
}
/**************无线模式**************/
void Wifi_Send()
{
	while(atk_8266_send_cmd("AT","OK",20))//检查WIFI模块是否在线
	{
		atk_8266_quit_trans();//退出透传
		atk_8266_send_cmd("AT+CIPMODE=0","OK",200);  //关闭透传模式	
		LCD_ShowString(30,200,200,16,16,(u8*)"NO Module. Please Check");
	}
	while(atk_8266_send_cmd("ATE0","OK",20));//关闭回显
	LCD_Clear(WHITE);
	LCD_ShowString(30,200,200,16,16,(u8*)"Find Wifi Module       ");
	while(1)
	{
		
	}
}
/******************/
int main(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	delay_init();	    	 //延时函数初始化	  
  FSMC_SRAM_Init();		//初始化外部SRAM  
	//USART1_TX   GPIOA.9
	//USART1_RX	  GPIOA.10
	uart_init(9600);	 	//串口初始化为9600
	usart3_init(115200);		//初始化串口3 
 	usmart_dev.init(72);		//初始化USMART		
 	LED_Init();		  			//初始化与LED连接的硬件接口
	KEY_Init();					//初始化按键
	LCD_Init();			   		//初始化LCD  
	BEEP_Init();        		//蜂鸣器初始化	 
	W25QXX_Init();				//初始化W25Q128
 	//my_mem_init(SRAMIN);		//初始化内部内存池
	//exfuns_init();				//为fatfs相关变量申请内存  
	POINT_COLOR=RED;
	/********菜单显示程序******/
	menu_display();
	/**********初始化OV7725_OV7670*****************/
	while(1)
	{
		if(OV7725_Init()==0)
		{
			LCD_ShowString(30,190,200,16,16,"OV7725 Init OK");
			delay_ms(200);
			OV7725_Window_Set(320,240,0);//QVGA模式输出
			OV7725_CS=0;
			break;
		}
		else
		{
			LCD_ShowString(30,190,200,16,16,"OV7725_OV7670 Error!!");
			delay_ms(200);
			LCD_Fill(30,190,239,246,WHITE);
			delay_ms(200);
		}
	}
	
	
	TIM6_Int_Init(10000,7199);			//10Khz计数频率,1秒钟中断									  
	EXTI8_Init();						//使能外部中断8,捕获帧中断			
	LCD_Clear(WHITE);
	/**************菜单选择后进入主程序*************************/
	switch(mode)
	{
		case 0: start_mode(); break;
		case 1: SD_Save_mode(); break;
		case 2: Display_mode(); break;
		case 3: SD_Send_PIC_mode(); break;
		case 4: SD_Send_feature_mode(); break;
		case 5: Wifi_Send(); break;
		default: break;
	}
	
}

