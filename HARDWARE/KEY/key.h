#ifndef __KEY_H
#define __KEY_H

#include "stm32f10x.h"

#define KEY1_PIN    GPIO_Pin_6
#define KEY2_PIN    GPIO_Pin_7
#define KEY_GPIO    GPIOB

#define KEY_NONE        0
#define KEY1_PRESS      1
#define KEY2_PRESS      2

void KEY_Init(void);
u8 KEY_Scan(void);

#endif