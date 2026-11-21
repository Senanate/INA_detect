#ifndef __USER_TASKS_H__
#define __USER_TASKS_H__

#include "adc.h"

void KeyTask(void *argument);
void LCDTestTask(void *argument);

//ADC采集任务用到变量
extern volatile float adc_average_raw[ADC_CHANNELS];
extern volatile float adc_high_diff_raw;

extern volatile uint32_t adc_processed_block_count;
extern volatile uint32_t adc_timeout_count;
/**
 * @brief 接收采样数据块，发布原始码值均值及固定高档测量结果。
 * @param argument 任务参数，当前未使用。
 */
void ADCTask(void *argument);
#endif /* __USER_TASKS_H__ */
