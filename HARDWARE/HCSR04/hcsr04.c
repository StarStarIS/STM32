#include "hcsr04.h"
#include "delay.h"

/* 引脚定义：按你的板子实际连接 */
#define HCSR04_TRIG_GPIO      GPIOB
#define HCSR04_TRIG_PIN       GPIO_Pin_15

#define HCSR04_ECHO_GPIO      GPIOB
#define HCSR04_ECHO_PIN       GPIO_Pin_14

/* 超时上限，单位 us */
#define HCSR04_TIMEOUT_US     30000

static void HCSR04_SendTrig(void)
{
    GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
    delay_us(2);

    GPIO_SetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
    delay_us(20);   /* >10us */
    GPIO_ResetBits(HCSR04_TRIG_GPIO, HCSR04_TRIG_PIN);
}

void HCSR04_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

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
}

/* 返回 ECHO 高电平持续时间，单位 us；失败返回 0 */
u32 HCSR04_GetEchoTimeUs(void)
{
    u32 timeout = 0;
    u32 echo_time = 0;

    HCSR04_SendTrig();

    /* 等待 ECHO 变高 */
    while(GPIO_ReadInputDataBit(HCSR04_ECHO_GPIO, HCSR04_ECHO_PIN) == Bit_RESET)
    {
        timeout++;
        delay_us(1);

        if(timeout >= HCSR04_TIMEOUT_US)
        {
            return 0;
        }
    }

    /* 统计 ECHO 高电平时长 */
    while(GPIO_ReadInputDataBit(HCSR04_ECHO_GPIO, HCSR04_ECHO_PIN) == Bit_SET)
    {
        echo_time++;
        delay_us(1);

        if(echo_time >= HCSR04_TIMEOUT_US)
        {
            return 0;
        }
    }

    return echo_time;
}

/* 返回距离，单位 cm；失败返回 HCSR04_INVALID_DISTANCE */
u16 HCSR04_GetDistanceCm(void)
{
    u32 echo_time_us = 0;

    echo_time_us = HCSR04_GetEchoTimeUs();
    if(echo_time_us == 0)
    {
        return HCSR04_INVALID_DISTANCE;
    }

    return (u16)(echo_time_us / 58);
}

