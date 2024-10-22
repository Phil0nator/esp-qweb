#include <stdio.h>
#include "esp-qweb.h"
#include "static-containers.h"

#include "esp_log.h"
#include "esp_err.h"

#include "esp_http_server.h"


#define PTR_MIN(a,b)    (((a) < (b)) ? (a) : (b))
#define FILEPATH_MAX        (256)

static const char* TAG = "http-server";

typedef struct http_file_ent {
    const char* fname;
    const char* type;
    const char* content;
    size_t content_length;
} http_file_ent_t;

typedef struct http_post_cb_entry {
    const char* fpath;
    qweb_post_cb_t cb;
} http_post_cb_entry_t;


STC_VEC_DECL( http_post_cb_entry_t, http_post_entries );
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

static esp_err_t serv_get_handler(httpd_req_t* req) {
    
    const char* fpath_beg = req->uri;
    const char* fpath_end = uri_get_fpath_end(fpath_beg);
    size_t fpath_size = fpath_end - fpath_beg;

    ESP_LOGI(TAG, "GET: %s", req->uri);
    const http_file_ent_t* content = http_file_get_entry( fpath_beg, fpath_size );
    if (content) {
        
        httpd_resp_set_status(req, HTTPD_200);
        httpd_resp_set_type(req, content->type);
        httpd_resp_set_hdr(req, "Connection", "keep-alive");
        ESP_LOGI(TAG,"HTTP 200 OK: %ub", content->content_length);
        ESP_ERROR_CHECK( httpd_resp_send(req, content->content, content->content_length));

    } else {
        httpd_resp_send_404(req);
    }

    return ESP_OK;
}

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


static esp_err_t serv_post_handler(httpd_req_t* req) {
    const char* fpath_beg = req->uri;
    const char* fpath_end = uri_get_fpath_end(fpath_beg);
    size_t fpath_size = fpath_end - fpath_beg;

    ESP_LOGI(TAG, "POST: %s", req->uri);
    const http_post_cb_entry_t* cbent = http_post_cb_get_entry(fpath_beg, fpath_size);
    if (cbent) {
        if (req->content_len < QWEB_MAX_CONTENT_RECEIVE) {
            char *data;
            
            httpd_req_recv_all( req, &data);
            
            qweb_post_cb_ret_t ret = cbent->cb(req->uri, data);
            free(data);

            httpd_resp_set_status( req, ret.success ? HTTPD_200 : HTTPD_500 );
            httpd_resp_set_type(req, ret.resp_type);
            ESP_ERROR_CHECK(httpd_resp_send(req, ret.data, ret.nullterm ? HTTPD_RESP_USE_STRLEN : ret.size ));

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



void qweb_init() {
    ESP_LOGI(TAG, "starting webserver");
    esp_err_t err;
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "starting server on port: '%d'", config.server_port);
    if ((err = httpd_start(&server,&config)) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_download);
        httpd_register_uri_handler(server, &uri_post);
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
