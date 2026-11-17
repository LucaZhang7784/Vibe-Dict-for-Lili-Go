idf_component_register(SRCS "main.c" "dict_core.c" "sd_font.c" "online.c" "unipus.c" REQUIRES TactilitySDK esp_http_client esp_netif esp-tls mbedtls json)
target_compile_options(${COMPONENT_LIB} PRIVATE -fstack-usage -Werror=frame-larger-than=8192)
