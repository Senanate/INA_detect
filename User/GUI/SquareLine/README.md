# InaMeter 测量主页

`InaMeter.spj` 采用 SquareLine Studio 1.5.4 的工程格式。配置为 240×240、RGB565、LVGL 9.2.2，使用 Montserrat 14、24 和 36。将同目录下的 `.sll`、`.slp` 和 `Themes.slt` 一起保留。

当前固件运行 `User/GUI/ui.c` 中的同版 LVGL 布局，`measurement_view.c` 负责填充实时数值。SquareLine Studio 因试用许可证过期停在登录界面，`.spj` 尚待在编辑器中打开并实际导出。

## 在 Studio 中导出并替换布局

1. 激活许可证后，在启动器中选 **Open → Import Project**，打开 `InaMeter.spj`。在 **Project Settings** 中确认 LVGL 9.2.2、240×240、16 位色深和 **Flat export**。
2. 将 **LVGL include path** 配置为能导出 `#include "lvgl.h"` 的路径。用 **Export → Export UI Files** 先导出至单独的目录，然后检查生成的 `ui.h`。
3. 将 Keil 的 **User/GUI** 分组中的布局源文件指向导出目录，添加生成的 C 文件和头文件搜索路径。保留 `measurement_view.c` 和 `LCDTestTask` 中的 `ui_init()` 调用。固件已有显示驱动和 `lv_conf.h`。
4. 确认导出的 `ui.h` 声明 `ui_PowerValue`、`ui_VoltageValue`、`ui_CurrentValue`、`ui_MeasurementStatus` 四个 Label。更新布局时保留这四个名称及固定宽度，重新构建后检查长负数及异常状态占位。
