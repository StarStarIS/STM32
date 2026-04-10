#include "buzzer.h"

/*
 * 这里默认蜂鸣器为高电平响。
 * 如果上板后发现逻辑反了，只需要交换 On/Off 里的 SetBits 和 ResetBits。
 */
void BUZZER_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    /* PA15 默认复用为 JTAG，需关闭 JTAG，保留 SWD */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(BUZZER_GPIO, &GPIO_InitStructure);

    BUZZER_Off();
}

void BUZZER_On(void)
{
    GPIO_ResetBits(BUZZER_GPIO, BUZZER_PIN);
}

void BUZZER_Off(void)
{
    GPIO_SetBits(BUZZER_GPIO, BUZZER_PIN);
}