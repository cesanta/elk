/*
 * Copyright (c) 2013-2019 Cesanta Software Limited
 * All rights reserved
 *
 * This software is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. For the terms of this
 * license, see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively, you can license this software under a commercial
 * license, please contact us at https://mdash.net/home/company.html
 */

#ifndef ELK_H
#define ELK_H
#if defined(__cplusplus)
extern "C" {
#endif

#if defined(_MSC_VER) && _MSC_VER < 1700
typedef unsigned uint32_t;
typedef unsigned long uintptr_t;
#else
#include <stdint.h>
#endif

struct js;
typedef uint32_t jsval_t;

struct js *js_create(void *mem, int mem_size);
jsval_t js_eval(struct js *, const char *s, int len);
void js_gc(struct js *, jsval_t v);
// const char *js_fmt(struct js *, jsval_t v, char *buf, int len);
const char *js_str(struct js *, jsval_t value);
jsval_t *js_import(struct js *, const char *name, uintptr_t f, const char *sig);
const char *js_info(struct js *);
jsval_t js_parse(struct js *, const char *s, int len);
jsval_t js_ffi(struct js *, const char *decl);

#if defined(__cplusplus)
}
#endif
#endif /* ELK_H */
