idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES gps radio sd_card sensors ucam
)