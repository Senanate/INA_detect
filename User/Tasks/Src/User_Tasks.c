/* USER CODE BEGIN Include */
#include "Task_Init.h"
#include "key.h"
#include "beep.h"
#include "lcd_init.h"
#include "User_Tasks.h"
#include "adc.h"
#include "task.h"
#include "lvgl.h"
#include "lv_port_disp.h"

//公共变量定义区
#define ADC_QUEUE_WAIT_TICKS 100U
#define LCD_ADC_REFRESH_MS    200U // LVGL使用HAL毫秒时基，每200ms更新采样数值
#define LCD_LVGL_DELAY_TICKS  5U   // 当前系统tick为1ms，周期处理LVGL刷新和定时器
#define LCD_ADC_MARGIN       12U
#define LCD_ADC_FIRST_ROW_Y   48U
#define LCD_ADC_ROW_HEIGHT    34U
#define LCD_ADC_VALUE_X       132U

volatile float adc_average_raw[ADC_CHANNELS] = {0};
volatile float adc_high_diff_raw = 0.0f;

volatile uint32_t adc_processed_block_count = 0U;
volatile uint32_t adc_timeout_count = 0U;


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
    /* 标签顺序与ADC的五通道扫描顺序一致 */
    static const char * const channel_labels[ADC_CHANNELS] = {
        "PA5 VOL", "PA6 LOW", "PA7 MID", "PB0 HIGH", "PB1 REF"
    };
    lv_obj_t *adc_labels[ADC_CHANNELS];
    float adc_snapshot[ADC_CHANNELS];
    lv_obj_t *screen;
    lv_obj_t *title;
    uint32_t last_refresh_tick;

    (void)argument;

    /* 等待硬件初始化任务先完成，避免和其它外设初始化抢占 */
    osDelay(500);

    /* LVGL对象创建和刷新集中在本任务，毫秒计数由HAL的TIM11时基提供。 */
    lv_init();
    lv_tick_set_cb(HAL_GetTick);
    lv_port_disp_init();

    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    title = lv_label_create(screen);
    if (title == NULL)
    {
        Error_Handler();
    }
    lv_label_set_text(title, "ADC RAW (HIGH)");
    lv_obj_set_pos(title, LCD_ADC_MARGIN, LCD_ADC_MARGIN);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    /* 标签和数值控件创建一次，LVGL按实际变化区域刷新屏幕。 */
    for(uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
    {
        uint16_t row_y = LCD_ADC_FIRST_ROW_Y + channel * LCD_ADC_ROW_HEIGHT;
        lv_obj_t *name_label = lv_label_create(screen);
        adc_labels[channel] = lv_label_create(screen);
        if (name_label == NULL || adc_labels[channel] == NULL)
        {
            Error_Handler();
        }

        lv_label_set_text(name_label, channel_labels[channel]);
        lv_obj_set_pos(name_label, LCD_ADC_MARGIN, row_y);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0xFFFFFF), 0);

        lv_label_set_text(adc_labels[channel], "0.0");
        lv_obj_set_pos(adc_labels[channel], LCD_ADC_VALUE_X, row_y);
        lv_obj_set_width(adc_labels[channel], LCD_W - LCD_ADC_VALUE_X - LCD_ADC_MARGIN);
        lv_obj_set_style_text_align(adc_labels[channel], LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_font(adc_labels[channel], &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(adc_labels[channel], lv_color_hex(0xFFFF00), 0);
    }

    /* 利用无符号计数差使首次循环立即刷新，同时兼容毫秒计数回绕。 */
    last_refresh_tick = lv_tick_get() - LCD_ADC_REFRESH_MS;
    while(1)
    {
        uint32_t current_tick = lv_tick_get();
        if ((uint32_t)(current_tick - last_refresh_tick) >= LCD_ADC_REFRESH_MS)
        {
            last_refresh_tick = current_tick;

            /* 短临界区内拷贝五通道均值，LVGL操作放在临界区外。 */
            taskENTER_CRITICAL();
            for(uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
            {
                adc_snapshot[channel] = adc_average_raw[channel];
            }
            taskEXIT_CRITICAL();

            for(uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
            {
                /* 整数拆分保留一位小数，覆盖12位ADC范围并适配LV_USE_FLOAT=0。 */
                uint32_t scaled_value = (uint32_t)(adc_snapshot[channel] * 10.0f + 0.5f);
                lv_label_set_text_fmt(adc_labels[channel], "%lu.%lu",
                                       (unsigned long)(scaled_value / 10U),
                                       (unsigned long)(scaled_value % 10U));
            }
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
        status = osMessageQueueGet(ADC_MessageQueue,
                                   &block,
                                   NULL,
                                   ADC_QUEUE_WAIT_TICKS);

        if (status == osErrorTimeout)
        {
            adc_timeout_count++;
            continue;
        }

        if (status != osOK)
        {
            Error_Handler();
        }

        for (uint32_t channel = 0U;
             channel < ADC_CHANNELS;
             channel++)
        {
            uint32_t sum = 0U;

            for (uint32_t sample = 0U;
                 sample < ADC_TIMES;
                 sample++)
            {
                sum += block.samples[sample][channel];
            }

            adc_average_raw[channel] =
                (float)sum / (float)ADC_TIMES;
        }

        /* 保留符号，便于检查零点偏置和信号方向。 */
        adc_high_diff_raw =
            adc_average_raw[ADC_IDX_HIGH] -
            adc_average_raw[ADC_IDX_REF];

        adc_processed_block_count++;
    }
}
