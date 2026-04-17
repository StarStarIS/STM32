#include "stm32f10x.h"
#include "delay.h"
#include "sys.h"
#include "led.h"
#include "key.h"
#include "buzzer.h"
#include "hcsr04.h"
#include "oled.h"
#include "light.h"
#include "usart.h"
#include "dht11.h"
#include <string.h>

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

static u8 GetLightLevel(u16 light_raw)
{
    /*
     * 先给一组“能跑起来”的初始阈值：
     * ADC大 -> 更暗
     * ADC小 -> 更亮
     *
     * 一级：暗   -> RLED
     * 二级：中   -> GLED
     * 三级：亮   -> BLED
     *
     * 后面你上板后我们再按实测值微调。
     */
    if(light_raw > 750)
    {
        return 1;
    }
    else if(light_raw >= 400)
    {
        return 2;
    }
    else
    {
        return 3;
    }
}

static void LightLevel_Indicate(u8 level)
{
    LED_AllOff();
    BUZZER_Off();

    switch(level)
    {
        case 1:
            LED_R_On();
            break;

        case 2:
            LED_G_On();
            break;

        case 3:
            LED_B_On();
            break;

        default:
            break;
    }
}

static void WiFi_SendCmd(char *cmd)
{
    USART2_ClearBuf();
    printf("\r\n[STM32->WiFi] %s", cmd);   // 打印到电脑串口看
    USART2_SendString(cmd);
}

static u8 WiFi_WaitFor(char *keyword, u32 timeout_ms)
{
    u32 t = 0;

    while(t < timeout_ms)
    {
        if(strstr((char *)USART2_RX_BUF, keyword) != NULL)
        {
            return 1;
        }
        delay_ms(10);
        t += 10;
    }
    return 0;
}

static u8 WiFi_Init_And_Send12345(void)
{
    printf("\r\n===== WiFi Start =====\r\n");

    WiFi_EN_Init();
    WiFi_EN_Off();
    delay_ms(200);
    WiFi_EN_On();           // PB12拉高，WiFi开始工作
    delay_ms(2000);

    // 1. 试探AT
    WiFi_SendCmd("AT\r\n");
    if(!WiFi_WaitFor("OK", 1000))
    {
        printf("\r\nAT no response!\r\n");
        return 0;
    }
    printf("\r\nAT OK\r\n");

    // 2. 设为 STA 模式
    WiFi_SendCmd("AT+WMODE=1,1\r\n");
    if(!WiFi_WaitFor("OK", 1000))
    {
        printf("\r\nWMODE failed!\r\n");
        return 0;
    }
    printf("\r\nWMODE OK\r\n");

    // 3. 连接路由器
    // 这里把你的WiFi名和密码改掉
    WiFi_SendCmd("AT+WJAP=\"StarIS Xiaomi 17 Pro\",\"1234abcd\"\r\n");
    if(!(WiFi_WaitFor("WIFI_GOT_IP", 15000) || WiFi_WaitFor("OK", 15000)))
    {
        printf("\r\nWIFI connect failed!\r\n");
        return 0;
    }
    printf("\r\nWiFi connected\r\n");

    // 4. 创建 TCP Client
    // 这里把 IP 改成你电脑的 IPv4 地址
    WiFi_SendCmd("AT+SOCKET=4,10.160.155.95,9080,0,1\r\n");
    if(!(WiFi_WaitFor("connect success", 5000) || WiFi_WaitFor("OK", 5000)))
    {
        printf("\r\nSOCKET create failed!\r\n");
        return 0;
    }
    printf("\r\nSOCKET connected\r\n");

    // 5. 发送 12345
    WiFi_SendCmd("AT+SOCKETSENDLINE=1,5,12345\r\n");
    if(!WiFi_WaitFor("OK", 3000))
    {
        printf("\r\nSend 12345 failed!\r\n");
        return 0;
    }

    printf("\r\nSend 12345 success!\r\n");
    printf("\r\n===== WiFi Done =====\r\n");
    return 1;
}

int main(void)
{
    u8 key_value = KEY_NONE;
    u8 temperature = 0;
    u8 humidity = 0;
    u8 dht11_send_div = 0;
    u16 distance_cm = HCSR04_INVALID_DISTANCE;
    u16 light_raw = 0;
    u8 light_level = 0;
    SystemMode_t mode = MODE_IDLE;

    delay_init();

    LED_Init();
    KEY_Init();
    BUZZER_Init();
    HCSR04_Init();
    OLED_Init();
    LIGHT_Init();
    uart_init(115200);
    uart2_init(115200);
    delay_ms(500);
    WiFi_Init_And_Send12345();
    DHT11_Init();

    LED_AllOff();
    BUZZER_Off();
    OLED_ShowLightStatus(0, 0);

    delay_ms(1200);
    printf("System Ready\r\n");

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
                /* 非倒车模式下，做环境光检测 */
                light_raw = LIGHT_ReadAverage(8);
                light_level = GetLightLevel(light_raw);

                LightLevel_Indicate(light_level);
                OLED_ShowLightStatus(light_raw, light_level);

                dht11_send_div++;
                if(dht11_send_div >= 10)
                {
                    dht11_send_div = 0;
                    
                    if(DHT11_ReadData(&temperature, &humidity) == DHT11_OK)
                    {
                        printf("Temp: %dC, Humi: %d%%\r\n", temperature, humidity);
                    }
                    else
                    {
                        printf("DHT11 read fail\r\n");
                    }
                }

                delay_ms(100);
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