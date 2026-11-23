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
 * @brief 处理 DMA 采样块，按当前物理挡位发布测量结果，并在 AUTO 模式下执行切挡。
 * @param argument 任务参数，当前未使用。
 * @note 队列接收超时时发布超时状态并继续等待，切挡重启失败时结束任务。
 */
void ADCTask(void *argument);
#endif /* __USER_TASKS_H__ */
