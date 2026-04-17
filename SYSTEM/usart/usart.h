#ifndef __USART_H
#define __USART_H

#include "stdio.h"
#include "sys.h"

//////////////////////////////////////////////////////////////////////////////////
// 本程序只供学习使用，未经作者许可，不得用于其它任何用途
// ALIENTEK STM32开发板
// 串口1初始化
// 正点原子@ALIENTEK
// 技术论坛:www.openedv.com
// 修改日期:2012/8/18
// 版本：V1.5
// 版权所有，盗版必究
// Copyright(C) 广州市星翼电子科技有限公司 2009-2019
// All rights reserved
//////////////////////////////////////////////////////////////////////////////////

// V1.3修改说明
// 支持不同频率下的串口波特率设置
// 加入了对printf的支持
// 加入了串口接收命令功能
// 修正了printf第一个字符丢失的bug
// V1.4修改说明
// 1,修改串口初始化IO的bug
// 2,修改了USART_RX_STA，使串口最大接收字节数为2^14
// 3,修改了USART_REC_LEN，用于定义串口最大接收字节数(不能大于2^14)
// 4,修改了EN_USART1_RX的使能方式
// V1.5修改说明
// 1,增加了对UCOSII的支持

#define USART_REC_LEN          200     // USART1最大接收字节数
#define EN_USART1_RX           1       // 使能USART1接收

#define USART2_REC_LEN         500     // USART2最大接收字节数（给WiFi模块）

extern u8  USART_RX_BUF[USART_REC_LEN];    // USART1接收缓冲
extern u16 USART_RX_STA;                   // USART1接收状态

extern volatile u8  USART2_RX_BUF[USART2_REC_LEN]; // USART2接收缓冲
extern volatile u16 USART2_RX_CNT;                // USART2接收计数

// 如果使能了串口中断接收，请不要注释以下声明
void uart_init(u32 bound);      // USART1：连接电脑串口助手/printf
void uart2_init(u32 bound);     // USART2：连接WiFi模块

void USART2_SendByte(u8 ch);
void USART2_SendString(char *str);
void USART2_ClearBuf(void);

void WiFi_EN_Init(void);        // PB12
void WiFi_EN_On(void);
void WiFi_EN_Off(void);

#endif