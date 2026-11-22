/* USER CODE BEGIN Include */
#include <stdbool.h>

#include "Task_Init.h"
#include "key.h"
#include "beep.h"
#include "User_Tasks.h"
#include "adc.h"
#include "task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "measurement_view.h"
#include "ui.h"

//公共变量定义区
#define ADC_QUEUE_WAIT_TICKS 100U
#define LCD_ADC_REFRESH_MS    100U // LVGL使用HAL毫秒时基，每200ms更新采样数值
#define LCD_LVGL_DELAY_TICKS  5U   // 当前系统tick为1ms，周期处理LVGL刷新和定时器
#define ADC_MAX_CODE          4095U

volatile float adc_average_raw[ADC_CHANNELS] = {0};
volatile float adc_high_diff_raw = 0.0f;

volatile uint32_t adc_processed_block_count = 0U;
volatile uint32_t adc_timeout_count = 0U;

static measurement_t latest_measurement;

/*创建用户Freertos任务进程*/




void KeyTask(void *argument)
{
    key_event_t key_event;
    while(1)
    {  
        if(Key_Scan(&key_event)){
            /* 发送按键事件到消息队列 */
            osMessageQueuePut(Key_MessageQueue, &key_event, 0, 0);
            beep_open();
            osDelay(100);
            beep_close();
        //     /* 简单示例：按下时蜂鸣器响，松开时停止 */
        //     if(key_event.type == KEY_EVT_CLICK){
        //         beep_open();
        //     }else if(key_event.type == KEY_EVT_UP){
        //         beep_close();
        //     }
        // }
        }
        osDelay(10);
    }
}
/**
 * @brief LCD屏幕 LVGL显示任务。
 * @param argument 任务参数，当前未使用。
 * @note LVGL对象创建和刷新集中在本任务，毫秒计数由HAL的TIM11时基提供。
 */
void LCDTestTask(void *argument)
{
    uint32_t last_refresh_tick;

    (void)argument;

    /* 等待硬件初始化任务先完成，避免和其它外设初始化抢占 */
    osDelay(500);

    /* LVGL对象创建和刷新集中在本任务，毫秒计数由HAL的TIM11时基提供。 */
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_port_disp_init();

    ui_init();

    /* 利用无符号计数差使首次循环立即刷新，同时兼容毫秒计数回绕。 */
    last_refresh_tick = lv_tick_get() - LCD_ADC_REFRESH_MS;
    while(1)
    {
        uint32_t current_tick = lv_tick_get();
        if ((uint32_t)(current_tick - last_refresh_tick) >= LCD_ADC_REFRESH_MS)
        {
            measurement_t snapshot;
            last_refresh_tick = current_tick;

            /* 与 ADC 任务同步读取整组结果，绘图留在临界区外。 */
            taskENTER_CRITICAL();
            snapshot = latest_measurement;
            if (adc_error_code != HAL_ADC_ERROR_NONE)
            {
                snapshot.status = MEASUREMENT_ADC_ERROR;
            }
            taskEXIT_CRITICAL();

            MeasurementView_Update(snapshot);
        }

        lv_timer_handler();
        osDelay(LCD_LVGL_DELAY_TICKS);
    }
}
void ADCTask(void *argument)
{
    ADC_Block_t block;
    osStatus_t status;

    (void)argument;

    for (;;)
    {
        uint32_t sums[ADC_CHANNELS] = {0U};
        float averages[ADC_CHANNELS];
        float high_diff;
        bool saturated = false;
        measurement_t result;

        status = osMessageQueueGet(ADC_MessageQueue, &block, NULL, ADC_QUEUE_WAIT_TICKS);
        if (status == osErrorTimeout)
        {
            adc_timeout_count++;
            taskENTER_CRITICAL();
            latest_measurement.status = MEASUREMENT_ADC_TIMEOUT;
            taskEXIT_CRITICAL();
            continue;
        }

        if (status != osOK)
        {
            Error_Handler();
        }

        for (uint32_t sample = 0U; sample < ADC_TIMES; sample++)
        {
            const uint16_t *codes = block.samples[sample];

            /* 电压零码可表示零输入，电流和偏置通道应远离 ADC 两端。 */
            if (codes[ADC_IDX_VOL] >= ADC_MAX_CODE ||
                codes[ADC_IDX_HIGH] == 0U || codes[ADC_IDX_HIGH] >= ADC_MAX_CODE ||
                codes[ADC_IDX_REF] == 0U || codes[ADC_IDX_REF] >= ADC_MAX_CODE)
            {
                saturated = true;
            }

            for (uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
            {
                sums[channel] += codes[channel];
            }
        }

        for (uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
        {
            averages[channel] = (float)sums[channel] / (float)ADC_TIMES;//计算每个通道的平均值
        }

        high_diff = averages[ADC_IDX_HIGH] - averages[ADC_IDX_REF];//计算高档电流通道的差值，保留符号
        result = Measurement_CalculateHigh(averages[ADC_IDX_VOL], high_diff);
        if (saturated)
        {
            result.status = MEASUREMENT_ADC_LIMIT;
        }

        /* 原始调试值和测量结果来自同一数据块。换算在临界区外完成。 */
        taskENTER_CRITICAL();
        for (uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
        {
            adc_average_raw[channel] = averages[channel];
        }
        adc_high_diff_raw = high_diff;
        latest_measurement = result;
        adc_processed_block_count++;
        taskEXIT_CRITICAL();
    }
}
