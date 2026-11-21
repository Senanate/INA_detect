#include "lv_port_disp.h"
#include "lvgl.h"
#include "lcd.h"
#include "lcd_init.h"
#include "main.h"
/*LVGL显示移植层，将LVGL绘图缓冲区的像素数据通过LCD驱动发送到ST7789屏幕。*/
/* 24行RGB565像素占11520字节，采用静态单缓冲供LVGL分块绘制。 */
#define LCD_DRAW_BUFFER_LINES 24U

/**
 * @brief 将LVGL生成的矩形像素区域发送到LCD。
 * @param display 当前显示设备。
 * @param area 待刷新的区域，结束坐标包含最后一个像素。
 * @param pixel_map LVGL绘图缓冲区，在刷新结束前保持有效。
 * @note 当前LCD接口在DMA完成或终止后返回，RGB565字节交换由LCD驱动完成。
 */
static void disp_flush(lv_display_t *display,
                       const lv_area_t *area,
                       uint8_t *pixel_map)
{
    LCD_Color_Render_Async(area->x1, area->y1,
                           area->x2, area->y2,
                           (uint16_t *)pixel_map);

    /* LCD接口返回后释放绘图缓冲区，允许LVGL生成下一块像素。 */
    lv_display_flush_ready(display);
}

void lv_port_disp_init(void)
{
    /* 缓冲区按4字节对齐，生命周期覆盖全部显示刷新过程。 */
    LV_ATTRIBUTE_MEM_ALIGN
    static uint16_t draw_buffer[LCD_W * LCD_DRAW_BUFFER_LINES];
    lv_display_t *display;

    LCD_Init();

    display = lv_display_create(LCD_W, LCD_H);
    if (display == NULL)
    {
        Error_Handler();
    }

    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display, disp_flush);
    lv_display_set_buffers(display,
                           draw_buffer, NULL, sizeof(draw_buffer),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    LCD_Open_Light();
    LCD_Set_Light(80);
}
