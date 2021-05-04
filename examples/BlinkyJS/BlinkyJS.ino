extern "C" {
#include "elk.h"

void myDelay(int milli) { delay(milli);}
void myWrite(int pin, int val) { digitalWrite(pin, val); }
void myMode(int pin, int mode) { pinMode(pin, mode); }
}

char buf[300];  // Runtime JS memory
void setup() {
  struct js *js = js_create(buf, sizeof(buf));
  jsval_t global = js_glob(js), gpio = js_mkobj(js);    // Equivalent to:
  js_set(js, global, "gpio", gpio);                     // let gpio = {};
  js_set(js, global, "delay", js_import(js, (uintptr_t) myDelay, "vi"));
  js_set(js, gpio, "mode", js_import(js, (uintptr_t) myMode, "vii"));
  js_set(js, gpio, "write", js_import(js, (uintptr_t) myWrite, "vii"));

  js_eval(js, "let pin = 13;"     // LED pin. Usually 13, but double-check
          "gpio.mode(pin, 1);"    // Set OUTPUT mode on a LED pin
          "while (true) {"
          "  delay(300);"
          "  gpio.write(pin, 1);"
          "  delay(300);"
          "  gpio.write(pin, 0);"
          "}",
          ~0);
}

void loop() {
}