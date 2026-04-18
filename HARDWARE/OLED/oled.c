#include "oled.h"
#include "delay.h"
#include <stdio.h>

#define OLED_SCL_GPIO   GPIOA
#define OLED_SCL_PIN    GPIO_Pin_12

#define OLED_SDA_GPIO   GPIOA
#define OLED_SDA_PIN    GPIO_Pin_11

#define OLED_ADDR       0x78

static void OLED_SCL_Set(u8 val)
{
    if(val) GPIO_SetBits(OLED_SCL_GPIO, OLED_SCL_PIN);
    else    GPIO_ResetBits(OLED_SCL_GPIO, OLED_SCL_PIN);
}

static void OLED_SDA_Set(u8 val)
{
    if(val) GPIO_SetBits(OLED_SDA_GPIO, OLED_SDA_PIN);
    else    GPIO_ResetBits(OLED_SDA_GPIO, OLED_SDA_PIN);
}

static void OLED_IIC_Delay(void)
{
    delay_us(4);
}

static void OLED_IIC_Start(void)
{
    OLED_SDA_Set(1);
    OLED_SCL_Set(1);
    OLED_IIC_Delay();
    OLED_SDA_Set(0);
    OLED_IIC_Delay();
    OLED_SCL_Set(0);
}

static void OLED_IIC_Stop(void)
{
    OLED_SCL_Set(0);
    OLED_SDA_Set(0);
    OLED_IIC_Delay();
    OLED_SCL_Set(1);
    OLED_IIC_Delay();
    OLED_SDA_Set(1);
    OLED_IIC_Delay();
}

static void OLED_IIC_WaitAck(void)
{
    OLED_SDA_Set(1);
    OLED_IIC_Delay();
    OLED_SCL_Set(1);
    OLED_IIC_Delay();
    OLED_SCL_Set(0);
    OLED_IIC_Delay();
}

static void OLED_IIC_WriteByte(u8 byte)
{
    u8 i;

    for(i = 0; i < 8; i++)
    {
        OLED_SCL_Set(0);

        if(byte & 0x80) OLED_SDA_Set(1);
        else            OLED_SDA_Set(0);

        OLED_IIC_Delay();
        OLED_SCL_Set(1);
        OLED_IIC_Delay();
        OLED_SCL_Set(0);

        byte <<= 1;
    }
}

static void OLED_WriteCmd(u8 cmd)
{
    OLED_IIC_Start();
    OLED_IIC_WriteByte(OLED_ADDR);
    OLED_IIC_WaitAck();
    OLED_IIC_WriteByte(0x00);
    OLED_IIC_WaitAck();
    OLED_IIC_WriteByte(cmd);
    OLED_IIC_WaitAck();
    OLED_IIC_Stop();
}

static void OLED_WriteData(u8 data)
{
    OLED_IIC_Start();
    OLED_IIC_WriteByte(OLED_ADDR);
    OLED_IIC_WaitAck();
    OLED_IIC_WriteByte(0x40);
    OLED_IIC_WaitAck();
    OLED_IIC_WriteByte(data);
    OLED_IIC_WaitAck();
    OLED_IIC_Stop();
}

static void OLED_SetPos(u8 x, u8 page)
{
    OLED_WriteCmd(0xB0 + page);
    OLED_WriteCmd(((x & 0xF0) >> 4) | 0x10);
    OLED_WriteCmd(x & 0x0F);
}

void OLED_Clear(void)
{
    u8 page;
    u8 col;

    for(page = 0; page < 8; page++)
    {
        OLED_SetPos(0, page);
        for(col = 0; col < 128; col++)
        {
            OLED_WriteData(0x00);
        }
    }
}

static void OLED_ClearLine(u8 page)
{
    u8 col;

    OLED_SetPos(0, page);
    for(col = 0; col < 128; col++)
    {
        OLED_WriteData(0x00);
    }
}

static void OLED_ShowChar6x8(u8 x, u8 page, char ch)
{
    u8 i;
    const u8 *p = 0;

    static const u8 font_space[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
    static const u8 font_minus[6] = {0x08,0x08,0x08,0x08,0x08,0x00};
    static const u8 font_colon[6] = {0x00,0x36,0x36,0x00,0x00,0x00};

    static const u8 font_A[6] = {0x7E,0x11,0x11,0x11,0x7E,0x00};
    static const u8 font_C[6] = {0x3E,0x41,0x41,0x41,0x22,0x00};
    static const u8 font_D[6] = {0x7F,0x41,0x41,0x22,0x1C,0x00};
    static const u8 font_E[6] = {0x7F,0x49,0x49,0x49,0x41,0x00};
    static const u8 font_F[6] = {0x7F,0x09,0x09,0x09,0x01,0x00};
    static const u8 font_G[6] = {0x3E,0x41,0x49,0x49,0x7A,0x00};
    static const u8 font_H[6] = {0x7F,0x08,0x08,0x08,0x7F,0x00};
    static const u8 font_I[6] = {0x00,0x41,0x7F,0x41,0x00,0x00};
    static const u8 font_K[6] = {0x7F,0x08,0x14,0x22,0x41,0x00};
    static const u8 font_L[6] = {0x7F,0x40,0x40,0x40,0x40,0x00};
    static const u8 font_M[6] = {0x7F,0x02,0x0C,0x02,0x7F,0x00};
    static const u8 font_N[6] = {0x7F,0x04,0x08,0x10,0x7F,0x00};
    static const u8 font_O[6] = {0x3E,0x41,0x41,0x41,0x3E,0x00};
    static const u8 font_R[6] = {0x7F,0x09,0x19,0x29,0x46,0x00};
    static const u8 font_S[6] = {0x46,0x49,0x49,0x49,0x31,0x00};
    static const u8 font_T[6] = {0x01,0x01,0x7F,0x01,0x01,0x00};
    static const u8 font_U[6] = {0x3F,0x40,0x40,0x40,0x3F,0x00};
    static const u8 font_W[6] = {0x7F,0x20,0x18,0x20,0x7F,0x00};
    static const u8 font_Y[6] = {0x07,0x08,0x70,0x08,0x07,0x00};

    static const u8 font_0[6] = {0x3E,0x51,0x49,0x45,0x3E,0x00};
    static const u8 font_1[6] = {0x00,0x42,0x7F,0x40,0x00,0x00};
    static const u8 font_2[6] = {0x42,0x61,0x51,0x49,0x46,0x00};
    static const u8 font_3[6] = {0x21,0x41,0x45,0x4B,0x31,0x00};
    static const u8 font_4[6] = {0x18,0x14,0x12,0x7F,0x10,0x00};
    static const u8 font_5[6] = {0x27,0x45,0x45,0x45,0x39,0x00};
    static const u8 font_6[6] = {0x3C,0x4A,0x49,0x49,0x30,0x00};
    static const u8 font_7[6] = {0x01,0x71,0x09,0x05,0x03,0x00};
    static const u8 font_8[6] = {0x36,0x49,0x49,0x49,0x36,0x00};
    static const u8 font_9[6] = {0x06,0x49,0x49,0x29,0x1E,0x00};

    switch(ch)
    {
        case ' ': p = font_space; break;
        case '-': p = font_minus; break;
        case ':': p = font_colon; break;
        case 'A': p = font_A; break;
        case 'C': p = font_C; break;
        case 'D': p = font_D; break;
        case 'E': p = font_E; break;
        case 'F': p = font_F; break;
        case 'G': p = font_G; break;
        case 'H': p = font_H; break;
        case 'I': p = font_I; break;
        case 'K': p = font_K; break;
        case 'L': p = font_L; break;
        case 'M': p = font_M; break;
        case 'N': p = font_N; break;
        case 'O': p = font_O; break;
        case 'R': p = font_R; break;
        case 'S': p = font_S; break;
        case 'T': p = font_T; break;
        case 'U': p = font_U; break;
        case 'W': p = font_W; break;
        case 'Y': p = font_Y; break;
        case '0': p = font_0; break;
        case '1': p = font_1; break;
        case '2': p = font_2; break;
        case '3': p = font_3; break;
        case '4': p = font_4; break;
        case '5': p = font_5; break;
        case '6': p = font_6; break;
        case '7': p = font_7; break;
        case '8': p = font_8; break;
        case '9': p = font_9; break;
        default:  p = font_space; break;
    }

    OLED_SetPos(x, page);
    for(i = 0; i < 6; i++)
    {
        OLED_WriteData(p[i]);
    }
}

static void OLED_ShowStr6x8(u8 x, u8 page, const char *str)
{
    while(*str != '\0')
    {
        OLED_ShowChar6x8(x, page, *str);
        x += 6;
        str++;
    }
}

static void OLED_ShowTextLine(u8 page, const char *text)
{
    OLED_ClearLine(page);
    OLED_ShowStr6x8(0, page, text);
}

void OLED_ShowStatus(u16 distance_cm, u8 level)
{
    char line[22];

    if(distance_cm == 0xFFFF) sprintf(line, "D:---");
    else                      sprintf(line, "D:%3uCM", distance_cm);
    OLED_ShowTextLine(0, line);

    sprintf(line, "L:%u", level);
    OLED_ShowTextLine(2, line);
}

void OLED_ShowLightStatus(u16 light_raw, u8 level)
{
    char line[22];

    sprintf(line, "A:%4u", light_raw);
    OLED_ShowTextLine(0, line);

    sprintf(line, "L:%u", level);
    OLED_ShowTextLine(2, line);
}

void OLED_ShowSecurityStatus(u16 distance_cm, u8 is_night, u8 alarm_state, u8 wifi_connected, u8 silenced)
{
    char line[22];

    if(distance_cm == 0xFFFF) sprintf(line, "D:---");
    else                      sprintf(line, "D:%3uCM", distance_cm);
    OLED_ShowTextLine(0, line);

    if(is_night) OLED_ShowTextLine(2, "N:NIGHT");
    else         OLED_ShowTextLine(2, "N:DAY");

    switch(alarm_state)
    {
        case 1:
            OLED_ShowTextLine(4, "A:WARN");
            break;

        case 2:
            OLED_ShowTextLine(4, "A:ALRM");
            break;

        default:
            OLED_ShowTextLine(4, "A:SAFE");
            break;
    }

    sprintf(line, "W:%s M:%s", wifi_connected ? "ON" : "OFF", silenced ? "ON" : "OFF");
    OLED_ShowTextLine(6, line);
}

void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    OLED_SCL_Set(1);
    OLED_SDA_Set(1);

    delay_ms(100);

    OLED_WriteCmd(0xAE);
    OLED_WriteCmd(0xD5);
    OLED_WriteCmd(0x80);
    OLED_WriteCmd(0xA8);
    OLED_WriteCmd(0x3F);
    OLED_WriteCmd(0xD3);
    OLED_WriteCmd(0x00);
    OLED_WriteCmd(0x40);
    OLED_WriteCmd(0x8D);
    OLED_WriteCmd(0x14);
    OLED_WriteCmd(0x20);
    OLED_WriteCmd(0x02);
    OLED_WriteCmd(0xA1);
    OLED_WriteCmd(0xC8);
    OLED_WriteCmd(0xDA);
    OLED_WriteCmd(0x12);
    OLED_WriteCmd(0x81);
    OLED_WriteCmd(0xCF);
    OLED_WriteCmd(0xD9);
    OLED_WriteCmd(0xF1);
    OLED_WriteCmd(0xDB);
    OLED_WriteCmd(0x20);
    OLED_WriteCmd(0xA4);
    OLED_WriteCmd(0xA6);
    OLED_WriteCmd(0xAF);

    OLED_Clear();
}
