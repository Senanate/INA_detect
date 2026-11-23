#ifndef POWER_METER_UI_H
#define POWER_METER_UI_H

#include "lvgl.h"

extern lv_obj_t *ui_PowerValue;
extern lv_obj_t *ui_VoltageValue;
extern lv_obj_t *ui_CurrentValue;
extern lv_obj_t *ui_MeasurementStatus;
extern lv_obj_t *ui_Range;
/** @brief 在当前屏幕创建测量主页。须在显示初始化后由 GUI 任务调用一次。 */
void ui_init(void);

#endif
