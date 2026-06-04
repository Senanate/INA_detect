// includes

#include "tim.h"
#include "adc.h"
#include "i2c.h"


#include "Hardware_InitTask.h"


// bsp
#include "key.h"






void Hardware_InitTask(void *argument)
{
    while(1)
    {
        vTaskSuspendAll();


        MX_USB_DEVICE_Init(); 
        Key_Init();




        
        xTaskResumeAll();
		vTaskDelete(NULL);
    }
}