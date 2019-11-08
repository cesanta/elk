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
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned uint32_t;
typedef long intptr_t;
#define snprintf _snprintf
#define strtod(p, e) \
  (((p)[0] == '0' && (p)[1] == 'x') ? strtoul(p + 2, e, 16) : strtod(p, e))
#else
#include <stdint.h>
#endif

struct js;
typedef uint32_t jsval_t;

struct js *js_create(void *mem, unsigned long mem_size);
jsval_t js_eval(struct js *js, const char *s, int len);
void js_gc(struct js *js, jsval_t v);
const char *js_fmt(struct js *js, jsval_t v, char *buf, int len);
extern void js_debug(struct js *, const char *);

#define js_ffi(js, fn, decl)                                                \
  do {                                                                      \
    char buf[64];                                                           \
    int n = snprintf(buf, sizeof(buf), "let %s=ffi(0,'%s@%lx')", #fn, decl, \
                     (unsigned long) (void *) fn);                          \
    js_eval(js, buf, n);                                                    \
  } while (0)

#if defined(__cplusplus)
}
#endif
#endif /* ELK_H */
