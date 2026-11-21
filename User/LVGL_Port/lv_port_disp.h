#ifndef __LV_PORT_DISP_H__
#define __LV_PORT_DISP_H__

/**
 * @brief 初始化ST7789并注册LVGL显示设备。
 * @note 调用前需完成SPI、PWM外设初始化及lv_init()，由显示任务调用一次。
 *       显示设备创建失败时进入工程统一错误处理。
 */
void lv_port_disp_init(void);

#endif /* __LV_PORT_DISP_H__ */
