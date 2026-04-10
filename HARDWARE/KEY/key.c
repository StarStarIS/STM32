#include "key.h"
#include "delay.h"

void KEY_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = KEY1_PIN | KEY2_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   // 上拉输入
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(KEY_GPIO, &GPIO_InitStructure);
}

/* 按键按下返回一次，松手后才允许下一次触发 */
u8 KEY_Scan(void)
{
    if(GPIO_ReadInputDataBit(KEY_GPIO, KEY1_PIN) == 0)
    {
        delay_ms(10);
        if(GPIO_ReadInputDataBit(KEY_GPIO, KEY1_PIN) == 0)
        {
            while(GPIO_ReadInputDataBit(KEY_GPIO, KEY1_PIN) == 0);
            return KEY1_PRESS;
        }
    }

    if(GPIO_ReadInputDataBit(KEY_GPIO, KEY2_PIN) == 0)
    {
        delay_ms(10);
        if(GPIO_ReadInputDataBit(KEY_GPIO, KEY2_PIN) == 0)
        {
            while(GPIO_ReadInputDataBit(KEY_GPIO, KEY2_PIN) == 0);
            return KEY2_PRESS;
        }
    }

    return KEY_NONE;
}