#ifndef __LED_H
#define __LED_H

#include "stm32f10x.h"

#define LED_R_PIN   GPIO_Pin_13
#define LED_G_PIN   GPIO_Pin_14
#define LED_B_PIN   GPIO_Pin_15
#define LED_GPIO    GPIOC

#define LED_ALL_PINS   (LED_R_PIN | LED_G_PIN | LED_B_PIN)

void LED_Init(void);
void LED_R_On(void);
void LED_R_Off(void);
void LED_G_On(void);
void LED_G_Off(void);
void LED_B_On(void);
void LED_B_Off(void);
void LED_AllOff(void);
void LED_AllOn(void);
void LED_RunStep(void);

#endif