# Elk: a restricted single-file JS engine for embedded systems

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://www.gnu.org/licenses/old-licenses/gpl-2.0.en.html)
[![Build Status](https://travis-ci.org/cpq/elk.svg?branch=master)](https://travis-ci.org/cpq/elk)
[![Code Coverage](https://codecov.io/gh/cpq/elk/branch/master/graph/badge.svg)](https://codecov.io/gh/cpq/elk)

## Features

- Clean ISO C, ISO C++. Builds on old (VC98) and modern compilers, from 8-bit (e.g. Arduino Uno) to 64-bit systems
- No dependencies
- Implements a subset of ES6 with limitations
- Preallocates all necessary memory and never calls `malloc`
  at run time. Upon OOM, the VM is halted
- Low footprint: RAM ~500 bytes + instance size, flash ~15 KB
  - JS object takes 6 bytes, each property: 16 bytes,
  JS string: length + 4 bytes, any other type: 4 bytes
- Strings are byte strings, not Unicode:
  `'ы'.length === 2`, `'ы'[0] === '\xd1'`, `'ы'[1] === '\x8b'`
- Simple API to import existing C functions into JS
- JS VM executes JS source directly, no AST/bytecode is generated
- Limitations: max string length is 64KB, numbers hold
  32-bit `float` value or 23-bit integer value, no standard JS library

## Call C from Javascript

```c
#include <stdio.h>
#include "elk.h"

// C function that adds two numbers. Will be called from JS
int sum(int a, int b) {
  return a + b;
}

int main(void) {
  struct js *vm = js_create(500); // Create JS instance that takes 500 bytes
  js_import(vm, sum, "iii");      // Import C function "sum" into JS
  js_eval(vm, "sum(1, 2);", -1);  // Call "sum"
  js_destroy(vm);                 // Destroy VM instance
  return 0;
}
```

## Call Javascript from C
```c
js_val_t result = js_eval(vm, "YOUR JS CODE", -1);
printf("result: %s\n", js_stringify(vm, result));
```

## Blinky in JavaScript on Arduino Uno

```c
#include <elk.h>

extern "C" void myDelay(int milli) { delay(milli); }
extern "C" void myWrite(int pin, int val) { digitalWrite(pin, val); }

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

void loop() { delay(1000); }
```

## Supported standard operations and constructs

| Name                       | Operation                                    |
| -------------------------- | -------------------------------------------- |
| Operations                 | All but `!=`, `==`. Use `!==`, `===` instead	|
| typeof                     | `typeof(...)`                               	|
| while                      | `while (...) {...}`                         	|
| Conditional                | `if (...) ...`, but no `else`               	|
| Declarations, simple types | `let a, b, c = 12.3, d = 'a', e = null, f = true, g = false; ` |
| Functions                  | `let f = function(x, y) { return x + y; }; `	|
| Objects                    | `let obj = {a: 1, f: function(x) { return x * 2}}; obj.f();`   |


## Unsupported standard operations and constructs

| Name           | Operation                                              |
| -------------- | ------------------------------------------------------ |
| Arrays         | `let arr = [1, 2, 'hi there']`                       	|
| Loops/switch   | `for (...) { ... }`,`for (let k in obj) { ... }`, `do { ... } while (...)`, `switch (...) {...}` |
| Equality       | `==`, `!=`  (note: use strict equality `===`, `!==`)  	|
| var            | `var ...`  (note: use `let ...`)                      	|
| delete         | `delete obj.k`                                        	|
| Closures       | `let f = function() { let x = 1; return function() { return x; } };`	|
| Const, etc     | `const ...`, `await ...` , `void ...` , `new ...`, `instanceof ...` 	|
| Standard types | No `Date`, `ReGexp`, `Function`, `String`, `Number`   	|
| Prototypes     | No prototype based inheritance                        	|


## C/C++ API

<dl>
  <dt><tt>struct js *js_create(unsigned long size);</tt></dt>
  <dd>Create Javascript instance, preallocate <tt>size</tt> bytes</dd>
  <dt><tt>void js_destroy(struct js *);</tt></dt>
  <dd>Destroy Javascript instance</dd>
  <dt><tt>jsval_t js_eval(struct js *, const char *buf, int len);</tt></dt>
  <dd>Evaluate JS code. If `len == -1`, then `strlen(buf)` is called to get the tt length</dd>
  <dt><tt>const char *js_stringify(struct js *, jsval_t v);</tt></dt>
  <dd>Stringify JS value, works like JSON.stringify()</dd>
  <dt><tt>jsval_t js_get_global(struct js *);</tt></dt>
  <dd>Get global namespace object</dd>
  <dt><tt>jsval_t js_set(struct js *, jsval_t obj, jsval_t k, jsval_t v);</tt></dt>
  <dd>Set attribute k to value v in object obj. k must be a string</dd>
  <dt><tt>jsval_t js_mk_obj(struct js *);</tt></dt>
  <dd>Create object</dd>
  <dt><tt>jsval_t js_mk_str(struct js *, const char *, int len);</tt></dt>
  <dd>Create string</dd>
  <dt><tt>jsval_t js_mk_num(float value);</tt></dt>
  <dd>Create number</dd>
  <dt><tt>float js_to_float(jsval_t v);</tt></dt>
  <dd>Extract number from a JS value</dd>
  <dt><tt>char *js_to_str(struct js *, jsval_t, jslen_t *);</tt></dt>
  <dd>Extract string from a JS value</dd>
</dl>


## Importing C functions

Elk can import C/C++ functions that satisfy the following conditions:
- A function must have 6 or less parameters, but no more than 6
- Parameters types must be:
   - C integer types that are machine word wide or smaller - like `char`, `uint16_t`, `int`, `long`, etc
   - Pointer types
   - C `double` types
- C `double` parameters could be only 1st ot 2nd. For example, function
  `void foo(double x, double y, struct bar *)` could be imported, but
  `void foo(struct bar *, double x, double y)` could not
- C++ functions must be declared as `extern C`

Thus, functions with C types `float` or `bool` cannot be imported.

The following macro is used to import C function into the JS instance:

```c
js_import(struct js *js, void (*func)(void), const char *signature);
```

- `js`: JS instance
- `func`: C function
- `signature`: specifies C function signature that tells how JS engine
   should marshal JS arguments to the C function.
	 First letter specifies return value type, following letters - parameters:
   - `d`: C `double` type
   - `i`: C integer type: `char`, `short`, `int`, `long`
   - `s`: C nul-terminated string, `char *` type
   - `j`: marshals `jsval_t`
   - `m`: marshals current `struct js *`
   - `p`: marshals C pointer
   - `v`: valid only for return type, means `void`


| Example C function  					| Example JS import statement 							|
| ----------- 					| ------------------- 							|
| `int sum(int)`			  | `js_import(js, sum, "ii")`				|
| `double sub(double a, double b)`			  | `js_import(js, sub, "ddd")`				|
| `int rand(void)`			  | `js_import(js, rand, "i")`				|
| `unsigned long strlen(char *s)`			  | `js_import(js, strlen, "is")`				|
| `char *js_stringify(struct js *, js_val_t)`			  | `js_import(js, js_stringify, "smj")`				|

## Importing C functions that call C callbacks

It is possible to marshal JS function as C callback. A C callback function
should take between 1 and 6 arguments. One of these arguments must be a `void *`
pointer, that is passed to the C callback by the imported function. We call
this `void *` parameter a "userdata" parameter.

The C callback specification is enclosed into the square brackets `[...]`.
In addition to the signature letters above, a new letter `u` is available
that specifies userdata parameter. Here is a complete example:

```c
#include <stdio.h>
#include "elk.h"

// C function that invokes a callback and returns the result of invocation
int f(int (*callback)(int a, int b, void *userdata), void *userdata) {
  return callback(1, 2, userdata);
}

int main(void) {
  struct js *vm = js_create(500);
  js_import(vm, f, "i[iiiu]u");
  jsval_t v = js_eval(vm, "f(function(a,b,c){return a + b;}, 0);", -1);
  printf("RESULT: %g\n", js_to_float(v));  // Should print "3"
  js_destroy(vm);
  return 0;
}
```

## Build stand-alone binary

```
$ make elk
$ ./elk -e 'let o = {a: 1}; o.a += 1; o;'
{"a":2}
```

## LICENSE

Dual license: GPLv2 or commercial. For commercial licensing, technical support
and integration help, please contact support@cesanta.com

