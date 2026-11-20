// includes

#include "tim.h"
#include "adc.h"
#include "i2c.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usb_device.h"
#include "gate.h"

#include "Hardware_InitTask.h"


// bsp
#include "key.h"






void Hardware_InitTask(void *argument)
{
    while(1)
    {
        vTaskSuspendAll();


        Key_Init();




        
        xTaskResumeAll();
        Gate_Port_Init();
        Gate_Set_Mode(GATE_MODE_HIGH);

        /* Length 是半字样本数量，共 250 个，缓冲区占 500 字节。 */
        if (HAL_ADC_Start_DMA(&hadc1,
                            (uint32_t *)&adc_raw_buffer[0][0],
                            ADC_DMA_ROWS * ADC_CHANNELS) != HAL_OK)
        {
            Error_Handler();
        }

        /* ADC 和 DMA 就绪后，再开启触发源。 */
        if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
        {
            (void)HAL_ADC_Stop_DMA(&hadc1);
            Error_Handler();
        }
		vTaskDelete(NULL);
    }
}