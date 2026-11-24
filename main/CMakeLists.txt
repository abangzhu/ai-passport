idf_component_register(
    SRCS "main.c"
         "marvy_game.c"
         "marvy_session.c"
         "marvy_view.c"
         "marvy_services.c"
         "marvy_music.c"
         "marvy_logo.c"
         "../assets/fonts/marvy_font_16.c"
         "../assets/fonts/marvy_font_20.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer esp_hw_support nvs_flash
)

target_add_binary_data(${COMPONENT_LIB} "../assets/music/marvy_bgm.pcm"
                      BINARY RENAME_TO marvy_bgm_pcm)
