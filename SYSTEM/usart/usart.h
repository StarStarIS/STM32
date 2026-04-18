#ifndef __USART_H
#define __USART_H

#include "stdio.h"
#include "sys.h"

#define USART_REC_LEN          200
#define EN_USART1_RX           1

#define USART2_REC_LEN         500
#define USART2_DEBUG_REC_LEN   1024
#define USART2_RING_LEN        512

extern u8  USART_RX_BUF[USART_REC_LEN];
extern u16 USART_RX_STA;

extern volatile u8  USART2_RX_BUF[USART2_REC_LEN];
extern volatile u16 USART2_RX_CNT;

void uart_init(u32 bound);
void uart2_init(u32 bound);
void uart4_init(u32 bound);

void USART2_SendByte(u8 ch);
void USART2_SendString(const char *str);
void USART2_ClearBuf(void);
u8   USART2_ReadByte(u8 *ch);
void USART2_DebugFlushToUSART1(void);

void UART4_SendByte(u8 ch);
void UART4_SendBytes(const u8 *data, u16 len);

void WiFi_EN_Init(void);
void WiFi_EN_On(void);
void WiFi_EN_Off(void);

#endif
