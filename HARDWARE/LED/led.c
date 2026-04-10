#include "led.h"

/* 本板LED按原理图判断为低电平点亮 */
static void LED_Write(uint16_t pin, FunctionalState state)
{
    if(state == ENABLE)
    {
        GPIO_ResetBits(LED_GPIO, pin);   // 低电平亮
    }
    else
    {
        GPIO_SetBits(LED_GPIO, pin);     // 高电平灭
    }
}

void LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    GPIO_InitStructure.GPIO_Pin = LED_ALL_PINS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(LED_GPIO, &GPIO_InitStructure);

    LED_AllOff();
}

void LED_R_On(void)  { LED_Write(LED_R_PIN, ENABLE); }
void LED_R_Off(void) { LED_Write(LED_R_PIN, DISABLE); }

void LED_G_On(void)  { LED_Write(LED_G_PIN, ENABLE); }
void LED_G_Off(void) { LED_Write(LED_G_PIN, DISABLE); }

void LED_B_On(void)  { LED_Write(LED_B_PIN, ENABLE); }
void LED_B_Off(void) { LED_Write(LED_B_PIN, DISABLE); }

void LED_AllOff(void)
{
    GPIO_SetBits(LED_GPIO, LED_ALL_PINS);
}

void LED_AllOn(void)
{
    GPIO_ResetBits(LED_GPIO, LED_ALL_PINS);
}

void LED_RunStep(void)
{
    static u8 step = 0;

    LED_AllOff();

    switch(step)
    {
        case 0: LED_R_On(); break;
        case 1: LED_G_On(); break;
        case 2: LED_B_On(); break;
        default: break;
    }

    step++;
    if(step >= 3)
    {
        step = 0;
    }
}