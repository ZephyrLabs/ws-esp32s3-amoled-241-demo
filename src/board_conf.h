#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"

#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_lcd_rm690b0.h"
#include "esp_lcd_touch_ft5x06.h"

#include "misc_conf.h"

static const char *TAG = "demo";

/* Controller Host configuration */
#define LCD_HOST    SPI2_HOST // FSPI/HSPI
#define TOUCH_HOST  I2C_NUM_0 // I2C0

/* Display color depth */
#define LCD_BIT_PER_PIXEL       (16)

/* Display pin configuration */
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_LCD_CS            (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_LCD_PCLK          (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_LCD_DATA0         (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_LCD_DATA1         (GPIO_NUM_12)
#define EXAMPLE_PIN_NUM_LCD_DATA2         (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_LCD_DATA3         (GPIO_NUM_14)
#define EXAMPLE_PIN_NUM_LCD_RST           (GPIO_NUM_21)
#define EXAMPLE_PIN_NUM_BK_LIGHT          (-1)

static const rm690b0_lcd_init_cmd_t lcd_init_cmds[] = {
    {0xFE, (uint8_t []){0x20}, 1, 0},	// Write/Select CMD mode page - Manufacture Command Set Page Pane.
    {0x26, (uint8_t []){0x0A}, 1, 0}, // MIPI off.
    {0x24, (uint8_t []){0x80}, 1, 0}, // SPI writes RAM.

    {0xFE, (uint8_t []){0x00}, 1, 0}, // Write/Select CMD mode page - User Command Set.
    {0x3A, (uint8_t []){0x55}, 1, 0}, // Pixel format RGB565.
    {0xC2, (uint8_t []){0x00}, 1, 10}, // Set display mode - use internal timing.
    {0x35, (uint8_t []){0x00}, 0, 0}, // Tearing Output - Enabled.
    {0x51, (uint8_t []){0x00}, 1, 10}, // Display brightness to 0 (min/off).
    {0x11, (uint8_t []){0x00}, 0, 80}, // Display exit sleep mode.
    {0x2A, (uint8_t []){0x00,0x10,0x01,0xD1}, 4, 0}, // Set Column Start Address, 0x10 - 0x1D1, 450px.
    {0x2B, (uint8_t []){0x00,0x00,0x02,0x57}, 4, 0}, // Set Row Start Address, 0x0 - 0x257, 600px.
    // {0x30, (uint8_t []){0x00, 0x01,0x02, 0x56}, 4, 0}, // Set Partial Area for Display - row 0x0 to row 0x256.
    {0x29, (uint8_t []){0x00}, 0, 10},  // Turn on display.

    #if (AMOLED_ROTATION == ROTATE_90)
        {0x36, (uint8_t []){0x30}, 1, 0},  // Swap row, column scanlines (rotates display)
    #endif

    {0x51, (uint8_t []){0xFF}, 1, 0}, // Display brightness to 255 (max).
};

esp_lcd_panel_io_handle_t brightness_handle;

/* Touch Panel configuration */
#define EXAMPLE_USE_TOUCH                 1

#if EXAMPLE_USE_TOUCH
    #define EXAMPLE_PIN_NUM_TOUCH_SCL         (GPIO_NUM_48)
    #define EXAMPLE_PIN_NUM_TOUCH_SDA         (GPIO_NUM_47)
    #define EXAMPLE_PIN_NUM_TOUCH_RST         (gpio_num_t)(-1)
    #define EXAMPLE_PIN_NUM_TOUCH_INT         (gpio_num_t)(-1)

    esp_lcd_touch_handle_t tp = NULL;
#endif

/* Locking Mechanism */
static bool demo_lvgl_lock(int timeout_ms) {
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}

static void demo_lvgl_unlock(void) {
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

/* LVGL callback implementations */
static void demo_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, unsigned char* color_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    #if (AMOLED_ROTATION == ROTATE_90)
      const int offsetx1 = area->x1;
      const int offsetx2 = area->x2;
      const int offsety1 = area->y1 + 0x10;
      const int offsety2 = area->y2 + 0x10;
    #else
      const int offsetx1 = area->x1 + 0x10; // 0x10 column offset
      const int offsetx2 = area->x2 + 0x10;
      const int offsety1 = area->y1;
      const int offsety2 = area->y2;
    #endif

    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

static bool demo_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    lv_display_t* disp = *(lv_display_t**)(user_ctx);
    lv_disp_flush_ready(disp);
    return false;
}

static void demo_lvgl_update_cb(lv_event_t *e) {
    lv_display_t* disp = (lv_display_t *)lv_event_get_target(e);

    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)lv_display_get_user_data(disp);

    switch (lv_display_get_rotation(disp))
    {
        case LV_DISPLAY_ROTATION_0:
            // Rotate LCD display
            esp_lcd_panel_swap_xy(panel_handle, false);
            esp_lcd_panel_mirror(panel_handle, true, false);
            break;
        case LV_DISPLAY_ROTATION_90:
            // Rotate LCD display
            esp_lcd_panel_swap_xy(panel_handle, true);
            esp_lcd_panel_mirror(panel_handle, true, true);
            break;
        case LV_DISPLAY_ROTATION_180:
            // Rotate LCD display
            esp_lcd_panel_swap_xy(panel_handle, false);
            esp_lcd_panel_mirror(panel_handle, false, true);
            break;
        case LV_DISPLAY_ROTATION_270:
            // Rotate LCD display
            esp_lcd_panel_swap_xy(panel_handle, true);
            esp_lcd_panel_mirror(panel_handle, false, false);
            break;
    }
}

void demo_lvgl_rounder_cb(lv_event_t *e) {
    lv_area_t *area = (lv_area_t *)lv_event_get_param(e);

    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;
    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // round the start of coordinate down to the nearest 2M number
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;

    // round the end of coordinate up to the nearest 2N+1 number
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

static void demo_increase_lvgl_tick(void *arg) {
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

#if EXAMPLE_USE_TOUCH
    static void demo_lvgl_touch_cb(lv_indev_t *tch, lv_indev_data_t *data)
    {
        esp_lcd_touch_handle_t tp = (esp_lcd_touch_handle_t)lv_indev_get_user_data(tch);
        assert(tp);
    
        uint16_t tp_x;
        uint16_t tp_y;
        uint8_t tp_cnt = 0;
        /* Read data from touch controller into memory */
        esp_lcd_touch_read_data(tp);
    
        /* Read data from touch controller */
        bool tp_pressed = esp_lcd_touch_get_coordinates(tp, &tp_x, &tp_y, NULL, &tp_cnt, 1);
    
        if (tp_pressed && tp_cnt > 0) {
            data->point.x = tp_x ;
            data->point.y = tp_y ;
            data->state = LV_INDEV_STATE_PRESSED;
            // ESP_LOGD(TAG, "Touch position: %d,%d", tp_x, tp_y);
        } else {
            data->state = LV_INDEV_STATE_RELEASED;
        }
    }
#endif
