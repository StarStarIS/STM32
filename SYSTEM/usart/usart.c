#include "sys.h"
#include "usart.h"

#if SYSTEM_SUPPORT_OS
#include "includes.h"
#endif

#if 1
#pragma import(__use_no_semihosting)
struct __FILE
{
    int handle;
};

FILE __stdout;

void _sys_exit(int x)
{
    x = x;
}

int fputc(int ch, FILE *f)
{
    while((USART1->SR & USART_SR_TXE) == 0);
    USART1->DR = (u8)ch;
    return ch;
}
#endif

#if EN_USART1_RX
u8 USART_RX_BUF[USART_REC_LEN];
u16 USART_RX_STA = 0;
#endif

volatile u8 USART2_RX_BUF[USART2_REC_LEN];
volatile u16 USART2_RX_CNT = 0;

static volatile u8 USART2_DEBUG_BUF[USART2_DEBUG_REC_LEN];
static volatile u16 USART2_DEBUG_WR = 0;
static volatile u16 USART2_DEBUG_RD = 0;

static void USART2_DebugPushByte(u8 ch)
{
    u16 next = USART2_DEBUG_WR + 1;

    if(next >= USART2_DEBUG_REC_LEN)
    {
        next = 0;
    }

    if(next != USART2_DEBUG_RD)
    {
        USART2_DEBUG_BUF[USART2_DEBUG_WR] = ch;
        USART2_DEBUG_WR = next;
    }
}

void USART2_DebugFlushToUSART1(void)
{
    while(USART2_DEBUG_RD != USART2_DEBUG_WR)
    {
        if((USART1->SR & USART_SR_TXE) == 0)
        {
            break;
        }

        USART1->DR = USART2_DEBUG_BUF[USART2_DEBUG_RD];
        USART2_DEBUG_RD++;
        if(USART2_DEBUG_RD >= USART2_DEBUG_REC_LEN)
        {
            USART2_DEBUG_RD = 0;
        }
    }
}

void WiFi_EN_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
}

void WiFi_EN_On(void)
{
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

void WiFi_EN_Off(void)
{
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
}

void USART2_ClearBuf(void)
{
    u16 i;

    USART2_RX_CNT = 0;
    for(i = 0; i < USART2_REC_LEN; i++)
    {
        USART2_RX_BUF[i] = 0;
    }
}

void USART2_SendByte(u8 ch)
{
    while(USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, ch);
}

void USART2_SendString(char *str)
{
    while(*str)
    {
        USART2_SendByte(*str++);
    }
}

void uart_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

void uart2_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART2, &USART_InitStructure);
    USART_ITConfig(USART2, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART2, ENABLE);

    USART2_ClearBuf();
    USART2_DEBUG_WR = 0;
    USART2_DEBUG_RD = 0;
}

#if EN_USART1_RX
void USART1_IRQHandler(void)
{
    u8 Res;

#if SYSTEM_SUPPORT_OS
    OSIntEnter();
#endif

    if((USART1->SR & USART_SR_RXNE) != 0)
    {
        Res = (u8)USART1->DR;

        if((USART_RX_STA & 0x8000) == 0)
        {
            if(USART_RX_STA & 0x4000)
            {
                if(Res != 0x0a)
                {
                    USART_RX_STA = 0;
                }
                else
                {
                    USART_RX_STA |= 0x8000;
                }
            }
            else
            {
                if(Res == 0x0d)
                {
                    USART_RX_STA |= 0x4000;
                }
                else
                {
                    USART_RX_BUF[USART_RX_STA & 0x3FFF] = Res;
                    USART_RX_STA++;
                    if(USART_RX_STA > (USART_REC_LEN - 1))
                    {
                        USART_RX_STA = 0;
                    }
                }
            }
        }
    }

#if SYSTEM_SUPPORT_OS
    OSIntExit();
#endif
}
#endif

void USART2_IRQHandler(void)
{
    u16 status;
    u8 Res;

    status = USART2->SR;
    if((status & (USART_SR_RXNE | USART_SR_ORE)) != 0)
    {
        Res = (u8)USART2->DR;

        if(USART2_RX_CNT < (USART2_REC_LEN - 1))
        {
            USART2_RX_BUF[USART2_RX_CNT++] = Res;
            USART2_RX_BUF[USART2_RX_CNT] = 0;
        }
        else
        {
            USART2_RX_CNT = 0;
        }

        USART2_DebugPushByte(Res);
    }
}
