#ifndef __BUZZER_H
#define __BUZZER_H

#include "stm32f10x.h"

#define BUZZER_PIN   GPIO_Pin_15
#define BUZZER_GPIO  GPIOA

void BUZZER_Init(void);
void BUZZER_On(void);
void BUZZER_Off(void);

#endif