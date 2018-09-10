#include <janet/janet.h>
#include "mongoose.h"
#include <stdio.h>

typedef struct {
    struct mg_connection *conn;
    JanetFiber *fiber;
} ConnectionWrapper;

static int connection_mark(void *p, size_t size) {
    (void) size;
    ConnectionWrapper *cw = (ConnectionWrapper *)p;
    struct mg_connection *conn = cw->conn;
    JanetFiber *fiber = cw->fiber;
    janet_mark(janet_wrap_fiber(fiber));
    janet_mark(janet_wrap_abstract(conn->mgr));
    return 0;
}

static struct JanetAbstractType Connection_jt = {
    ":mongoose.connection",
    NULL,
    connection_mark
};

static int manager_gc(void *p, size_t size) {
    (void) size;
    mg_mgr_free((struct mg_mgr *) p);
    return 0;
}

static int manager_mark(void *p, size_t size) {
    (void) size;
    struct mg_mgr *mgr = (struct mg_mgr *)p;
    /* Iterate all connections, and mark then */
    struct mg_connection *conn = mgr->active_connections;
    while (conn) {
        ConnectionWrapper *cw = conn->user_data;
        if (cw) {
            janet_mark(janet_wrap_abstract(cw));
        }
        conn = conn->next;
    }
    return 0;
}

static struct JanetAbstractType Manager_jt = {
    ":mongoose.manager",
    manager_gc,
    manager_mark
};

static int cfun_poll(JanetArgs args) {
    struct mg_mgr *mgr;
    int32_t wait;
    JANET_FIXARITY(args, 2);
    JANET_ARG_ABSTRACT(mgr, args, 0, &Manager_jt);
    JANET_ARG_INTEGER(wait, args, 1);
    mg_mgr_poll(mgr, wait);
    JANET_RETURN_ABSTRACT(args, mgr);
}

static Janet mg2janetstr(struct mg_str str) {
    return janet_stringv((const uint8_t *) str.p, str.len);
}

static Janet build_http_request(struct mg_connection *c, struct http_message *hm) {
    JanetTable *payload = janet_table(10);
    janet_table_put(payload, janet_csymbolv(":body"), mg2janetstr(hm->body));
    janet_table_put(payload, janet_csymbolv(":uri"), mg2janetstr(hm->uri));
    janet_table_put(payload, janet_csymbolv(":query-string"), mg2janetstr(hm->query_string));
    janet_table_put(payload, janet_csymbolv(":method"), mg2janetstr(hm->method));
    janet_table_put(payload, janet_csymbolv(":protocol"), mg2janetstr(hm->proto));
    janet_table_put(payload, janet_csymbolv(":connection"), janet_wrap_abstract(c->user_data));
    /* Add headers */
    JanetTable *headers = janet_table(5);
    for (int i = 0; i < MG_MAX_HTTP_HEADERS; i++) {
        if (hm->header_names[i].len == 0)
            break;
        janet_table_put(headers, 
                mg2janetstr(hm->header_names[i]),
                mg2janetstr(hm->header_values[i]));
    }
    janet_table_put(payload, janet_csymbolv(":headers"), janet_wrap_table(headers));
    return janet_wrap_table(payload);
}

/* Send an HTTP reply. */
static void send_http(struct mg_connection *c, Janet res) {
    switch (janet_type(res)) {
        default:
            mg_send_head(c, 500, 0, "");
            break;
        case JANET_INTEGER:
            mg_send_head(c, janet_unwrap_integer(res), 0, "");
            break;
        case JANET_STRING:
        case JANET_BUFFER:
            {
                const uint8_t *bytes;
                int32_t len;
                janet_bytes_view(res, &bytes, &len);
                mg_printf(c, 
                        "HTTP/1.1 200 OK\r\nContent-Length: %d\r\n"
                        "Content-Type: text/plain\r\n\r\n%s", 
                        len, (const char *) bytes);
                break;
            }
        case JANET_TABLE:
        case JANET_STRUCT:
            {
                const JanetKV *kvs;
                int32_t kvlen, kvcap;
                janet_dictionary_view(res, &kvs, &kvlen, &kvcap);

                Janet status = janet_dictionary_get(kvs, kvcap, janet_csymbolv(":status"));
                Janet headers = janet_dictionary_get(kvs, kvcap, janet_csymbolv(":headers"));
                Janet body = janet_dictionary_get(kvs, kvcap, janet_csymbolv(":body"));

                int code;
                if (janet_checktype(status, JANET_NIL))
                    code = 200;
                else if (janet_checktype(status, JANET_INTEGER))
                    code = janet_unwrap_integer(status);
                else
                    break;

                const JanetKV *headerkvs;
                int32_t headerlen, headercap;
                if (janet_checktype(headers, JANET_NIL)) {
                    headerkvs = NULL;
                    headerlen = 0;
                    headercap = 0;
                } else if (!janet_dictionary_view(headers, &headerkvs, &headerlen, &headercap)) {
                    break;
                }

                const uint8_t *bodybytes;
                int32_t bodylen;
                if (janet_checktype(body, JANET_NIL)) {
                    bodybytes = NULL;
                    bodylen = 0;
                } else if (!janet_bytes_view(body, &bodybytes, &bodylen)) {
                    break;
                }

                mg_send_response_line(c, code, NULL);
                for (const JanetKV *kv = janet_dictionary_next(headerkvs, headercap, NULL);
                        kv;
                        kv = janet_dictionary_next(headerkvs, headercap, kv)) {
                    const uint8_t *name = janet_to_string(kv->key);
                    const uint8_t *value = janet_to_string(kv->value);
                    mg_printf(c, "%s: %s\r\n", (const char *)name, (const char *)value);
                }

                if (bodylen) {
                    mg_printf(c, "Content-Length: %d\r\n", bodylen);
                }
                mg_printf(c, "\r\n%.*s", bodylen, (const char *)bodybytes);
            }
            break;
    }
    c->flags |= MG_F_SEND_AND_CLOSE;
}

/* The dispatching event handler. This handler is what
 * is presented to mongoose, but it dispatches to dynamically
 * defined handlers. */
static void http_handler(struct mg_connection *c, int ev, void *p) {
    Janet evdata;
    switch (ev) {
        default:
            return;
        case MG_EV_HTTP_REQUEST:
            evdata = build_http_request(c, (struct http_message *)p);
            break;
    }
    ConnectionWrapper *cw;
    JanetFiber *fiber;
    cw = (ConnectionWrapper *)(c->user_data);
    fiber = cw->fiber;
    Janet out;
    JanetFiberStatus status = janet_continue(fiber, evdata, &out);
    if (status != JANET_STATUS_DEAD && status != JANET_STATUS_PENDING) {
        janet_stacktrace(fiber, "mongooose", out);
        return;
    }
    send_http(c, out);
}

static int cfun_manager(JanetArgs args) {
    void *mgr = janet_abstract(&Manager_jt, sizeof(struct mg_mgr));
    mg_mgr_init(mgr, NULL);
    JANET_RETURN_ABSTRACT(args, mgr);
}

/* Common functionality for binding */
static int do_bind(JanetArgs args, struct mg_connection **connout,
        void (*handler)(struct mg_connection *, int, void *)) {
    struct mg_mgr *mgr;
    struct mg_connection *conn;
    const uint8_t *port;
    JanetFunction *onConnection;
    JanetFiber *fiber;
    JANET_FIXARITY(args, 3);
    JANET_ARG_ABSTRACT(mgr, args, 0, &Manager_jt);
    JANET_ARG_STRING(port, args, 1);
    JANET_ARG_FUNCTION(onConnection, args, 2);
    conn = mg_bind(mgr, (const char *)port, handler);
    fiber = janet_fiber(onConnection, 64);
    ConnectionWrapper *cw = janet_abstract(&Connection_jt, sizeof(ConnectionWrapper));
    cw->conn = conn;
    cw->fiber = fiber;
    conn->user_data = cw;
    Janet out;
    JanetFiberStatus status = janet_continue(fiber, janet_wrap_abstract(cw), &out);
    if (status != JANET_STATUS_PENDING) {
        janet_stacktrace(fiber, "mongooose binding", out);
    }
    *connout = conn;
    JANET_RETURN(args, args.v[0]);
}

static int cfun_bind_http(JanetArgs args) {
    struct mg_connection *conn = NULL;
    int status = do_bind(args, &conn, http_handler);
    if (status) return status;
    mg_set_protocol_http_websocket(conn);
    return status;
}

static const JanetReg cfuns[] = {
    {"manager", cfun_manager},
    {"poll", cfun_poll},
    {"bind-http", cfun_bind_http},
    {NULL, NULL}
};

JANET_MODULE_ENTRY (JanetArgs args) {
    JanetTable *env = janet_env(args);
    janet_cfuns(env, "mongoose", cfuns);
    return 0;
}
