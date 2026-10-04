idf_component_register(SRCS "main.c" "../../flipper/core/rid.c" "../../flipper/vendor/opendroneid.c"
    INCLUDE_DIRS "../../flipper/core" "../../flipper/vendor"
    REQUIRES esp_wifi nvs_flash driver esp_timer)
