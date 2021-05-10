// Copyright (c) 2013-2021 Cesanta Software Limited
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
// license, please contact us at https://cesanta.com/contact.html

#pragma once
#include <stdint.h>

#define JS_VERSION "2.0.8"

struct js;                 // JS engine (opaque)
typedef uint64_t jsval_t;  // JS value placeholder

struct js *js_create(void *buf, size_t len);         // Create JS instance
const char *js_str(struct js *, jsval_t val);        // Stringify JS value
jsval_t js_eval(struct js *, const char *, size_t);  // Execute JS code
jsval_t js_glob(struct js *);                        // Return global object
jsval_t js_mkobj(struct js *);                       // Create a new object
jsval_t js_import(struct js *, uintptr_t, const char *);   // Import native func
void js_set(struct js *, jsval_t, const char *, jsval_t);  // Set obj attribute
int js_usage(struct js *);                                 // Return mem usage
