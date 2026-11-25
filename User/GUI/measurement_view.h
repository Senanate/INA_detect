#ifndef MEASUREMENT_VIEW_H
#define MEASUREMENT_VIEW_H

#include "key.h"
#include "measurement.h"

/**
 * @brief 创建 SquareLine 设置页，绑定高亮行并同步初始值。
 * @note 在 ui_init() 后由 GUI 任务调用一次。主页提示创建失败时进入 Error_Handler()。
 */
void MeasurementView_Init(void);

/**
 * @brief 由 GUI 任务处理来自按键队列的事件。
 * @param event 调用方提供的按键事件，须为有效指针。
 * @return 事件被页面处理时返回 true，可用于触发按键音。忽略的事件返回 false。
 */
bool MeasurementView_HandleKey(const key_event_t *event);

/** @return 按键音开启时返回 true，关闭时返回 false。由 GUI 任务读取。 */
bool MeasurementView_KeySoundEnabled(void);

/** @return 测量主页读取快照的时间间隔，单位为毫秒。 */
uint32_t MeasurementView_RefreshIntervalMs(void);

/**
 * @brief 更新测量读数和状态。须在 ui_init() 后由 GUI 任务调用。
 * @param measurement 本次采样快照，异常状态时用占位符替换读数。
 */
void MeasurementView_Update(measurement_t measurement);

#endif
