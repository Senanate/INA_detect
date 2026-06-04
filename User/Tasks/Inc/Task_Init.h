#ifndef __TASK_INIT_H__
#define __TASK_INIT_H__

#include "FreeRTOS.h"
#include "cmsis_os.h"

extern osMessageQueueId_t Key_MessageQueue;


void Task_Init(void *argument);

#endif

