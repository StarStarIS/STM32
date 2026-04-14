#include "light.h"

/*
 * 板上亮度采样点：
 * AD0 -> PA0 -> ADC1_IN0
 *
 * 分压关系（按原理图）：
 * 3.3V --- R11(10k) --- AD0 --- R12(GL5528) --- GND
 *
 * 因此：
 * 越亮 -> 光敏电阻越小 -> AD0电压越低 -> ADC值越小
 * 越暗 -> 光敏电阻越大 -> AD0电压越高 -> ADC值越大
 */

void LIGHT_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef  ADC_InitStructure;

    /* 开时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    /* ADC 时钟不能太高，PCLK2=72MHz 时，72/6=12MHz，合适 */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* PA0 配成模拟输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* ADC1 基本配置 */
    ADC_DeInit(ADC1);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 规则通道1 选 CH0，采样时间取长一点，稳定些 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1, ADC_SampleTime_239Cycles5);

    /* 使能 ADC */
    ADC_Cmd(ADC1, ENABLE);

    /* 校准 */
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1) == SET);

    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1) == SET);
}

u16 LIGHT_ReadRaw(void)
{
    /* 启动一次转换 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* 等待转换完成 */
    while(ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);

    return ADC_GetConversionValue(ADC1);
}

u16 LIGHT_ReadAverage(u8 times)
{
    u8  i;
    u32 sum = 0;

    if(times == 0)
    {
        times = 1;
    }

    for(i = 0; i < times; i++)
    {
        sum += LIGHT_ReadRaw();
    }

    return (u16)(sum / times);
}