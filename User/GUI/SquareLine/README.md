# 测量主页的 SquareLine 导出与接入

当前固件使用 `InaGUI` 的 SquareLine Studio 1.6.3 导出代码，原始导出位于 `D:\stm32_freertos\squarline\UI`。页面为 240×240、RGB565、LVGL 9.2.2，包含 `MeterCn16` 中文字体。此目录下的 `InaMeter.spj` 是先前的布局草稿。

固件在 `LCDTestTask` 初始化显示后调用 `ui_init()`。页面和字体由 SquareLine 导出，`measurement_view.c` 负责填充实时数值。

## 在 Studio 中导出并替换布局

1. 在 **Project Settings** 确认 LVGL 9.2.2、240×240、16 位色深和 **Flat export**。先用 **Export → Export UI Files** 导出到单独的目录。
2. 将导出的 `ui.c`、`ui_Meter.c`、`ui_font_MeterCn16.c` 及相应头文件复制到 `User/GUI`。将 `ui.h` 的 LVGL 引用设为 `#include "lvgl.h"`。带中文字符串的 `ui_Meter.c` 在当前 ARMCC 5 工具链下保存为 UTF-8 with BOM。
3. Keil 的 **User/GUI** 分组已包含上述三个 C 文件。Simple Theme 已在 `lv_conf.h` 启用，`lv_theme_simple.c` 已加入 **Middlewares/LVGL/Theme**。本页只用 Panel 和 Label，通用的 `ui_helpers.c` 与空的组件钩子无需参与编译。以后增加事件或控件时，按实际引用补齐源文件和 LVGL 配置。
4. 在 `ui_Meter.h` 确认保留 `ui_PowerValue`、`ui_VoltageValue`、`ui_CurrentValue`、`ui_MeasurementStatus`。重复导出时保留手写的 `measurement_view.c`，重新构建后检查长负数及异常状态占位。
