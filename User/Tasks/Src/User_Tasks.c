/* USER CODE BEGIN Include */
#include "Task_Init.h"
#include "key.h"
#include "beep.h"
#include "lcd.h"
#include "lcd_init.h"
#include "User_Tasks.h"
#include "adc.h"
#include "task.h"

//公共变量定义区
#define ADC_QUEUE_WAIT_TICKS 100U
#define LCD_ADC_REFRESH_TICKS 200U // 当前系统tick为1ms，数值约每200ms刷新一次
#define LCD_ADC_FONT_SIZE     24U
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
void LCDTestTask(void *argument)
{
    /* 标签顺序与ADC的五通道扫描顺序一致 */
    static const char * const channel_labels[ADC_CHANNELS] = {
        "PA5 VOL", "PA6 LOW", "PA7 MID", "PB0 HIGH", "PB1 REF"
    };
    float adc_snapshot[ADC_CHANNELS];
    const uint16_t digit_width = LCD_ADC_FONT_SIZE / 2U;

    (void)argument;

    /* 等待硬件初始化任务先完成，避免和其它外设初始化抢占 */
    osDelay(500);

    /* 打开LCD背光PWM，并设置一个适中的亮度 */
    LCD_Open_Light();
    LCD_Set_Light(80);

    /* 初始化ST7789屏幕控制器和LCD控制引脚 */
    LCD_Init();
    LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
    LCD_ShowString(12, 12, (const uint8_t *)"ADC RAW (HIGH)",
                   WHITE, BLACK, 16, 0);

    /* 固定标签只绘制一次，循环中仅更新数值，减少刷屏闪烁 */
    for(uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
    {
        uint16_t row_y = LCD_ADC_FIRST_ROW_Y + channel * LCD_ADC_ROW_HEIGHT;
        LCD_ShowString(12, row_y, (const uint8_t *)channel_labels[channel],
                       WHITE, BLACK, LCD_ADC_FONT_SIZE, 0);
    }

    while(1)
    {
        /* 短临界区内拷贝五通道均值，防止ADC任务在读取中途更新数据。
           LCD绘制放在临界区外，避免阻塞DMA中断。 */
        taskENTER_CRITICAL();
        for(uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
        {
            adc_snapshot[channel] = adc_average_raw[channel];
        }
        taskEXIT_CRITICAL();

        for(uint32_t channel = 0U; channel < ADC_CHANNELS; channel++)
        {
            uint16_t row_y = LCD_ADC_FIRST_ROW_Y + channel * LCD_ADC_ROW_HEIGHT;
            /* 原始均值保留一位小数，32位中间值可覆盖0~4095的完整范围 */
            uint32_t scaled_value = (uint32_t)(adc_snapshot[channel] * 10.0f + 0.5f);

            /* 固定四位整数显示区会覆盖旧字符，数值变小时也能清除残影 */
            LCD_ShowIntNum(LCD_ADC_VALUE_X, row_y, (uint16_t)(scaled_value / 10U),
                           4, YELLOW, BLACK, LCD_ADC_FONT_SIZE);
            LCD_ShowChar(LCD_ADC_VALUE_X + 4U * digit_width, row_y, '.',
                         YELLOW, BLACK, LCD_ADC_FONT_SIZE, 0);
            LCD_ShowIntNum(LCD_ADC_VALUE_X + 5U * digit_width, row_y,
                           (uint16_t)(scaled_value % 10U), 1,
                           YELLOW, BLACK, LCD_ADC_FONT_SIZE);
        }

        osDelay(LCD_ADC_REFRESH_TICKS);
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
