#ifndef QWEB_H
#define QWEB_H

#include <stdlib.h>
#include <stdbool.h>


#define QWEB_MAX_CONTENT_RECEIVE     (10240)


#define HTTP_MIME_TYPE_HTML     "text/html"
#define HTTP_MIME_TYPE_JS       "application/javascript"
#define HTTP_MIME_TYPE_PLAIN    "text/plain"
#define HTTP_MIME_TYPE_BINARY   "application/octet-stream"


/**
 * @brief Register a file with the http server
 * @param path path to register the file at
 * @param type mime type
 * @param embed_name the identifier generated for the embedded data
 *  (see https://docs.espressif.com/projects/esp-idf/en/v5.1.4/esp32/api-guides/build-system.html#embedding-binary-data)
 */
#define QWEB_FILE(path, type, embed_name) do {\
    extern const char embed_name##_start[] asm("_binary_" __STRING(embed_name) "_start");\
    extern const char embed_name##_end[] asm("_binary_" __STRING(embed_name) "_end");\
    qweb_register_page(path, type, embed_name##_start, embed_name##_end - embed_name##_start);}\
    while (0)


#define QWEB_FILE_DYN(path, type, databuffer, starting_len) \
    qweb_register_page(path, type, databuffer, starting_len)



#define QWEB_POST_RET_OK\
    ((qweb_post_cb_ret_t){ .data="", .resp_type="text", .success=true, .dynamic=false, .nullterm=true, .size = 0  })
#define QWEB_POST_RET_FAIL\
    ((qweb_post_cb_ret_t){ .data="", .resp_type="text", .success=false, .dynamic=false, .nullterm=true, .size = 0  })
#define QWEB_POST_RET_OK_STAT_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=true, .dynamic=false, .nullterm=true, .size = 0  })
#define QWEB_POST_RET_OK_STAT_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=true, .dynamic=false, .nullterm=false, .size = _size  })
#define QWEB_POST_RET_OK_DYN_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=true, .dynamic=true, .nullterm=true, .size = 0  })
#define QWEB_POST_RET_OK_DYN_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=true, .dynamic=true, .nullterm=false, .size = _size  })
#define QWEB_POST_RET_FAIL_STAT_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=false, .dynamic=false, .nullterm=true, .size = 0  })
#define QWEB_POST_RET_FAIL_STAT_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=false, .dynamic=false, .nullterm=false, .size = _size  })
#define QWEB_POST_RET_FAIL_DYN_STR(_data, type)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=false, .dynamic=true, .nullterm=true, .size = 0  })
#define QWEB_POST_RET_FAIL_DYN_BIN(_data, type, _size)\
    ((qweb_post_cb_ret_t){ .data=_data, .resp_type=type, .success=false, .dynamic=true, .nullterm=false, .size = _size  })


/**
 * @brief POST responsen returned from callback
 *  (must be freeable)
 */
typedef struct qweb_post_cb_ret {
    char* data;                 // content
    const char* resp_type;      // e.g: text/json
    bool success: 1;            // send 200 OK, or 500 Internal Server Error
    bool dynamic: 1;            // was data dynamically allocated
    bool nullterm: 1;           // is the data null terminated
    size_t size;                // size for data, if not null terminated
} qweb_post_cb_ret_t;




typedef qweb_post_cb_ret_t (*qweb_post_cb_t)(const char* uri, const char* data, size_t data_len);
#define QWEB_POST_CB(path, cb)  qweb_register_post_cb(path, cb)

/**
 * @brief Identifier for quick lookups of pages
 * 
 */
typedef size_t qweb_server_page_id_t;

/**
 * @brief Register a page with the server's internal file system
 * 
 * @param fpath path to register to
 * @param ctype mime type
 * @param content file contents
 * @param content_length file size
 * @return qweb_server_page_id_t page identifier
 */
qweb_server_page_id_t qweb_register_page(const char* fpath, const char* ctype, const char* content, size_t content_length);

/**
 * @brief Adjust the content length of a page
 * 
 * @param page page identifier
 * @param content_length new content length
 */
void qweb_page_trunc(qweb_server_page_id_t page, size_t content_length);
/**
 * @brief Adjust the content length of a page
 * 
 * @param fpath page path
 * @param length new content length
 */
void qweb_page_trunc_path(const char* fpath, size_t length);

/**
 * @brief Register a callback for a POST request to a given path
 * 
 * @param path path to register
 * @param cb callback
 */
void qweb_register_post_cb(const char* path, qweb_post_cb_t cb);


/**
 * @brief Cleanup extra memory used during page and callback
 *  registration
 */
void qweb_cleanup_registry();

/**
 * @brief Free all resources used, fully destroy
 *  the qweb server and files.
 */
void qweb_free();

void qweb_init();

#endif