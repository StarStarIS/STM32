#include "oled.h"
#include "delay.h"

/* 原理图对应关系：
 * OLED_SCL -> PA12
 * OLED_SDA -> PA11
 */
#define OLED_SCL_GPIO   GPIOA
#define OLED_SCL_PIN    GPIO_Pin_12

#define OLED_SDA_GPIO   GPIOA
#define OLED_SDA_PIN    GPIO_Pin_11

/* 常见 SSD1306 I2C 地址（SA0=0 时） */
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

/* 第一版先不严格读 ACK，直接给时钟继续走 */
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
    OLED_IIC_WriteByte(0x00);   /* 控制字节：后面是命令 */
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
    OLED_IIC_WriteByte(0x40);   /* 控制字节：后面是数据 */
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
    u8 page, col;

    for(page = 0; page < 8; page++)
    {
        OLED_SetPos(0, page);
        for(col = 0; col < 128; col++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/* 只做本项目够用的小字库：空格、-、:、D、L、0~9 */
static void OLED_ShowChar6x8(u8 x, u8 page, char ch)
{
    u8 i;
    const u8 *p = 0;

    static const u8 font_space[6] = {0x00,0x00,0x00,0x00,0x00,0x00};
    static const u8 font_minus[6] = {0x08,0x08,0x08,0x08,0x08,0x00};
    static const u8 font_colon[6] = {0x00,0x36,0x36,0x00,0x00,0x00};

    static const u8 font_D[6]     = {0x7F,0x41,0x41,0x22,0x1C,0x00};
    static const u8 font_L[6]     = {0x7F,0x40,0x40,0x40,0x40,0x00};

    static const u8 font_0[6]     = {0x3E,0x51,0x49,0x45,0x3E,0x00};
    static const u8 font_1[6]     = {0x00,0x42,0x7F,0x40,0x00,0x00};
    static const u8 font_2[6]     = {0x42,0x61,0x51,0x49,0x46,0x00};
    static const u8 font_3[6]     = {0x21,0x41,0x45,0x4B,0x31,0x00};
    static const u8 font_4[6]     = {0x18,0x14,0x12,0x7F,0x10,0x00};
    static const u8 font_5[6]     = {0x27,0x45,0x45,0x45,0x39,0x00};
    static const u8 font_6[6]     = {0x3C,0x4A,0x49,0x49,0x30,0x00};
    static const u8 font_7[6]     = {0x01,0x71,0x09,0x05,0x03,0x00};
    static const u8 font_8[6]     = {0x36,0x49,0x49,0x49,0x36,0x00};
    static const u8 font_9[6]     = {0x06,0x49,0x49,0x29,0x1E,0x00};

    switch(ch)
    {
        case ' ': p = font_space; break;
        case '-': p = font_minus; break;
        case ':': p = font_colon; break;
        case 'D': p = font_D; break;
        case 'L': p = font_L; break;
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

static void OLED_Show3Digit(u8 x, u8 page, u16 num)
{
    u8 hundreds, tens, ones;

    if(num > 999) num = 999;

    hundreds = num / 100;
    tens     = (num / 10) % 10;
    ones     = num % 10;

    if(hundreds == 0) OLED_ShowChar6x8(x,      page, ' ');
    else              OLED_ShowChar6x8(x,      page, hundreds + '0');

    if((hundreds == 0) && (tens == 0)) OLED_ShowChar6x8(x + 6,  page, ' ');
    else                               OLED_ShowChar6x8(x + 6,  page, tens + '0');

    OLED_ShowChar6x8(x + 12, page, ones + '0');
}

static void OLED_ShowDash3(u8 x, u8 page)
{
    OLED_ShowChar6x8(x,      page, '-');
    OLED_ShowChar6x8(x + 6,  page, '-');
    OLED_ShowChar6x8(x + 12, page, '-');
}

void OLED_ShowStatus(u16 distance_cm, u8 level)
{
    /* 第1行：D:xxx */
    OLED_ShowStr6x8(0, 0, "D:");
    if(distance_cm == 0xFFFF) OLED_ShowDash3(18, 0);
    else                      OLED_Show3Digit(18, 0, distance_cm);

    /* 清掉后面残留 */
    OLED_ShowStr6x8(36, 0, "      ");

    /* 第3行：L:x */
    OLED_ShowStr6x8(0, 2, "L:");
    if(level <= 9) OLED_ShowChar6x8(18, 2, level + '0');
    else           OLED_ShowChar6x8(18, 2, '-');

    OLED_ShowStr6x8(24, 2, "      ");
}

void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = OLED_SCL_PIN | OLED_SDA_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;   /* 软件I2C，开漏输出 */
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    OLED_SCL_Set(1);
    OLED_SDA_Set(1);

    delay_ms(100);

    OLED_WriteCmd(0xAE);   /* display off */
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
    OLED_WriteCmd(0x02);   /* page addressing mode */
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
    OLED_WriteCmd(0xAF);   /* display on */

    OLED_Clear();
}