#ifndef __HTTP_H
#define __HTTP_H

#define REQUEST_BUFFER_SIZE     4096
#define MAX_HEADER_SIZE         25

#include <stddef.h>
#include <time.h>
#include "buffer.h"

typedef struct _request         request_t;
typedef struct _response        response_t;
typedef struct _http_parser     http_parser_t;
typedef struct _http_header     http_header_t;
typedef struct _common_headers  common_headers_t;

typedef enum {
    HTTP_VERSION_0_9 = 9,
    HTTP_VERSION_1_0 = 10,
    HTTP_VERSION_1_1 = 11
} http_version_e;

typedef enum _http_methods {
    HTTP_METHOD_EXTENDED = 0,
    HTTP_METHOD_GET = 1, 
    HTTP_METHOD_POST, 
    HTTP_METHOD_PUT, 
    HTTP_METHOD_DELETE,
    HTTP_METHOD_HEAD,
    HTTP_METHOD_TRACE,
    HTTP_METHOD_CONNECT,
    HTTP_METHOD_OPTIONS
} http_method_e;

typedef enum _parser_state {
    PARSER_STATE_BAD_REQUEST = -1,
    PARSER_STATE_COMPLETE = 0,
    PARSER_STATE_METHOD,
    PARSER_STATE_PATH,
    PARSER_STATE_QUERY_STR,
    PARSER_STATE_VERSION,
    PARSER_STATE_HEADER_NAME,
    PARSER_STATE_HEADER_COLON,
    PARSER_STATE_HEADER_VALUE,
    PARSER_STATE_HEADER_CR,
    PARSER_STATE_HEADER_LF,
    PARSER_STATE_HEADER_COMPLETE_CR,
} parser_state_e;


typedef enum _connection_opt {
    CONN_CLOSE = 0,
    CONN_KEEP_ALIVE
} connection_opt_e;


struct _common_headers {
    // Fields for standard headers
    char                    *host;
    char                    *referer;
    connection_opt_e        conn_opt;
    char                    *keep_alive;

    char                    *accept;
    char                    *accept_encoding;
    char                    *accept_lang;
    char                    *accept_charset;

    char                    *cache_ctrl;
    time_t                  if_mod_since;
    char                    *if_none_match;
    char                    *user_agent;

    char                    *cookie;

    // Response headers
    time_t                  date;
    char                    *server;
    time_t                  last_mod;
    char                    *etag;
    char                    *accept_ranges;

    char                    *content_type;
    int                     *content_len;

};


struct _request {
    char                    *path;
    char                    *query_str;
    char                    *method;
    http_version_e          version;

    common_headers_t        headers_in;
    common_headers_t        headers_out;

    int                     header_count;

    buf_queue_t             *out_buf_q;
};


struct _http_header {
    char    *name;
    char    *value;
};


struct _http_parser {
    char                    *path;
    char                    *query_str;
    char                    *method;
    char                    *http_version;

    http_header_t           headers[MAX_HEADER_SIZE];
    int                     header_count;

    // All the fields' value is stored in this buffer 
    char                    _buffer[REQUEST_BUFFER_SIZE];
    int                     _buf_idx;

    // Current token, it points to an offset to _buffer
    char                    *_cur_tok;

    // Parser state
    parser_state_e          _state;
};


int parser_init(http_parser_t *parser);
int parser_destroy(http_parser_t *parser);
int parser_reset(http_parser_t *parser);
int parser_parse_request(http_parser_t *parser, const char *data, const size_t data_len, size_t *consumed_len);

#endif /* end of include guard: __HTTP_H */
