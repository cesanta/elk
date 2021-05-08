// Copyright (c) 2021 Cesanta Software Limited
// All rights reserved

#include <Arduino.h>

extern "C" {
#include "elk.h"
#include "mjson.h"
#include "mongoose.h"

static const size_t s_js_size = 8 * 1024;
static struct js *s_js;
static struct mg_mgr s_mgr;

// These functions below will be imported into the JS engine.
// Note that they are inside the extern "C" section.

static void gpio_write(int pin, int val) {
  // LOG(LL_INFO, ("write %d -> %d", pin, val));
  digitalWrite(pin, val);
}

static int gpio_read(int pin) {
  return digitalRead(pin);
}

static void gpio_mode(int pin, int mode) {
  // LOG(LL_INFO, ("mode %d -> %d", pin, mode));
  pinMode(pin, mode);
}

static struct mg_timer *mktimer(int ms, void (*fn)(void *), void *userdata) {
  struct mg_timer *t = (struct mg_timer *) calloc(1, sizeof(*t));
  mg_timer_init(t, ms, MG_TIMER_REPEAT, fn, userdata);
  // LOG(LL_INFO, ("%p", t));
  return t;
}

static void deltimer(struct mg_timer *t) {
  // LOG(LL_INFO, ("%p", t));
  mg_timer_free(t);
  free(t);
}

static void mylog(const char *msg) {
  LOG(LL_INFO, ("%s", msg));
}

static unsigned long ram(void) {
  return xPortGetFreeHeapSize();
}

static unsigned long usage(void) {
  return js_usage(s_js);
}

static jsval_t myeval(const char *code) {
  return js_eval(s_js, code, strlen(code));
}

static const char *mystr(jsval_t v) {
  return js_str(s_js, v);
}

static char *mkhash(char *buf) {
  unsigned char sha[21] = "", entropy[20];
  mg_sha1_ctx sha_ctx;
  mg_random(entropy, sizeof(entropy));
  mg_sha1_init(&sha_ctx);
  mg_sha1_update(&sha_ctx, entropy, sizeof(entropy));
  mg_sha1_final(sha, &sha_ctx);
  mg_base64_encode(sha, sizeof(sha), buf);
  return buf;
}

typedef void (*mqtt_cb_t)(const char *, const char *, void *userdata);
static mqtt_cb_t s_mqtt_cb;
static void *s_mqtt_cb_userdata;
static void mqset(mqtt_cb_t cb, void *userdata) {
  // LOG(LL_INFO, ("%p %p", cb, userdata));
  s_mqtt_cb = cb;
  s_mqtt_cb_userdata = userdata;
}

static void mqtt_cb(struct mg_connection *c, int ev, void *evd, void *fnd) {
  if (ev == MG_EV_MQTT_MSG && s_mqtt_cb != NULL) {
    // LOG(LL_INFO, ("%p %p", s_mqtt_cb, s_mqtt_cb_userdata));
    struct mg_mqtt_message *mm = (struct mg_mqtt_message *) evd;
    std::string topic(mm->topic.ptr, mm->topic.len);
    std::string message(mm->data.ptr, mm->data.len);
    s_mqtt_cb(topic.c_str(), message.c_str(), s_mqtt_cb_userdata);
  }
}

static struct mg_connection *mqcon(const char *url) {
  char buf[100];
  struct mg_mqtt_opts opts = {.client_id = mg_str(mkhash(buf))};
  return mg_mqtt_connect(&s_mgr, url, &opts, mqtt_cb, NULL);
}

static void mqdis(struct mg_connection *c) {
  if (c) c->is_closing = 1;
}

static void mqpub(struct mg_connection *c, const char *topic, const char *msg) {
  struct mg_str t = mg_str(topic), m = mg_str(msg);
  if (c) mg_mqtt_pub(c, &t, &m);
}

static void mqsub(struct mg_connection *c, const char *topic) {
  struct mg_str t = mg_str(topic);
  if (c) mg_mqtt_sub(c, &t);
}
}

static struct js *jsinit(void *mem, size_t size) {
  struct js *js = js_create(mem, size);
  jsval_t gpio = js_mkobj(js);

  js_set(js, js_glob(js), "gpio", gpio);
  js_set(js, gpio, "mode", js_import(js, (uintptr_t) gpio_mode, "vii"));
  js_set(js, gpio, "write", js_import(js, (uintptr_t) gpio_write, "vii"));
  js_set(js, gpio, "read", js_import(js, (uintptr_t) gpio_read, "ii"));

  jsval_t timer = js_mkobj(js);
  js_set(js, js_glob(js), "timer", timer);
  js_set(js, timer, "create", js_import(js, (uintptr_t) mktimer, "pi[vu]u"));
  js_set(js, timer, "delete", js_import(js, (uintptr_t) deltimer, "vp"));

  js_set(js, js_glob(js), "log", js_import(js, (uintptr_t) mylog, "vs"));
  js_set(js, js_glob(js), "ram", js_import(js, (uintptr_t) ram, "i"));
  js_set(js, js_glob(js), "str", js_import(js, (uintptr_t) mystr, "sj"));
  js_set(js, js_glob(js), "usage", js_import(js, (uintptr_t) usage, "i"));
  js_set(js, js_glob(js), "eval", js_import(js, (uintptr_t) myeval, "js"));

  jsval_t mqtt = js_mkobj(js);
  js_set(js, js_glob(js), "mqtt", mqtt);
  js_set(js, mqtt, "connect", js_import(js, (uintptr_t) mqcon, "ps"));
  js_set(js, mqtt, "disconnect", js_import(js, (uintptr_t) mqdis, "vp"));
  js_set(js, mqtt, "publish", js_import(js, (uintptr_t) mqpub, "vpss"));
  js_set(js, mqtt, "subscribe", js_import(js, (uintptr_t) mqsub, "vps"));
  js_set(js, mqtt, "setfn", js_import(js, (uintptr_t) mqset, "v[vssu]u"));

  return js;
}

static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    if (mg_http_match_uri(hm, "/ws")) {
      mg_ws_upgrade(c, hm, NULL);
    } else {
      mg_http_reply(c, 302, "Location: http://elk-js.com/\r\n", "");
    }
  } else if (ev == MG_EV_WS_MSG) {
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    LOG(LL_VERBOSE_DEBUG, ("WS msg: %.*s", (int) wm->data.len, wm->data.ptr));
    char *response = NULL;
    jsonrpc_process(wm->data.ptr, wm->data.len, mjson_print_dynamic_buf,
                    &response, NULL);
    size_t n = response ? strlen(response) : 0;
    while (n > 1 && response[n - 1] == '\n') n--;
    LOG(LL_INFO, ("RESP: [%.*s]", (int) n, response));
    mg_ws_send(c, response, n, WEBSOCKET_OP_TEXT);
    free(response);
  }
  (void) fn_data;
}

static void rpc_cb_exec(struct jsonrpc_request *r) {
  char buf[r->params_len] = "";
  mjson_get_string(r->params, r->params_len, "$.code", buf, sizeof(buf));
  LOG(LL_VERBOSE_DEBUG, ("[%s]", buf));
  if (buf[0] != '\0') {
    jsval_t cres = js_eval(s_js, "cleanup();", ~0);
    LOG(LL_INFO, ("cleanup result: %s", js_str(s_js, cres)));
    s_js = jsinit(s_js, s_js_size);
    jsval_t v = js_eval(s_js, buf, strlen(buf));
    jsonrpc_return_success(r, "%Q", js_str(s_js, v));
  } else {
    jsonrpc_return_error(r, 500, "missing code", NULL);
  }
}

static void log_cb(const void *buf, int len, void *userdata) {
  struct mg_mgr *m = (struct mg_mgr *) userdata;
  char *data = NULL;
  printf("%.*s", len, buf);
  mjson_printf(mjson_print_dynamic_buf, &data, "{%Q:%Q,%Q:%V}", "name", "log",
               "data", len, buf);
  for (struct mg_connection *c = m->conns; c != NULL; c = c->next) {
    if (!c->is_websocket) continue;
    mg_ws_send(c, data, strlen(data), WEBSOCKET_OP_TEXT);
  }
  free(data);
}

static void webtask(void) {
  jsonrpc_init(NULL, NULL);
  jsonrpc_export("exec", rpc_cb_exec);
  s_js = jsinit(malloc(s_js_size), s_js_size);
  // mg_log_set("3");
  mg_mgr_init(&s_mgr);
  mg_log_set_callback(log_cb, &s_mgr);
  mg_http_listen(&s_mgr, "http://0.0.0.0:80", cb, &s_mgr);
  LOG(LL_INFO, ("Starting Mongoose v%s", MG_VERSION));
  for (;;) mg_mgr_poll(&s_mgr, 100);
}

class JS {
 public:
  JS() {
  }
  bool begin(void) {
    xTaskCreate((void (*)(void *)) webtask, "web server", 16384, NULL, 1, NULL);
  }
};

static JS JS;
