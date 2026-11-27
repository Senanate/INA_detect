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
#define FUSB_CONTROL3_AUTO_RETRY   (1U << 0)
#define FUSB_CONTROL3_N_RETRIES_2  (2U << 1)
#define FUSB_CONTROL3_N_RETRIES_3  (3U << 1)
#define FUSB_STATUS1A_TOGSS_SHIFT  3U
#define FUSB_STATUS1A_TOGSS_MASK   0x07U
#define FUSB_STATUS1_RX_EMPTY      (1U << 5)
#define FUSB_RX_TOKEN_MASK         0xE0U
#define FUSB_RX_TOKEN_SOP          0xE0U
#define PD_HEADER_EXTENDED         (1U << 15)
#define PD_HEADER_OBJECT_SHIFT     12U
#define PD_HEADER_OBJECT_MASK      0x07U
#define PD_HEADER_MSG_ID_SHIFT     9U
#define PD_HEADER_MSG_ID_MASK      0x07U
#define PD_OBJECT_BYTES            4U
#define PD_CRC_BYTES               4U

/* FUSB302 TX FIFO 控制令牌 */
#define FUSB_TX_TOKEN_SYNC1        0x12U
#define FUSB_TX_TOKEN_SYNC2        0x13U
#define FUSB_TX_TOKEN_PACKSYM      0x80U   /* 低 5 位填发送字节数 (Header + Data) */
#define FUSB_TX_TOKEN_JAM_CRC      0xFFU   /* 硬件自动附加 CRC32 */
#define FUSB_TX_TOKEN_EOP          0x14U   /* 帧结束符 */
#define FUSB_TX_TOKEN_TXOFF        0xFEU   /* 发完关闭发送器 */
#define FUSB_TX_TOKEN_TXON         0xA1U   /* 立即启动发送 */

/* PD Request 报文常量 */
#define PD_MSG_REQUEST             0x02U
#define PD_REQUEST_PAYLOAD_BYTES   (2U + PD_OBJECT_BYTES)

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

/* 连续写入数据到 TX FIFO */
static HAL_StatusTypeDef FUSB302_WriteFifo(uint8_t *data, uint16_t size)
{
    return HAL_I2C_Mem_Write(&hi2c1, FUSB302_I2C_ADDR_7BIT << 1U,
                            FUSB_REG_FIFO, I2C_MEMADD_SIZE_8BIT,
                            data, size, FUSB302_I2C_TIMEOUT_MS);
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

    /* Sink 模式使能事件上报，保持 AUTO_PRE 关闭。 */
    status = FUSB302_WriteReg(FUSB_REG_CONTROL0, 0U);
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

HAL_StatusTypeDef FUSB302_SendRequestRdo(uint32_t rdo, uint16_t spec_rev,
                                       uint8_t message_id)
{
    uint16_t header;
    uint8_t tx_buf[15];
    uint8_t retries;
    HAL_StatusTypeDef status;

    if ((spec_rev != PD_SPEC_REV_20 && spec_rev != PD_SPEC_REV_30) ||
        message_id > PD_HEADER_MSG_ID_MASK)
    {
        return HAL_ERROR;
    }

    /* Sink/UFP 的角色位均为 0，发送序号由调用方随协商状态维护。 */
    header = (uint16_t)((1U << PD_HEADER_OBJECT_SHIFT) |
                        ((uint16_t)message_id << PD_HEADER_MSG_ID_SHIFT) |
                        spec_rev | PD_MSG_REQUEST);

    /* 1. SOP 同步序列 (3×SYNC1 + 1×SYNC2) */
    tx_buf[0]  = FUSB_TX_TOKEN_SYNC1;
    tx_buf[1]  = FUSB_TX_TOKEN_SYNC1;
    tx_buf[2]  = FUSB_TX_TOKEN_SYNC1;
    tx_buf[3]  = FUSB_TX_TOKEN_SYNC2;

    /* 2. 数据长度令牌: 2B Header + 4B RDO = 6 字节 */
    tx_buf[4]  = FUSB_TX_TOKEN_PACKSYM | PD_REQUEST_PAYLOAD_BYTES;

    /* 3. Header (小端) */
    tx_buf[5]  = (uint8_t)(header & 0xFFU);
    tx_buf[6]  = (uint8_t)((header >> 8U) & 0xFFU);

    /* 4. RDO (小端) 32位 */
    tx_buf[7]  = (uint8_t)(rdo & 0xFFU);
    tx_buf[8]  = (uint8_t)((rdo >> 8U) & 0xFFU);
    tx_buf[9]  = (uint8_t)((rdo >> 16U) & 0xFFU);
    tx_buf[10] = (uint8_t)((rdo >> 24U) & 0xFFU);

    /* 5. 硬件生成 CRC + EOP + 关闭发送器 + 启动发送 */
    tx_buf[11] = FUSB_TX_TOKEN_JAM_CRC;
    tx_buf[12] = FUSB_TX_TOKEN_EOP;
    tx_buf[13] = FUSB_TX_TOKEN_TXOFF;
    tx_buf[14] = FUSB_TX_TOKEN_TXON;

    /* 硬件重发保持同一个 MessageID，协议复位由 PD 任务处理。 */
    retries = spec_rev == PD_SPEC_REV_30 ? FUSB_CONTROL3_N_RETRIES_2 :
              FUSB_CONTROL3_N_RETRIES_3;
    status = FUSB302_WriteReg(FUSB_REG_CONTROL3, FUSB_CONTROL3_AUTO_RETRY | retries);
    if (status != HAL_OK)
    {
        return status;
    }

    return FUSB302_WriteFifo(tx_buf, sizeof(tx_buf));
}

#define FUSB_REG_INTERRUPTA       0x3EU
#define FUSB_INTA_HARDRST         (1U << 0)
#define FUSB_INTA_TXSENT          (1U << 2)
#define FUSB_INTA_RETRYFAIL       (1U << 4)
#define FUSB_STATUS0_VBUSOK       (1U << 7)
#define FUSB_STATUS0_ACTIVITY     (1U << 6)
#define FUSB_STATUS0_BC_LVL_3A    0x03U
#define FUSB_RESET_PD_RESET       (1U << 1)

HAL_StatusTypeDef FUSB302_ReadPdStatus(FUSB302_PdStatus_t *status)
{
    uint8_t regs[5]; /* InterruptA、InterruptB、Status0、Status1、Interrupt */
    if (status == NULL)
    {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef result = FUSB302_ReadReg(FUSB_REG_INTERRUPTA, regs, sizeof(regs));
    if (result != HAL_OK)
    {
        return result;
    }

    status->hard_reset = (regs[0] & FUSB_INTA_HARDRST) != 0U;
    status->tx_acked = (regs[0] & FUSB_INTA_TXSENT) != 0U;
    status->tx_failed = (regs[0] & FUSB_INTA_RETRYFAIL) != 0U;
    status->vbus_ok = (regs[2] & FUSB_STATUS0_VBUSOK) != 0U;
    status->sink_tx_ok =
        (regs[2] & (FUSB_STATUS0_ACTIVITY | FUSB_STATUS0_BC_LVL_3A)) ==
        FUSB_STATUS0_BC_LVL_3A;
    return HAL_OK;
}

HAL_StatusTypeDef FUSB302_ResetPd(void)
{
    return FUSB302_WriteReg(FUSB_REG_RESET, FUSB_RESET_PD_RESET);
}
