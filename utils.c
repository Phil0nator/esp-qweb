#include "utils.h"

#include <string.h>

#include "esp_http_server.h"

const char* qs_get_start(const char* qs) {
    return strchr(qs, '?');
}
char* qs_get_field(const char* qs, const char* fieldname) {
    char *field = malloc(32);
    size_t fieldsize = 32;
    while (httpd_query_key_value( qs, fieldname, field, fieldsize ) == ESP_ERR_HTTPD_RESULT_TRUNC) {
        fieldsize *= 2;
        field = realloc(field,fieldsize);
    }
    field = realloc(field, strlen(field));
    return field;
}
