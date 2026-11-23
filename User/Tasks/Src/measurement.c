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

/* 各挡零偏用该挡空载时的电流通道减 REF 的均值标定。增益系数取标准表读数与换算读数之比。 */
#define HIGH_CURRENT_ZERO_CODE      0.0f //高档位电流采样零偏，设置为0
#define MID_CURRENT_ZERO_CODE       0.0f //中档位电流采样零偏，设置为0
#define LOW_CURRENT_ZERO_CODE       0.0f

#define VOLTAGE_GAIN_CORRECTION     1.0f

#define HIGH_CURRENT_GAIN_CORRECTION 1.0f
#define MID_CURRENT_GAIN_CORRECTION 1.0f
#define LOW_CURRENT_GAIN_CORRECTION 1.0f

measurement_t Measurement_CalculateHigh(float voltage_code, float current_diff_code)
{
    return Measurement_CalculateRange(voltage_code, current_diff_code, HIGH_CUR);
}

float Measurement_CurrentA(float diff_code, uint8_t range)
{
    /* 表项按 gate.h 中的物理挡位编号排列。 */
    static const float shunt_ohm[] = {
        0.0f, LOW_CURRENT_SHUNT_OHM, MID_CURRENT_SHUNT_OHM, HIGH_CURRENT_SHUNT_OHM
    };
    static const float zero_code[] = {
        0.0f, LOW_CURRENT_ZERO_CODE, MID_CURRENT_ZERO_CODE, HIGH_CURRENT_ZERO_CODE
    };
    static const float gain_corr[] = {
        1.0f, LOW_CURRENT_GAIN_CORRECTION, MID_CURRENT_GAIN_CORRECTION, HIGH_CURRENT_GAIN_CORRECTION
    };

    if (range < LOW_CUR || range > HIGH_CUR)
    {
        return 0.0f;
    }

    return (diff_code - zero_code[range]) *
           (ADC_REFERENCE_V / (ADC_CURRENT_CODE_COUNT * HIGH_CURRENT_AMP_GAIN *
                               shunt_ohm[range]) * gain_corr[range]);
}

measurement_t Measurement_CalculateRange(float voltage_code, float diff_code, uint8_t range)
{
    measurement_t result = {0};

    result.range = range;
    if (range < LOW_CUR || range > HIGH_CUR)
    {
        result.status = MEASUREMENT_ADC_ERROR;
        return result;
    }

    /* 沿用参考固件的电压满刻度与电流 LSB 口径。 */
    result.voltage_v = voltage_code * (ADC_REFERENCE_V * VOLTAGE_DIVIDER_RATIO /
                                      ADC_VOLTAGE_FULL_SCALE_CODE * VOLTAGE_GAIN_CORRECTION);
    result.current_a = Measurement_CurrentA(diff_code, range);
    result.power_w = result.voltage_v * result.current_a;
    result.status = MEASUREMENT_VALID;
    return result;
}
