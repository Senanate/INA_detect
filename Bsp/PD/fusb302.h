#ifndef FUSB302_H
#define FUSB302_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"

#define FUSB302_MAX_DATA_OBJECTS  7U
#define FUSB302_TOGSS_SNK_CC1     5U
#define FUSB302_TOGSS_SNK_CC2     6U

/* PD 报文头中已移到位 [7:6] 的版本字段。 */
#define PD_SPEC_REV_20             (1U << 6)
#define PD_SPEC_REV_30             (2U << 6)

/* 普通 SOP 报文，data_objects 中仅前 object_count 项有效。 */
typedef struct
{
    uint16_t header;
    uint8_t object_count;
    uint32_t data_objects[FUSB302_MAX_DATA_OBJECTS];
} FUSB302_Packet_t;

/**
 * @brief 复位 FUSB302 并启动 Sink 自动检测，每轮连接开始时调用一次。
 * @return HAL_OK 表示检测已启动，其余返回值表示 I²C 写入失败。
 */
HAL_StatusTypeDef FUSB302_StartSinkDetection(void);

/**
 * @brief 读取 CC 自动检测结果。
 * @param togss 输出 TOGSS 原始值，0 表示检测中，5 和 6 分别对应 CC1 和 CC2。
 * @return HAL_OK 表示输出有效，空指针返回 HAL_ERROR，I²C 错误原样返回。
 */
HAL_StatusTypeDef FUSB302_ReadToggleStatus(uint8_t *togss);

/**
 * @brief 选定活动 CC 并启用 PD 接收及自动 GoodCRC。
 * @param togss 取 FUSB302_TOGSS_SNK_CC1 或 FUSB302_TOGSS_SNK_CC2。
 * @return HAL_OK 表示配置完成，非法方向返回 HAL_ERROR，I²C 错误原样返回。
 */
HAL_StatusTypeDef FUSB302_ConfigurePdRx(uint8_t togss);

/**
 * @brief 轮询 RX FIFO，最多读取一帧普通 SOP 报文。由同一个 PD 任务调用。
 * @param packet 输出报文，收到完整帧时更新。
 * @param received 返回 HAL_OK 时，true 表示收到一帧，false 表示 FIFO 为空。
 * @return HAL_OK 表示轮询完成，空指针或不支持的帧返回 HAL_ERROR，I²C 错误原样返回。
 * @note 取走部分报文后出错会尝试清空 RX FIFO。若清空也失败，返回清空时的错误。
 */
HAL_StatusTypeDef FUSB302_ReadPacket(FUSB302_Packet_t *packet, bool *received);

/**
 * @brief 启动单数据对象的 SOP Request 发送。
 * @param rdo 根据 Source_Capabilities 组装的 32 位 Request Data Object。
 * @param spec_rev 已移位的版本字段，取 PD_SPEC_REV_20 或 PD_SPEC_REV_30。
 * @param message_id 由 PD 任务维护的发送序号，范围为 0 到 7。
 * @return HAL_OK 表示 TX FIFO 写入并启动发送。非法参数返回 HAL_ERROR，I²C 错误原样返回。
 * @note 由同一个 PD 任务串行调用。等待发送结果后再提交下一包，I²C 写入失败后重新初始化收发。
 */
HAL_StatusTypeDef FUSB302_SendRequestRdo(uint32_t rdo, uint16_t spec_rev,
                                       uint8_t message_id);

typedef struct
{
    bool vbus_ok;
    bool sink_tx_ok;
    bool tx_acked;
    bool tx_failed;
    bool hard_reset;
} FUSB302_PdStatus_t;

/**
 * @brief 读取 PD 状态并清除已读事件，由 PDTask 独占调用。
 * @param status 状态输出。
 * @return HAL_OK 表示输出有效，空指针返回 HAL_ERROR，I²C 错误原样返回。
 */
HAL_StatusTypeDef FUSB302_ReadPdStatus(FUSB302_PdStatus_t *status);

/** 复位 PD 收发逻辑。失败时返回 HAL 的 I²C 错误。 */
HAL_StatusTypeDef FUSB302_ResetPd(void);

#endif /* FUSB302_H */
