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
 * @brief 按物理挡位扣除零偏，并通过放大倍数和采样电阻将差值码换算为电流。
 * @param diff_code 电流通道减 REF 的差值码，可为负数。
 * @param range 调用方须传入 LOW_CUR、MID_CUR 或 HIGH_CUR，用于索引标定参数。
 * @return 带符号的电流值，单位为 A。
 */
float Measurement_CurrentA(float diff_code, uint8_t range)
{
    /* 表项按 gate.h 中的物理挡位编号排列。 */
    static const float shut_ohm[]={0.0f,LOW_CURRENT_SHUNT_OHM,MID_CURRENT_SHUNT_OHM,HIGH_CURRENT_SHUNT_OHM};
    static const float zero_code[] = {0.0f, LOW_CURRENT_ZERO_CODE, MID_CURRENT_ZERO_CODE, HIGH_CURRENT_ZERO_CODE};
    static const float gain_corr[] = {1.0f, LOW_CURRENT_GAIN_CORRECTION, MID_CURRENT_GAIN_CORRECTION, HIGH_CURRENT_GAIN_CORRECTION};

    return (diff_code - zero_code[range]) *
           (ADC_REFERENCE_V * gain_corr[range] / (ADC_CURRENT_CODE_COUNT * HIGH_CURRENT_AMP_GAIN *
                               shut_ohm[range]));
}

/**
 * @brief 用同一采样块的均值计算电压、电流和功率。
 * @param voltage_code 电压通道的平均码值。
 * @param diff_code 当前挡电流通道平均码值减 REF 平均码值。
 * @param range 该块采样时的物理挡位，必须是 LOW_CUR、MID_CUR 或 HIGH_CUR。
 * @return 本块的电压、电流、功率和挡位。ADC 有效性由调用方判定。
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