# Elk: a tiny JS engine for embedded systems

[![Build Status](https://github.com/cesanta/elk/workflows/build/badge.svg)](https://github.com/cesanta/elk/actions)
[![License: GPLv2/Commercial](https://img.shields.io/badge/License-GPLv2%20or%20Commercial-green.svg)](https://opensource.org/licenses/gpl-2.0.php)
[![Code Coverage](https://codecov.io/gh/cesanta/elk/branch/master/graph/badge.svg)](https://codecov.io/gh/cesanta/elk)


Elk is a tiny embeddable JavaScript engine that implements a small but usable
subset of ES6. It is degined for microcontroller development. Instead of
writing firmware code in C/C++, Elk allows to develop in JS. 
Another usecase is to provide customers with a secure, protected scripting
environment for product customisation.

Elk features include:

- Cross platform. Works anywhere from 8-bit microcontollers to 64-bit servers
- Zero dependencies. Builds cleanly by ISO C or ISO C++ compilers
- Easy to embed: just copy `elk.c` and `elk.h` to your source tree
- Very small and simple embedding API
- Allows to call C/C++ functions from Javascript and vice versa
- Does not use malloc. Operates with a given memory buffer only
- Small footprint: about 20KB on flash/disk, about 100 bytes RAM for core VM
- No bytecode. Interprets JS code directly

Below is a demontration on a classic 16Mhz Arduino Nano board which has
2k RAM and 30k flash (see [full sketch](examples/BlinkyJS/BlinkyJS.ino)):

![Elk on Arduino Nano](test/nano.gif)

## Call Javascript from C
```c
#include <stdio.h>
#include "elk.h"

int main(void) {
  char mem[200];
  struct js *js = js_create(mem, sizeof(mem));  // Create JS instance
  jsval_t v = js_eval(js, "1 + 2 * 3", ~0);     // Execute JS code
  printf("result: %s\n", js_str(js, v));        // result: 7
  return 0;
}
```

## Call C from Javascript

This demonstrates how JS code can import and call existing C functions:

```c
#include <stdio.h>
#include "elk.h"

// C function that adds two numbers. Will be called from JS
int sum(int a, int b) {
  return a + b;
}

int main(void) {
  char mem[200];
  struct js *js = js_create(mem, sizeof(mem));  // Create JS instance
  jsval_t v = js_import(js, sum, "iii");        // Import C function "sum"
  js_set(js, js_glob(js), "f", v);              // Under the name "f"
  jsval_t result = js_eval(js, "f(3, 4);", ~0); // Call "f"
  printf("result: %s\n", js_str(js, result));   // result: 7
  return 0;
}
```

## Restrictions

- Every statement must end with a semicolon `;`
- No `!=`, `==`. Use strict comparison `!==`, `===`
- No `var`, no `const`. Use `let`, strict mode only
- No `do`, `switch`, `for`. Use `while`
- No ternary operator `a ? b : c`
- No arrays, closures, prototypes, `this`, `new`, `delete`
- No standard library: no `Date`, `Regexp`, `Function`, `String`, `Number`
- Strings are binary data chunks, not Unicode strings

## Performance

Since Elk parses and interprets JS code on the fly, it is not meant to be
used in a performance-critical scenarios. For example, below are the numbers
for a simple loop code on a different architectures.

```javascript
let a = 0;        // 97 milliseconds on a 16Mhz 8-bit Atmega328P (Arduino Uno and alike)
while (a < 100)   // 16 milliseconds on a 48Mhz SAMD21
  a++;            //  5 milliseconds on a 133Mhz Raspberry RP2040
                  //  2 milliseconds on a 240Mhz ESP32
```

## Build options

Available preprocessor definitions:

| Name         | Default   | Description |
| ------------ | --------- | ----------- |
|`JS_EXPR_MAX` | 20        | Maximum tokens in expression. Expression evaluation function declares an on-stack array `jsval_t stk[JS_EXPR_MAX];`. Increase to allow very long expressions. Reduce to save C stack space. |
|`JS_DUMP`     | undefined | Define to enable `js_dump(struct js *)` function which prints JS memory internals to stdout |
|`JS_GC_THRESHOLD` | 80 | A percentage (from 0 to 100) of runtime memory when GC is triggered. A trigger point is a beginning of statement block (function body, loop body, etc) |

Note: on ESP32 or ESP8266, compiled functions go into the `.text` ELF
section and subsequently into the IRAM MCU memory. It is possible to save
IRAM space by copying Elk code into the irom section before linking.
First, compile the object file, then rename `.text` section, e.g. for ESP8266:

```sh
$ xtensa-lx106-elf-gcc ... elk.c -c tmp
$ xtensa-lx106-elf-objcopy --rename-section .text=.irom0.text tmp elk.a
```

## API reference

### js\_create()

```c
struct js *js_create(void *buf, size_t len);
```

Initialize JS engine in a given memory block. Elk will only use that memory
block to hold its runtime, and never use any extra memory.
Return: a non-`NULL` opaque pointer on success, or `NULL` when
`len` is too small. The minimum `len` is about 100 bytes.

The given memory buffer is laid out in the following way:
```
  | <-------------------------------- len ------------------------------> |
  | struct js, ~100 bytes  |   runtime vars    |    free memory           | 
```

### js\_eval()

```c
jsval_t js_eval(struct js *, const char *buf, size_t len);
```

Evaluate JS code in `buf`, `len` and return result of the evaluation.  During
the evaluation, Elk stores variables in the "runtime" memory section. When
`js_eval()` returns, Elk does not keep any reference to the evaluated code: all
strings, functions, etc, are copied to the runtime.

Important note: the returned result is valid only before the next call to
`js_eval()`. The reason is that `js_eval()` triggers a garbage collection.
A garbage collection is mark-and-sweep.

The runtime footprint is as follows:
- An empty object is 8 bytes
- Each object property is 16 bytes
- A string is 4 bytes + string length, aligned to 4 byte boundary
- A C stack usage is ~200 bytes per nested expression evaluation


### js\_str()

```c
const char *js_str(struct js *, jsval_t val);
```

Stringify JS value `val` and return a pointer to a 0-terminated result.
The string is allocated in the "free" memory section. If there is no
enough space there, an empty string is returned. The returned pointer
is valid until the next `js_eval()` call.


### js\_import()

```c
jsval_t js_import(struct js *js, uintptr_t funcaddr, const char *signature);
```

Import an existing C function with address `funcaddr` and signature `signature`.
Return imported function, suitable for subsequent `js_set()`.

- `js`: JS instance
- `funcaddr`: C function address: `(uintptr_t) &my_function`
- `signature`: specifies C function signature that tells how JS engine
   should marshal JS arguments to the C function.
	 First letter specifies return value type, following letters - parameters:
   - `b`: C `bool` type
   - `d`: C `double` type
   - `i`: C integer type: `char`, `short`, `int`, `long`
   - `s`: C string, a nul-terminated `char *`
   - `j`: marshals `jsval_t`
   - `m`: marshals current `struct js *`. In JS, pass `null`
   - `p`: marshals C pointer
   - `v`: valid only for the return value, means `void`

The imported C function must satisfy the following requirements:

- A function must have maximum 6 parameters
- C `double` parameters could be only 1st ot 2nd. For example, function
  `void foo(double x, double y, struct bar *)` could be imported, but
  `void foo(struct bar *, double x, double y)` could not
- C++ functions must be declared as `extern "C"`
- Functions with `float` params cannot be imported. Write wrappers with `double`

Here are some example of the import specifications:
- `int sum(int)` -> `js_import(js, (uintptr_t) sum, "ii")`
- `double sub(double a, double b)` -> `js_import(js, (uintptr_t) sub, "ddd")`
- `int rand(void)` -> `js_import(js, (uintptr_t) rand, "i")`
- `unsigned long strlen(char *s)` -> `js_import(js, (uintptr_t) strlen, "is")`
- `char *js_str(struct js *, js_val_t)` -> `js_import(js, (uintptr_t) js_str, "smj")`

In some cases, C APIs use callback functions. For example, a timer C API could
specify a time interval, a C function to call, and a function parameter. It is
possible to marshal JS function as a C callback - in other words, it is
possible to pass JS functions as C callbacks.

A C callback function should take between 1 and 6 arguments. One of these
arguments must be a `void *` pointer, that is passed to the C callback by the
imported function. We call this `void *` parameter a "userdata" parameter.

The C callback specification is enclosed into the square brackets `[...]`.
In addition to the signature letters above, a new letter `u` is available
that specifies userdata parameter. In JS, pass `null` for `u` param.
Here is a complete example:

```c
#include <stdio.h>
#include "elk.h"

// C function that invokes a callback and returns the result of invocation
int f(int (*fn)(int a, int b, void *userdata), void *userdata) {
  return fn(1, 2, userdata);
}

int main(void) {
  char mem[500];
  struct js *js = js_create(mem, sizeof(mem));
  js_set(js, js_glob(js), "f", js_import(js, f, "i[iiiu]u"));
  jsval_t v = js_eval(js, "f(function(a,b,c){return a + b;}, 0);", ~0);
  printf("result: %s\n", js_str(js, v));  // result: 3
  return 0;
}
```

### js\_set(), js\_glob(), js\_mkobj()

```c
jsval_t js_glob(struct js *);   // Return global object
jsval_t js_mkobj(struct js *);  // Create a new object
void js_set(struct js *, jsval_t obj, const char *key, jsval_t val);  // Assign property to an object
```

These are helper functions for assigning properties to objects. The
anticipated use case is to give names to imported C functions.

Importing a C function `sum` into the global namespace:

```c
  jsval_t global_namespace = js_glob(js);
  jsval_t imported_function = js_import(js, (uintptr_t) sum, "iii");
  js_set(js, global_namespace, "f", imported_function);
```

Use `js_mkobj()` to create a dedicated object to hold groups of functions
and keep a global namespace tidy. For example, all GPIO related functions
can go into the `gpio` object:

```c
  jsval_t gpio = js_mkobj(js);              // Equivalent to:
  js_set(js, js_glob(js), "gpio", gpio);    // let gpio = {};

  js_set(js, gpio, "mode",  js_import(js, (uintptr_t) func1, "iii");  // Create gpio.mode(pin, mode)
  js_set(js, gpio, "read",  js_import(js, (uintptr_t) func2, "ii");   // Create gpio.read(pin)
  js_set(js, gpio, "write", js_import(js, (uintptr_t) func3, "iii");  // Create gpio.write(pin, value)
```

### js\_usage()

```c
int js_usage(struct js *);
```

Return memory usage percenage - a number between 0 and 100.
If required, trigger a garbage collection by evaluating an empty expression:
`js_eval(js, "", 0)`.


## LICENSE

Dual license: GPLv2 or commercial. For commercial licensing, technical support
and integration help, please contact us at https://cesanta.com/contact.html

