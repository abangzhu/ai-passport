idf_component_register(
    SRCS "main.c"
         "marvy_game.c"
         "marvy_session.c"
         "marvy_view.c"
         "marvy_services.c"
         "../assets/fonts/marvy_font_16.c"
         "../assets/fonts/marvy_font_20.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_timer esp_hw_support nvs_flash
)
