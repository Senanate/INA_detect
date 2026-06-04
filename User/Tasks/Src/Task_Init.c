#include "Task_Init.h"
#include "Hardware_InitTask.h"

// bsp
#include "key.h"


//user任务
#include "User_Tasks.h"
/*队列定义-------------------------*/
// Key task 中发送出的按键信息的消息队列
// 流向为 KeyTask -> LVGLtask
osMessageQueueId_t Key_MessageQueue;

/* Tasks ---------------------------------------------------------------------*/
//硬件初始化任务
osThreadId_t Hardware_Init_Handle;
const osThreadAttr_t Hardware_Init_attributes = {
  .name = "Hardware_InitTask",
  .stack_size = 128 * 10,
  .priority = (osPriority_t) osPriorityHigh,
};


//按键任务初始化
osThreadId_t KeyTaskHandle;
const osThreadAttr_t KeyTask_attributes = {
  .name = "KeyTask",
  .stack_size = 128 * 10,
  .priority = (osPriority_t) osPriorityNormal,
};

//LCD测试任务初始化
osThreadId_t LCDTestTaskHandle;
const osThreadAttr_t LCDTestTask_attributes = {
  .name = "LCDTestTask",
  .stack_size = 128 * 10,
  .priority = (osPriority_t) osPriorityLow,
};






void Task_Init(void *argument)
{
    /*创建消息队列*/
    Key_MessageQueue = osMessageQueueNew(4, sizeof(key_event_t), NULL);

    /*创建任务*/
    Hardware_Init_Handle = osThreadNew(Hardware_InitTask, NULL, &Hardware_Init_attributes);
    KeyTaskHandle = osThreadNew(KeyTask, NULL, &KeyTask_attributes);
    LCDTestTaskHandle = osThreadNew(LCDTestTask, NULL, &LCDTestTask_attributes);

}