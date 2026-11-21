#ifndef MEASUREMENT_H
#define MEASUREMENT_H

typedef enum
{
    MEASUREMENT_WAITING = 0,
    MEASUREMENT_VALID,
    MEASUREMENT_ADC_LIMIT,
    MEASUREMENT_ADC_TIMEOUT,
    MEASUREMENT_ADC_ERROR
} measurement_status_t;

typedef struct
{
    float voltage_v;
    float current_a;
    float power_w;
    measurement_status_t status;
} measurement_t;

/**
 * @brief 换算固定高档的一组 ADC 均值，保留电流和功率的符号。
 * @param voltage_code 电压通道的平均码值，范围 0 到 4095。
 * @param current_diff_code 同一数据块的 HIGH 均值减 REF 均值。
 * @return 使用参考系数计算的 V、A、W。调用方负责判断 ADC 饱和及采集错误。
 */
measurement_t Measurement_CalculateHigh(float voltage_code, float current_diff_code);

#endif
