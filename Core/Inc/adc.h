/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#define ADC_CHANNELS       5U //5个通道ADC采样
#define ADC_TIMES          25U //每次采样25次(2.5ms)
#define ADC_DMA_ROWS       (ADC_TIMES * 2U) 

//ADC采样通道顺序 rank1=PA5, rank2=PA6, rank3=PA7, rank4=PB0, rank5=PB1
#define ADC_IDX_VOL        0U
#define ADC_IDX_LOW         1U
#define ADC_IDX_MID         2U
#define ADC_IDX_HIGH        3U
#define ADC_IDX_REF         4U

/* 每个队列消息保存一半 DMA 缓冲区的数据。 */
typedef struct
{
    uint16_t samples[ADC_TIMES][ADC_CHANNELS];
} ADC_Block_t;

extern uint16_t adc_raw_buffer[ADC_DMA_ROWS][ADC_CHANNELS];

extern volatile uint32_t adc_queue_drop_count;
extern volatile uint32_t adc_error_code;
/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

