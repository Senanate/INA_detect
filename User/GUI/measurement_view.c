#include "measurement_view.h"
#include "ui.h"

static void update_label(lv_obj_t *label, const char *text)
{
    if (lv_strcmp(lv_label_get_text(label), text) != 0)
    {
        lv_label_set_text(label, text);
    }
}

static void update_reading(lv_obj_t *label, float value)
{
    char text[16];
    float absolute_value = value < 0.0f ? -value : value;
    /* 12 位 ADC 的换算范围远小于 uint32_t，整数格式化适配 LV_USE_FLOAT=0。 */
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

    switch (measurement.status)
    {
        case MEASUREMENT_VALID:       status_text = "UNCALIBRATED"; break;
        case MEASUREMENT_ADC_LIMIT:   status_text = "ADC SATURATED"; break;
        case MEASUREMENT_ADC_TIMEOUT: status_text = "ADC TIMEOUT"; break;
        case MEASUREMENT_ADC_ERROR:   status_text = "ADC ERROR"; break;
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
}
