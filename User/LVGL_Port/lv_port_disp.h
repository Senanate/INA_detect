#ifndef __LV_PORT_DISP_H__
#define __LV_PORT_DISP_H__

#include <stdbool.h>
#include <stdint.h>

#define LCD_DEFAULT_BACKLIGHT_PERCENT 80U

/**
 * @brief 初始化ST7789并注册LVGL显示设备。
 * @note 调用前需完成SPI、PWM外设初始化及lv_init()，由显示任务调用一次。
 *       显示设备创建失败时进入工程统一错误处理。
 */
void lv_port_disp_init(void);

/** @brief 设置显示背光。percent 应为 5 到 100，超出范围时维持当前值。 */
void lv_port_disp_set_backlight(uint8_t percent);

/**
 * @brief 从显示任务切换 ST7789 方向，成功后请求当前页面整屏重绘。
 * @param degrees 方向角度，只接受 0、90、180、270。
 * @return 命令发送成功返回 true，参数无效或 SPI 失败返回 false。
 */
bool lv_port_disp_set_rotation(uint16_t degrees);

/** @return 最近一次成功设置的硬件方向角度。 */
uint16_t lv_port_disp_rotation(void);

#endif /* __LV_PORT_DISP_H__ */
