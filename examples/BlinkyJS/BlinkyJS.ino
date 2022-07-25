#include "elk.h"

#if 0
void pr(const char *buf) {
  Serial.println(buf);
};

jsval_t x(struct js *js, jsval_t *args, int nargs) {
  int pin = (int) js_getnum(args[0]);
  // int pin = 13;
  Serial.print("add: ");
  Serial.println((unsigned long) x);
  pinMode(pin, 1);
  digitalWrite(pin, 1);
  return js_mknum(0);
}
#endif

jsval_t myDelay(struct js *js, jsval_t *args, int nargs) {
  delay(js_getnum(args[0]));
  return js_mknum(0);
}
jsval_t myWrite(struct js *js, jsval_t *args, int nargs) {
  digitalWrite(js_getnum(args[0]), js_getnum(args[1]));
  return js_mknum(0);
}
jsval_t myMode(struct js *js, jsval_t *args, int nargs) {
  pinMode(js_getnum(args[0]), js_getnum(args[1]));
  return js_mknum(0);
}

char buf[300];  // Runtime JS memory
void setup() {
  struct js *js = js_create(buf, sizeof(buf));
  jsval_t global = js_glob(js), gpio = js_mkobj(js);  // Equivalent to:
#if 0
  js_set(js, global, "x", js_mkfun(x));
#endif
  js_set(js, global, "delay", js_mkfun(myDelay));  // Import delay()
  js_set(js, global, "gpio", gpio);                // let gpio = {};
  js_set(js, gpio, "mode", js_mkfun(myMode));      // Import gpio.mode()
  js_set(js, gpio, "write", js_mkfun(myWrite));    // Import gpio.write()

  // Serial.begin(115200);
  // for (;;) Serial.println(js_str(js, js_eval(js, "'abc'", ~0U))),
  // delay(1000);

  js_eval(js,
          "let pin = 13;"       // LED pin. Usually 13, but double-check
          "gpio.mode(pin, 1);"  // Set OUTPUT mode on a LED pin
          "for (;;) {"
          "  delay(300);"
          "  gpio.write(pin, 1);"
          "  delay(300);"
          "  gpio.write(pin, 0);"
          "}",
          ~0U);
}

void loop() {
}
