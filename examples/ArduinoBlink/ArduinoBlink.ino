#include "elk.h"  // Add Elk library

extern "C" void myDelay(int milli) { delay(milli); }
extern "C" void myWrite(int pin, int val) { digitalWrite(pin, val); }
extern "C" void myMode(int pin, int mode) { pinMode(pin, mode); }
extern "C" void initSerial(int baud) { Serial.begin(baud); }
extern "C" void writeSerial(const char *str) { Serial.println(str); }

struct js *js;
#define JS_SIZE 400 // Memory for the JS engine in bytes

void setup() {
  js = js_create(malloc(JS_SIZE), JS_SIZE);
  js_import(js, "delay", (uintptr_t) myDelay, "vi");
  js_import(js, "digitalWrite", (uintptr_t) myWrite, "vii");
  js_import(js, "pinMode", (uintptr_t) myMode, "vii");
  js_import(js, "initSerial", (uintptr_t) initSerial, "vi");
  js_import(js, "writeSerial", (uintptr_t) writeSerial, "vs");
  js_import(js, "jsinfo", (uintptr_t) js_info, "sm");

  js_eval(js, "let ledPin = 13, ms = 100;", 0); // LedPin 13, blink interval 100ms
  js_eval(js, "pinMode(ledPin, 1);", 0);        // Set LED pin to OUTPUT mode
  js_eval(js, "initSerial(9600);", 0);          // Init serial console
}

void loop() {
  js_eval(js, "delay(ms); digitalWrite(ledPin, 1); delay(ms); digitalWrite(ledPin, 0);", 0);
  js_eval(js, "writeSerial(jsinfo(null));", 0);  // Print JS memory stats
}

