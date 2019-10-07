#include "elk.h"  // Sketch -> Add File -> elk.h and elk.c

extern "C" void myDelay(int milli) {
  delay(milli);
}
extern "C" void myWrite(int pin, int val) {
  digitalWrite(pin, val);
}

void setup() {
  pinMode(13, OUTPUT); // Initialize the pin as an output
  struct js *vm = js_create(500);
  js_import(vm, myDelay, "vi");
  js_import(vm, myWrite, "vii");
  js_eval(vm,
          "while (1) { "
          "  myWrite(13, 0); "
          "  myDelay(100); "
          "  myWrite(13, 1); "
          "  myDelay(100); "
          "}",
          -1);
}

void loop() {
  delay(1000);
}
