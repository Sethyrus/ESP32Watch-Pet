idf_component_register(
    SRCS "pet_app.c" "pet_ui.c" "pet_radio.c" "pet_store.c" "pet_log.c"
    INCLUDE_DIRS "include"
    REQUIRES watch_board lvgl pet_core pet_render pet_game
    PRIV_REQUIRES esp_timer esp_wifi esp_event esp_hw_support nvs_flash waveshare__esp32_s3_touch_amoled_2_06
)
