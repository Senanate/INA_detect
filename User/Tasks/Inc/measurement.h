#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdint.h>

typedef enum
{
    MEASUREMENT_WAITING = 0,
    MEASUREMENT_VALID,
    MEASUREMENT_ADC_LIMIT,
    MEASUREMENT_ADC_TIMEOUT,
    MEASUREMENT_ADC_ERROR,
    MEASUREMENT_SWITCHING
} measurement_status_t;

typedef struct
{
    float voltage_v;
    float current_a;
    float power_w;
    measurement_status_t status;
    uint8_t range;
} measurement_t;

/**
 * @brief 换算固定高档的一组 ADC 均值，保留电流和功率的符号。
 * @param voltage_code 电压通道的平均码值，范围 0 到 4095。
 * @param current_diff_code 同一数据块的 HIGH 均值减 REF 均值。
 * @return 使用参考系数计算的 V、A、W。调用方负责判断 ADC 饱和及采集错误。
 */
measurement_t Measurement_CalculateHigh(float voltage_code, float current_diff_code);

/**
 * @brief 根据不同的量程计算电流。
 * @param diff_code 差值码。
 * @param range 物理挡位，取 LOW_CUR、MID_CUR 或 HIGH_CUR。
 * @return 带符号的电流，单位为 A。挡位无效时返回 0.0f。
 */
float Measurement_CurrentA(float diff_code, uint8_t range);

/**
 * @brief 根据不同的量程计算测量值。
 * @param voltage_code 电压码。
 * @param diff_code 差值码。
 * @param range 物理挡位，取 LOW_CUR、MID_CUR 或 HIGH_CUR。
 * @return 测量快照。挡位无效时状态为 MEASUREMENT_ADC_ERROR。
 */
measurement_t Measurement_CalculateRange(float voltage_code,
                                         float diff_code,
                                         uint8_t range);

#endif
