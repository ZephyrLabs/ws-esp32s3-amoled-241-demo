# Waveshare ESP32-S3 AMOLED 2.41 v1 - LVGL v9 DEMO

LVGL v9 based demo for Waveshare ESP32-S3 AMOLED 2.41" Inch development board (rev. 1), based on Waveshare SDK, using pioarduino platform-espressif32 Arduino SDK.

Copy `lib/lv_conf.h` to `.pio/libdeps/esp32-s3-devkitm-1/lv_conf.h`.

`DEMO` parameter in `src/misc_conf.h` can be changed to select a particular demo:

| `DEMO` | description |
|---|---|
| `BRIGHTNESS_DEMO` | A custom demo with text and a slider bar to adjust screen brightness. |
| `WIDGETS_DEMO` | A widgets example |
| `MUSIC_DEMO` | A modern, smartphone-like music player demo. |
| `STRESS_DEMO` | A stress test for LVGL. |
| `BENCHMARK_DEMO` | A demo to measure the performance of LVGL or to compare different settings. |
