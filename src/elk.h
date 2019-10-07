// Copyright (c) 2013-2019 Cesanta Software Limited
// All rights reserved
//
// This software is dual-licensed: you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation. For the terms of this
// license, see <http://www.gnu.org/licenses/>.
//
// You are free to use this software under the terms of the GNU General
// Public License, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// Alternatively, you can license this software under a commercial
// license, please contact us at https://mdash.net/home/company.html

#ifndef ELK_H
#define ELK_H
#if defined(__cplusplus)
extern "C" {
#endif

#if !defined(_MSC_VER) || _MSC_VER >= 1700
#include <stdint.h>
#else
typedef unsigned uint32_t;
#endif

typedef uint32_t jsval_t;     // JS value placeholder

struct js *js_create(unsigned long allocated_ram_size);  // Create instance
void js_destroy(struct js *);              // Destroy instance
jsval_t js_get_global(struct js *);        // Get global namespace object
jsval_t js_eval(struct js *, const char *buf, int len);  // Evaluate expression
jsval_t js_set(struct js *, jsval_t obj, jsval_t k, jsval_t);  // Set attribute
const char *js_stringify(struct js *, jsval_t val);         // Stringify value
void js_info(struct js *, FILE *);                          // Print debug info
void js_gc(struct js *js, jsval_t v);  // Garbage collect js_eval()-ed value

jsval_t js_mk_num(float value);                         // Pack number
float js_to_num(jsval_t v);                             // Unpack number
jsval_t js_mk_str(struct js *, const char *, int len);  // Pack string
char *js_to_str(struct js *, jsval_t, unsigned *);      // Unpack string

// Use JS_UNDEFINED, JS_NULL, JS_TRUE, JS_FALSE for other scalar types

struct cfn {
  const char *name;    // function name
  const char *decl;    // Declaration of return values and arguments
  void (*fn)(void);    // Pointer to C function
  int id;              // Function ID
  struct cfn *next;    // Next in a chain
};

struct cfn *js_cfnhead(struct js *);  // Return head of the cfn linked list
void js_addcfn(struct js *, jsval_t obj, struct cfn *cf);  // Import C func

#define js_import(vm, fn, decl)                                      \
  do {                                                               \
    static struct cfn x = {#fn, decl, (void (*)(void)) fn, 0, NULL}; \
    js_addcfn((vm), js_get_global(vm), &x);                          \
  } while (0)

#define MK_VAL(t, p) ((jsval_t) 0xff800000 | ((jsval_t)(t) << 19) | (p))
#define JS_UNDEFINED MK_VAL(JS_TYPE_UNDEF, 0)
#define JS_ERROR MK_VAL(JS_TYPE_ERROR, 0)
#define JS_TRUE MK_VAL(JS_TYPE_BOOL, 1)
#define JS_FALSE MK_VAL(JS_TYPE_BOOL, 0)
#define JS_NULL MK_VAL(JS_TYPE_NULL, 0)
#define JS_NAN MK_VAL(JS_TYPE_NAN, 0)

#define MK_EMPTY_OBJ() MK_VAL(JS_TYPE_OBJECT, 0)

// clang-format off
enum jstype {
  JS_TYPE_UNDEF, JS_TYPE_NULL, JS_TYPE_BOOL, JS_TYPE_STRING, JS_TYPE_OBJECT,
	JS_TYPE_ARRAY, JS_TYPE_FUNCTION, JS_TYPE_NUMBER, JS_TYPE_NAN, JS_TYPE_ERROR,
	JS_TYPE_C_FUNCTION, JS_TYPE_IREF, JS_TYPE_PREF
};
// clang-format on

#if defined(__cplusplus)
}
#endif
#endif  // ELK_H
