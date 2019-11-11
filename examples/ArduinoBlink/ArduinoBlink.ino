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
  js_import(js, "delay", (unsigned long) (void *) myDelay, "vi");
  js_import(js, "digitalWrite", (unsigned long) (void *) myWrite, "vii");
  js_eval(js,
          "while (1) { "
          "  digitalWrite(13, 0); "
          "  delay(100); "
          "  digitalWrite(13, 1); "
          "  delay(100); "
          "}",
          -1);
}

void loop() {
  delay(1000);
}
