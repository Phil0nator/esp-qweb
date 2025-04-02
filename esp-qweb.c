#include <stdio.h>
#include "esp-qweb.h"
#include "static-containers.h"

#include "esp_log.h"
#include "esp_err.h"

#include "esp_http_server.h"
#include "lcl_hmap.h"

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
    bool supress_log: 1;            // supress logs about this post
} http_post_cb_entry_t;

typedef struct qweb_server {
    const char* name;
    lcl_hmap_t* files;
    lcl_hmap_t* post_cbs;

    size_t max_recvlen;

    httpd_handle_t httpd;
    httpd_uri_t get_uri;
    httpd_uri_t post_uri;
    
} qweb_server_t;



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
 *  This function will search the file system for the correct file
 *  and send it to the client.
 */
static esp_err_t serv_get_handler(httpd_req_t* req) {
    
    const char* fpath_beg = req->uri;
    const char* fpath_end = uri_get_fpath_end(fpath_beg);
    size_t fpath_size = fpath_end - fpath_beg;

    ESP_LOGI(TAG, "GET: %s", req->uri);
    char* fpath = malloc(fpath_size+1);
    strncpy(fpath, fpath_beg, fpath_size);
    fpath[fpath_size] = '\0';

    qweb_server_t* server = (qweb_server_t*) req->user_ctx;
    const http_file_ent_t* content;
    lcl_any_t content_any = NULL;
    lcl_hmap_get( server->files, fpath, &content_any ); // error handled later
    free(fpath);
    content = lcl_any2ptr(content_any);

    // If the file exists
    if (content) {
        // Construct reply
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_set_type(req, content->type);
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        ESP_LOGI(TAG,"HTTP 200 OK: %ub", content->content_length);

        // Send reply
        ESP_ERROR_CHECK( httpd_resp_send(req, content->content, content->content_length));

    } else {
        // 404 for files that don't exist
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

    qweb_server_t* server = (qweb_server_t*) req->user_ctx;
    
    char* fpath = malloc(fpath_size+1);
    strncpy(fpath, fpath_beg, fpath_size);
    fpath[fpath_size] = '\0';

    const http_post_cb_entry_t* cbent;
    lcl_any_t cbent_any = NULL;
    lcl_hmap_get( server->post_cbs, fpath, &cbent_any ); // error handling done later
    free(fpath);

    cbent = lcl_any2ptr(cbent_any);

    // Search file system for a post request handler with the correct fpath
    // const http_post_cb_entry_t* cbent = http_post_cb_get_entry(fpath_beg, fpath_size);
    
    // If found...
    if (cbent) {
        // Ensure that the maximum data content size is not exceeded
        if (req->content_len < server->max_recvlen) {
            
            if (!cbent->supress_log) {
                ESP_LOGI(TAG, "POST: %s", req->uri);
            }

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
            
            esp_err_t send_err = httpd_resp_send(
                req, 
                ret.dynamic ? ret.d_data : ret.s_data , 
                ret.nullterm ? HTTPD_RESP_USE_STRLEN : ret.size 
            );

            if (send_err != ESP_OK) {
                ESP_LOGE(TAG, "Could not send resonse, got ESP_ERROR: (%d)", send_err);
            }

            // For qweb_post_cb_ret_t responses with a dynamic buffer, it needs to be freed 
            if (ret.dynamic) {
                free(ret.d_data);
            }

            return ESP_OK;
        } else {
            ESP_LOGE(
                TAG, 
                "Attempted to post content of length %ub, which is too large for the http-server buffer size %ub", 
                req->content_len, 
                server->max_recvlen
            );
        }
    } else {
        ESP_LOGE(TAG, "Could not find post callback for POST %s", fpath_beg);
    } 

    // Reply error to the client
    httpd_resp_send_500(req);
    return ESP_OK;

}



qweb_server_t* qweb_init(const qweb_server_config_t* cfg) {

    ESP_LOGI(TAG, "starting webserver");
    esp_err_t err;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    qweb_server_t* server = calloc(sizeof(qweb_server_t), 1);

    config.server_port = cfg->port;
    config.max_open_sockets = cfg->max_sockets;
    server->name = cfg->name;
    server->max_recvlen = cfg->max_recvlen;
    config.stack_size = cfg->stack_size;
    

    httpd_uri_t get_uri = {
        .method = HTTP_GET,
        .uri = "/*",
        .user_ctx = server,
        .handler = serv_get_handler
    };
    
    httpd_uri_t post_uri = {
        .method = HTTP_POST,
        .uri = "/*",
        .user_ctx = server,
        .handler = serv_post_handler
    };

    server->get_uri = get_uri;
    server->post_uri = post_uri;
    lcl_hmap_init(&server->files, lcl_hash_djb2, lcl_streq);
    lcl_hmap_init(&server->post_cbs, lcl_hash_djb2, lcl_streq);

    ESP_LOGI(TAG, "starting server on port: '%d'", config.server_port);
    if ((err = httpd_start(&server->httpd,&config)) == ESP_OK) {
        // Register handler for all files
        httpd_register_uri_handler(server->httpd, &server->get_uri);
        // Register handler for all post requests
        httpd_register_uri_handler(server->httpd, &server->post_uri);
    } else {
        ESP_ERROR_CHECK(err);
    }

    return server;

}


void qweb_register_file(qweb_server_t* server, const char* fpath, const char* ctype, const char* content, size_t content_length) {
    http_file_ent_t entry = {
        .fname = fpath,
        .type = ctype,
        .content = content,
        .content_length = content_length
    };
    ESP_LOGI(TAG, "Registering file \"%s\" -> \"%s\"", fpath, ctype);
    http_file_ent_t *ent_alloc = (http_file_ent_t*) malloc(sizeof(http_file_ent_t));
    *ent_alloc = entry;
    
    bool old_w;
    lcl_any_t old;
    lcl_hmap_insert( server->files, (lcl_any_t) fpath, ent_alloc, &old, &old_w );
    if (old_w) {
        free(old);
    }
    
}
void qweb_register_post_cb(qweb_server_t* server, const char *path, qweb_post_handler_t handler)
{
    http_post_cb_entry_t entry = {
        .fpath = path,
        .cb = handler.cb,
        .supress_log = handler.supress_log
    };

    ESP_LOGI(TAG, "registering post callback: { \"%s\" } ", path);
    
    http_post_cb_entry_t *ent_alloc = (http_post_cb_entry_t*) malloc(sizeof(http_post_cb_entry_t));
    *ent_alloc = entry;
    
    bool old_w;
    lcl_any_t old;
    lcl_hmap_insert( server->post_cbs, (lcl_any_t) path, ent_alloc, &old, &old_w );
    if (old_w) {
        free(old);
    }


}



void qweb_file_trunc_path(qweb_server_t* server, const char* fpath, size_t length) {
    http_file_ent_t* content;
    lcl_any_t content_any = NULL;
    lcl_hmap_get( server->files, (lcl_any_t) fpath, &content_any ); // error handled later
    content = lcl_any2ptr(content_any);
    if (content) {
        content->content_length = length;
    }
}



void qweb_free(qweb_server_t* server) {

    httpd_stop(server->httpd);
    lcl_hmap_free(&server->files, NULL, LCL_DEALLOC_FREE);
    lcl_hmap_free(&server->post_cbs, NULL, LCL_DEALLOC_FREE);

}