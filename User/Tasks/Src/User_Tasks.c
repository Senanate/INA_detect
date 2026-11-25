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
#include "math.h"
#include "gate.h"
#include "tim.h"

//公共变量定义区
#define ADC_QUEUE_WAIT_TICKS 100U
#define LCD_LVGL_DELAY_TICKS  5U   // 当前系统tick为1ms，周期处理LVGL刷新和定时器
#define KEY_SCAN_DELAY_TICKS 10U
#define KEY_BEEP_DURATION_MS 50U
#define ADC_MAX_CODE          4095U


/* 两个开关分别控制是否允许自动降入 MID 和 LOW，0 表示禁用。 */
#define AUTO_ALLOW_MID       0
#define AUTO_ALLOW_LOW       0
/* 降档需要连续低电流块。当前每块 2.5 ms，20 块约 50 ms。 */
#define AUTO_DOWN_BLOCKS    20U
/* GPIO 切换后的模拟量稳定等待，单位为 RTOS tick。0 表示暂停自动切档。 */
#define AUTO_SETTLE_TICKS     50U

volatile float adc_average_raw[ADC_CHANNELS] = {0};
volatile float adc_high_diff_raw = 0.0f;

volatile uint32_t adc_processed_block_count = 0U;
volatile uint32_t adc_timeout_count = 0U;
static volatile uint32_t key_queue_drop_count = 0U;

static measurement_t latest_measurement;
/*其他功能函数*/
/**
 * @brief 根据一块采样的电流峰值和有效性决定目标挡位。
 * @param current_range 生成这块采样时的实际硬件挡位。
 * @param peak_a 本块电流的最大绝对值，单位为 A。
 * @param current_rail 本块所选电流通道是否触及 ADC 量程边界。
 * @param quiet_blocks 连续满足降档条件的块数，由 ADCTask 跨块保存。
 * @param allow_down 是否允许自动降档。
 * @return 目标挡位。返回 current_range 表示维持当前挡位。
 */
static uint8_t AutoRange_Next(uint8_t current_range, float peak_a, bool current_rail,
                              bool allow_down, uint8_t *quiet_blocks)
{
    /* 单块过载即可升档。ADC 触顶或触底时，直接请求高挡。 */
    if (current_rail ||
        (current_range == MID_CUR && peak_a >= 0.050f) ||
        (current_range == LOW_CUR && peak_a >= 0.00050f))
    {
        *quiet_blocks = 0U;
        return current_rail || current_range != LOW_CUR ? HIGH_CUR : MID_CUR;
    }
    /* 降档使用回差和连续块确认，避免负载在临界点附近来回切换。 */
    bool can_down = allow_down &&
                    ((current_range == HIGH_CUR && AUTO_ALLOW_MID && peak_a <= 0.004f) ||
                     (current_range == MID_CUR && AUTO_ALLOW_LOW && peak_a <= 0.00004f));
    if (!can_down)
    {
        *quiet_blocks = 0U;
        return current_range;
    }
    if (++(*quiet_blocks) >= AUTO_DOWN_BLOCKS)
    {
        *quiet_blocks = 0U;
        return current_range == HIGH_CUR ? MID_CUR : LOW_CUR;
    }

    return current_range;
}

/**
 * @brief 暂停采样，切换物理流路，在模拟信号稳定后恢复采样。
 * @param next_range 要切换到的物理挡位。
 * @param settle_ticks 模拟前端稳定所需的等待时间，单位为 RTOS tick。
 * @retval true 切换完成，ADC/DMA 与 TIM2 均已启动。
 * @retval false 切换或重启失败，已尝试停采并将流路拨回高挡。
 * @note 由 ADCTask 在临界区外调用，函数需要清队列并阻塞等待。
 */
static bool ADC_ChangeRange(uint8_t next_range, uint32_t settle_ticks)
{
    /* 先停 TIM2 触发与 ADC/DMA，再清掉回调已入队的旧挡位数据块。 */
    if (HAL_TIM_Base_Stop(&htim2) != HAL_OK ||
        HAL_ADC_Stop_DMA(&hadc1) != HAL_OK ||
        osMessageQueueReset(ADC_MessageQueue) != osOK)
    {
        goto failed;
    }

    /* 切换 GPIO 后让模拟前端稳定，这段时间保持停止采样。 */
    flow_route_selection(next_range);
    if (osDelay(settle_ticks) != osOK)
    {
        goto failed;
    }

    /* ADC/DMA 从缓冲区起点重新接收数据，就绪后再允许 TIM2 触发。 */
    __HAL_TIM_SET_COUNTER(&htim2, 0U);
    if (HAL_ADC_Start_DMA(&hadc1, (uint32_t *)&adc_raw_buffer[0][0],
                          ADC_DMA_ROWS * ADC_CHANNELS) != HAL_OK ||
        HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        goto failed;
    }
    return true;

failed:
    /* 尽力停止采样并把流路拨回高挡，错误状态由调用方发布。 */
    (void)HAL_TIM_Base_Stop(&htim2);
    (void)HAL_ADC_Stop_DMA(&hadc1);
    flow_route_selection(HIGH_CUR);
    return false;
}


/*创建用户Freertos任务进程*/




void KeyTask(void *argument)
{
    key_event_t key_event;
    (void)argument;

    for (;;)
    {
        /* 不等待 GUI 消费队列，保持按键消抖和长按检测的扫描节拍。 */
        if (Key_Scan(&key_event) &&
            osMessageQueuePut(Key_MessageQueue, &key_event, 0U, 0U) != osOK)
        {
            key_queue_drop_count++;
        }
        osDelay(KEY_SCAN_DELAY_TICKS);
    }
}
/**
 * @brief LCD屏幕 LVGL显示任务。
 * @param argument 任务参数，当前未使用。
 * @note LVGL对象创建和刷新集中在本任务，毫秒计数由HAL的TIM11时基提供。
 */
void LCDTestTask(void *argument)
{
    uint32_t last_refresh_tick = 0U;
    uint32_t beep_start_tick = 0U;
    bool meter_needs_refresh = true;
    bool beep_active = false;
    key_event_t key_event;

    (void)argument;

    /* 等待硬件初始化任务先完成，避免和其它外设初始化抢占 */
    osDelay(500);

    /* LVGL对象创建和刷新集中在本任务，毫秒计数由HAL的TIM11时基提供。 */
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_port_disp_init();

    ui_init();
    MeasurementView_Init();

    while(1)
    {
        /* 设置、显示和蜂鸣器由同一任务操作。长按亮度连发只调值。 */
        while (osMessageQueueGet(Key_MessageQueue, &key_event, NULL, 0U) == osOK)
        {
            if (MeasurementView_HandleKey(&key_event) && key_event.type == KEY_EVT_CLICK)
            {
                if (key_event.id == KEY_ID_B)
                {
                    meter_needs_refresh = true;
                }
                if (MeasurementView_KeySoundEnabled())
                {
                    if (!beep_active)
                    {
                        beep_open();
                        beep_active = true;
                    }
                    beep_start_tick = lv_tick_get();
                }
            }
        }

        uint32_t current_tick = lv_tick_get();
        if (beep_active && (!MeasurementView_KeySoundEnabled() ||
                            (uint32_t)(current_tick - beep_start_tick) >= KEY_BEEP_DURATION_MS))
        {
            beep_close();
            beep_active = false;
        }

        /* 返回主页立刻读取最新快照。无符号计数差兼容毫秒计数回绕。 */
        if (lv_screen_active() == ui_Meter &&
            (meter_needs_refresh ||
             (uint32_t)(current_tick - last_refresh_tick) >= MeasurementView_RefreshIntervalMs()))
        {
            measurement_t snapshot;
            meter_needs_refresh = false;
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
    /* 连续降档计数跨块保存。峰值和过载标志在每块内重新统计。 */
    uint8_t quiet_blocks = 0U;
    osStatus_t status;
    uint32_t last_drops = adc_queue_drop_count;
    (void)argument;

    for (;;)
    {
        /* DMA 半满和全满回调各提交一半缓冲区的副本，每条消息含 25 次扫描。 */
        status = osMessageQueueGet(ADC_MessageQueue, &block, NULL, ADC_QUEUE_WAIT_TICKS);

        if (status == osErrorTimeout)
        {
            /* 采样出现间断，重新累计降档条件，并让 UI 停止显示旧读数。 */
            quiet_blocks = 0U;
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

        /* 每块只读取一次实际挡位，同一挡位用于选通道和查换算系数。 */
        uint8_t current_range = Gate_get_status();
        uint8_t current_ch = ADC_IDX_HIGH;
        if (current_range == MID_CUR)
        {
            current_ch = ADC_IDX_MID;
        }
        else if (current_range == LOW_CUR)
        {
            current_ch = ADC_IDX_LOW;
        }

        uint32_t sums[ADC_CHANNELS] = {0U};
			float averages[ADC_CHANNELS];
        float peak_a = 0.0f;
        bool voltage_limit = false;
        bool ref_bad = false;
        bool current_rail = false;

        for (uint32_t sample = 0U; sample < ADC_TIMES; sample++)
        {
            const uint16_t *codes = block.samples[sample];
            uint16_t current_code = codes[current_ch];
            uint16_t ref_code = codes[ADC_IDX_REF];
            /* 同次扫描的电流码减偏置参考码，取绝对值仅用于判挡。 */
            float diff = (float)current_code - (float)ref_code;
            float sample_a = fabsf(Measurement_CurrentA(diff, current_range));

            /* 任意一个样本触及边界就标记整块，避免均值掩盖瞬时饱和。 */
            voltage_limit |= codes[ADC_IDX_VOL] >= ADC_MAX_CODE;
            ref_bad |= ref_code == 0U || ref_code >= ADC_MAX_CODE;
            current_rail |= current_code == 0U || current_code >= ADC_MAX_CODE;
            if (sample_a > peak_a)
            {
                peak_a = sample_a;
            }
            for (uint32_t ch = 0U; ch < ADC_CHANNELS; ch++)
            {
                sums[ch] += codes[ch];
            }
        }

        for (uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
        {
            averages[channel] = (float)sums[channel] / (float)ADC_TIMES;
        }

        /* 显示取整块均值换算，保留电流和功率的正负方向。 */
        measurement_t result = Measurement_CalculateRange(
            averages[ADC_IDX_VOL], averages[current_ch] - averages[ADC_IDX_REF], current_range);
        if (voltage_limit || ref_bad || current_rail)
        {
            result.status = MEASUREMENT_ADC_LIMIT;
        }
        /* 丢块或电压顶格时应中断连续降档计数，电流过载仍可触发升档。 */
        uint32_t drops = adc_queue_drop_count;
        bool allow_down = !voltage_limit && drops == last_drops;
        last_drops = drops;

        uint8_t next_range = current_range;
        if (adc_error_code != HAL_ADC_ERROR_NONE)
        {
            result.status = MEASUREMENT_ADC_ERROR;
            quiet_blocks = 0U;
        }
        else if (Gate_Get_Mode() == GATE_MODE_AUTO && AUTO_SETTLE_TICKS != 0U)
        {
            /* REF 失效后差分电流不可用，直接请求回到高挡。 */
            if (ref_bad)
            {
                next_range = HIGH_CUR;
                quiet_blocks = 0U;
            }
            else
            {
                next_range = AutoRange_Next(current_range, peak_a, current_rail, allow_down,
                                            &quiet_blocks);
            }
        }
        else
        {
            quiet_blocks = 0U;
        }

        /* 切换期间先发布过渡状态，让 UI 暂停显示测量数值。 */
        if (next_range != current_range)
        {
            result.status = MEASUREMENT_SWITCHING;
        }

        /* 用短临界区成组发布，避免 UI 同时读到不同采样块的字段。 */
        taskENTER_CRITICAL();
        for (uint32_t ch = 0U; ch < ADC_CHANNELS; ch++)
        {
            adc_average_raw[ch] = averages[ch];
        }
        adc_high_diff_raw = averages[ADC_IDX_HIGH] - averages[ADC_IDX_REF];
        latest_measurement = result;
        adc_processed_block_count++;
        taskEXIT_CRITICAL();

        /* 换挡和等待在临界区外执行。重启失败时结束采集任务并保留错误状态。 */
        if (next_range != current_range && !ADC_ChangeRange(next_range, AUTO_SETTLE_TICKS))
        {
            taskENTER_CRITICAL();
            latest_measurement.range = Gate_get_status();
            latest_measurement.status = MEASUREMENT_ADC_ERROR;
            taskEXIT_CRITICAL();
            osThreadExit();
        }
    }
}
