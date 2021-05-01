extern "C" {
#include "elk.h"

void myDelay(int milli) { delay(milli);}
void myWrite(int pin, int val) { digitalWrite(pin, val); }
void myMode(int pin, int mode) { pinMode(pin, mode); }
}

char mem[300]; // Memory for the JS engine in bytes
struct js *js;

void setup() {
  js = js_create(mem, sizeof(mem));
#if 0  
  js_import(js, "delay", (uintptr_t) myDelay, "vi");
  js_import(js, "digitalWrite", (uintptr_t) myWrite, "vii");
  js_import(js, "pinMode", (uintptr_t) myMode, "vii");

  js_eval(js, "let ledPin = 13, ms = 100;", 0); // LedPin 13, blink interval 100ms
  js_eval(js, "pinMode(ledPin, 1);", 0);        // Set LED pin to OUTPUT mode
#endif
}

void loop() {
  js_eval(js, "delay(ms); digitalWrite(ledPin, 1); delay(ms); digitalWrite(ledPin, 0);", ~0);
}

