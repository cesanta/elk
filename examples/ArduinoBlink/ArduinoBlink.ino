#include <elk.h>  // Add Elk library

extern "C" void myDelay(int milli) {
  delay(milli);
}
extern "C" void myWrite(int pin, int val) {
  digitalWrite(pin, val);
}

void setup() {
  pinMode(13, OUTPUT);
  void *mem = malloc(500);
  struct js *js = js_create(mem, 500);
  js_ffi(js, myDelay, "vi");
  js_ffi(js, myWrite, "vii");
  js_eval(js,
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
