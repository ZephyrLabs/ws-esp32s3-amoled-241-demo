#include <Arduino.h>
#include <stdio.h>

#include "board_conf.h"

static lv_display_t* display_handle;

static lv_indev_t* indev_handle;

static lv_obj_t* slider;

static uint8_t brightness_value = 100;

void set_brightness(uint8_t brightness)
{
    uint32_t lcd_cmd = 0x51;
    lcd_cmd &= 0xff;
    lcd_cmd <<= 8;
    lcd_cmd |= 0x02 << 24;
    uint8_t param = brightness;

    param = (param > 10) ? param : 10;
    param = (param > 225) ? 225 : param;

    esp_lcd_panel_io_tx_param(brightness_handle, lcd_cmd, &param, 1);
}

static void demo_lvgl_port_task(void *arg) {
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
    while (1) {
        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (demo_lvgl_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            demo_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

static void brightness_slider_cb(lv_event_t* e) {
  LV_UNUSED(e);

  brightness_value = lv_slider_get_value(slider);
}

void setup() {
    ESP_LOGI(TAG, "Initialize SPI bus");

    const spi_bus_config_t buscfg = rm690b0_PANEL_BUS_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_PCLK,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA0,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA1,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA2,
                                                                  EXAMPLE_PIN_NUM_LCD_DATA3,
                                                                  EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * LCD_BIT_PER_PIXEL / 8);

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    ESP_LOGI(TAG, "Install panel IO");

    esp_lcd_panel_io_handle_t io_handle = NULL;
    const esp_lcd_panel_io_spi_config_t io_config = rm690b0_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS,
                                                                                demo_notify_lvgl_flush_ready,
                                                                                &display_handle);
    rm690b0_vendor_config_t vendor_config = {
        .init_cmds = lcd_init_cmds,
        .init_cmds_size = sizeof(lcd_init_cmds) / sizeof(lcd_init_cmds[0]),
        .flags = {
            .use_qspi_interface = 1,
        },
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    brightness_handle = io_handle;

    esp_lcd_panel_handle_t panel_handle = NULL;

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config = &vendor_config,
    };

    ESP_LOGI(TAG, "Install rm690b0 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_rm690b0(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    #if EXAMPLE_USE_TOUCH
        ESP_LOGI(TAG, "Initialize I2C bus");

        const i2c_config_t i2c_conf = {
            .mode = I2C_MODE_MASTER,
            .sda_io_num = EXAMPLE_PIN_NUM_TOUCH_SDA,
            .scl_io_num = EXAMPLE_PIN_NUM_TOUCH_SCL,
            .sda_pullup_en = GPIO_PULLUP_ENABLE,
            .scl_pullup_en = GPIO_PULLUP_ENABLE,
            .master = {.clk_speed = 300 * 1000,},
        };

        ESP_ERROR_CHECK(i2c_param_config(TOUCH_HOST, &i2c_conf));
        ESP_ERROR_CHECK(i2c_driver_install(TOUCH_HOST, i2c_conf.mode, 0, 0, 0));

        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TOUCH_HOST, &tp_io_config, &tp_io_handle));

        const esp_lcd_touch_config_t tp_cfg = {
            .x_max = EXAMPLE_LCD_V_RES-1,
            .y_max = EXAMPLE_LCD_H_RES-1,
            .rst_gpio_num = EXAMPLE_PIN_NUM_TOUCH_RST,
            .int_gpio_num = EXAMPLE_PIN_NUM_TOUCH_INT,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
            #if (AMOLED_ROTATION == ROTATE_90)
                .swap_xy = 1,
                .mirror_x = 0,
                .mirror_y = 1,
            #else
                .swap_xy = 0,
                .mirror_x = 0,
                .mirror_y = 0,
            #endif
            },
        };

        ESP_LOGI(TAG, "Initialize touch controller");
        ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp));
    #endif

    ESP_LOGI(TAG, "Initialize LVGL library");

    lv_init();

    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    lv_color_t *buf1 = (lv_color_t*)heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t),
                                                   MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED);
    assert(buf1);

    lv_color_t *buf2 = (lv_color_t*)heap_caps_malloc(EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t),
                                                     MALLOC_CAP_DMA | MALLOC_CAP_CACHE_ALIGNED);
    assert(buf2);

    ESP_LOGI(TAG, "Register display_handle driver to LVGL");

    display_handle = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_display_set_user_data(display_handle, panel_handle);
    lv_display_set_color_format(display_handle, LV_COLOR_FORMAT_RGB565_SWAPPED);
    lv_display_set_flush_cb(display_handle, demo_lvgl_flush_cb);
    lv_display_add_event_cb(display_handle, demo_lvgl_rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    lv_display_add_event_cb(display_handle, demo_lvgl_update_cb, LV_EVENT_RESOLUTION_CHANGED, NULL);
    lv_display_set_buffers(display_handle, buf1, buf2, EXAMPLE_LCD_H_RES * EXAMPLE_LVGL_BUF_HEIGHT * sizeof(lv_color_t), LV_DISP_RENDER_MODE_PARTIAL);

    ESP_LOGI(TAG, "Install LVGL tick timer");

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &demo_increase_lvgl_tick,
        .name = "lvgl_tick"
    };

    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    #if EXAMPLE_USE_TOUCH
        indev_handle = lv_indev_create();
        lv_indev_set_type(indev_handle, LV_INDEV_TYPE_POINTER);
        lv_indev_set_disp(indev_handle, display_handle);
        lv_indev_set_read_cb(indev_handle, demo_lvgl_touch_cb);
        lv_indev_set_user_data(indev_handle, tp);
        lv_indev_enable(indev_handle, true);
    #endif

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);

    xTaskCreate(demo_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);

    if (demo_lvgl_lock(-1)) {
        #if DEMO == BRIGHTNESS_DEMO
            lv_obj_t* txt = lv_label_create(lv_scr_act());
            lv_label_set_text(txt, "hello world!");
            lv_obj_set_style_text_font(txt, &lv_font_montserrat_24, 0);
            lv_obj_align(txt, LV_ALIGN_CENTER, 0, 0);

            slider = lv_slider_create(lv_scr_act());
            lv_obj_set_size(slider, 300, 16);
            lv_obj_align(slider, LV_ALIGN_CENTER, 0, 100);
            lv_slider_set_range(slider, 0, 255);
            lv_slider_set_value(slider, brightness_value, LV_ANIM_OFF);
            lv_obj_add_event_cb(slider, brightness_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

        #elif DEMO == WIDGETS_DEMO
            lv_demo_widgets();

        #elif DEMO == MUSIC_DEMO
            lv_demo_music();

        #elif DEMO == STRESS_DEMO
            lv_demo_stress();

        #elif DEMO == BENCHMARK_DEMO
            lv_demo_benchmark();

        #endif

      demo_lvgl_unlock();
    }
}

void loop() {
    /*
        the brightness command is sent on the same bus as the display data,
        lock to acquire the bus, release once done, otherwise the lvgl display flush
        callback can enter a race condition by writing data at the same time.
    */
    if (demo_lvgl_lock(-1)){
        set_brightness(brightness_value);
        demo_lvgl_unlock();
    }

    delay(100);
}
