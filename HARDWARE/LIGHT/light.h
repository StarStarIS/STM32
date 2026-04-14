#ifndef __LIGHT_H
#define __LIGHT_H

#include "stm32f10x.h"

void LIGHT_Init(void);
u16  LIGHT_ReadRaw(void);
u16  LIGHT_ReadAverage(u8 times);

#endif