#include "dht11.h"
#include "delay.h"
#include "sys.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

/*
 * 板上连接：
 * DHT11 DATA -> PB9，并且有 R10(10k) 上拉到 3.3V
 */
#define DHT11_GPIO      GPIOB
#define DHT11_PIN       GPIO_Pin_9
#define DHT11_RCC       RCC_APB2Periph_GPIOB

#define DHT11_DQ_IN     PBin(9)
#define DHT11_DQ_OUT    PBout(9)

static void DHT11_IO_Out(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_Init(DHT11_GPIO, &GPIO_InitStructure);
}

static void DHT11_IO_In(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = DHT11_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DHT11_GPIO, &GPIO_InitStructure);
}

static void DHT11_Reset(void)
{
    DHT11_IO_Out();
    DHT11_DQ_OUT = 0;
    delay_ms(20);
    DHT11_DQ_OUT = 1;
    delay_us(30);
}

static u8 DHT11_Check(void)
{
    u16 retry = 0;

    DHT11_IO_In();

    while(DHT11_DQ_IN && (retry < 100))
    {
        retry++;
        delay_us(1);
    }
    if(retry >= 100)
    {
        return DHT11_ERROR;
    }

    retry = 0;
    while((!DHT11_DQ_IN) && (retry < 100))
    {
        retry++;
        delay_us(1);
    }
    if(retry >= 100)
    {
        return DHT11_ERROR;
    }

    return DHT11_OK;
}

static u8 DHT11_ReadBit(void)
{
    u16 retry = 0;

    while(DHT11_DQ_IN && (retry < 100))
    {
        retry++;
        delay_us(1);
    }

    retry = 0;
    while((!DHT11_DQ_IN) && (retry < 100))
    {
        retry++;
        delay_us(1);
    }

    delay_us(40);

    if(DHT11_DQ_IN)
    {
        retry = 0;
        while(DHT11_DQ_IN && (retry < 100))
        {
            retry++;
            delay_us(1);
        }
        return 1;
    }

    return 0;
}

static u8 DHT11_ReadByte(void)
{
    u8 i;
    u8 data = 0;

    for(i = 0; i < 8; i++)
    {
        data <<= 1;
        data |= DHT11_ReadBit();
    }

    return data;
}

void DHT11_Init(void)
{
    RCC_APB2PeriphClockCmd(DHT11_RCC, ENABLE);
    DHT11_IO_Out();
    DHT11_DQ_OUT = 1;
}

u8 DHT11_ReadData(u8 *temperature, u8 *humidity)
{
    u8 buf[5];
    u8 i;

    if((temperature == 0) || (humidity == 0))
    {
        return DHT11_ERROR;
    }

    DHT11_Reset();
    if(DHT11_Check() != DHT11_OK)
    {
        return DHT11_ERROR;
    }

    for(i = 0; i < 5; i++)
    {
        buf[i] = DHT11_ReadByte();
    }

    DHT11_IO_Out();
    DHT11_DQ_OUT = 1;

    if((u8)(buf[0] + buf[1] + buf[2] + buf[3]) != buf[4])
    {
        return DHT11_ERROR;
    }

    *humidity = buf[0];
    *temperature = buf[2];

    return DHT11_OK;
}