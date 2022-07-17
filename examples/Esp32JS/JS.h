// Copyright (c) 2021 Cesanta Software Limited
// All rights reserved

#include <Arduino.h>
#include "elk.h"
#include "mongoose.h"

#define JS_MEM_SIZE 8192

static struct js *s_js;      // JS instance
static struct mg_mgr s_mgr;  // Mongoose event manager

// A C resource that requires cleanup after JS instance deallocation For
// example, a network connection, or a timer, are resources that are handled by
// C code. JS just holds a reference to it - an ID of some sort.  When a JS
// instance is deleted, we need to cleanup all C resources that instance has
// allocated. Therefore when we allocate a C resource and give away a handle to
// the JS, we insert a "deallocation descriptor", struct resource, to the list
// s_rhead.
struct resource {
  void *data;               // Resource data
  void (*cleanup)(void *);  // Cleanup function
  struct resource *next;    // Next resource
};
static struct resource *s_rhead;  // Allocated C resources

static void addresource(void (*fn)(void *), void *data) {
  struct resource *r = (struct resource *) calloc(1, sizeof(r));
  r->data = (void *) data;
  r->cleanup = fn;
  r->next = s_rhead;
  s_rhead = r;
}

static void delresource(void (*fn)(void *), void *data) {
  struct resource **head = &s_rhead, *r;
  while (*head && (*head)->cleanup != fn && (*head)->data != data)
    head = &(*head)->next;
  if (*head) r = *head, r->cleanup(r->data), *head = r->next, free(r);
}

// These functions below will be imported into the JS engine.
// Note that they are inside the extern "C" section.
static jsval_t gpio_write(struct js *js, jsval_t *args, int nargs) {
  int pin, val;
  jsval_t res = js_checkargs(js, args, nargs, "ii", &pin, &val);
  if (js_type(res) == JS_UNDEF) {
    MG_INFO(("gpio.write %d -> %d", pin, val));
    digitalWrite(pin, val);
  }
  return res;
}

static jsval_t gpio_read(struct js *js, jsval_t *args, int nargs) {
  int pin;
  jsval_t res = js_checkargs(js, args, nargs, "i", &pin);
  if (js_type(res) == JS_UNDEF) {
    MG_INFO(("gpio.read %d", pin));
    res = js_mknum(digitalRead(pin));
  }
  return res;
}

static jsval_t gpio_mode(struct js *js, jsval_t *args, int nargs) {
  int pin, mode;
  jsval_t res = js_checkargs(js, args, nargs, "ii", &pin, &mode);
  if (js_type(res) == JS_UNDEF) {
    MG_INFO(("gpio.mode %d -> %d", pin, mode));
    pinMode(pin, mode);
  }
  return res;
}

void timer_cleanup(void *data) {
  unsigned long id = (unsigned long) data;
  struct mg_timer **head = (struct mg_timer **) &s_mgr.timers, *t;
  while (*head && (*head)->id != id) head = &(*head)->next;
  if (*head) {
    MG_INFO(("%lu (%s)", id, (char *) (*head)->arg));
    t = *head, *head = t->next, free(t->arg), free(t);
  }
}

static void js_timer_fn(void *userdata) {
  char buf[20];
  mg_snprintf(buf, sizeof(buf), "%s();", (char *) userdata);
  MG_INFO(("Calling JS: %s", buf));
  jsval_t res = js_eval(s_js, buf, ~0U);
  if (js_type(res) == JS_ERR)
    MG_ERROR(("%s: %s", (char *) userdata, js_str(s_js, res)));
}

static jsval_t mktimer(struct js *js, jsval_t *args, int nargs) {
  int milliseconds = 0;
  const char *funcname = NULL;
  jsval_t res = js_checkargs(js, args, nargs, "is", &milliseconds, &funcname);
  if (js_type(res) == JS_UNDEF) {
    struct mg_timer *t = mg_timer_add(&s_mgr, milliseconds, MG_TIMER_REPEAT,
                                      js_timer_fn, strdup(funcname));
    MG_INFO(("mktimer %lu, %d ms, fn %s", t->id, milliseconds, funcname));
    addresource(timer_cleanup, (void *) t->id);
    res = js_mknum(t->id);
  }
  return res;
}

static jsval_t deltimer(struct js *js, jsval_t *args, int nargs) {
  unsigned long id;
  jsval_t res = js_checkargs(js, args, nargs, "i", &id);
  if (js_type(res) == JS_UNDEF) {
    delresource(timer_cleanup, (void *) id);
  }
  return res;
}

static jsval_t js_log(struct js *js, jsval_t *args, int nargs) {
  char buf[1024];
  size_t n = 0;
  for (int i = 0; i < nargs; i++) {
    const char *space = i == 0 ? "" : " ";
    n += mg_snprintf(buf + n, sizeof(buf) - n, "%s%s", space,
                     js_str(js, args[i]));
  }
  buf[sizeof(buf) - 1] = '\0';
  MG_INFO(("JS-> %s", buf));
  return js_mkval(JS_UNDEF);
}

static struct js *jsinit(void *mem, size_t size) {
  struct js *js = js_create(mem, size);

  js_set(js, js_glob(js), "log", js_mkfun(js_log));

  jsval_t gpio = js_mkobj(js);
  js_set(js, js_glob(js), "gpio", gpio);
  js_set(js, gpio, "write", js_mkfun(gpio_write));
  js_set(js, gpio, "mode", js_mkfun(gpio_mode));
  js_set(js, gpio, "read", js_mkfun(gpio_read));

  jsval_t timer = js_mkobj(js);
  js_set(js, js_glob(js), "timer", timer);
  js_set(js, timer, "create", js_mkfun(mktimer));
  js_set(js, timer, "delete", js_mkfun(deltimer));

  return js;
}

static char *rpc_exec(struct mg_str req) {
  char *code = mg_json_get_str(req, "$.params.code");
  // MG_INFO(("Code: [%s]", code));
  if (code) {
    // Deallocate all resources
    for (struct resource *tmp, *r = s_rhead; r != NULL; r = tmp) {
      tmp = r->next;
      r->cleanup(r->data);
      free(r);
    }
    s_rhead = NULL;
    s_js = jsinit(s_js, JS_MEM_SIZE);
    jsval_t v = js_eval(s_js, code, ~0U);
    // js_dump(s_js);
    free(code);
    return mg_mprintf("%Q", js_str(s_js, v));
  } else {
    return mg_mprintf("%Q", "missing code");
  }
}

static void logstats(void) {
  MG_INFO(("Free C RAM: %u, JS RAM: used: %d%% of %d", esp_get_free_heap_size(),
           js_usage(s_js), JS_MEM_SIZE));
}

static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  if (ev == MG_EV_HTTP_MSG) {
    struct mg_http_message *hm = (struct mg_http_message *) ev_data;
    MG_INFO(("HTTP msg: %.*s %.*s", (int) hm->method.len, hm->method.ptr,
             (int) hm->uri.len, hm->uri.ptr));
    if (mg_http_match_uri(hm, "/ws")) {
      mg_ws_upgrade(c, hm, NULL);
    } else {
      mg_http_reply(c, 302, "Location: http://elk-js.com/\r\n", "");
    }
  } else if (ev == MG_EV_WS_MSG) {
    struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
    // MG_INFO(("WS msg: %.*s", (int) wm->data.len, wm->data.ptr));
    char *method = mg_json_get_str(wm->data, "$.method");
    char *response = NULL;
    if (method != NULL && strcmp(method, "exec") == 0) {
      response = rpc_exec(wm->data);
    }
    long id = mg_json_get_long(wm->data, "$.id", 0);
    MG_INFO(("RESP: [%s]", response));
    mg_ws_printf(c, WEBSOCKET_OP_TEXT, "{%Q:%ld,%Q:%s}", "id", id, "result",
                 response);
    free(response);
    free(method);
    logstats();
  }
  (void) fn_data;
}

static void log_cb(uint8_t ch) {
  static char buf[256];
  static size_t len;
  buf[len++] = ch;
  if (ch == '\n' || len >= sizeof(buf)) {
    fwrite(buf, 1, len, stdout);
    char *data = mg_mprintf("{%Q:%Q,%Q:%V}", "name", "log", "data", len, buf);
    for (struct mg_connection *c = s_mgr.conns; c != NULL; c = c->next) {
      if (!c->is_websocket) continue;
      mg_ws_send(c, data, strlen(data), WEBSOCKET_OP_TEXT);
    }
    free(data);
    len = 0;
  }
}

static void *webtask(void *param) {
  s_js = jsinit(malloc(JS_MEM_SIZE), JS_MEM_SIZE);
  // mg_log_set("3");
  mg_mgr_init(&s_mgr);
  mg_log_set_fn(log_cb);
  mg_http_listen(&s_mgr, "http://0.0.0.0:8080", cb, &s_mgr);
  MG_INFO(("Starting Mongoose v%s", MG_VERSION));
  MG_INFO(("Go to http://elk-js.com, enter my IP and connect"));
  for (;;) mg_mgr_poll(&s_mgr, 100);
  return param;
}

class JS {
 public:
  JS() {
  }
  void begin(void) {
    xTaskCreate(webtask, "web server", 16384, NULL, 1, NULL);
  }
};

static JS JS;
