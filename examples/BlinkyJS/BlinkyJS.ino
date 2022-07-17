#include "elk.h"

jsval_t jsDelay(struct js *js, jsval_t *args, int nargs) {
  long ms = js_getnum(args[0]);
  delay(ms);
  return js_mknum(0);
}
jsval_t jsWrite(struct js *js, jsval_t *args, int nargs) {
  int pin = js_getnum(args[0]);
  int val = js_getnum(args[1]);
  digitalWrite(pin, val);
  return js_mknum(0);
}
jsval_t jsMode(struct js *js, jsval_t *args, int nargs) {
  int pin = js_getnum(args[0]);
  int mode = js_getnum(args[1]);
  pinMode(pin, mode);
  return js_mknum(0);
}

char buf[300];  // Runtime JS memory
struct js *js;  // JS instance

void setup() {
  js = js_create(buf, sizeof(buf));
  jsval_t global = js_glob(js), gpio = js_mkobj(js);  // Equivalent to:
  js_set(js, global, "gpio", gpio);                   // let gpio = {};
  js_set(js, global, "delay", js_mkfun(jsDelay));     // import delay()
  js_set(js, gpio, "mode", js_mkfun(jsMode));         // import gpio.mode()
  js_set(js, gpio, "write", js_mkfun(jsWrite));       // import gpio.write()

  js_eval(js, "let pin = 13;", ~0U);       // LED_BUILTIN pin. Usually 13
  js_eval(js, "gpio.mode(pin, 1);", ~0U);  // Set OUTPUT mode on a LED pin
}

void loop() {
  js_eval(js,
          "delay(300);"
          "gpio.write(pin, 1);"
          "delay(300);"
          "gpio.write(pin, 0);",
          ~0U);
}
