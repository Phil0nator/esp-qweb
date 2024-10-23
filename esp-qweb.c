#include <stdio.h>
#include "esp-qweb.h"
#include "static-containers.h"

#include "esp_log.h"
#include "esp_err.h"

#include "esp_http_server.h"

// Pointer comparison
#define PTR_MIN(a,b)    (((a) < (b)) ? (a) : (b))

// Internal Maximum
#define FILEPATH_MAX        (256)

static const char* TAG = "qweb-server";

/**
 * @brief A file entry in the internal file system
 */
typedef struct http_file_ent {
    const char* fname;              // file name or path used to GET this file
    const char* type;               // MIME type
    const char* content;            // data content
    size_t content_length;          // data length
} http_file_ent_t;

/**
 * @brief A post request handler entry in the internal file system
 */
typedef struct http_post_cb_entry {
    const char* fpath;              // file name or path used to POST to this handler
    qweb_post_cb_t cb;              // callback function to handle requests
} http_post_cb_entry_t;

/**
 * @brief Construct a new stc vec decl object for post request handler entries
 */
STC_VEC_DECL( http_post_cb_entry_t, http_post_entries );

/**
 * @brief Construct a new stc vec decl object for file entries
 */
STC_VEC_DECL( http_file_ent_t, http_file_entries );


static http_file_ent_t* http_file_get_entry(const char* fpath, size_t fpath_size) {
    STC_VEC_FOREACH( i, http_file_entries ) {
        if (strncmp(http_file_entries[i].fname, fpath, fpath_size) == 0) {
            return &http_file_entries[i];
        }
    }

    return NULL;
}

static const http_post_cb_entry_t* http_post_cb_get_entry(const char* fpath, size_t fpath_size) {
    STC_VEC_FOREACH(i, http_post_entries) {
        if (strncmp(http_post_entries[i].fpath, fpath, fpath_size) == 0) {
            return &http_post_entries[i];
        }
    }

    return NULL;
}

static const char* uri_get_fpath_end( const char* uri ) {

    const char* end = uri + strlen(uri);
    const char* qmark = strchr(uri, '?');
    qmark = qmark ? qmark : end;
    const char* htag = strchr(uri, '#');
    htag = htag ? htag : end;

    return PTR_MIN(end, PTR_MIN(qmark, htag));

}

/**
 * @brief global handler for all get requests.
 *  This function will search the page file system for the correct page
 *  and send it to the client.
 */
static esp_err_t serv_get_handler(httpd_req_t* req) {
    
    const char* fpath_beg = req->uri;
    const char* fpath_end = uri_get_fpath_end(fpath_beg);
    size_t fpath_size = fpath_end - fpath_beg;

    ESP_LOGI(TAG, "GET: %s", req->uri);

    // Search the file system for a page entry with the correct fpath
    const http_file_ent_t* content = http_file_get_entry( fpath_beg, fpath_size );

    // If the page exists
    if (content) {
        // Construct reply
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_set_type(req, content->type);
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        ESP_LOGI(TAG,"HTTP 200 OK: %ub", content->content_length);

        // Send reply
        ESP_ERROR_CHECK( httpd_resp_send(req, content->content, content->content_length));

    } else {
        // 404 for pages that don't exist
        httpd_resp_send_404(req);
    }

    return ESP_OK;
}


/**
 * @brief Receive the entire content of a request
 * 
 * @param req request
 * @param dest data destination
 * @return esp_err_t 
 */
static esp_err_t httpd_req_recv_all(httpd_req_t* req, char** dest) {
    char* data = malloc(req->content_len + 1);
    data[req->content_len] = 0; // NULL terminate everything, just to be sure
    size_t received = 0;
    while(received < req->content_len) {
        int recv_amt;
        if ((recv_amt = httpd_req_recv(req, &data[received], req->content_len - received)) < 0) {
            free(data);
            return ESP_FAIL;
        }
        received += recv_amt;
    }
    *dest = data;
    return ESP_OK;
}

/**
 * @brief global handler for all post requests.
 *  This function distributes incoming post requests to
 *  their registered handler functions.
 */
static esp_err_t serv_post_handler(httpd_req_t* req) {
    const char* fpath_beg = req->uri;
    const char* fpath_end = uri_get_fpath_end(fpath_beg);
    size_t fpath_size = fpath_end - fpath_beg;

    ESP_LOGI(TAG, "POST: %s", req->uri);

    // Search file system for a post request handler with the correct fpath
    const http_post_cb_entry_t* cbent = http_post_cb_get_entry(fpath_beg, fpath_size);
    
    // If found...
    if (cbent) {
        // Ensure that the maximum data content size is not exceeded
        if (req->content_len < QWEB_MAX_CONTENT_RECEIVE) {
            
            // Load the entire data
            char *data;
            httpd_req_recv_all( req, &data);
            
            // Call the post handler providing the data
            qweb_post_cb_ret_t ret = cbent->cb(req->uri, data, req->content_len);

            // Free the data immediately because it my be very large
            free(data);

            // Use the `qweb_post_cb_ret_t` to construct a response
            httpd_resp_set_status( req, ret.success ? HTTPD_200 : HTTPD_500 );
            httpd_resp_set_type(req, ret.resp_type);
            
            // Send the response
            // (determine if we need to provide the data size or use the null terminator)
            ESP_ERROR_CHECK(httpd_resp_send(req, ret.data, ret.nullterm ? HTTPD_RESP_USE_STRLEN : ret.size ));

            // For qweb_post_cb_ret_t responses with a dynamic buffer, it needs to be freed 
            if (ret.dynamic) {
                free(ret.data);
            }

            return ESP_OK;
        } else {
            ESP_LOGE(
                TAG, 
                "Attempted to post content of length %ub, which is too large for the http-server buffer size %ub", 
                req->content_len, 
                QWEB_MAX_CONTENT_RECEIVE
            );
        }
    } else {
        ESP_LOGE(TAG, "Could not find post callback for %s", fpath_beg);
    } 

    // Reply error to the client
    httpd_resp_send_500(req);
    return ESP_OK;

}

httpd_uri_t uri_download = {
    .method = HTTP_GET,
    .uri = "/*",
    .user_ctx = NULL,
    .handler = serv_get_handler
};

httpd_uri_t uri_post = {
    .method = HTTP_POST,
    .uri = "/*",
    .user_ctx = NULL,
    .handler = serv_post_handler
};

// Handler for the global qweb httpd instance
static httpd_handle_t qweb_server = NULL;

void qweb_init() {

    ESP_LOGI(TAG, "starting webserver");
    esp_err_t err;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "starting server on port: '%d'", config.server_port);
    if ((err = httpd_start(&qweb_server,&config)) == ESP_OK) {
        // Register handler for all pages
        httpd_register_uri_handler(qweb_server, &uri_download);
        // Register handler for all post requests
        httpd_register_uri_handler(qweb_server, &uri_post);
    } else {
        ESP_ERROR_CHECK(err);
    }

}


qweb_server_page_id_t qweb_register_page(const char* fpath, const char* ctype, const char* content, size_t content_length) {
    http_file_ent_t entry = {
        .fname = fpath,
        .type = ctype,
        .content = content,
        .content_length = content_length
    };
    ESP_LOGI(TAG, "Registering page \"%s\" -> \"%s\"    : (%d/%d)", fpath, ctype, STC_VEC_CNT(http_file_entries), STC_VEC_CAP(http_file_entries));
    STC_VEC_PUSH(http_file_entries, entry);
    return STC_VEC_CNT(http_file_entries) - 1;
}
void qweb_register_post_cb(const char *path, qweb_post_cb_t cb)
{
    http_post_cb_entry_t entry = {
        .fpath = path,
        .cb=cb
    };

    STC_VEC_PUSH(http_post_entries, entry);
    ESP_LOGI(TAG, "registering post callback: { \"%s\" }    (%u/%u)", path, STC_VEC_CNT(http_post_entries) - 1, STC_VEC_CAP(http_post_entries));

}

void qweb_page_trunc(qweb_server_page_id_t page, size_t content_length) {
    http_file_entries[page].content_length = content_length;
}
void qweb_page_trunc_path(const char* fpath, size_t length) {
    http_file_ent_t* entry = http_file_get_entry( fpath, strlen(fpath) );
    if (entry) {
        entry->content_length = length;
    }
}

void qweb_cleanup_registry()
{
    // cleanup extra capacity
    STC_VEC_CLEANUP(http_file_entries);
    STC_VEC_CLEANUP(http_post_entries);
}


void qweb_free() {

    httpd_stop(qweb_server);
    free( http_file_entries );
    http_file_entries = NULL;
    free( http_post_entries );
    http_post_entries = NULL;

    STC_VEC_CNT(http_file_entries) = 0;
    STC_VEC_CAP(http_file_entries) = 0;
    STC_VEC_CNT(http_post_entries) = 0;
    STC_VEC_CAP(http_post_entries) = 0;


}