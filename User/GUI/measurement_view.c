#include "measurement_view.h"
#include "ui.h"
#include "ui_Settings.h"
#include "gate.h"
#include "lv_port_disp.h"
#include "main.h"

#define BRIGHTNESS_MIN_PERCENT 10U
#define BRIGHTNESS_MAX_PERCENT 100U
#define BRIGHTNESS_STEP_PERCENT 10U

enum
{
    SETTING_BRIGHTNESS,
    SETTING_KEY_SOUND,
    SETTING_REFRESH,
    SETTING_ROTATION,
    SETTING_COUNT
};

static const uint32_t refresh_intervals_ms[] = {100U, 250U, 500U};
static lv_obj_t *setting_rows[SETTING_COUNT];
static uint8_t selected_setting;
static uint8_t brightness_percent = LCD_DEFAULT_BACKLIGHT_PERCENT;
static bool key_sound_enabled = true;
static uint8_t refresh_index;

/* 描边只表示实体按键选中的行，改变透明度可避免影响控件的布局。 */
static void update_settings_focus(void)
{
    for (uint8_t i = 0U; i < SETTING_COUNT; i++)
    {
        lv_obj_set_style_outline_opa(setting_rows[i],
                                     i == selected_setting ? LV_OPA_COVER : LV_OPA_TRANSP,
                                     LV_PART_MAIN);
    }
}

/* 用固件当前的设置值更新指定标签，覆盖 SquareLine 导出的预览文字。 */
static void update_setting_value(uint8_t setting)
{
    switch (setting)
    {
        case SETTING_BRIGHTNESS:
            lv_label_set_text_fmt(ui_BrightnessValue, "%u%%", (unsigned int)brightness_percent);
            break;
        case SETTING_KEY_SOUND:
            lv_label_set_text_static(ui_KeySoundValue, key_sound_enabled ? "ON" : "OFF");
            break;
        case SETTING_REFRESH:
            lv_label_set_text_fmt(ui_RefreshValue, "%lu ms",
                                  (unsigned long)MeasurementView_RefreshIntervalMs());
            break;
        case SETTING_ROTATION:
            lv_label_set_text_fmt(ui_RotationValue, "%u deg", (unsigned int)lv_port_disp_rotation());
            break;
    }
}

void MeasurementView_Init(void)
{
    ui_Settings_screen_init();

    /* SquareLine 复制的行沿用了 BrightnessRow 名称，按页面顺序绑定到设置项。 */
    setting_rows[SETTING_BRIGHTNESS] = ui_BrightnessRow;
    setting_rows[SETTING_KEY_SOUND]  = ui_BrightnessRow2;
    setting_rows[SETTING_REFRESH]    = ui_BrightnessRow3;
    setting_rows[SETTING_ROTATION]   = ui_BrightnessRow4;

    for (uint8_t i = 0U; i < SETTING_COUNT; i++)
    {
        /* 这版导出的 Panel 没有描边，补上宽度和颜色供按键高亮使用。 */
        lv_obj_set_style_outline_width(setting_rows[i], 2, LV_PART_MAIN);
        lv_obj_set_style_outline_color(setting_rows[i],
                                       lv_color_hex(0x6CE8BB),
                                       LV_PART_MAIN);

        update_setting_value(i);
    }
    update_settings_focus();

    /* 测量页的导出布局未包含实体按键提示，在右下角补一个标签。 */
    lv_obj_t *hint = lv_label_create(ui_Meter);
    if (hint == NULL)
    {
        Error_Handler();
    }
    lv_label_set_text_static(hint, "B Settings");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6CE8BB), LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_RIGHT, -8, -8);
}

bool MeasurementView_HandleKey(const key_event_t *event)
{
    bool click = event->type == KEY_EVT_CLICK;
    bool settings_active = lv_screen_active() == ui_Settings;

    /* B 在两页之间切换。选项上下文和数值跟随已创建的设置页保留。 */
    if (click && event->id == KEY_ID_B)
    {
        lv_screen_load(settings_active ? ui_Meter : ui_Settings);
        return true;
    }
    if (!settings_active)
    {
        return false;
    }

    if (click && (event->id == KEY_ID_L || event->id == KEY_ID_R))
    {
        /* 左移用 SETTING_COUNT - 1，实现首尾循环且避免索引变成负值。 */
        uint8_t step = event->id == KEY_ID_R ? 1U : SETTING_COUNT - 1U;
        selected_setting = (selected_setting + step) % SETTING_COUNT;
        update_settings_focus();
        return true;
    }

    if (event->id != KEY_ID_Y && event->id != KEY_ID_N)
    {
        return false;
    }
    /* 方向和开关只响应短按，允许亮度长按连调。 */
    bool brightness_hold = selected_setting == SETTING_BRIGHTNESS &&
                           (event->type == KEY_EVT_LONG || event->type == KEY_EVT_REPEAT);
    if (!click && !brightness_hold)
    {
        return false;
    }

    bool increase = event->id == KEY_ID_Y;
    switch (selected_setting)
    {
        case SETTING_BRIGHTNESS:
            if (increase && brightness_percent < BRIGHTNESS_MAX_PERCENT)
            {
                brightness_percent += BRIGHTNESS_STEP_PERCENT;
            }
            else if (!increase && brightness_percent > BRIGHTNESS_MIN_PERCENT)
            {
                brightness_percent -= BRIGHTNESS_STEP_PERCENT;
            }
            lv_port_disp_set_backlight(brightness_percent);
            break;
        case SETTING_KEY_SOUND:
            key_sound_enabled = increase;
            break;
        case SETTING_REFRESH:
        {
            const uint8_t count = sizeof(refresh_intervals_ms) / sizeof(refresh_intervals_ms[0]);
            uint8_t step = increase ? 1U : count - 1U;
            refresh_index = (refresh_index + step) % count;
            break;
        }
        case SETTING_ROTATION:
        {
            /* 加 270 度再取余等价于反转 90 度，结果仍是非负角度。 */
            uint16_t step = increase ? 90U : 270U;
            uint16_t degrees = (lv_port_disp_rotation() + step) % 360U;
            if (!lv_port_disp_set_rotation(degrees))
            {
                lv_label_set_text_static(ui_RotationValue, "SPI ERR");
                return true;
            }
            break;
        }
    }

    update_setting_value(selected_setting);
    return true;
}

bool MeasurementView_KeySoundEnabled(void)
{
    return key_sound_enabled;
}

uint32_t MeasurementView_RefreshIntervalMs(void)
{
    return refresh_intervals_ms[refresh_index];
}

/* 读数或状态未变化时跳过写入，减少标签重绘。 */
static void update_label(lv_obj_t *label, const char *text)
{
    if (lv_strcmp(lv_label_get_text(label), text) != 0)
    {
        lv_label_set_text(label, text);
    }
}

/* 先四舍五入到千分位，再用整数格式化，兼容 LV_USE_FLOAT=0。 */
static void update_reading(lv_obj_t *label, float value)
{
    char text[16];
    float absolute_value = value < 0.0f ? -value : value;
    /* 12 位 ADC 的换算范围乘以 1000 后仍可用 uint32_t 存储。 */
    uint32_t thousandths = (uint32_t)(absolute_value * 1000.0f + 0.5f);

    lv_snprintf(text, sizeof(text), "%s%lu.%03lu",
                value < 0.0f && thousandths != 0U ? "-" : "",
                (unsigned long)(thousandths / 1000U),
                (unsigned long)(thousandths % 1000U));
    update_label(label, text);
}

void MeasurementView_Update(measurement_t measurement)
{
    const char *status_text;
    const char *range_text;

    switch (measurement.status)
    {
        case MEASUREMENT_VALID:       status_text = "UNCALIBRATED"; break;
        case MEASUREMENT_ADC_LIMIT:   status_text = "ADC SATURATED"; break;
        case MEASUREMENT_ADC_TIMEOUT: status_text = "ADC TIMEOUT"; break;
        case MEASUREMENT_ADC_ERROR:   status_text = "ADC ERROR"; break;
        case MEASUREMENT_SWITCHING:   status_text = "SWITCHING RANGE"; break;
        default:                     status_text = "WAITING FOR ADC"; break;
    }

    if (measurement.status == MEASUREMENT_VALID)
    {
        update_reading(ui_PowerValue, measurement.power_w);
        update_reading(ui_VoltageValue, measurement.voltage_v);
        update_reading(ui_CurrentValue, measurement.current_a);
    }
    else
    {
        update_label(ui_PowerValue, "--");
        update_label(ui_VoltageValue, "--");
        update_label(ui_CurrentValue, "--");
    }
    update_label(ui_MeasurementStatus, status_text);

    /* 从测量快照取物理挡位，和同一快照中的读数、状态一起显示。 */
    switch (measurement.range)
    {
        case HIGH_CUR: range_text = "HIGH RANGE"; break;
        case MID_CUR:  range_text = "MID RANGE"; break;
        case LOW_CUR:  range_text = "LOW RANGE"; break;
        default:       range_text = "--"; break;
    }

    update_label(ui_Range, range_text);
}
