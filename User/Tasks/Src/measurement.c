#include "measurement.h"

/* 参数来自 Power-Pico 参考硬件，使用实测基准和已知负载标定后再更新。 */
#define ADC_REFERENCE_V             3.0f
#define ADC_VOLTAGE_FULL_SCALE_CODE 4095.0f
#define ADC_CURRENT_CODE_COUNT      4096.0f
#define VOLTAGE_DIVIDER_RATIO       11.0f
#define HIGH_CURRENT_AMP_GAIN       50.0f
#define HIGH_CURRENT_SHUNT_OHM      0.005f

/* 零偏取多次空载 HIGH-REF 均值，增益系数取标准表读数与换算读数之比。 */
#define HIGH_CURRENT_ZERO_CODE      0.0f
#define VOLTAGE_GAIN_CORRECTION     1.0f
#define HIGH_CURRENT_GAIN_CORRECTION 1.0f

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
