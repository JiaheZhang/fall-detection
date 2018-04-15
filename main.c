#include "sys.h"
#include "delay.h"
#include "usart.h" 
#include "led.h" 		 	 
#include "lcd.h"  
#include "key.h"     
#include "usmart.h" 
#include "malloc.h"
#include "sdio_sdcard.h"  
#include "w25qxx.h"    
#include "ff.h"  
#include "exfuns.h"   
#include "text.h"
#include "piclib.h"	
#include "string.h"		
#include "math.h"	 
#include "ov7670.h"
#include "beep.h" 
#include "timer.h" 
#include "exti.h"
#include "ov7725.h"
#include "sram.h" 


#define  OV7725 1
#define  OV7670 2
#define key_down PEin(3)
#define key_left PEin(2)
#define key_up PAin(0)

extern u8 ov_sta;	//在exit.c里面定义
extern u8 ov_frame;	//在timer.c里面定义

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
u8 flag_reflash = 0;
float aspectRatio = 0;//宽高比
float inclination = 0;//倾斜角度
u8 isFall = 0;//是否摔倒
u8 isPerson = 0;//是否有人

u32 N = 0;//图像的和(N)
u32 sum_pix;//计算和的临时变量
u16 Ex;//均值
u16 Ey;
float cov[2][2];//协方差矩阵
float tanTable[90] = {0.009,0.026,0.044,0.061,0.079,0.096,0.114,
0.132,0.149,0.167,0.185,0.203,0.222,0.240,0.259,0.277,0.296,0.315,
0.335,0.354,0.374,0.394,0.414,0.435,0.456,0.477,0.499,0.521,0.543,
0.566,0.589,0.613,0.637,0.662,0.687,0.713,0.740,0.767,0.795,0.824,
0.854,0.885,0.916,0.949,0.983,1.018,1.054,1.091,1.130,1.171,1.213,
1.257,1.303,1.351,1.402,1.455,1.511,1.570,1.632,1.698,1.767,1.842,
1.921,2.006,2.097,2.194,2.300,2.414,2.539,2.675,2.824,2.989,3.172,
3.376,3.606,3.867,4.165,4.511,4.915,5.396,5.976,6.691,7.596,8.777,
10.385,12.706,16.350,22.904,38.188,114.589};
//用于my_tan()中查表计算arctan
	
	
	
/***********以下是储存在片外RAM的变量***************/
u8 pixel_back[240][320][3] __attribute__((at(0X680ff000)));//储存背景图片的所有像素点
u8 pixel[240][320][3]  __attribute__((at(0X68137400)));
u8 pixel_gray[240][320] __attribute__((at(0X6816f800)));//灰度图片
//u32 testsram[250000] __attribute__((at(0X68100000)));//测试用数组


u8 my_abs(int a )
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
u8 my_arctan(float value)
{
    if(value < 0.009)  return 0;//边界值
    if(value > 115)  return 90;
    int left = 0,right = 89;
    return get_value(left,right,value);
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
				if(pixel_gray[i][j] == 1)
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
		ov_frame++; 
		LCD_Scan_Dir(DFT_SCAN_DIR);	//恢复默认扫描方向 
			
	}
}
/*******************************************
图像处理程序
********************************************/
void image_process()
{
	u16 i,j;
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
				temp_gray = (temp_r + (temp_g >> 1) + temp_b);
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
		
		if(pix_x[i] >= 20 && pix_x[i + 1] >= 20)
		{
			x1 = i;
			break;
		}
		else
			x1 = 0;
			
	}
	for(i = 239;i > 0;i--)
	{
		if(pix_x[i] >= 20 && pix_x[i - 1] >= 20)
		{
			x2 = i;
			break;
		}
		else
			x2 = 0;
	}
	
	for(i = 0;i < 319;i++)
	{
		if(pix_y[i] >= 15 && pix_y[i + 1] >= 15)
		{
			y1 = i;
			break;
		}
		else
			y1 = 0;
	}
	for(i = 319;i > 0;i--)
	{
		
		if(pix_y[i] >= 15 && pix_y[i - 1] >= 15)
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
		aspectRatio = (x2 - x1) / (y2 - y1);
	}
	else
		aspectRatio = 0;
	
	/***********协方差算姿态角***********/
	if(isPerson == 1)
	{
		for(i = x1;i <= x2;i++)//算出个数N
		{
			N += pix_x[i];
		}
		
		/***********计算均值 Ex Ey******/
		for(i = x1;i <= x2;i++)
		{
			sum_pix += pix_x[i] * (i - x1);
		}
		Ex = x1 + sum_pix / N;
		
		sum_pix = 0;
		for(i = y1;i <= y2;i++)
		{
			sum_pix += pix_y[i] * (i - y1);
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
			sum_pix += (Ex - i) * (Ex - i) * pix_x[i];
		}
		cov[0][0] = sum_pix / (N - 1);
		
		sum_pix = 0;
		for(i = y1;i <= y2;i++)
		{
			sum_pix += (Ey - i) * (Ey - i) * pix_y[i];
		}
		cov[1][1] = sum_pix / (N - 1);
		
		sum_pix = 0;
		for(i = x1;i <= x2;i++)
		{
			for(j = y1;j <= y2;j++)
			{
				if(pixel_gray[i][j] != 0 )
				{
					sum_pix += (Ex - i) * (Ey - j);
				}
			}
		}
		cov[0][1] = sum_pix / (N - 1);
		cov[1][0] = cov[0][1];
		
	}
	
	if(aspectRatio >= 1.5)
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



int main(void)
{	 
	u8 key;		 
 	u8 i=0;	     
 
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);//设置中断优先级分组为组2：2位抢占优先级，2位响应优先级
	delay_init();	    	 //延时函数初始化	  
  FSMC_SRAM_Init();		//初始化外部SRAM  
	uart_init(115200);	 	//串口初始化为115200
 	usmart_dev.init(72);		//初始化USMART		
 	LED_Init();		  			//初始化与LED连接的硬件接口
	KEY_Init();					//初始化按键
	LCD_Init();			   		//初始化LCD  
	BEEP_Init();        		//蜂鸣器初始化	 
	W25QXX_Init();				//初始化W25Q128
 	my_mem_init(SRAMIN);		//初始化内部内存池
	exfuns_init();				//为fatfs相关变量申请内存  
	POINT_COLOR=RED;      
											  
	while(1)//初始化OV7725_OV7670
	{
		if(OV7725_Init()==0)
		{
			LCD_ShowString(30,190,200,16,16,"OV7725 Init OK       ");
			while(1)
			{
				key=KEY_Scan(0);
				if(key==KEY0_PRES)
				{
					OV7725_Window_Set(320,240,0);//QVGA模式输出
					break;
				}
				i++;
				LCD_ShowString(30,206,200,16,16,"press KEY0"); //闪烁显示提示信息
				if(i==200)
				{	
					LCD_Fill(30,206,200,250+16,WHITE);
					i=0; 
				}
				delay_ms(5);
			}				
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
	
	/***************************************/
	TIM6_Int_Init(10000,7199);			//10Khz计数频率,1秒钟中断									  
	EXTI8_Init();						//使能外部中断8,捕获帧中断			
	LCD_Clear(BLACK);
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

