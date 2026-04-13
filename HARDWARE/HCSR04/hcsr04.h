#ifndef __HCSR04_H
#define __HCSR04_H

#include "stm32f10x.h"

#define HCSR04_INVALID_DISTANCE  0xFFFF

void HCSR04_Init(void);
u32  HCSR04_GetEchoTimeUs(void);
u16  HCSR04_GetDistanceCm(void);


#endif