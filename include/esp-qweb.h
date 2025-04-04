#ifndef QWEB_H
#define QWEB_H

#include <stdlib.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief Default maximum amount receivable by the web server
 */
#define QWEB_MAX_CONTENT_RECEIVE     (10240)

/////////////////////
// MIME types
/////////////////////
#define HTTP_MIME_HTML     "text/html"
#define HTTP_MIME_CSS      "text/css"
#define HTTP_MIME_JS       "application/javascript"
#define HTTP_MIME_JSON     "application/json"
#define HTTP_MIME_XML      "application/xml"
#define HTTP_MIME_PLAIN    "text/plain"
#define HTTP_MIME_BINARY   "application/octet-stream"
#define HTTP_MIME_PNG      "image/png"
#define HTTP_MIME_JPEG     "image/jpeg"
#define HTTP_MIME_GIF      "image/gif"
#define HTTP_MIME_BMP      "image/bmp"
#define HTTP_MIME_SVG      "image/svg+xml"
#define HTTP_MIME_WEBP     "image/webp"
#define HTTP_MIME_AVIF     "image/avif"
#define HTTP_MIME_PDF      "application/pdf"
#define HTTP_MIME_ZIP      "application/zip"
#define HTTP_MIME_GZIP     "application/gzip"
#define HTTP_MIME_TAR      "application/x-tar"
#define HTTP_MIME_MP3      "audio/mpeg"
#define HTTP_MIME_WAV      "audio/wav"
#define HTTP_MIME_OGG      "audio/ogg"
#define HTTP_MIME_MP4      "video/mp4"
#define HTTP_MIME_WEBM     "video/webm"
#define HTTP_MIME_ICS      "text/calendar"
#define HTTP_MIME_CSV      "text/csv"
#define HTTP_MIME_RTF      "application/rtf"
#define HTTP_MIME_TTF      "font/ttf"
#define HTTP_MIME_WOFF     "font/woff"
#define HTTP_MIME_WOFF2    "font/woff2"
#define HTTP_MIME_EOT      "application/vnd.ms-fontobject"
#define HTTP_MIME_ICO      "image/x-icon"
#define HTTP_MIME_M3U8     "application/vnd.apple.mpegurl"
#define HTTP_MIME_TS       "video/mp2t"
#define HTTP_MIME_JAR      "application/java-archive"
#define HTTP_MIME_WASM     "application/wasm"
#define HTTP_MIME_FORM_URLENCODED "application/x-www-form-urlencoded"
#define HTTP_MIME_FORM_DATA "multipart/form-data"



/**
 * @brief Register a file with the http server
 * @param server server to register to
 * @param path path to register the file at
 * @param type mime type
 * @param embed_name the identifier generated for the embedded data
 *  (see https://docs.espressif.com/projects/esp-idf/en/v5.1.4/esp32/api-guides/build-system.html#embedding-binary-data)
 */
#define QWEB_FILE(server, path, type, embed_name) do {\
    extern const char embed_name##_start[] asm("_binary_" __STRING(embed_name) "_start");\
    extern const char embed_name##_end[] asm("_binary_" __STRING(embed_name) "_end");\
    qweb_register_file(server, path, type, embed_name##_start, embed_name##_end - embed_name##_start);}\
    while (0)


/**
 * @brief Register a dynamic buffer as a file with the server
 * @param server server to register to
 * @param path path to register the dynamic buffer at
 * @param type mime type
 * @param databuffer buffer pointer
 * @param starting_len length of data
 */
#define QWEB_FILE_DYN(server, path, type, databuffer, starting_len) \
    qweb_register_file(server, path, type, databuffer, starting_len)


/**
 * @brief a generic OK response from a post handler
 */
#define QWEB_POST_RET_OK\
    ((qweb_post_cb_ret_t){ .s_data="", .resp_type="text", .success=true, .dynamic=false, .nullterm=true, .size = 0  })

/**
 * @brief a generic FAIL response from a post handler
 */
#define QWEB_POST_RET_FAIL\
    ((qweb_post_cb_ret_t){ .s_data="", .resp_type="text", .success=false, .dynamic=false, .nullterm=true, .size = 0  })

/**
 * @brief An OK response from a post handler with a static null-terminated string used as data
 */
#define QWEB_POST_RET_OK_STAT_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .s_data=_data, .resp_type=type, .success=true, .dynamic=false, .nullterm=true, .size = 0  })

/**
 * @brief An OK response from a post handler with a static buffer used as data
 */
#define QWEB_POST_RET_OK_STAT_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .s_data=_data, .resp_type=type, .success=true, .dynamic=false, .nullterm=false, .size = _size  })

/**
 * @brief An OK response from a post handler with a dynamic (malloc'd) null terminated string as data
 * @note the string will be free'd by qweb after it is sent to the client
 */
#define QWEB_POST_RET_OK_DYN_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .d_data=_data, .resp_type=type, .success=true, .dynamic=true, .nullterm=true, .size = 0  })
    
/**
 * @brief An OK response from a post handler with a dynamic (malloc'd) buffer as data
 * @note the buffer will be free'd by qweb after it is sent to the client
 */
#define QWEB_POST_RET_OK_DYN_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .d_data=_data, .resp_type=type, .success=true, .dynamic=true, .nullterm=false, .size = _size  })

/**
 * @brief A FAIL response from a post handler with a static null-terminated string used as data
 */
#define QWEB_POST_RET_FAIL_STAT_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .s_data=_data, .resp_type=type, .success=false, .dynamic=false, .nullterm=true, .size = 0  })

/**
 * @brief A FAIL response from a post handler with a static buffer used as data
 */
#define QWEB_POST_RET_FAIL_STAT_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .s_data=_data, .resp_type=type, .success=false, .dynamic=false, .nullterm=false, .size = _size  })

/**
 * @brief A FAIL response from a post handler with a dynamic (malloc'd) null terminated string as data
 * @note the string will be free'd by qweb after it is sent to the client
 */
#define QWEB_POST_RET_FAIL_DYN_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .d_data=_data, .resp_type=type, .success=false, .dynamic=true, .nullterm=true, .size = 0  })

    
/**
 * @brief A FAIL response from a post handler with a dynamic (malloc'd) buffer as data
 * @note the buffer will be free'd by qweb after it is sent to the client
 */
#define QWEB_POST_RET_FAIL_DYN_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .d_data=_data, .resp_type=type, .success=false, .dynamic=true, .nullterm=false, .size = _size  })



typedef struct qweb_server qweb_server_t;
typedef struct qweb_server_config {
    uint16_t port;
    size_t stack_size;
    uint16_t max_sockets;
    size_t max_recvlen;
    const char* name;
#ifdef CONFIG_QWEB_EN_SSL
    bool ssl;
    struct {
        const uint8_t* cert;
        size_t certlen;
        const uint8_t* privkey;
        size_t privkeylen;
    } ssl_config;
#endif
} qweb_server_config_t;

#define QWEB_SERVER_CFG_DEFAULT(_name) (qweb_server_config_t)\
    { .port = 80, .stack_size = 4096, .max_sockets = 7, .max_recvlen = QWEB_MAX_CONTENT_RECEIVE, .name = _name }

#ifdef CONFIG_QWEB_EN_SSL
#define QWEB_SSL_SERVER_CFG_DEFAULT(_name)  (qweb_server_config_t)\
    { .port = 0, .stack_size = 10240, .max_sockets = 4, .max_recvlen = QWEB_MAX_CONTENT_RECEIVE, .name = _name, .ssl = true }
#endif

#define QWEB_ASSIGN_EMBEDDED(destbegin, destlen, embed_name) do {\
    extern const char embed_name##_start[] asm("_binary_" __STRING(embed_name) "_start");\
    extern const char embed_name##_end[] asm("_binary_" __STRING(embed_name) "_end");\
    destbegin = (embed_name##_start);\
    destlen = ((embed_name##_end)-(embed_name##start)); }\
    while (0)



qweb_server_t* qweb_init(const qweb_server_config_t* config);

/**
 * @brief POST responsen returned from callback
 *  (must be freeable)
 */
typedef struct qweb_post_cb_ret {

    union {
        const char* s_data;     // static content
        char* d_data;           // dynamic content
    };
    
    const char* resp_type;      // e.g: text/json
    bool success: 1;            // send 200 OK, or 500 Internal Server Error
    bool dynamic: 1;            // was data dynamically allocated
    bool nullterm: 1;           // is the data null terminated
    size_t size;                // size for data, if not null terminated
} qweb_post_cb_ret_t;



/**
 * @brief A post request callback handler
 * @param uri the uri from the client
 * @param data data from the client
 * @param data_len content length (data size)
 * @returns a post request return value struct to indicate a response to the client
 */
typedef qweb_post_cb_ret_t (*qweb_post_cb_t)(const char* uri, const char* data, size_t data_len);



typedef struct qweb_post_handler {
    qweb_post_cb_t cb;
    bool supress_log: 1;
} qweb_post_handler_t;

#define QWEB_POST_HANDLER_DEFAULT(_cb)   (qweb_post_handler_t) { .cb=_cb, .supress_log = false }


/**
 * @brief Register a file with the server's internal file system
 * 
 * @param fpath path to register to
 * @param ctype mime type
 * @param content file contents
 * @param content_length file size
 */
void qweb_register_file(qweb_server_t* server, const char* fpath, const char* ctype, const char* content, size_t content_length);

/**
 * @brief Adjust the content length of a file
 * 
 * @param fpath file path
 * @param length new content length
 */
void qweb_file_trunc_path(qweb_server_t* server, const char* fpath, size_t length);

/**
 * @brief Register a callback for a POST request to a given path
 * 
 * @param path path to register
 * @param cb callback
 */
void qweb_register_post_cb(qweb_server_t* server, const char* path, qweb_post_handler_t handler);

esp_err_t qweb_unregister_file(qweb_server_t* server, const char* path);
esp_err_t qweb_unregister_post_cb(qweb_server_t* server, const char* path);

/**
 * @brief Free all resources used, fully destroy
 *  the qweb server and files.
 */
void qweb_free(qweb_server_t* server);


#endif