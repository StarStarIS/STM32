#include "stm32f10x.h"
#include "delay.h"
#include "sys.h"
#include "led.h"
#include "key.h"
#include "buzzer.h"
#include "hcsr04.h"
#include "oled.h"

typedef enum
{
    MODE_IDLE = 0,
    MODE_REVERSE
} SystemMode_t;

static u8 GetAlarmLevel(u16 distance_cm)
{
    if((distance_cm == HCSR04_INVALID_DISTANCE) || (distance_cm > 100))
    {
        return 0;
    }
    else if(distance_cm > 50)
    {
        return 1;
    }
    else if(distance_cm > 20)
    {
        return 2;
    }
    else
    {
        return 3;
    }
}

/* 根据距离进行分级声光报警 */
static void ReverseAlarm_ByDistance(u16 distance_cm)
{
    /* 无效值或者距离较远：绿色灯亮，蜂鸣器不响 */
    if((distance_cm == HCSR04_INVALID_DISTANCE) || (distance_cm > 100))
    {
        LED_AllOff();
        LED_G_On();
        BUZZER_Off();
        delay_ms(80);
    }
    /* 50cm ~ 100cm：蓝灯亮，慢速间歇响 */
    else if(distance_cm > 50)
    {
        LED_AllOff();
        LED_B_On();

        BUZZER_On();
        delay_ms(60);
        BUZZER_Off();
        delay_ms(440);
    }
    /* 20cm ~ 50cm：红灯亮，快速间歇响 */
    else if(distance_cm > 20)
    {
        LED_AllOff();
        LED_R_On();

        BUZZER_On();
        delay_ms(80);
        BUZZER_Off();
        delay_ms(170);
    }
    /* <= 20cm：三色灯全亮，蜂鸣器连续响 */
    else
    {
        LED_AllOn();
        BUZZER_On();
        delay_ms(80);
    }
}

int main(void)
{
    u8 key_value = KEY_NONE;
    u16 distance_cm = HCSR04_INVALID_DISTANCE;
    SystemMode_t mode = MODE_IDLE;

    delay_init();

    LED_Init();
    KEY_Init();
    BUZZER_Init();
    HCSR04_Init();
    OLED_Init();

    LED_AllOff();
    BUZZER_Off();
    OLED_ShowStatus(HCSR04_INVALID_DISTANCE, 0);

    while(1)
    {
        key_value = KEY_Scan();

        /* KEY1：进入/退出倒车模式 */
        if(key_value == KEY1_PRESS)
        {
            if(mode == MODE_IDLE)
            {
                mode = MODE_REVERSE;
            }
            else
            {
                mode = MODE_IDLE;
            }

            LED_AllOff();
            BUZZER_Off();
        }

        switch(mode)
        {
            case MODE_IDLE:
                LED_AllOff();
                BUZZER_Off();
                OLED_ShowStatus(HCSR04_INVALID_DISTANCE, 0);
                delay_ms(50);
                break;

            case MODE_REVERSE:
                distance_cm = HCSR04_GetDistanceCm();
                OLED_ShowStatus(distance_cm, GetAlarmLevel(distance_cm));
                ReverseAlarm_ByDistance(distance_cm);
                break;

            default:
                mode = MODE_IDLE;
                LED_AllOff();
                BUZZER_Off();
                OLED_ShowStatus(HCSR04_INVALID_DISTANCE, 0);
                break;
        }
    }
}