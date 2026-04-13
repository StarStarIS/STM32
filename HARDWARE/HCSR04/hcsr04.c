#include "hcsr04.h"
#include "delay.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_tim.h"
#include "misc.h"

/* 板上实际连接 */
#define HCSR04_TRIG_GPIO      GPIOB
#define HCSR04_TRIG_PIN       GPIO_Pin_15

#define HCSR04_ECHO_GPIO      GPIOB
#define HCSR04_ECHO_PIN       GPIO_Pin_14

/* 超时上限：30ms，对应约5m以上，足够覆盖HC-SR04有效范围 */
#define HCSR04_TIMEOUT_US     30000

volatile static u8  g_hcsr04_busy = 0;
volatile static u8  g_hcsr04_done = 0;
volatile static u32 g_hcsr04_echo_us = 0;

static void HCSR04_SendTrig(void)
{
    GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
    delay_us(2);

    GPIO_SetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
    delay_us(15);     /* >10us */
    GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
}

void HCSR04_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;

    /* 时钟使能 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    /* PB15 -> TRIG，推挽输出 */
    GPIO_InitStructure.GPIO_Pin = HCSR04_TRIG_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HCSR04_TRIG_GPIO, &GPIO_InitStructure);

    /* PB14 -> ECHO，浮空输入 */
    GPIO_InitStructure.GPIO_Pin = HCSR04_ECHO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(HCSR04_ECHO_GPIO, &GPIO_InitStructure);

    GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);

    /* TIM2 设成 1MHz 计数（1计数=1us）
       这里按你当前常见 STM32F103 标准工程 72MHz 主频配置，
       TIM2 时钟通常也是 72MHz，因此 PSC=71。 */
    TIM_TimeBaseStructure.TIM_Period = 0xFFFF;
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_SetCounter(TIM2, 0);
    TIM_Cmd(TIM2, ENABLE);

    /* PB14 -> EXTI14 */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource14);

    EXTI_InitStructure.EXTI_Line = EXTI_Line14;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    /* EXTI15_10 中断 */
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

/* 返回 ECHO 高电平时长，单位 us；失败返回 0 */
u32 HCSR04_GetEchoTimeUs(void)
{
    u32 timeout = 0;

    g_hcsr04_busy = 1;
    g_hcsr04_done = 0;
    g_hcsr04_echo_us = 0;

    HCSR04_SendTrig();

    while(g_hcsr04_done == 0)
    {
        delay_us(1);
        timeout++;

        if(timeout >= HCSR04_TIMEOUT_US)
        {
            g_hcsr04_busy = 0;
            return 0;
        }
    }

    return g_hcsr04_echo_us;
}

u16 HCSR04_GetDistanceCm(void)
{
    u32 echo_time_us = 0;

    echo_time_us = HCSR04_GetEchoTimeUs();
    if(echo_time_us == 0)
    {
        return HCSR04_INVALID_DISTANCE;
    }

    /* 有效范围粗判，避免异常值直接参与显示/报警 */
    if((echo_time_us < 116) || (echo_time_us > 23200))
    {
        return HCSR04_INVALID_DISTANCE;
    }

    return (u16)(echo_time_us / 58);
}

/* PB14 的 EXTI 中断服务函数 */
void EXTI15_10_IRQHandler(void)
{
    if(EXTI_GetITStatus(EXTI_Line14) != RESET)
    {
        /* 只在一次有效测量期间响应 */
        if(g_hcsr04_busy)
        {
            /* 上升沿：ECHO 变高，计时清零开始 */
            if(GPIO_ReadInputDataBit(HCSR04_ECHO_GPIO, HCSR04_ECHO_PIN) == Bit_SET)
            {
                TIM_SetCounter(TIM2, 0);
            }
            /* 下降沿：ECHO 变低，读取高电平宽度 */
            else
            {
                g_hcsr04_echo_us = TIM_GetCounter(TIM2);
                g_hcsr04_done = 1;
                g_hcsr04_busy = 0;
            }
        }

        EXTI_ClearITPendingBit(EXTI_Line14);
    }
}