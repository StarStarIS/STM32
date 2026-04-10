#include "stm32f10x.h"
#include "delay.h"
#include "sys.h"
#include "led.h"
#include "key.h"
#include "buzzer.h"

typedef enum
{
    MODE_IDLE = 0,
    MODE_LED_RUN,
    MODE_BUZZER_ALARM
}SystemMode_t;

int main(void)
{
    u8 key_value = KEY_NONE;
    SystemMode_t mode = MODE_IDLE;

    delay_init();

    LED_Init();
    KEY_Init();
    BUZZER_Init();

    LED_AllOff();
    BUZZER_Off();

    while(1)
    {
        key_value = KEY_Scan();

        if(key_value == KEY1_PRESS)
        {
            mode = MODE_LED_RUN;
            BUZZER_Off();
        }
        else if(key_value == KEY2_PRESS)
        {
            mode = MODE_BUZZER_ALARM;
            LED_AllOff();
        }

        switch(mode)
        {
            case MODE_IDLE:
                LED_AllOff();
                BUZZER_Off();
                break;

            case MODE_LED_RUN:
                BUZZER_Off();
                LED_RunStep();
                delay_ms(200);
                break;

            case MODE_BUZZER_ALARM:
                LED_AllOff();

                /* 滴滴叫声 */
                BUZZER_On();
                delay_ms(100);
                BUZZER_Off();
                delay_ms(100);

                BUZZER_On();
                delay_ms(100);
                BUZZER_Off();
                delay_ms(250);
                break;

            default:
                mode = MODE_IDLE;
                break;
        }
    }
}