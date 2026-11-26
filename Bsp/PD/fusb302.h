#ifndef FUSB302_H
#define FUSB302_H

#include "stm32f4xx_hal.h"

HAL_StatusTypeDef FUSB302_StartSinkDetection(void);
HAL_StatusTypeDef FUSB302_ReadToggleStatus(uint8_t *togss);



#endif /* FUSB302_H */