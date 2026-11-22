#include "measurement.h"
#include "gate.h"
/* 参数来自 Power-Pico 参考硬件，使用实测基准和已知负载标定后再更新。 */
#define ADC_REFERENCE_V             3.0f
#define ADC_VOLTAGE_FULL_SCALE_CODE 4095.0f
#define ADC_CURRENT_CODE_COUNT      4096.0f
#define VOLTAGE_DIVIDER_RATIO       11.0f

#define HIGH_CURRENT_AMP_GAIN       50.0f //INA190A2固定电压增益为50倍

#define HIGH_CURRENT_SHUNT_OHM      0.005f //高档位电流采样电阻阻值，0.005欧姆
#define MID_CURRENT_SHUNT_OHM       0.5f //中档位电流采样电阻阻值，0.5欧姆
#define LOW_CURRENT_SHUNT_OHM       50.0f //低档位电流采样电阻阻值，50欧姆

/* 零偏取多次空载 HIGH-REF 均值，增益系数取标准表读数与换算读数之比。 */
#define HIGH_CURRENT_ZERO_CODE      0.0f //高档位电流采样零偏，设置为0
#define MID_CURRENT_ZERO_CODE       0.0f //中档位电流采样零偏，设置为0
#define LOW_CURRENT_ZERO_CODE       0.0f

#define VOLTAGE_GAIN_CORRECTION     1.0f

#define HIGH_CURRENT_GAIN_CORRECTION 1.0f
#define MID_CURRENT_GAIN_CORRECTION 1.0f
#define LOW_CURRENT_GAIN_CORRECTION 1.0f

measurement_t Measurement_CalculateHigh(float voltage_code, float current_diff_code)
{
    measurement_t result;

    /* 沿用参考固件的电压满刻度与电流 LSB 口径。 */
    result.voltage_v = voltage_code * (ADC_REFERENCE_V * VOLTAGE_DIVIDER_RATIO /
                                      ADC_VOLTAGE_FULL_SCALE_CODE * VOLTAGE_GAIN_CORRECTION);
    result.current_a = (current_diff_code - HIGH_CURRENT_ZERO_CODE) *
                       (ADC_REFERENCE_V / (ADC_CURRENT_CODE_COUNT * HIGH_CURRENT_AMP_GAIN *
                                           HIGH_CURRENT_SHUNT_OHM) * HIGH_CURRENT_GAIN_CORRECTION);
    result.power_w = result.voltage_v * result.current_a;
    result.status = MEASUREMENT_VALID;
    return result;
}
/**
 * @brief 根据不同的量程计算电流。
 * @param diff_code 差值码。
 * @param range 量程。
 * @return 计算得到的电流值。
 */
float Measurement_CurrentA(float diff_code, uint8_t range)
{
    // 计算电流值，考虑不同量程的采样电阻和零偏。
    static const float shut_ohm[]={0.0f,LOW_CURRENT_SHUNT_OHM,MID_CURRENT_SHUNT_OHM,HIGH_CURRENT_SHUNT_OHM};
    static const float zero_code[] = {0.0f, LOW_CURRENT_ZERO_CODE, MID_CURRENT_ZERO_CODE, HIGH_CURRENT_ZERO_CODE};
    static const float gain_corr[] = {1.0f, LOW_CURRENT_GAIN_CORRECTION, MID_CURRENT_GAIN_CORRECTION, HIGH_CURRENT_GAIN_CORRECTION};

    return (diff_code - zero_code[range]) *
           (ADC_REFERENCE_V * gain_corr[range] / (ADC_CURRENT_CODE_COUNT * HIGH_CURRENT_AMP_GAIN *
                               shut_ohm[range]));// 计算电流值大小，（传入的相对参考ADC的电压差值-零偏）*ADC参考电压/（ADC满刻度*增益*采样电阻）=电流值
}

/**
 * @brief 根据不同的量程计算测量值。
 * @param voltage_code 电压码。
 * @param diff_code 差值码。
 * @param range 量程。
 * @return 计算得到的测量值。
 */
measurement_t Measurement_CalculateRange(float voltage_code, float diff_code, uint8_t range)
{
    measurement_t result={0};
    result.voltage_v = voltage_code * (ADC_REFERENCE_V * VOLTAGE_DIVIDER_RATIO /
                                      ADC_VOLTAGE_FULL_SCALE_CODE * VOLTAGE_GAIN_CORRECTION);
    result.current_a = Measurement_CurrentA(diff_code, range);
    result.power_w = result.voltage_v * result.current_a;
    result.status = MEASUREMENT_VALID;
    result.range = range;
    return result;
}