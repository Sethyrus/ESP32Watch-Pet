idf_component_register(
    SRCS "pet_app.c" "pet_ui.c" "pet_radio.c" "pet_store.c" "pet_log.c" "pet_audio.c"
         "fonts/pet_font_12.c" "fonts/pet_font_14.c" "fonts/pet_font_16.c"
         "fonts/pet_font_20.c" "fonts/pet_font_24.c" "fonts/pet_font_28.c"
    INCLUDE_DIRS "include"
    REQUIRES watch_board lvgl pet_core pet_render pet_game pet_sound
    PRIV_REQUIRES esp_timer esp_wifi esp_event esp_hw_support nvs_flash waveshare__esp32_s3_touch_amoled_2_06
)
