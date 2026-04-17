#include "sys.h"
#include "usart.h"

//////////////////////////////////////////////////////////////////////////////////
//如果使用ucos,则包括下面的头文件即可.
#if SYSTEM_SUPPORT_OS
#include "includes.h"					//ucos 使用
#endif
//////////////////////////////////////////////////////////////////////////////////
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//ALIENTEK STM32开发板
//串口1初始化
//正点原子@ALIENTEK
//技术论坛:www.openedv.com
//修改日期:2012/8/18
//版本：V1.5
//版权所有，盗版必究。
//Copyright(C) 广州市星翼电子科技有限公司 2009-2019
//All rights reserved
//********************************************************************************
//V1.3修改说明
//支持适应不同频率下的串口波特率设置.
//加入了对printf的支持
//增加了串口接收命令功能.
//修正了printf第一个字符丢失的bug
//V1.4修改说明
//1,修改串口初始化IO的bug
//2,修改了USART_RX_STA,使得串口最大接收字节数为2的14次方
//3,增加了USART_REC_LEN,用于定义串口最大允许接收的字节数(不大于2的14次方)
//4,修改了EN_USART1_RX的使能方式
//V1.5修改说明
//1,增加了对UCOSII的支持
//////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////
//加入以下代码,支持printf函数,而不需要选择use MicroLIB
#if 1
#pragma import(__use_no_semihosting)
//标准库需要的支持函数
struct __FILE
{
	int handle;
};

FILE __stdout;
//定义_sys_exit()以避免使用半主机模式
void _sys_exit(int x)
{
	x = x;
}
//重定义fputc函数
int fputc(int ch, FILE *f)
{
	while((USART1->SR & 0X40) == 0);   //循环发送,直到发送完毕
    USART1->DR = (u8)ch;
	return ch;
}
#endif


#if EN_USART1_RX   //如果使能了接收
//串口1中断服务程序
//注意,读取USARTx->SR能避免莫名其妙的错误
u8 USART_RX_BUF[USART_REC_LEN];     //USART1接收缓冲,最大USART_REC_LEN个字节.
//接收状态
//bit15，接收完成标志
//bit14，接收到0x0d
//bit13~0，接收到的有效字节数目
u16 USART_RX_STA = 0;               //USART1接收状态标记
#endif

//USART2（WiFi）接收缓冲
volatile u8 USART2_RX_BUF[USART2_REC_LEN];
volatile u16 USART2_RX_CNT = 0;


/************************************************
函数名：WiFi_EN_Init
功能  ：初始化WiFi模块使能脚 PB12
************************************************/
void WiFi_EN_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);  //使能GPIOB时钟

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;       //推挽输出
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_ResetBits(GPIOB, GPIO_Pin_12); //默认关闭WiFi模块
}

/************************************************
函数名：WiFi_EN_On
功能  ：拉高PB12，使能WiFi模块
************************************************/
void WiFi_EN_On(void)
{
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

/************************************************
函数名：WiFi_EN_Off
功能  ：拉低PB12，关闭WiFi模块
************************************************/
void WiFi_EN_Off(void)
{
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);
}

/************************************************
函数名：USART2_ClearBuf
功能  ：清空USART2接收缓冲
************************************************/
void USART2_ClearBuf(void)
{
	u16 i;
	USART2_RX_CNT = 0;
	for(i = 0; i < USART2_REC_LEN; i++)
	{
		USART2_RX_BUF[i] = 0;
	}
}

/************************************************
函数名：USART2_SendByte
功能  ：USART2发送1个字节
************************************************/
void USART2_SendByte(u8 ch)
{
	while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
	USART_SendData(USART2, ch);
}

/************************************************
函数名：USART2_SendString
功能  ：USART2发送字符串
************************************************/
void USART2_SendString(char *str)
{
	while(*str)
	{
		USART2_SendByte(*str++);
	}
}


/************************************************
函数名：uart_init
功能  ：USART1初始化（连接电脑串口助手、printf输出）
************************************************/
void uart_init(u32 bound)
{
	//GPIO端口设置
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);	//使能USART1，GPIOA时钟

	//USART1_TX   GPIOA.9
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; //PA.9
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;	//复用推挽输出
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIOA.9

	//USART1_RX	  GPIOA.10初始化
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;//PA10
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
	GPIO_Init(GPIOA, &GPIO_InitStructure);//初始化GPIOA.10

	//Usart1 NVIC 配置
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3; //抢占优先级3
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;        //子优先级3
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;           //IRQ通道使能
	NVIC_Init(&NVIC_InitStructure);                           //根据指定的参数初始化VIC寄存器

	//USART1 初始化设置
	USART_InitStructure.USART_BaudRate = bound;                                     //串口波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;                     //字长为8位数据格式
	USART_InitStructure.USART_StopBits = USART_StopBits_1;                          //一个停止位
	USART_InitStructure.USART_Parity = USART_Parity_No;                             //无奇偶校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None; //无硬件数据流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;                 //收发模式

	USART_Init(USART1, &USART_InitStructure);                    //初始化串口1
	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);              //开启串口1接收中断
	USART_Cmd(USART1, ENABLE);                                  //使能串口1
}


/************************************************
函数名：uart2_init
功能  ：USART2初始化（连接WiFi模块）
说明  ：PA2 -> USART2_TX
        PA3 -> USART2_RX
************************************************/
void uart2_init(u32 bound)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE); //GPIOA、AFIO时钟
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);                       //USART2时钟

	//USART2_TX   GPIOA.2
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;      //复用推挽输出
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//USART2_RX   GPIOA.3
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;//浮空输入
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	//USART2 NVIC配置
	NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	//USART2 初始化设置
	USART_InitStructure.USART_BaudRate = bound;
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	USART_InitStructure.USART_Parity = USART_Parity_No;
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

	USART_Init(USART2, &USART_InitStructure);               //初始化串口2
	USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);         //开启串口2接收中断
	USART_Cmd(USART2, ENABLE);                             //使能串口2

	USART2_ClearBuf();
}


#if EN_USART1_RX
/************************************************
函数名：USART1_IRQHandler
功能  ：串口1中断服务程序
************************************************/
void USART1_IRQHandler(void)                 //串口1中断服务程序
{
	u8 Res;
#if SYSTEM_SUPPORT_OS 		//如果SYSTEM_SUPPORT_OS为真，则需要支持OS.
	OSIntEnter();
#endif
	if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)  //接收中断(接收到的数据必须是0x0d 0x0a结尾)
	{
		Res = USART_ReceiveData(USART1);	//读取接收到的数据

		if((USART_RX_STA & 0x8000) == 0)   //接收未完成
		{
			if(USART_RX_STA & 0x4000)      //接收到了0x0d
			{
				if(Res != 0x0a) USART_RX_STA = 0;   //接收错误,重新开始
				else USART_RX_STA |= 0x8000;        //接收完成了
			}
			else //还没收到0X0D
			{
				if(Res == 0x0d) USART_RX_STA |= 0x4000;
				else
				{
					USART_RX_BUF[USART_RX_STA & 0X3FFF] = Res;
					USART_RX_STA++;
					if(USART_RX_STA > (USART_REC_LEN - 1)) USART_RX_STA = 0; //接收数据错误,重新开始接收
				}
			}
		}
	}
#if SYSTEM_SUPPORT_OS 	//如果SYSTEM_SUPPORT_OS为真，则需要支持OS.
	OSIntExit();
#endif
}
#endif


/************************************************
函数名：USART2_IRQHandler
功能  ：串口2中断服务程序（WiFi模块）
说明  ：收到的数据自动转发到USART1，方便电脑串口观察
************************************************/
void USART2_IRQHandler(void)
{
	u8 Res;

	if(USART_GetITStatus(USART2, USART_IT_RXNE) != RESET)
	{
		Res = USART_ReceiveData(USART2);   //读取WiFi模块返回数据

		if(USART2_RX_CNT < (USART2_REC_LEN - 1))
		{
			USART2_RX_BUF[USART2_RX_CNT++] = Res;
			USART2_RX_BUF[USART2_RX_CNT] = 0;   //字符串结尾，方便strstr查找
		}
		else
		{
			USART2_RX_CNT = 0; //防止越界，重新开始
		}

		//把WiFi模块返回的数据转发到USART1，方便在电脑串口助手查看
		while((USART1->SR & 0X40) == 0);
		USART1->DR = Res;
	}
}