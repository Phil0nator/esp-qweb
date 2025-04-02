idf_component_register(SRCS "esp-qweb.c"
                    INCLUDE_DIRS "include"
                    PRIV_REQUIRES esp_http_server lightweight-collections)