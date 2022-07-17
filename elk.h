// Copyright (c) 2013-2022 Cesanta Software Limited
// All rights reserved
//
// This software is dual-licensed: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License version 3 as
// published by the Free Software Foundation. For the terms of this
// license, see http://www.fsf.org/licensing/licenses/agpl-3.0.html
//
// You are free to use this software under the terms of the GNU General
// Public License, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See the GNU General Public License for more details.
//
// Alternatively, you can license this software under a commercial
// license, please contact us at https://cesanta.com/contact.html

#define JS_VERSION "3.0.0"
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct js;                 // JS engine (opaque)
typedef uint64_t jsval_t;  // JS value placeholder

struct js *js_create(void *buf, size_t len);         // Create JS instance
jsval_t js_eval(struct js *, const char *, size_t);  // Execute JS code
jsval_t js_glob(struct js *);                        // Return global object
void js_set(struct js *, jsval_t, const char *, jsval_t);  // Set obj attr
const char *js_str(struct js *, jsval_t val);              // Stringify JS value
jsval_t js_checkargs(struct js *, jsval_t *, int, const char *, ...);  // Check
int js_usage(struct js *js);  // Return mem usage %
void js_dump(struct js *);    // Print debug info

// Type check
enum { JS_UNDEF, JS_NULL, JS_TRUE, JS_FALSE, JS_STR, JS_NUM, JS_ERR, JS_PRIV };
int js_type(jsval_t val);  // Return JS value type

// Create JS values from C values
jsval_t js_mkval(int type);     // Create undef, null, true, false
jsval_t js_mkobj(struct js *);  // Create object
jsval_t js_mkstr(struct js *, const void *, size_t);           // Create string
jsval_t js_mknum(double);                                      // Create number
jsval_t js_mkerr(struct js *js, const char *fmt, ...);         // Create error
jsval_t js_mkfun(jsval_t (*fn)(struct js *, jsval_t *, int));  // Create func

// Extract C values from JS values
double js_getnum(jsval_t val);  // Get number
int js_getbool(jsval_t val);    // Get boolean value, 0 or 1
char *js_getstr(struct js *js, jsval_t val, size_t *len);  // Get string

#ifdef __cplusplus
}
#endif
