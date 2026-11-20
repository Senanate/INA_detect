#ifndef __TASK_INIT_H__
#define __TASK_INIT_H__

#include "FreeRTOS.h"
#include "cmsis_os.h"

extern osMessageQueueId_t Key_MessageQueue; // Key task 中发送出的按键信息的消息队列
extern osMessageQueueId_t ADC_MessageQueue; // ADC DMA采集数据队列

void Task_Init(void *argument);

#endif

