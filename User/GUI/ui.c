#include "ui.h"
#include "main.h"

/* 仅包含布局。数据绑定独立于页面，可以用同名 SquareLine 控件替换。 */
lv_obj_t *ui_PowerValue;
lv_obj_t *ui_VoltageValue;
lv_obj_t *ui_CurrentValue;
lv_obj_t *ui_MeasurementStatus;

static lv_obj_t *create_card(lv_obj_t *screen, int32_t y, int32_t height, uint32_t color)
{
    lv_obj_t *card = lv_obj_create(screen);
    if (card == NULL)
    {
        Error_Handler();
    }

    lv_obj_remove_style_all(card);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, 10, y);
    lv_obj_set_size(card, 220, height);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    return card;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, int32_t x, int32_t y,
                              int32_t width, const lv_font_t *font, uint32_t color,
                              lv_text_align_t align)
{
    lv_obj_t *label = lv_label_create(parent);
    if (label == NULL)
    {
        Error_Handler();
    }

    lv_obj_remove_style_all(label);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, align, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(label, text);
    return label;
}

void ui_init(void)
{
    const lv_font_t *small = &lv_font_montserrat_14;
    const lv_font_t *reading = &lv_font_montserrat_24;
    const uint32_t muted = 0xB4C7D8;
    const uint32_t white = 0xF1F8FF;
    const uint32_t green = 0x6CE8BB;
    const uint32_t surface = 0x172636;
    lv_obj_t *screen = lv_screen_active();
    lv_obj_t *power_card;
    lv_obj_t *voltage_card;
    lv_obj_t *current_card;

    lv_obj_remove_style_all(screen);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0C1622), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    create_label(screen, "DC METER", 12, 12, 120, small, white, LV_TEXT_ALIGN_LEFT);
    create_label(screen, "HIGH RANGE", 128, 12, 100, small, muted, LV_TEXT_ALIGN_RIGHT);

    power_card = create_card(screen, 39, 74, 0x13332F);
    create_label(power_card, "POWER", 12, 10, 100, small, green, LV_TEXT_ALIGN_LEFT);
    create_label(power_card, "W", 182, 42, 26, small, green, LV_TEXT_ALIGN_RIGHT);
    ui_PowerValue = create_label(power_card, "--", 12, 28, 161,
                                 &lv_font_montserrat_36, white, LV_TEXT_ALIGN_RIGHT);

    voltage_card = create_card(screen, 121, 38, surface);
    create_label(voltage_card, "VOLTAGE", 12, 11, 77, small, muted, LV_TEXT_ALIGN_LEFT);
    create_label(voltage_card, "V", 182, 11, 26, small, muted, LV_TEXT_ALIGN_RIGHT);
    ui_VoltageValue = create_label(voltage_card, "--", 89, 4, 84,
                                   reading, white, LV_TEXT_ALIGN_RIGHT);

    current_card = create_card(screen, 167, 38, surface);
    create_label(current_card, "CURRENT", 12, 11, 77, small, muted, LV_TEXT_ALIGN_LEFT);
    create_label(current_card, "A", 182, 11, 26, small, muted, LV_TEXT_ALIGN_RIGHT);
    ui_CurrentValue = create_label(current_card, "--", 89, 4, 84,
                                   reading, white, LV_TEXT_ALIGN_RIGHT);

    ui_MeasurementStatus = create_label(screen, "WAITING FOR ADC", 12, 215, 216,
                                       small, 0xF4C36A, LV_TEXT_ALIGN_LEFT);
}
