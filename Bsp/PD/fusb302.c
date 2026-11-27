#include <stddef.h>
#include "i2c.h"
#include "fusb302.h"

#define FUSB302_I2C_ADDR_7BIT   0x22U
#define FUSB302_I2C_TIMEOUT_MS  10U

/* FUSB302 关键寄存器地址 */
#define FUSB_REG_DEVICE_ID         0x01U
#define FUSB_REG_SWITCHES0         0x02U
#define FUSB_REG_SWITCHES1         0x03U
#define FUSB_REG_CONTROL0          0x06U
#define FUSB_REG_CONTROL1          0x07U
#define FUSB_REG_CONTROL2          0x08U
#define FUSB_REG_CONTROL3          0x09U
#define FUSB_REG_MASK              0x0AU
#define FUSB_REG_POWER             0x0BU
#define FUSB_REG_RESET             0x0CU
#define FUSB_REG_MASKA             0x0EU
#define FUSB_REG_MASKB             0x0FU
#define FUSB_REG_STATUS1A          0x3DU
#define FUSB_REG_STATUS0           0x40U
#define FUSB_REG_STATUS1           0x41U
#define FUSB_REG_FIFO              0x43U

/* Switches0、Switches1 和 Power 的配置位 */
#define FUSB_PDWN_BOTH             0x03U  /* 使能 CC1、CC2 上的 Sink 下拉 Rd */
#define FUSB_MEAS_CC1              (1U << 2)
#define FUSB_MEAS_CC2              (1U << 3)
#define FUSB_TX_CC1                (1U << 0)
#define FUSB_TX_CC2                (1U << 1)
#define FUSB_AUTO_CRC              (1U << 2)
#define FUSB_GOODCRC_PD20          (1U << 5)  /* 自动 GoodCRC 使用 PD 2.0 版本字段 */
#define FUSB_POWER_CC_DETECT       0x07U
#define FUSB_POWER_PD_RX           0x0FU  /* 收包时额外启用内部振荡器 */

/* 自动检测、FIFO 状态和报文字段 */
#define FUSB_RESET_SW_RES          (1U << 0)
#define FUSB_CONTROL1_CONFIG_MASK  0x73U  /* 保留 SOP 过滤和 BIST 配置位 */
#define FUSB_CONTROL1_RX_FLUSH     (1U << 2)
#define FUSB_CONTROL2_MODE_SNK     (2U << 1)
#define FUSB_CONTROL2_TOGGLE       (1U << 0)
#define FUSB_STATUS1A_TOGSS_SHIFT  3U
#define FUSB_STATUS1A_TOGSS_MASK   0x07U
#define FUSB_STATUS1_RX_EMPTY      (1U << 5)
#define FUSB_RX_TOKEN_MASK         0xE0U
#define FUSB_RX_TOKEN_SOP          0xE0U
#define PD_HEADER_EXTENDED         (1U << 15)
#define PD_HEADER_OBJECT_SHIFT     12U
#define PD_HEADER_OBJECT_MASK      0x07U
#define PD_OBJECT_BYTES            4U
#define PD_CRC_BYTES               4U

/* 单字节寄存器读取和 FIFO 连续取字节共用 I²C 访问。
 * 读 FIFO 时，寄存器地址保持在 0x43，芯片按顺序弹出数据。
 */
static HAL_StatusTypeDef FUSB302_ReadReg(uint8_t reg, uint8_t *data, uint16_t size)
{
    return HAL_I2C_Mem_Read(&hi2c1, FUSB302_I2C_ADDR_7BIT << 1U,
                           reg, I2C_MEMADD_SIZE_8BIT,
                           data, size, FUSB302_I2C_TIMEOUT_MS);
}

/* 写入单字节寄存器，保留 HAL 的 I²C 错误状态。 */
static HAL_StatusTypeDef FUSB302_WriteReg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c1, FUSB302_I2C_ADDR_7BIT << 1U,
                            reg, I2C_MEMADD_SIZE_8BIT,
                            &value, 1U, FUSB302_I2C_TIMEOUT_MS);
}

/* 一旦取走部分报文，FIFO 读指针就无法回退。清空后从下一帧重新开始。 */
static HAL_StatusTypeDef FUSB302_RecoverRxFifo(HAL_StatusTypeDef packet_status)
{
    uint8_t control1;
    HAL_StatusTypeDef status = FUSB302_ReadReg(FUSB_REG_CONTROL1, &control1, 1U);
    if (status != HAL_OK)
    {
        return status;
    }

    status = FUSB302_WriteReg(FUSB_REG_CONTROL1,
                             (control1 & FUSB_CONTROL1_CONFIG_MASK) | FUSB_CONTROL1_RX_FLUSH);
    if (status != HAL_OK)
    {
        return status;
    }

    return packet_status;
}

HAL_StatusTypeDef FUSB302_StartSinkDetection(void)
{
    HAL_StatusTypeDef status = FUSB302_WriteReg(FUSB_REG_RESET, FUSB_RESET_SW_RES);
    if (status != HAL_OK)
    {
        return status;
    }

    status = FUSB302_WriteReg(FUSB_REG_POWER, FUSB_POWER_CC_DETECT);
    if (status != HAL_OK)
    {
        return status;
    }

    return FUSB302_WriteReg(FUSB_REG_CONTROL2,
                             FUSB_CONTROL2_MODE_SNK | FUSB_CONTROL2_TOGGLE);
}

HAL_StatusTypeDef FUSB302_ReadToggleStatus(uint8_t *togss)
{
    HAL_StatusTypeDef status;
    uint8_t status1a;

    if (togss == NULL)
    {
        return HAL_ERROR;
    }

    status = FUSB302_ReadReg(FUSB_REG_STATUS1A, &status1a, 1U);
    if (status == HAL_OK)
    {
        *togss = (uint8_t)((status1a >> FUSB_STATUS1A_TOGSS_SHIFT) &
                           FUSB_STATUS1A_TOGSS_MASK);
    }

    return status;
}

HAL_StatusTypeDef FUSB302_ConfigurePdRx(uint8_t togss)
{
    uint8_t measure_cc;
    uint8_t tx_cc;
    HAL_StatusTypeDef status;

    switch (togss)
    {
        case FUSB302_TOGSS_SNK_CC1:
            measure_cc = FUSB_MEAS_CC1;
            tx_cc = FUSB_TX_CC1;
            break;

        case FUSB302_TOGSS_SNK_CC2:
            measure_cc = FUSB_MEAS_CC2;
            tx_cc = FUSB_TX_CC2;
            break;

        default:
            return HAL_ERROR;
    }

    status = FUSB302_WriteReg(FUSB_REG_POWER, FUSB_POWER_PD_RX);
    if (status != HAL_OK)
    {
        return status;
    }

    /* 退出自动检测前先配置 Rd，接管 CC 时保持 Sink 端接。 */
    status = FUSB302_WriteReg(FUSB_REG_SWITCHES0, FUSB_PDWN_BOTH | measure_cc);
    if (status != HAL_OK)
    {
        return status;
    }

    status = FUSB302_WriteReg(FUSB_REG_CONTROL2, FUSB_CONTROL2_MODE_SNK);
    if (status != HAL_OK)
    {
        return status;
    }

    /* 在活动 CC 上启用 BMC 发射，芯片才能及时回送 GoodCRC。 */
    return FUSB302_WriteReg(FUSB_REG_SWITCHES1,
                             FUSB_GOODCRC_PD20 | FUSB_AUTO_CRC | tx_cc);
}

HAL_StatusTypeDef FUSB302_ReadPacket(FUSB302_Packet_t *packet, bool *received)
{
    uint8_t status1;
    uint8_t prefix[3];  /* 1 字节 SOP token 和 2 字节 PD 报文头 */
    uint8_t body[FUSB302_MAX_DATA_OBJECTS * PD_OBJECT_BYTES + PD_CRC_BYTES];
    uint16_t header;
    uint8_t count;
    HAL_StatusTypeDef status;

    if (packet == NULL || received == NULL)
    {
        return HAL_ERROR;
    }

    *received = false;
    status = FUSB302_ReadReg(FUSB_REG_STATUS1, &status1, 1U);
    if (status != HAL_OK)
    {
        return status;
    }
    if ((status1 & FUSB_STATUS1_RX_EMPTY) != 0U)
    {
        return HAL_OK;
    }

    status = FUSB302_ReadReg(FUSB_REG_FIFO, prefix, sizeof(prefix));
    if (status != HAL_OK)
    {
        return FUSB302_RecoverRxFifo(status);
    }

    /* SOP token 的低五位未定义。这里只接收充电器发来的 SOP 报文。 */
    if ((prefix[0] & FUSB_RX_TOKEN_MASK) != FUSB_RX_TOKEN_SOP)
    {
        return FUSB302_RecoverRxFifo(HAL_ERROR);
    }

    header = (uint16_t)prefix[1] | ((uint16_t)prefix[2] << 8U);
    if ((header & PD_HEADER_EXTENDED) != 0U)
    {
        /* 扩展报文需要按扩展头处理长度，不能套用普通对象数的读法。 */
        return FUSB302_RecoverRxFifo(HAL_ERROR);
    }
    count = (uint8_t)((header >> PD_HEADER_OBJECT_SHIFT) & PD_HEADER_OBJECT_MASK);

    /* CRC 由芯片校验，这里仍要读完尾部四字节，让 FIFO 对齐下一帧。 */
    status = FUSB302_ReadReg(FUSB_REG_FIFO, body, count * PD_OBJECT_BYTES + PD_CRC_BYTES);
    if (status != HAL_OK)
    {
        return FUSB302_RecoverRxFifo(status);
    }

    packet->header = header;
    packet->object_count = count;
    for (uint8_t i = 0U; i < count; ++i)
    {
        const uint8_t *data = &body[i * PD_OBJECT_BYTES];

        packet->data_objects[i] = (uint32_t)data[0] |
                                  ((uint32_t)data[1] << 8U) |
                                  ((uint32_t)data[2] << 16U) |
                                  ((uint32_t)data[3] << 24U);
    }

    *received = true;
    return HAL_OK;
}
