#ifndef MEASUREMENT_VIEW_H
#define MEASUREMENT_VIEW_H

#include "measurement.h"

/**
 * @brief 更新测量读数和状态。须在 ui_init() 后由 GUI 任务调用。
 * @param measurement 本次采样快照，异常状态时用占位符替换读数。
 */
void MeasurementView_Update(measurement_t measurement);

#endif
