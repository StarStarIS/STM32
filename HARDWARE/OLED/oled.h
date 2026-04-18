#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowStatus(u16 distance_cm, u8 level);
void OLED_ShowLightStatus(u16 light_raw, u8 level);
void OLED_ShowSecurityStatus(u16 distance_cm, u8 is_night, u8 alarm_state, u8 wifi_connected, u8 silenced);

#endif
