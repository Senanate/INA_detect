#include <stddef.h>
#include "i2c.h"

#define FUSB302_I2C_ADDR_7BIT   0x22U
#define FUSB302_I2C_TIMEOUT_MS  10

/* FUSB302 关键寄存器地址 */
#define FUSB_REG_DEVICE_ID      0x01
#define FUSB_REG_SWITCHES0      0x02
#define FUSB_REG_SWITCHES1      0x03
#define FUSB_REG_CONTROL0       0x06
#define FUSB_REG_CONTROL1       0x07
#define FUSB_REG_CONTROL2       0x08
#define FUSB_REG_CONTROL3       0x09
#define FUSB_REG_MASK           0x0A
#define FUSB_REG_POWER          0x0B
#define FUSB_REG_RESET          0x0C
#define FUSB_REG_MASKA          0x3E
#define FUSB_REG_MASKB          0x3F
#define FUSB_REG_STATUS0        0x40

/* FUSB302 寄存器位定义 */
#define FUSB_REG_STATUS1A          0x3DU
#define FUSB_RESET_SW_RES          (1U << 0)
#define FUSB_POWER_CC_DETECT       0x07U
#define FUSB_CONTROL2_MODE_SNK     (2U << 1) 
#define FUSB_CONTROL2_TOGGLE       (1U << 0)
#define FUSB_STATUS1A_TOGSS_SHIFT  3U
#define FUSB_STATUS1A_TOGSS_MASK   0x07U



static HAL_StatusTypeDef FUSB302_ReadReg(uint8_t reg, uint8_t *value)
{
    return HAL_I2C_Mem_Read(&hi2c1, FUSB302_I2C_ADDR_7BIT << 1U,
                           reg, I2C_MEMADD_SIZE_8BIT,
                           value, 1U, FUSB302_I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef FUSB302_WriteReg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c1, FUSB302_I2C_ADDR_7BIT << 1U,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            &value, 1U, FUSB302_I2C_TIMEOUT_MS);
}


/* 启动 Sink 自动检测。HAL_OK 表示配置已写入，检测结果需随后读取。 */
HAL_StatusTypeDef FUSB302_StartSinkDetection()
{   
   HAL_StatusTypeDef status;
   status = FUSB302_WriteReg(FUSB_REG_RESET, FUSB_RESET_SW_RES); // 复位FUSB302
   if (status != HAL_OK) {
       return status;
   }

   status = FUSB302_WriteReg(FUSB_REG_POWER, FUSB_POWER_CC_DETECT); // 使能CC检测
   if(status != HAL_OK) {
       return status;
   }
   return FUSB302_WriteReg(FUSB_REG_CONTROL2, FUSB_CONTROL2_MODE_SNK | FUSB_CONTROL2_TOGGLE); // 设置为Sink模式并启动Toggle

}


/* 读取 TOGSS。仅在返回 HAL_OK 时更新 *togss。
 * 0 表示仍在检测，5 表示 Sink on CC1，6 表示 Sink on CC2。
 */
HAL_StatusTypeDef FUSB302_ReadToggleStatus(uint8_t *togss)
{
    HAL_StatusTypeDef status;
    uint8_t status1a;

    if (togss == NULL)
    {
        return HAL_ERROR;
    }

    status = FUSB302_ReadReg(FUSB_REG_STATUS1A, &status1a);
    if (status == HAL_OK)
    {
        *togss = (uint8_t)((status1a >> FUSB_STATUS1A_TOGSS_SHIFT) &
                           FUSB_STATUS1A_TOGSS_MASK);
    }

    return status;
}