#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl.h"
#include "demos/lv_demos.h"

static SemaphoreHandle_t lvgl_mux = NULL;

/* Screen rotation */
#define ROTATE_90      0
#define ROTATE_NONE    1
#define AMOLED_ROTATION  ROTATE_NONE

#if (AMOLED_ROTATION == ROTATE_90)
    #define EXAMPLE_LCD_H_RES                 600    
    #define EXAMPLE_LCD_V_RES                 450
#else
    #define EXAMPLE_LCD_H_RES                 450  
    #define EXAMPLE_LCD_V_RES                 600
#endif

/* LVGL */
#define EXAMPLE_LVGL_BUF_HEIGHT        (EXAMPLE_LCD_V_RES / 10)

#define EXAMPLE_LVGL_TICK_PERIOD_MS    5

#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (32 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2


/* Demo */
#define BRIGHTNESS_DEMO     1
#define WIDGETS_DEMO        2
#define MUSIC_DEMO          3
#define STRESS_DEMO         4
#define BENCHMARK_DEMO      5

#define DEMO    BENCHMARK_DEMO
