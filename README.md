# Elk: a tiny JS engine for embedded systems

Elk is a tiny embeddable JavaScript engine that implements a small, but usable
subset of ES6. Features include:

- Zero dependencies
- Does not use dynamic memory allocation
- Tiny: less than 15KB on flash, less than 200 bytes RAM for core VM
- Very low native stack memory usage
- Very small public API
- Allows to call C/C++ functions from Javascript and vice versa
- Limitations:
	 - Max string length is 8KB
	 - Numbers hold 32-bit `float` value or 23-bit integer value
	 - No standard JS library

## Call C from Javascript

```c
#include <stdio.h>
#include "elk.h"

// C function that adds two numbers. Will be called from JS
int sum(int a, int b) {
  return a + b;
}

int main(void) {
  char mem[500];	// Preallocated JS memory buffer
  struct js *js = js_create(mem, sizeof(mem));  // Create JS instance
  js_import(js, "sum", sum, "iii");    	// Import C function "sum" into JS
  js_eval(js, "sum(1, 2);", 0);         // Call "sum"
  return 0;
}
```

## Call Javascript from C
```c
jsval_t v = js_eval(js, "1 + 2 * 3", 0); 	// Execute JS code
printf("result: %s\n", js_str(js, v));  	// result: 7
js_gc(js, v);                             // Garbage collect
```

## Blinky in JavaScript on Arduino Uno

This example works on 8-bit Arduino with 2k of RAM, blinking an LED
and printing JS memory usage statistics to the serial console:

See [ArduinoBlink.ino](examples/ArduinoBlink/ArduinoBlink.ino)

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

See [elk.h](elk.h):

| Signature | Description |
| --------- | ----------- |
| `struct js *js_create(void *mem, int size)` | Initialize JS engine in a given memory chunk. |
| `jsval_t js_eval(struct js *, const char *buf, int len)` | Evaluate JS code, return JS value. If `len` is 0, then `strlen(code)` is used. |
| `void js_gc(struct js *, jsval_t v)` | Deallocate JS value obtained by `js_eval()` call. |
| `const char *js_info(struct js *)` | Return JSON string with the Elk engine internal stats. |
| `jsval_t *js_import(struct js *, const char *name, uintptr_t fn, const char *sig)` | Import C function into the JS environment |
| `char *js_str(struct js *, jsval_t v)` | Stringifies JS value, like `JSON.stringify` |
| `jsval_t js_parse(struct js *, const char *str, int len)` | Parses a string into a JS value, like `JSON.parse()` |
 

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

The following function is used to import C function into the JS instance:

```c
js_import(struct js *js, const char *name, unsigned long addr, const char *signature);
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
  char mem[500];
  struct js *js = js_create(mem, sizeof(mem));
  js_import(js, f, "i[iiiu]u");
  jsval_t v = js_eval(js, "f(function(a,b,c){return a + b;}, 0);", 0);
	printf("result: %s\n", js_str(js, v));  // result: 3
  return 0;
}
```

## Build stand-alone binary

```
$ cc -o elk examples/posix/main.c -I src -L src/linux-x64 -ldl -lelk
$ ./elk -e 'let o = {a: 1}; o.a += 1; o;'
{"a":2}
$ ./elk -e 'let f = function(a,b) { return a * b }; f(2, strlen("abc"))'
6
```

## LICENSE

Dual license: GPLv2 or commercial. For commercial licensing, technical support
and integration help, please contact support@cesanta.com

