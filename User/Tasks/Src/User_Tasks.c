/* USER CODE BEGIN Include */
#include "Task_Init.h"
#include "key.h"
#include "beep.h"
#include "lcd.h"
#include "lcd_init.h"






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
    (void)argument;

    /* 等待硬件初始化任务先完成，避免和其它外设初始化抢占 */
    osDelay(500);

    /* 打开LCD背光PWM，并设置一个适中的亮度 */
    LCD_Open_Light();
    LCD_Set_Light(80);

    /* 初始化ST7789屏幕控制器和LCD控制引脚 */
    LCD_Init();

    while(1)
    {
        /* 全屏刷红色，用于确认屏幕写GRAM正常 */
        LCD_Fill(0, 0, LCD_W, LCD_H, RED);
        osDelay(1000);

        /* 全屏刷绿色，用于确认RGB565颜色顺序正常 */
        LCD_Fill(0, 0, LCD_W, LCD_H, GREEN);
        osDelay(1000);

        /* 全屏刷蓝色，用于确认三基色显示正常 */
        LCD_Fill(0, 0, LCD_W, LCD_H, BLUE);
        osDelay(1000);

        /* 全屏刷白色，用于确认背光和显示区域完整 */
        LCD_Fill(0, 0, LCD_W, LCD_H, WHITE);
        osDelay(1000);

        /* 全屏刷黑色，用于确认清屏效果 */
        LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
        osDelay(1000);
    }
}