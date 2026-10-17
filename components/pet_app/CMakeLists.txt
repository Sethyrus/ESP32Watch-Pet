idf_component_register(
    SRCS "pet_app.c"
    INCLUDE_DIRS "include"
    REQUIRES watch_board lvgl pet_core pet_render
    PRIV_REQUIRES esp_timer nvs_flash waveshare__esp32_s3_touch_amoled_2_06
)
