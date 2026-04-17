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
#include <stdio.h>
#include <string.h>

typedef enum
{
    MODE_IDLE = 0,
    MODE_REVERSE
} SystemMode_t;

#define WIFI_USE_EN_PIN      1
#define WIFI_START_DELAY_S   10
#define WIFI_SSID            "StarIS Xiaomi 17 Pro"
#define WIFI_PASSWORD        "1234abcd"
#define WIFI_SERVER_IP       "10.160.155.95"
#define WIFI_SERVER_PORT     9080

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

static void ReverseAlarm_ByDistance(u16 distance_cm)
{
    if((distance_cm == HCSR04_INVALID_DISTANCE) || (distance_cm > 100))
    {
        LED_AllOff();
        LED_G_On();
        BUZZER_Off();
        delay_ms(80);
    }
    else if(distance_cm > 50)
    {
        LED_AllOff();
        LED_B_On();

        BUZZER_On();
        delay_ms(60);
        BUZZER_Off();
        delay_ms(440);
    }
    else if(distance_cm > 20)
    {
        LED_AllOff();
        LED_R_On();

        BUZZER_On();
        delay_ms(80);
        BUZZER_Off();
        delay_ms(170);
    }
    else
    {
        LED_AllOn();
        BUZZER_On();
        delay_ms(80);
    }
}

static u8 GetLightLevel(u16 light_raw)
{
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

static u8 WiFi_BufferHas(const char *keyword)
{
    if(keyword == 0)
    {
        return 0;
    }

    return (strstr((char *)USART2_RX_BUF, keyword) != 0);
}

static void WiFi_PrintLastReply(const char *stage)
{
    printf("\r\n[%s reply]\r\n%s\r\n", stage, (char *)USART2_RX_BUF);
}

static void WiFi_DelayWithDebug(u32 total_ms)
{
    u32 elapsed = 0;

    while(elapsed < total_ms)
    {
        USART2_DebugFlushToUSART1();
        delay_ms(10);
        elapsed += 10;
    }

    USART2_DebugFlushToUSART1();
}

static void WiFi_SendCmd(const char *cmd)
{
    USART2_ClearBuf();
    USART2_DebugFlushToUSART1();
    printf("\r\n[STM32->WiFi] %s", cmd);
    USART2_SendString((char *)cmd);
}

static u8 WiFi_WaitFor(const char *keyword, u32 timeout_ms)
{
    u32 t = 0;

    while(t < timeout_ms)
    {
        USART2_DebugFlushToUSART1();

        if(WiFi_BufferHas(keyword))
        {
            return 1;
        }

        delay_ms(10);
        t += 10;
    }

    USART2_DebugFlushToUSART1();
    return 0;
}

static u8 WiFi_WaitForAny(u32 timeout_ms, const char *keyword1, const char *keyword2, const char *keyword3)
{
    u32 t = 0;

    while(t < timeout_ms)
    {
        USART2_DebugFlushToUSART1();

        if(WiFi_BufferHas(keyword1) || WiFi_BufferHas(keyword2) || WiFi_BufferHas(keyword3))
        {
            return 1;
        }

        if(WiFi_BufferHas("ERROR") || WiFi_BufferHas("+CME ERROR"))
        {
            return 0;
        }

        delay_ms(10);
        t += 10;
    }

    USART2_DebugFlushToUSART1();
    return 0;
}

static u8 WiFi_CheckATReady(void)
{
    u8 try_cnt;

    for(try_cnt = 0; try_cnt < 5; try_cnt++)
    {
        WiFi_SendCmd("AT\r\n");
        if(WiFi_WaitForAny(1200, "OK", "ready", 0))
        {
            printf("\r\nAT OK\r\n");
            return 1;
        }

        WiFi_DelayWithDebug(300);
    }

    printf("\r\nAT no response!\r\n");
    WiFi_PrintLastReply("AT");
    return 0;
}

static u8 WiFi_WaitForGotIP(u32 timeout_ms)
{
    u32 t = 0;

    while(t < timeout_ms)
    {
        USART2_DebugFlushToUSART1();

        if(WiFi_BufferHas("WIFI_GOT_IP"))
        {
            return 1;
        }

        if(WiFi_BufferHas("WIFI_DISCONNECT") || WiFi_BufferHas("ERROR") || WiFi_BufferHas("+CME ERROR"))
        {
            return 0;
        }

        delay_ms(10);
        t += 10;
    }

    USART2_DebugFlushToUSART1();
    return 0;
}

static u8 WiFi_Init_And_Send12345(void)
{
    char cmd[128];

    printf("\r\n===== WiFi Start =====\r\n");
    printf("SSID  : %s\r\n", WIFI_SSID);
    printf("Server: %s:%d\r\n", WIFI_SERVER_IP, WIFI_SERVER_PORT);

#if WIFI_USE_EN_PIN
    WiFi_EN_Init();
    WiFi_EN_Off();
    delay_ms(200);
    WiFi_EN_On();
#endif
    WiFi_DelayWithDebug(2000);

    if(!WiFi_CheckATReady())
    {
        return 0;
    }

    WiFi_SendCmd("AT+WMODE=1,1\r\n");
    if(!WiFi_WaitFor("OK", 1000))
    {
        printf("\r\nWMODE failed!\r\n");
        WiFi_PrintLastReply("WMODE");
        return 0;
    }
    printf("\r\nWMODE OK\r\n");

    sprintf(cmd, "AT+WJAP=\"%s\",\"%s\"\r\n", WIFI_SSID, WIFI_PASSWORD);
    WiFi_SendCmd(cmd);
    if(!WiFi_WaitForAny(15000, "WIFI_CONNECT", "WIFI_GOT_IP", "OK"))
    {
        printf("\r\nWJAP no response!\r\n");
        WiFi_PrintLastReply("WJAP");
        return 0;
    }
    if(WiFi_BufferHas("ERROR") || WiFi_BufferHas("+CME ERROR"))
    {
        printf("\r\nWJAP command failed!\r\n");
        WiFi_PrintLastReply("WJAP");
        return 0;
    }
    printf("\r\nWJAP accepted, wait for IP...\r\n");

    if(!WiFi_WaitForGotIP(25000))
    {
        printf("\r\nWIFI connect failed!\r\n");
        printf("Please check SSID/password, hotspot status, and whether the module can see this hotspot.\r\n");
        WiFi_PrintLastReply("WJAP");
        return 0;
    }
    printf("\r\nWiFi connected\r\n");

    sprintf(cmd, "AT+SOCKET=4,%s,%d,0,1\r\n", WIFI_SERVER_IP, WIFI_SERVER_PORT);
    WiFi_SendCmd(cmd);
    if(!WiFi_WaitForAny(5000, "connect success", "ConID=1", 0))
    {
        printf("\r\nSOCKET create failed!\r\n");
        WiFi_PrintLastReply("SOCKET");
        return 0;
    }
    printf("\r\nSOCKET connected\r\n");

    WiFi_SendCmd("AT+SOCKETSENDLINE=1,5,12345\r\n");
    if(!WiFi_WaitFor("OK", 3000))
    {
        printf("\r\nSend 12345 failed!\r\n");
        WiFi_PrintLastReply("SOCKETSENDLINE");
        return 0;
    }

    printf("\r\nSend 12345 success!\r\n");
    printf("\r\n===== WiFi Done =====\r\n");
    return 1;
}

static void Boot_WaitBeforeWiFi(void)
{
    u8 sec;

    printf("\r\nPower-on delay: wait %d s before WiFi start.\r\n", WIFI_START_DELAY_S);

    for(sec = WIFI_START_DELAY_S; sec > 0; sec--)
    {
        printf("WiFi starts in %d s\r\n", sec);
        delay_ms(1000);
    }
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
    DHT11_Init();
    Boot_WaitBeforeWiFi();
    WiFi_Init_And_Send12345();

    LED_AllOff();
    BUZZER_Off();
    OLED_ShowLightStatus(0, 0);

    delay_ms(1200);
    printf("System Ready\r\n");

    while(1)
    {
        USART2_DebugFlushToUSART1();
        key_value = KEY_Scan();

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
