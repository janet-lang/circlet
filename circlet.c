#include <janet.h>
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
    "mongoose.connection",
    NULL,
    connection_mark,
#ifdef JANET_ATEND_GCMARK
    JANET_ATEND_GCMARK
#endif
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
    "mongoose.manager",
    manager_gc,
    manager_mark,
#ifdef JANET_ATEND_GCMARK
    JANET_ATEND_GCMARK
#endif
};

static Janet cfun_poll(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 2);
    struct mg_mgr *mgr = janet_getabstract(argv, 0, &Manager_jt);
    int32_t wait = janet_getinteger(argv, 1);
    mg_mgr_poll(mgr, wait);
    return argv[0];
}

static Janet mg2janetstr(struct mg_str str) {
    return janet_stringv((const uint8_t *) str.p, str.len);
}

/* Turn a string value into c string */
static const char *getstring(Janet x, const char *dflt) {
    if (janet_checktype(x, JANET_STRING)) {
        const uint8_t *bytes = janet_unwrap_string(x);
        return (const char *)bytes;
    } else {
        return dflt;
    }
}

static Janet build_http_request(struct mg_connection *c, struct http_message *hm) {
    JanetTable *payload = janet_table(10);
    janet_table_put(payload, janet_ckeywordv("body"), mg2janetstr(hm->body));
    janet_table_put(payload, janet_ckeywordv("uri"), mg2janetstr(hm->uri));
    janet_table_put(payload, janet_ckeywordv("query-string"), mg2janetstr(hm->query_string));
    janet_table_put(payload, janet_ckeywordv("method"), mg2janetstr(hm->method));
    janet_table_put(payload, janet_ckeywordv("protocol"), mg2janetstr(hm->proto));
    janet_table_put(payload, janet_ckeywordv("connection"), janet_wrap_abstract(c->user_data));
    /* Add headers */
    JanetTable *headers = janet_table(5);
    for (int i = 0; i < MG_MAX_HTTP_HEADERS; i++) {
        if (hm->header_names[i].len == 0)
            break;
        Janet key = mg2janetstr(hm->header_names[i]);
        Janet value = mg2janetstr(hm->header_values[i]);
        Janet header = janet_table_get(headers, key);
        switch (janet_type(header)) {
            case JANET_NIL:
                janet_table_put(headers, key, value);
                break;
            case JANET_ARRAY:
                janet_array_push(janet_unwrap_array(header), value);
                break;
            default:
                {
                    Janet newHeader[2] = { header, value };
                    janet_table_put(headers, key, janet_wrap_array(janet_array_n(newHeader, 2)));
                    break;
                }
        }
    }
    janet_table_put(payload, janet_ckeywordv("headers"), janet_wrap_table(headers));
    return janet_wrap_table(payload);
}

/* Send an HTTP reply. This should try not to panic, as at this point we
 * are outside of the janet interpreter. Instead, send a 500 response with
 * some formatted error message. */
static void send_http(struct mg_connection *c, Janet res, void *ev_data) {
    switch (janet_type(res)) {
        default:
            mg_send_head(c, 500, 0, "");
            break;
        case JANET_TABLE:
        case JANET_STRUCT:
            {
                const JanetKV *kvs;
                int32_t kvlen, kvcap;
                janet_dictionary_view(res, &kvs, &kvlen, &kvcap);

                /* Get response kind and check for special handling methods. */
                Janet kind = janet_dictionary_get(kvs, kvcap, janet_ckeywordv("kind"));
                if (janet_checktype(kind, JANET_KEYWORD)) {
                    const uint8_t *kindstr = janet_unwrap_keyword(kind);

                    /* Check for serving static files */
                    if (!janet_cstrcmp(kindstr, "static")) {
                        /* Construct static serve options */
                        struct mg_serve_http_opts opts;
                        memset(&opts, 0, sizeof(opts));
                        Janet root = janet_dictionary_get(kvs, kvcap, janet_ckeywordv("root"));
                        opts.document_root = getstring(root, NULL);
                        mg_serve_http(c, (struct http_message *) ev_data, opts);
                        return;
                    }

                    /* Check for serving single file */
                    if (!janet_cstrcmp(kindstr, "file")) {
                        Janet filev = janet_dictionary_get(kvs, kvcap, janet_ckeywordv("file"));
                        Janet mimev = janet_dictionary_get(kvs, kvcap, janet_ckeywordv("mime"));
                        const char *mime = getstring(mimev, "text/plain");
                        const char *filepath;
                        if (!janet_checktype(filev, JANET_STRING)) {
                            mg_send_head(c, 500, 0, "expected string :file option to serve a file");
                            break;
                        }
                        filepath = getstring(filev, "");
                        mg_http_serve_file(c, (struct http_message *)ev_data, filepath, mg_mk_str(mime), mg_mk_str(""));
                        return;
                    }
                }

                /* Serve a generic HTTP response */

                Janet status = janet_dictionary_get(kvs, kvcap, janet_ckeywordv("status"));
                Janet headers = janet_dictionary_get(kvs, kvcap, janet_ckeywordv("headers"));
                Janet body = janet_dictionary_get(kvs, kvcap, janet_ckeywordv("body"));

                int code;
                if (janet_checktype(status, JANET_NIL))
                    code = 200;
                else if (janet_checkint(status))
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
                    int32_t header_len;
                    const Janet *header_items;
                    if (janet_indexed_view(kv->value, &header_items, &header_len)) {
                        /* Array-like of headers */
                        for (int32_t i = 0; i < header_len; i++) {
                            const uint8_t *value = janet_to_string(header_items[i]);
                            mg_printf(c, "%s: %s\r\n", (const char *)name, (const char *)value);
                        }
                    } else {
                        /* Single header */
                        const uint8_t *value = janet_to_string(kv->value);
                        mg_printf(c, "%s: %s\r\n", (const char *)name, (const char *)value);
                    }
                }

                mg_printf(c, "Content-Length: %d\r\n", bodylen);
                mg_printf(c, "\r\n");
                if (bodylen) mg_send(c, bodybytes, bodylen);
            }
            break;
    }
    mg_printf(c, "\r\n");
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
    JanetSignal status = janet_continue(fiber, evdata, &out);
    if (status != JANET_SIGNAL_OK && status != JANET_SIGNAL_YIELD) {
        janet_stacktrace(fiber, out);
        return;
    }
    send_http(c, out, p);
}

static Janet cfun_manager(int32_t argc, Janet *argv) {
    janet_fixarity(argc, 0);
    (void) argv;
    void *mgr = janet_abstract(&Manager_jt, sizeof(struct mg_mgr));
    mg_mgr_init(mgr, NULL);
    return janet_wrap_abstract(mgr);
}

/* Common functionality for binding */
static void do_bind(int32_t argc, Janet *argv, struct mg_connection **connout,
        void (*handler)(struct mg_connection *, int, void *)) {
    janet_fixarity(argc, 3);

    /* We use opts, so that we can read the error reason from mongoose if bind fails.
    As described here https://github.com/cesanta/mongoose/issues/983 */
    struct mg_bind_opts opts;
    memset(&opts, 0, sizeof(opts));
    const char *err = NULL;
    opts.error_string = &err;

    struct mg_mgr *mgr = janet_getabstract(argv, 0, &Manager_jt);
    const uint8_t *port = janet_getstring(argv, 1);
    JanetFunction *onConnection = janet_getfunction(argv, 2);
    struct mg_connection *conn = mg_bind_opt(mgr, (const char *)port, handler, opts);
    if (NULL == conn) {
        janet_panicf("could not bind to %s, reason being: %s", port, err);
    }
    JanetFiber *fiber = janet_fiber(onConnection, 64, 0, NULL);
    ConnectionWrapper *cw = janet_abstract(&Connection_jt, sizeof(ConnectionWrapper));
    cw->conn = conn;
    cw->fiber = fiber;
    conn->user_data = cw;
    Janet out;
    JanetSignal status = janet_continue(fiber, janet_wrap_abstract(cw), &out);
    if (status != JANET_SIGNAL_YIELD) {
        janet_stacktrace(fiber, out);
    }
    *connout = conn;
}

static Janet cfun_bind_http(int32_t argc, Janet *argv) {
    struct mg_connection *conn = NULL;
    do_bind(argc, argv, &conn, http_handler);
    mg_set_protocol_http_websocket(conn);
    return argv[0];
}



static int is_websocket(const struct mg_connection *nc) {
  return nc->flags & MG_F_IS_WEBSOCKET;
}

static Janet build_websocket_event(struct mg_connection *c, Janet event, struct websocket_message *wm) {
    JanetTable *payload;
    if (wm) {
       payload = janet_table(4);
       janet_table_put(payload, janet_ckeywordv("data"), janet_stringv((const uint8_t *) wm->data, wm->size));
    } else {
       payload = janet_table(3);
    }

    janet_table_put(payload, janet_ckeywordv("event"), event);
    janet_table_put(payload, janet_ckeywordv("protocol"), janet_cstringv("websocket"));
    janet_table_put(payload, janet_ckeywordv("connection"), janet_wrap_abstract(c->user_data));
    return janet_wrap_table(payload);
}

/* The dispatching event handler. This handler is what
 * is presented to mongoose, but it dispatches to dynamically
 * defined handlers. */
static void http_websocket_handler(struct mg_connection *c, int ev, void *p) {
    Janet evdata;

    switch (ev) {
        default:
            return;

        case MG_EV_HTTP_REQUEST: {
            http_handler(c, ev, p);
            return;
        }

        case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
            evdata = build_websocket_event(c, janet_ckeywordv("open"), NULL);
            break;
        }

        case MG_EV_WEBSOCKET_FRAME: {
            struct websocket_message *wm = (struct websocket_message *) p;
            evdata = build_websocket_event(c, janet_ckeywordv("message"), wm);
            break;
        }

        case MG_EV_CLOSE: {
            evdata = build_websocket_event(c, janet_ckeywordv("close"), NULL);
            break;
        }

    }

    ConnectionWrapper *cw;
    JanetFiber *fiber;
    cw = (ConnectionWrapper *)(c->user_data);
    fiber = cw->fiber;
    Janet out;
    JanetSignal status = janet_continue(fiber, evdata, &out);
    if (status != JANET_SIGNAL_OK && status != JANET_SIGNAL_YIELD) {
        janet_stacktrace(fiber, out);
        return;
    }
}

static Janet cfun_bind_http_websocket(int32_t argc, Janet *argv) {
    struct mg_connection *conn = NULL;
    do_bind(argc, argv, &conn, http_websocket_handler);
    mg_set_protocol_http_websocket(conn);
    return argv[0];
}

static Janet cfun_broadcast(int32_t argc, Janet *argv) {
  janet_fixarity(argc, 2);
  struct mg_mgr *mgr = janet_getabstract(argv, 0, &Manager_jt);
  const char *buf = janet_getcstring(argv, 1);
  struct mg_connection *c;
  for (c = mg_next(mgr, NULL); c != NULL; c = mg_next(mgr, c)) {
    mg_send_websocket_frame(c, WEBSOCKET_OP_TEXT, buf, strlen(buf));
  }

  return argv[0];
}

static const JanetReg cfuns[] = {
    {"manager", cfun_manager, NULL},
    {"poll", cfun_poll, NULL},
    {"bind-http", cfun_bind_http, NULL},
    {"broadcast", cfun_broadcast, NULL},
    {"bind-http-websocket", cfun_bind_http_websocket, NULL},
    {NULL, NULL, NULL}
};

extern const unsigned char *circlet_lib_embed;
extern size_t circlet_lib_embed_size;

JANET_MODULE_ENTRY(JanetTable *env) {
    janet_cfuns(env, "circlet", cfuns);
    janet_dobytes(env,
            circlet_lib_embed,
            circlet_lib_embed_size,
            "circlet_lib.janet",
            NULL);
}
