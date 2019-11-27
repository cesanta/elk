#include "elk.h"  // Add Elk library

extern "C" void myDelay(int milli) {
  delay(milli);
}
extern "C" void myWrite(int pin, int val) {
  digitalWrite(pin, val);
}
extern "C" void myMode(int pin, int mode) {
  pinMode(pin, mode);
}

struct js *js;

void setup() {
  js = js_create(malloc(700), 700);
  js_import(js, "f1", (uintptr_t) myDelay, "vi");
  js_import(js, "f2", (uintptr_t) myWrite, "vii");
  js_import(js, "f3", (uintptr_t) myMode, "vii");
  js_eval(js, "f3(13, 1);", 0);  // Set LED pin 13 to OUTPUT mode
}

void loop() {
  js_eval(js, "f1(200); f2(13, 1); f1(200); f2(13, 0);", 0);  // LED pin 13
}
