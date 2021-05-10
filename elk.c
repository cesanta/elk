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

#if defined(ARDUINO_AVR_NANO) || defined(ARDUINO_AVR_PRO) || \
    defined(ARDUINO_AVR_UNO)
#define JS_NOCB
#define NDEBUG
#endif

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elk.h"

#ifndef JS_EXPR_MAX
#define JS_EXPR_MAX 20
#endif

typedef uint32_t jsoff_t;

struct js {
  const char *code;  // Currently parsed code snippet
  char errmsg[36];   // Error message placeholder
  uint8_t tok;       // Last parsed token value
  uint8_t flags;     // Execution flags, see F_* enum below
  uint16_t lev;      // Recursion level
  jsoff_t clen;      // Code snippet length
  jsoff_t pos;       // Current parsing position
  jsoff_t toff;      // Offset of the last parsed token
  jsoff_t tlen;      // Length of the last parsed token
  jsval_t tval;      // Holds last parsed numeric or string literal value
  jsval_t scope;     // Current scope
#define F_NOEXEC 1   // Parse code, but not execute
#define F_LOOP 2     // We're inside the loop
#define F_CALL 4     // We're inside a function call
#define F_BREAK 8    // Exit the loop
#define F_RETURN 16  // Return has been executed
  uint8_t *mem;      // Available JS memory
  jsoff_t size;      // Memory size
  jsoff_t brk;       // Current mem usage boundary
  jsoff_t ncbs;      // Number of FFI-ed C "userdata" callback pointers
};

// A JS memory stores diffenent entities: objects, properties, strings
// All entities are packed to the beginning of a buffer.
// The `brk` marks the end of the used memory:
//
//    | entity1 | entity2| .... |entityN|         unused memory        | cbs |
//    |---------|--------|------|-------|------------------------------|-----|
//  js.mem                           js.brk                        js.size
//
//  Each entity is 4-byte aligned, therefore 2 LSB bits store entity type.
//  Object:   8 bytes: offset of the first property, offset of the upper obj
//  Property: 8 bytes + val: 4 byte next prop, 4 byte key offs, N byte value
//  String:   4xN bytes: 4 byte len << 2, 4byte-aligned 0-terminated data
//
// FFI userdata callback pointers "cbs" are placed past js.size. Since they
// are passed to the user's C code, they stay constant and are not GC-ed
#define MARK ~(((jsoff_t) ~0) >> 1)  // Entity deletion marker

// clang-format off
enum { 
  TOK_ERR, TOK_EOF, TOK_IDENTIFIER, TOK_NUMBER, TOK_STRING,
  TOK_SEMICOLON, TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
  // Keyword tokens
  TOK_BREAK = 50, TOK_CASE, TOK_CATCH, TOK_CLASS, TOK_CONST, TOK_CONTINUE,
  TOK_DEFAULT, TOK_DELETE, TOK_DO, TOK_ELSE, TOK_FINALLY, TOK_FOR,
  TOK_FUNC, TOK_IF, TOK_IN, TOK_INSTANCEOF, TOK_LET, TOK_NEW, TOK_RETURN,
  TOK_SWITCH, TOK_THIS, TOK_THROW, TOK_TRY, TOK_VAR, TOK_VOID, TOK_WHILE,
  TOK_WITH, TOK_YIELD, TOK_UNDEF, TOK_NULL, TOK_TRUE, TOK_FALSE,
  // JS Operator tokens
  TOK_DOT = 100, TOK_CALL, TOK_POSTINC, TOK_POSTDEC, TOK_NOT, TOK_NEG,
  TOK_TYPEOF, TOK_UPLUS, TOK_UMINUS, TOK_EXP, TOK_MUL, TOK_DIV, TOK_REM,
  TOK_PLUS, TOK_MINUS, TOK_SHL, TOK_SHR, TOK_ZSHR, TOK_LT, TOK_LE, TOK_GT,
  TOK_GE, TOK_EQ, TOK_NE, TOK_AND, TOK_XOR, TOK_OR, TOK_LAND, TOK_LOR,
  TOK_COLON, TOK_Q,  TOK_ASSIGN, TOK_PLUS_ASSIGN, TOK_MINUS_ASSIGN,
  TOK_MUL_ASSIGN, TOK_DIV_ASSIGN, TOK_REM_ASSIGN, TOK_SHL_ASSIGN,
  TOK_SHR_ASSIGN, TOK_ZSHR_ASSIGN, TOK_AND_ASSIGN, TOK_XOR_ASSIGN,
  TOK_OR_ASSIGN, TOK_COMMA,
};

enum {
  // IMPORTANT: T_OBJ, T_PROP, T_STR must go first.  That is required by the
  // memory layout functions: memory entity types are encoded in the 2 bits,
  // thus type values must be 0,1,2,3
  T_OBJ, T_PROP, T_STR, T_UNDEF, T_NULL, T_NUM, T_BOOL, T_FUNC, T_CODEREF,
  T_ERR
};

static const char *typestr(uint8_t t) {
  const char *names[] = { "object", "prop", "string", "undefined", "null",
                          "number", "boolean", "function", "nan" };
  return (t < sizeof(names) / sizeof(names[0])) ? names[t] : "??";
}

#ifdef JS32
// Pack JS values into uin32_t, float -infinity
// 32bit "float": 1 bit sign, 8 bits exponent, 23 bits mantissa
//
//  7f80 0000 = 01111111 10000000 00000000 00000000 = infinity
//  ff80 0000 = 11111111 10000000 00000000 00000000 = −infinity
//  ffc0 0001 = x1111111 11000000 00000000 00000001 = qNaN (on x86 and ARM)
//  ff80 0001 = x1111111 10000000 00000000 00000001 = sNaN (on x86 and ARM)
//
//  seeeeeee|emmmmmmm|mmmmmmmm|mmmmmmmm
//  11111111|1ttttvvv|vvvvvvvv|vvvvvvvv
//    INF     TYPE     PAYLOAD
static jsval_t tov(float d) { union { float d; jsval_t v; } u = {d}; return u.v; }
static float tod(jsval_t v) { union { jsval_t v; float d; } u = {v}; return u.d; }
static jsval_t mkval(uint8_t type, unsigned long data) { return ((jsval_t) 0xff800000) | ((jsval_t) (type) << 19) | data; }
static bool is_nan(jsval_t v) { return (v >> 23) == 0x1ff; }
static uint8_t vtype(jsval_t v) { return is_nan(v) ? (v >> 19) & 15 : T_NUM; }
static unsigned long vdata(jsval_t v) { return v & ~((jsval_t) 0xfff80000); }
static jsval_t mkcoderef(jsval_t off, jsoff_t len) { return mkval(T_CODEREF, (off & 0xfff) | ((len & 127) << 12)); }
static jsoff_t coderefoff(jsval_t v) { return v & 0xfff; }
static jsoff_t codereflen(jsval_t v) { return (v >> 12) & 127; }
#else
// Pack JS values into uin64_t, double nan, per IEEE 754
// 64bit "double": 1 bit sign, 11 bits exponent, 52 bits mantissa
//
// seeeeeee|eeeemmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm|mmmmmmmm
// 11111111|11110000|00000000|00000000|00000000|00000000|00000000|00000000 inf
// 11111111|11111000|00000000|00000000|00000000|00000000|00000000|00000000 qnan
//
// 11111111|1111tttt|vvvvvvvv|vvvvvvvv|vvvvvvvv|vvvvvvvv|vvvvvvvv|vvvvvvvv
//  NaN marker |type|  48-bit placeholder for values: pointers, strings
//
// On 64-bit platforms, pointers are really 48 bit only, so they can fit,
// provided they are sign extended
static jsval_t tov(double d) { union { double d; jsval_t v; } u = {d}; return u.v; }
static double tod(jsval_t v) { union { jsval_t v; double d; } u = {v}; return u.d; }
static jsval_t mkval(uint8_t type, unsigned long data) { return ((jsval_t) 0x7ff0 << 48) | ((jsval_t) (type) << 48) | data; }
static bool is_nan(jsval_t v) { return (v >> 52) == 0x7ff; }
static uint8_t vtype(jsval_t v) { return is_nan(v) ? ((v >> 48) & 15) : (uint8_t) T_NUM; }
static unsigned long vdata(jsval_t v) { return (unsigned long) (v & ~((jsval_t) 0x7fff << 48)); }
static jsval_t mkcoderef(jsval_t off, jsoff_t len) { return mkval(T_CODEREF, (off & 0xffffff) | ((len & 0xffffff) << 24)); }
static jsoff_t coderefoff(jsval_t v) { return v & 0xffffff; }
static jsoff_t codereflen(jsval_t v) { return (v >> 24) & 0xffffff; }
#endif
static uint8_t unhex(uint8_t c) { return (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'W' : (c >= 'A' && c <= 'F') ? c - '7' : 0; }
static uint64_t unhexn(const uint8_t *s, int len) { uint64_t v = 0; for (int i = 0; i < len; i++) { if (i > 0) v <<= 4; v |= unhex(s[i]); } return v; }
static bool is_space(int c) { return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f' || c == '\v'; }
static bool is_digit(int c) { return c >= '0' && c <= '9'; }
static bool is_xdigit(int c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static bool is_alpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static bool is_ident_begin(int c) { return c == '_' || c == '$' || is_alpha(c); }
static bool is_ident_continue(int c) { return c == '_' || c == '$' || is_alpha(c) || is_digit(c); }
static bool is_err(jsval_t v) { return vtype(v) == T_ERR; }
static bool is_op(uint8_t tok) { return tok >= TOK_DOT; }
static bool is_unary(uint8_t tok) { return tok >= TOK_POSTINC && tok <= TOK_UMINUS; }
static bool is_right_assoc(uint8_t tok) { return (tok >= TOK_NOT && tok <= TOK_UMINUS) || (tok >= TOK_Q && tok <= TOK_OR_ASSIGN); }
static bool is_assign(uint8_t tok) { return (tok >= TOK_ASSIGN && tok <= TOK_OR_ASSIGN); }
static void saveoff(struct js *js, jsoff_t off, jsoff_t val) { memcpy(&js->mem[off], &val, sizeof(val)); }
static void saveval(struct js *js, jsoff_t off, jsval_t val) { memcpy(&js->mem[off], &val, sizeof(val)); }
static jsoff_t loadoff(struct js *js, jsoff_t off) { jsoff_t v = 0; assert(js->brk <= js->size); memcpy(&v, &js->mem[off], sizeof(v)); return v; }
static jsoff_t offtolen(jsoff_t off) { return (off >> 2) - 1; }
static jsoff_t vstrlen(struct js *js, jsval_t v) { return offtolen(loadoff(js, vdata(v))); }
static jsval_t loadval(struct js *js, jsoff_t off) { jsval_t v = 0; memcpy(&v, &js->mem[off], sizeof(v)); return v; }
static jsval_t upper(struct js *js, jsval_t scope) { return mkval(T_OBJ, loadoff(js, vdata(scope) + sizeof(jsoff_t))); }
static jsoff_t align32(jsoff_t v) { return ((v + 3) >> 2) << 2; }
// clang-format on

// Forward declarations of the private functions
static size_t tostr(struct js *js, jsval_t value, char *buf, size_t len);
static jsval_t js_expr(struct js *js, uint8_t etok, uint8_t etok2);
static jsval_t js_stmt(struct js *js, uint8_t etok);
static jsval_t do_op(struct js *, uint8_t op, jsval_t l, jsval_t r);

// Stringify JS object
static size_t strobj(struct js *js, jsval_t obj, char *buf, size_t len) {
  size_t n = snprintf(buf, len, "%s", "{");
  jsoff_t next = loadoff(js, vdata(obj)) & ~3;  // Load first prop offset
  while (next < js->brk && next != 0) {         // Iterate over props
    jsoff_t koff = loadoff(js, next + sizeof(next));
    jsval_t val = loadval(js, next + sizeof(next) + sizeof(koff));
    // printf("PROP %u, koff %u\n", next & ~3, koff);
    n += snprintf(buf + n, len - n, "%s", n == 1 ? "" : ",");
    n += tostr(js, mkval(T_STR, koff), buf + n, len - n);
    n += snprintf(buf + n, len - n, "%s", ":");
    n += tostr(js, val, buf + n, len - n);
    next = loadoff(js, next) & ~3;  // Load next prop offset
  }
  return n + snprintf(buf + n, len - n, "%s", "}");
}

// Stringify numeric JS value
static size_t strnum(jsval_t value, char *buf, size_t len) {
  double dv = tod(value), iv;
  const char *fmt = modf(dv, &iv) == 0.0 ? "%.17g" : "%g";
  return snprintf(buf, len, fmt, dv);
}

// Return mem offset and length of the JS string
static jsoff_t vstr(struct js *js, jsval_t value, jsoff_t *len) {
  jsoff_t off = vdata(value);
  if (len) *len = offtolen(loadoff(js, off));
  return off + sizeof(off);
}

// Stringify string JS value
static size_t strstring(struct js *js, jsval_t value, char *buf, size_t len) {
  jsoff_t n, off = vstr(js, value, &n);
  // printf("STRING: len %u, off %lu %zu\n", n, off - sizeof(off), len);
  return snprintf(buf, len, "\"%.*s\"", (int) n, (char *) js->mem + off);
}

// Stringify JS function
static size_t strfunc(struct js *js, jsval_t value, char *buf, size_t len) {
  jsoff_t n, off = vstr(js, value, &n), isjs = (js->mem[off] == '(');
  char *p = (char *) &js->mem[off];
  return isjs ? snprintf(buf, len, "function%.*s", (int) n, p)  // JS function
              : snprintf(buf, len, "\"%.*s\"", (int) n, p);     // C function
}

static jsval_t js_err(struct js *js, const char *fmt, ...) {
  va_list ap;
  size_t n = snprintf(js->errmsg, sizeof(js->errmsg), "%s", "ERROR: ");
  va_start(ap, fmt);
  vsnprintf(js->errmsg + n, sizeof(js->errmsg) - n, fmt, ap);
  va_end(ap);
  js->errmsg[sizeof(js->errmsg) - 1] = '\0';
  // printf("ERR: [%s]\n", js->errmsg);
  js->pos = js->clen;  // We're done.. Jump to the end of code
  js->tok = TOK_EOF;
  return mkval(T_ERR, 0);
}

// Stringify JS value into the given buffer
static size_t tostr(struct js *js, jsval_t value, char *buf, size_t len) {
  // clang-format off
  switch (vtype(value)) {
    case T_UNDEF: return snprintf(buf, len, "%s", "undefined");
    case T_NULL:  return snprintf(buf, len, "%s", "null");
    case T_BOOL:  return snprintf(buf, len, "%s", vdata(value) & 1 ? "true" : "false");
    case T_OBJ:   return strobj(js, value, buf, len);
    case T_STR:   return strstring(js, value, buf, len);
    case T_NUM:   return strnum(value, buf, len);
    case T_FUNC:  return strfunc(js, value, buf, len);
    default:      return snprintf(buf, len, "VTYPE%d", vtype(value));
  }
  // clang-format on
}

// Stringify JS value into a free JS memory block
const char *js_str(struct js *js, jsval_t value) {
  // Leave jsoff_t placeholder between js->brk and a stringify buffer,
  // in case if next step is convert it into a JS variable
  char *buf = (char *) &js->mem[js->brk + sizeof(jsoff_t)];
  if (is_err(value)) return js->errmsg;
  if (js->brk + sizeof(jsoff_t) >= js->size) return "";
  tostr(js, value, buf, js->size - js->brk - sizeof(jsoff_t));
  // printf("JSSTR: %d [%s]\n", vtype(value), buf);
  return buf;
}

static bool js_truthy(struct js *js, jsval_t v) {
  uint8_t t = vtype(v);
  return (t == T_BOOL && vdata(v) != 0) || (t == T_NUM && tod(v) != 0.0) ||
         (t == T_OBJ || t == T_FUNC) || (t == T_STR && vstrlen(js, v) > 0);
}

static jsoff_t js_alloc(struct js *js, size_t size) {
  jsoff_t ofs = js->brk;
  size = align32((jsoff_t) size);  // 4-byte align, (n + k - 1) / k * k
  if (js->brk + size > js->size) return ~(jsoff_t) 0;
  js->brk += (jsoff_t) size;
  return ofs;
}

static jsval_t mkentity(struct js *js, jsoff_t b, const void *buf, size_t len) {
  jsoff_t ofs = js_alloc(js, len + sizeof(b));
  if (ofs == (jsoff_t) ~0) return js_err(js, "oom");
  memcpy(&js->mem[ofs], &b, sizeof(b));
  // Using memmove - in case we're stringifying data from the free JS mem
  if (buf != NULL) memmove(&js->mem[ofs + sizeof(b)], buf, len);
  if ((b & 3) == T_STR) js->mem[ofs + sizeof(b) + len] = 0;  // 0-terminate
  // printf("MKE: %u @ %u type %d\n", js->brk - ofs, ofs, b & 3);
  return mkval(b & 3, ofs);
}

static jsval_t mkstr(struct js *js, const void *ptr, size_t len) {
  // printf("MKSTR: [%.*s] -> off %u\n", (int) len, (char *) ptr, js->brk);
  return mkentity(js, (jsoff_t)(((len + 1) << 2) | T_STR), ptr, len + 1);
}

static jsval_t mkobj(struct js *js, jsoff_t parent) {
  return mkentity(js, 0 | T_OBJ, &parent, sizeof(parent));
}

static jsval_t setprop(struct js *js, jsval_t obj, jsval_t k, jsval_t v) {
  jsoff_t koff = vdata(k);                    // Key offset
  jsoff_t b, head = vdata(obj);               // Property list head
  char buf[sizeof(koff) + sizeof(v)];         // Property memory layout
  memcpy(&b, &js->mem[head], sizeof(b));      // Load current 1st prop offset
  memcpy(buf, &koff, sizeof(koff));           // Initialize prop data: copy key
  memcpy(buf + sizeof(koff), &v, sizeof(v));  // Copy value
  jsoff_t brk = js->brk | T_OBJ;              // New prop offset
  memcpy(&js->mem[head], &brk, sizeof(brk));  // Repoint head to the new prop
  // printf("PROP: %u -> %u\n", b, brk);
  return mkentity(js, (b & ~3) | T_PROP, buf, sizeof(buf));  // Create new prop
}

// Return T_OBJ/T_PROP/T_STR entity size based on the first word in memory
static inline jsoff_t esize(jsoff_t w) {
  // clang-format off
  switch (w & 3) {
    case T_OBJ:   return sizeof(jsoff_t) + sizeof(jsoff_t);
    case T_PROP:  return sizeof(jsoff_t) + sizeof(jsoff_t) + sizeof(jsval_t);
    case T_STR:   return sizeof(jsoff_t) + align32(w >> 2);
    default:      return (jsoff_t) ~0;
  }
  // clang-format on
}

static bool is_mem_entity(uint8_t t) {
  return t == T_OBJ || t == T_PROP || t == T_STR || t == T_FUNC;
}

static void js_fixup_offsets(struct js *js, jsoff_t start, jsoff_t size) {
  for (jsoff_t n, v, off = 0; off < js->brk; off += n) {  // start from 0!
    v = loadoff(js, off);
    n = esize(v & ~MARK);
    if (v & MARK) continue;  // To be deleted, don't bother
    if ((v & 3) != T_OBJ && (v & 3) != T_PROP) continue;
    if (v > start) saveoff(js, off, v - size);
    if ((v & 3) == T_PROP) {
      jsoff_t koff = loadoff(js, off + sizeof(off));
      if (koff > start) saveoff(js, off + sizeof(off), koff - size);
      jsval_t val = loadval(js, off + sizeof(off) + sizeof(off));
      if (is_mem_entity(vtype(val)) && vdata(val) > start) {
        // printf("MV %u %lu -> %lu\n", off, vdata(val), vdata(val) - size);
        saveval(js, off + sizeof(off) + sizeof(off),
                mkval(vtype(val), vdata(val) - size));
      }
    }
  }
  for (jsoff_t i = 0; i < js->ncbs; i++) {
    jsoff_t base = js->size + i * 3 * sizeof(jsoff_t) + sizeof(jsoff_t);
    jsoff_t o1 = loadoff(js, base), o2 = loadoff(js, base + sizeof(o1));
    if (o1 > start) saveoff(js, base, o1 - size);
    if (o2 > start) saveoff(js, base + sizeof(jsoff_t), o2 - size);
  }
  // Fixup js->scope
  jsoff_t off = vdata(js->scope);
  if (off > start) js->scope = mkval(T_OBJ, off - size);
}

static void js_delete_marked_entities(struct js *js) {
  for (jsoff_t n, v, off = 0; off < js->brk; off += n) {
    v = loadoff(js, off);
    n = esize(v & ~MARK);
    if (v & MARK) {  // This entity is marked for deletion, remove it
      // printf("DEL: %4u %d %x\n", off, v & 3, n);
      // assert(off + n <= js->brk);
      js_fixup_offsets(js, off, n);
      memmove(&js->mem[off], &js->mem[off + n], js->brk - off - n);
      js->brk -= n;  // Shrink brk boundary by the size of deleted entity
      n = 0;         // We shifted data, next iteration stay on this offset
    }
  }
}

static void js_mark_all_entities_for_deletion(struct js *js) {
  for (jsoff_t v, off = 0; off < js->brk; off += esize(v)) {
    v = loadoff(js, off);
    saveoff(js, off, v | MARK);
  }
}

static jsoff_t js_unmark_entity(struct js *js, jsoff_t off) {
  jsoff_t v = loadoff(js, off);
  if (v & MARK) {
    saveoff(js, off, v & ~MARK);
    // printf("UNMARK %5u\n", off);
    if ((v & 3) == T_OBJ) js_unmark_entity(js, v & ~(MARK | 3));
    if ((v & 3) == T_PROP) {
      js_unmark_entity(js, v & ~(MARK | 3));                 // Unmark next prop
      js_unmark_entity(js, loadoff(js, off + sizeof(off)));  // Unmark key
      jsval_t val = loadval(js, off + sizeof(off) + sizeof(off));
      if (is_mem_entity(vtype(val))) js_unmark_entity(js, vdata(val));
    }
  }
  return v & ~(MARK | 3);
}

static void js_unmark_used_entities(struct js *js) {
  for (jsval_t scope = js->scope;;) {
    js_unmark_entity(js, vdata(scope));
    jsoff_t off = loadoff(js, vdata(scope)) & ~3;
    while (off < js->brk && off != 0) off = js_unmark_entity(js, off);
    if (vdata(scope) == 0) break;  // Last (global) scope processed
    scope = upper(js, scope);
  }
  for (jsoff_t i = 0; i < js->ncbs; i++) {
    jsoff_t base = js->size + i * 3 * sizeof(jsoff_t) + sizeof(jsoff_t);
    js_unmark_entity(js, loadoff(js, base));
    js_unmark_entity(js, loadoff(js, base + sizeof(jsoff_t)));
  }
}

void js_gc(struct js *js) {
  // printf("GC RUN\n");
  js_mark_all_entities_for_deletion(js);
  js_unmark_used_entities(js);
  js_delete_marked_entities(js);
}

// Skip whitespaces and comments
static jsoff_t skiptonext(const char *code, jsoff_t len, jsoff_t n) {
  // printf("SKIP: [%.*s]\n", len - n, &code[n]);
  while (n < len) {
    if (is_space(code[n])) {
      n++;
    } else if (n + 1 < len && code[n] == '/' && code[n + 1] == '/') {
      for (n += 2; n < len && code[n] != '\n';) n++;
    } else if (n + 3 < len && code[n] == '/' && code[n + 1] == '*') {
      for (n += 4; n < len && (code[n - 2] != '*' || code[n - 1] != '/');) n++;
    } else {
      break;
    }
  }
  return n;
}

static bool streq(const char *buf, size_t len, const char *p, size_t n) {
  return n == len && memcmp(buf, p, len) == 0;
}

static uint8_t parsekeyword(const char *buf, size_t len) {
  // clang-format off
  switch (buf[0]) {
    case 'b': if (streq("break", 5, buf, len)) return TOK_BREAK; break;
    case 'c': if (streq("class", 5, buf, len)) return TOK_CLASS; if (streq("case", 4, buf, len)) return TOK_CASE; if (streq("catch", 5, buf, len)) return TOK_CATCH; if (streq("const", 5, buf, len)) return TOK_CONST; if (streq("continue", 8, buf, len)) return TOK_CONTINUE; break;
    case 'd': if (streq("do", 2, buf, len)) return TOK_DO;  if (streq("default", 7, buf, len)) return TOK_DEFAULT; break; // if (streq("delete", 6, buf, len)) return TOK_DELETE; break;
    case 'e': if (streq("else", 4, buf, len)) return TOK_ELSE; break;
    case 'f': if (streq("for", 3, buf, len)) return TOK_FOR; if (streq("function", 8, buf, len)) return TOK_FUNC; if (streq("finally", 7, buf, len)) return TOK_FINALLY; if (streq("false", 5, buf, len)) return TOK_FALSE; break;
    case 'i': if (streq("if", 2, buf, len)) return TOK_IF; if (streq("in", 2, buf, len)) return TOK_IN; if (streq("instanceof", 10, buf, len)) return TOK_INSTANCEOF; break;
    case 'l': if (streq("let", 3, buf, len)) return TOK_LET; break;
    case 'n': if (streq("new", 3, buf, len)) return TOK_NEW; if (streq("null", 4, buf, len)) return TOK_NULL; break;
    case 'r': if (streq("return", 6, buf, len)) return TOK_RETURN; break;
    case 's': if (streq("switch", 6, buf, len)) return TOK_SWITCH; break;
    case 't': if (streq("try", 3, buf, len)) return TOK_TRY; if (streq("this", 4, buf, len)) return TOK_THIS; if (streq("throw", 5, buf, len)) return TOK_THROW; if (streq("true", 4, buf, len)) return TOK_TRUE; if (streq("typeof", 6, buf, len)) return TOK_TYPEOF; break;
    case 'u': if (streq("undefined", 9, buf, len)) return TOK_UNDEF; break;
    case 'v': if (streq("var", 3, buf, len)) return TOK_VAR; if (streq("void", 4, buf, len)) return TOK_VOID; break;
    case 'w': if (streq("while", 5, buf, len)) return TOK_WHILE; if (streq("with", 4, buf, len)) return TOK_WITH; break;
    case 'y': if (streq("yield", 5, buf, len)) return TOK_YIELD; break;
  }
  // clang-format on
  return TOK_IDENTIFIER;
}

static uint8_t parseident(const char *buf, jsoff_t len, jsoff_t *tlen) {
  if (is_ident_begin(buf[0])) {
    while (*tlen < len && is_ident_continue(buf[*tlen])) (*tlen)++;
    return parsekeyword(buf, *tlen);
  }
  return TOK_ERR;
}

static uint8_t nexttok(struct js *js) {
  js->tok = TOK_ERR;
  js->toff = js->pos = skiptonext(js->code, js->clen, js->pos);
  js->tlen = 0;
  const char *buf = js->code + js->toff;
  // clang-format off
  if (js->toff >= js->clen) { js->tok = TOK_EOF; return js->tok; }
#define TOK(T, LEN) { js->tok = T; js->tlen = (LEN); break; }
#define LOOK(OFS, CH) js->toff + OFS < js->clen && buf[OFS] == CH
  switch (buf[0]) {
    case '?': TOK(TOK_Q, 1);
    case ':': TOK(TOK_COLON, 1);
    case '(': TOK(TOK_LPAREN, 1);
    case ')': TOK(TOK_RPAREN, 1);
    case '{': TOK(TOK_LBRACE, 1);
    case '}': TOK(TOK_RBRACE, 1);
    case ';': TOK(TOK_SEMICOLON, 1);
    case ',': TOK(TOK_COMMA, 1);
    case '!': if (LOOK(1, '=') && LOOK(2, '=')) TOK(TOK_NE, 3); TOK(TOK_NOT, 1);
    case '.': TOK(TOK_DOT, 1);
    case '~': TOK(TOK_NEG, 1);
    case '-': if (LOOK(1, '-')) TOK(TOK_POSTDEC, 2); if (LOOK(1, '=')) TOK(TOK_MINUS_ASSIGN, 2); TOK(TOK_MINUS, 1);
    case '+': if (LOOK(1, '+')) TOK(TOK_POSTINC, 2); if (LOOK(1, '=')) TOK(TOK_PLUS_ASSIGN, 2); TOK(TOK_PLUS, 1);
    case '*': if (LOOK(1, '*')) TOK(TOK_EXP, 2); if (LOOK(1, '=')) TOK(TOK_MUL_ASSIGN, 2); TOK(TOK_MUL, 1);
    case '/': if (LOOK(1, '=')) TOK(TOK_DIV_ASSIGN, 2); TOK(TOK_DIV, 1);
    case '%': if (LOOK(1, '=')) TOK(TOK_REM_ASSIGN, 2); TOK(TOK_REM, 1);
    case '&': if (LOOK(1, '&')) TOK(TOK_LAND, 2); if (LOOK(1, '=')) TOK(TOK_AND_ASSIGN, 2); TOK(TOK_AND, 1);
    case '|': if (LOOK(1, '|')) TOK(TOK_LOR, 2); if (LOOK(1, '=')) TOK(TOK_OR_ASSIGN, 2); TOK(TOK_OR, 1);
    case '=': if (LOOK(1, '=') && LOOK(2, '=')) TOK(TOK_EQ, 3); TOK(TOK_ASSIGN, 1);
    case '<': if (LOOK(1, '<') && LOOK(2, '=')) TOK(TOK_SHL_ASSIGN, 3); if (LOOK(1, '<')) TOK(TOK_SHL, 2); if (LOOK(1, '=')) TOK(TOK_LE, 2); TOK(TOK_LT, 1);
    case '>': if (LOOK(1, '>') && LOOK(2, '=')) TOK(TOK_SHR_ASSIGN, 3); if (LOOK(1, '>')) TOK(TOK_SHR, 2); if (LOOK(1, '=')) TOK(TOK_GE, 2); TOK(TOK_GT, 1);
    case '^': if (LOOK(1, '=')) TOK(TOK_XOR_ASSIGN, 2); TOK(TOK_XOR, 1);
    case '"': case '\'':
      js->tlen++;
      while (js->toff + js->tlen < js->clen && buf[js->tlen] != buf[0]) {
        uint8_t increment = 1;
        if (buf[js->tlen] == '\\') {
          if (js->toff + js->tlen + 2 > js->clen) break;
          increment = 2;
          if (buf[js->tlen + 1] == 'x') {
            if (js->toff + js->tlen + 4 > js->clen) break;
            increment = 4;
          }
        }
        js->tlen += increment;
      }
      if (buf[0] == buf[js->tlen]) js->tok = TOK_STRING, js->tlen++;
      break;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
      char *end;
      js->tval = tov(strtod(buf, &end)); // TODO(lsm): protect against OOB access
      TOK(TOK_NUMBER, (jsoff_t) (end - buf));
    }
    default: js->tok = parseident(buf, js->clen - js->toff, &js->tlen); break;
  }
  // clang-format on
  js->pos = js->toff + js->tlen;
  // printf("NEXT: %d [%.*s]\n", js->tok, (int) js->tlen, buf);
  return js->tok;
}

static uint8_t lookahead(struct js *js) {
  uint8_t tok = nexttok(js);
  js->pos -= js->tlen;
  return tok;
}

// Bubble sort operators by their priority. TOK_* enum is already sorted
static void sortops(uint8_t *ops, int nops, jsval_t *stk) {
  uint8_t prios[] = {19, 19, 17, 17, 16, 16, 16, 16, 16, 15, 14, 14, 14, 13, 13,
                     12, 12, 12, 11, 11, 11, 11, 10, 10, 9,  8,  7,  6,  5,  4,
                     4,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  1};
  // printf("PRIO: %d\n", prios[TOK_PLUS - TOK_DOT]);
  for (bool done = false; done == false;) {
    done = true;
    for (int i = 0; i + 1 < nops; i++) {
      uint8_t o1 = vdata(stk[ops[i]]) & 255, o2 = vdata(stk[ops[i + 1]]) & 255;
      uint8_t a = prios[o1 - TOK_DOT], b = prios[o2 - TOK_DOT], tmp = ops[i];
      bool swap = a < b;
      if (o1 == o2 && is_right_assoc(o1) && ops[i] < ops[i + 1]) swap = 1;
      if (swap) ops[i] = ops[i + 1], ops[i + 1] = tmp, done = false;
    }
  }
}

static void mkscope(struct js *js) {
  assert((js->flags & F_NOEXEC) == 0);
  jsoff_t prev = vdata(js->scope);
  js->scope = mkobj(js, prev);
  // printf("ENTER SCOPE %u, prev %u\n", (jsoff_t) vdata(js->scope), prev);
}

static void delscope(struct js *js) {
  js->scope = upper(js, js->scope);
  // printf("EXIT  SCOPE %u\n", (jsoff_t) vdata(js->scope));
}

static jsval_t js_block(struct js *js, bool create_scope) {
  jsval_t res = mkval(T_UNDEF, 0);
  jsoff_t brk1 = js->brk;
  if (create_scope) mkscope(js);  // Enter new scope
  jsoff_t brk2 = js->brk;
  while (js->tok != TOK_EOF && js->tok != TOK_RBRACE) {
    js->pos = skiptonext(js->code, js->clen, js->pos);
    if (js->pos < js->clen && js->code[js->pos] == '}') break;
    res = js_stmt(js, TOK_RBRACE);
    // printf(" blstmt [%.*s]\n", js->pos - pos, &js->code[pos]);
  }
  if (js->pos < js->clen && js->code[js->pos] == '}') js->pos++;
  // printf("BLOCKEND [%.*s]\n", js->pos - pos, &js->code[pos]);
  if (create_scope) delscope(js);       // Exit scope
  if (js->brk == brk2) js->brk = brk1;  // Fast scope GC
  return res;
}

static jsval_t js_eval_nogc(struct js *js, const char *buf, jsoff_t len) {
  jsval_t res = mkval(T_UNDEF, 0);
  js->tok = TOK_ERR;
  js->code = buf;
  js->clen = len;
  js->pos = 0;
  while (js->tok != TOK_EOF && !is_err(res)) {
    js->pos = skiptonext(js->code, js->clen, js->pos);
    if (js->pos >= js->clen) break;
    res = js_stmt(js, TOK_SEMICOLON);
  }
  return res;
}

static jsval_t resolveprop(struct js *js, jsval_t v) {
  if (vtype(v) != T_PROP) return v;
  v = loadval(js, vdata(v) + sizeof(jsoff_t) + sizeof(jsoff_t));
  return resolveprop(js, v);
}

static jsval_t assign(struct js *js, jsval_t lhs, jsval_t val) {
  saveval(js, (vdata(lhs) & ~3) + sizeof(jsoff_t) + sizeof(jsoff_t), val);
  return lhs;
}

static jsval_t do_assign_op(struct js *js, uint8_t op, jsval_t l, jsval_t r) {
  uint8_t m[] = {TOK_PLUS, TOK_MINUS, TOK_MUL, TOK_DIV, TOK_REM, TOK_SHL,
                 TOK_SHR,  TOK_ZSHR,  TOK_AND, TOK_XOR, TOK_OR};
  jsval_t res = do_op(js, m[op - TOK_PLUS_ASSIGN], resolveprop(js, l), r);
  return assign(js, l, res);
}

// Seach for property in a single object
static jsoff_t lkp(struct js *js, jsval_t obj, const char *buf, size_t len) {
  jsoff_t off = loadoff(js, vdata(obj)) & ~3;  // Load first prop offset
  // printf("LKP: %lu %u [%.*s]\n", vdata(obj), off, (int) len, buf);
  while (off < js->brk && off != 0) {  // Iterate over props
    jsoff_t koff = loadoff(js, off + sizeof(off));
    jsoff_t klen = (loadoff(js, koff) >> 2) - 1;
    const char *p = (char *) &js->mem[koff + sizeof(koff)];
    // printf("  %u %u[%.*s]\n", off, (int) klen, (int) klen, p);
    if (streq(buf, len, p, klen)) return off;  // Found !
    off = loadoff(js, off) & ~3;               // Load next prop offset
  }
  return 0;  // Not found
}

// Lookup variable in the scope chain
static jsval_t lookup(struct js *js, const char *buf, size_t len) {
  for (jsval_t scope = js->scope;;) {
    jsoff_t off = lkp(js, scope, buf, len);
    if (off != 0) return mkval(T_PROP, off);
    if (vdata(scope) == 0) break;
    scope = mkval(T_OBJ, loadoff(js, vdata(scope) + sizeof(jsoff_t)));
  }
  return js_err(js, "'%.*s' not found", (int) len, buf);
}

static jsval_t do_string_op(struct js *js, uint8_t op, jsval_t l, jsval_t r) {
  jsoff_t n1, off1 = vstr(js, l, &n1);
  jsoff_t n2, off2 = vstr(js, r, &n2);
  if (op == TOK_PLUS) {
    jsval_t res = mkstr(js, NULL, n1 + n2);
    if (vtype(res) == T_STR) {
      jsoff_t n, off = vstr(js, res, &n);
      memmove(&js->mem[off], &js->mem[off1], n1);
      memmove(&js->mem[off + n1], &js->mem[off2], n2);
    }
    return res;
  } else if (op == TOK_EQ) {
    bool eq = n1 == n2 && memcmp(&js->mem[off1], &js->mem[off2], n1) == 0;
    return mkval(T_BOOL, eq ? 1 : 0);
  } else if (op == TOK_NE) {
    bool eq = n1 == n2 && memcmp(&js->mem[off1], &js->mem[off2], n1) == 0;
    return mkval(T_BOOL, eq ? 0 : 1);
  } else {
    return js_err(js, "bad str op");
  }
}

static jsval_t do_dot_op(struct js *js, jsval_t l, jsval_t r) {
  const char *ptr = (char *) &js->code[coderefoff(r)];
  if (vtype(r) != T_CODEREF) return js_err(js, "ident expected");
  // Handle stringvalue.length
  if (vtype(l) == T_STR && streq(ptr, codereflen(r), "length", 6)) {
    return tov(offtolen(loadoff(js, vdata(l))));
  }
  if (vtype(l) != T_OBJ) return js_err(js, "lookup in non-obj");
  jsoff_t off = lkp(js, l, ptr, codereflen(r));
  return off == 0 ? mkval(T_UNDEF, 0) : mkval(T_PROP, off);
}

static jsval_t js_call_params(struct js *js) {
  jsoff_t pos = js->pos;
  if (nexttok(js) == TOK_RPAREN)
    return mkcoderef(pos, js->pos - pos - js->tlen);
  js->pos -= js->tlen;
  uint8_t flags = js->flags;
  js->flags |= F_NOEXEC;
  do {
    jsval_t res = js_expr(js, TOK_COMMA, TOK_RPAREN);
    if (is_err(res)) return res;
    if (vdata(res) == 0) js->tok = TOK_ERR;  // Expression had 0 tokens
  } while (js->tok == TOK_COMMA);
  js->flags = flags;
  if (js->tok != TOK_RPAREN) return js_err(js, "parse error");
  return mkcoderef(pos, js->pos - pos - js->tlen);
}

///////////////////////////////////////////////  C  FFI implementation start
// clang-format off
#define MAX_FFI_ARGS 6
typedef uintptr_t jw_t;
typedef jsval_t (*w6w_t)(jw_t, jw_t, jw_t, jw_t, jw_t, jw_t);
union ffi_val { void *p; w6w_t fp; jw_t w; double d; uint64_t u64; };
//struct fficbparam { struct js *js; const char *decl; jsval_t jsfunc; };

static jsval_t call_js(struct js *js, const char *fn, int fnlen);
#ifndef JS_NOCB
static jw_t fficb(uintptr_t param, union ffi_val *args) {
  jsoff_t size;
  memcpy(&size, (char *) param, sizeof(size));
  struct js *js = (struct js *) ((char *) param - size) - 1;
  //printf("FFICB: js %p, param %p %u\n", js, (void *) param, size);
  jsoff_t f1len, f1off = vstr(js, loadoff(js, size + sizeof(jsoff_t)), &f1len);
  jsoff_t f2len, f2off = vstr(js, loadoff(js, size + 2 * sizeof(jsoff_t)), &f2len);
  char buf[100], *decl = (char *) &js->mem[f1off];
  jsoff_t n = 0, max = (jsoff_t) sizeof(buf);
  //printf("F1: %u[%.*s]\n", f1len, (int) f1len, decl);
  //printf("F2: %u[%.*s]\n", f2len, (int) f2len, &js->mem[f2off]);
  while (f1len > 1 && decl[0] != '[') decl++, f1len--;
  for (jsoff_t i = 0; i + 2 < f1len && decl[i + 2] != ']'; i++) {
    if (n > 0) n += snprintf(buf + n, max - n, "%s", ",");
    switch (decl[i + 2]) {
      case 's': n += snprintf(buf + n, max - n, "'%s'", (char *) (uintptr_t) args[i].w); break;
      case 'i': n += snprintf(buf + n, max - n, "%ld", (long) args[i].w); break;
      case 'd': n += snprintf(buf + n, max - n, "%g", args[i].d); break;
      default: n += snprintf(buf + n, max - n, "%s", "null"); break;
    }
  }
  const char *code = js->code;             // Save current parser state
  jsoff_t clen = js->clen, pos = js->pos;  // code, position and code length
  js->code = buf;                          // Point parser to args
  js->clen = n;                            // Set args length
  js->pos = 0; 
  //jsoff_t fnlen, fnoff = vstr(js, cbp->jsfunc, &fnlen);
  //printf("CALLING %s %p [%s] -> [%.*s]\n", decl, args, buf, (int) f2len, (char *) &js->mem[f2off]);
  jsval_t res = call_js(js, (char *) &js->mem[f2off], f2len);
  js->code = code, js->clen = clen, js->pos = pos;  // Restore parser
  //printf("FFICB->[%s]\n", js_str(js, res));
  switch (decl[1]) {
    case 'v': return mkval(T_UNDEF, 0);
    case 'i': return (long) (is_nan(res) ? 0.0 : tod(res));
    case 'd': case 'p': return (jw_t) tod(res);
    case 's': if (vtype(res) == T_STR) return (jw_t) (js->mem + vstr(js, res, NULL));
  }
  return res;
}

static void ffiinitcbargs(union ffi_val *args, jw_t w1, jw_t w2, jw_t w3, jw_t w4, jw_t w5, jw_t w6) { args[0].w = w1; args[1].w = w2; args[2].w = w3; args[3].w = w4; args[4].w = w5; args[5].w = w6; }
static jsval_t fficb1(jw_t w1, jw_t w2, jw_t w3, jw_t w4, jw_t w5, jw_t w6) { union ffi_val args[6]; ffiinitcbargs(args, w1, w2, w3, w4, w5, w6); return fficb(w1, args); }
static jsval_t fficb2(jw_t w1, jw_t w2, jw_t w3, jw_t w4, jw_t w5, jw_t w6) { union ffi_val args[6]; ffiinitcbargs(args, w1, w2, w3, w4, w5, w6); return fficb(w2, args); }
static jsval_t fficb3(jw_t w1, jw_t w2, jw_t w3, jw_t w4, jw_t w5, jw_t w6) { union ffi_val args[6]; ffiinitcbargs(args, w1, w2, w3, w4, w5, w6); return fficb(w3, args); }
static jsval_t fficb4(jw_t w1, jw_t w2, jw_t w3, jw_t w4, jw_t w5, jw_t w6) { union ffi_val args[6]; ffiinitcbargs(args, w1, w2, w3, w4, w5, w6); return fficb(w4, args); }
static jsval_t fficb5(jw_t w1, jw_t w2, jw_t w3, jw_t w4, jw_t w5, jw_t w6) { union ffi_val args[6]; ffiinitcbargs(args, w1, w2, w3, w4, w5, w6); return fficb(w5, args); }
static jsval_t fficb6(jw_t w1, jw_t w2, jw_t w3, jw_t w4, jw_t w5, jw_t w6) { union ffi_val args[6]; ffiinitcbargs(args, w1, w2, w3, w4, w5, w6); return fficb(w6, args); }
// clang-format on
static w6w_t setfficb(const char *decl, int *idx) {
  w6w_t res = 0, cbs[] = {fficb1, fficb2, fficb3, fficb4, fficb5, fficb6};
  for (size_t i = 1; decl[i] != '\0' && decl[i] != ']'; i++) {
    if (i >= (sizeof(cbs) / sizeof(cbs[0]))) break;
    if (decl[i] == 'u') res = cbs[i - 1];  //, printf("SET CB %zu\n", i - 1);
    (*idx)++;
  }
  (*idx) += 2;
  return res;
}
#endif

// Call native C function
static jsval_t call_c(struct js *js, const char *fn, int fnlen, jsoff_t fnoff) {
  union ffi_val args[MAX_FFI_ARGS], res;
  jsoff_t cbp = 0;
  int n = 0, i, type = fn[0] == 'd' ? 1 : 0;
  for (i = 1; i < fnlen && fn[i] != '@' && n < MAX_FFI_ARGS; i++) {
    js->pos = skiptonext(js->code, js->clen, js->pos);
    if (js->pos >= js->clen) return js_err(js, "bad arg %d", n + 1);
    jsval_t v = resolveprop(js, js_expr(js, TOK_COMMA, TOK_RPAREN));
    // printf("  arg %d[%c] -> %s\n", n, fn[i], js_str(js, v));
    if (fn[i] == 'd' || (fn[i] == 'j' && sizeof(jsval_t) > sizeof(jw_t))) {
      type |= 1 << (n + 1);
    }
    uint8_t t = vtype(v);
    // clang-format off
    switch (fn[i]) {
#ifndef JS_NOCB
      case '[':
        // Create a non-GC-able FFI callback parameter inside JS runtime memory,
        // to make it live if C calls it after GC is performed. This param
        // should be a `void *` that goes to C, and stays intact. We allocate
        // it at the end of free memory block and never GC. It has 3 values:
        //    offset to the js->mem. Used by fficb() to obtain `struct js *` ptr
        //    offset of the caller func. Used to get C func signature
        //    offset of the cb func. Used to actually run JS callback
        js->ncbs++;
        js->size -= sizeof(jsoff_t) * 3;
        cbp = js->size;
        saveoff(js, cbp, cbp);
        saveoff(js, cbp + sizeof(jsoff_t), fnoff);
        saveoff(js, cbp + sizeof(jsoff_t) + sizeof(jsoff_t), vdata(v));
        args[n++].p = (void *) setfficb(&fn[i + 1], &i);
        //printf("CB PARAM SET: js %p, param %p %u\n", js, &js->mem[cbp], cbp);
        break;
#endif
      case 'd': if (t != T_NUM) return js_err(js, "bad arg %d", n + 1); args[n++].d = tod(v); break;
      case 'b': if (t != T_BOOL) return js_err(js, "bad arg %d", n + 1); args[n++].w = vdata(v); break;
      case 'i': if (t != T_NUM && t != T_BOOL) return js_err(js, "bad arg %d", n + 1); args[n++].w = t == T_BOOL ? (long) vdata(v) : (long) tod(v); break;
      case 's': if (t != T_STR) return js_err(js, "bad arg %d", n + 1); args[n++].p = js->mem + vstr(js, v, NULL); break;
      case 'p': if (t != T_NUM) return js_err(js, "bad arg %d", n + 1); args[n++].w = (jw_t) tod(v); break;
			case 'j': args[n++].u64 = v; break;
			case 'm': args[n++].p = js; break;
			case 'u': args[n++].p = &js->mem[cbp]; break;
      default: return js_err(js, "bad sig");
    }
    js->pos = skiptonext(js->code, js->clen, js->pos);
    if (js->pos < js->clen && js->code[js->pos] == ',') js->pos++;
  }
  uintptr_t f = (uintptr_t) unhexn((uint8_t *) &fn[i + 1], fnlen - i - 1);
  //printf("  type %d nargs %d addr %" PRIxPTR "\n", type, n, f);
  if (js->pos != js->clen) return js_err(js, "num args");
  if (fn[i] != '@') return js_err(js, "ffi");
  if (f == 0) return js_err(js, "ffi");
#ifndef WIN32
#define __cdecl
#endif
  switch (type) {
    case 0: res.u64 = ((uint64_t(__cdecl*)(jw_t,jw_t,jw_t,jw_t,jw_t,jw_t)) f)      (args[0].w, args[1].w, args[2].w, args[3].w, args[4].w, args[5].w); break;
    case 1: res.d = ((double(__cdecl*)(jw_t,jw_t,jw_t,jw_t,jw_t,jw_t)) f)    (args[0].w, args[1].w, args[2].w, args[3].w, args[4].w, args[5].w); break;
    case 2: res.u64 = ((uint64_t(__cdecl*)(double,jw_t,jw_t,jw_t,jw_t,jw_t)) f)    (args[0].d, args[1].w, args[2].w, args[3].w, args[4].w, args[5].w); break;
    case 3: res.d = ((double(__cdecl*)(double,jw_t,jw_t,jw_t,jw_t,jw_t)) f)  (args[0].d, args[1].w, args[2].w, args[3].w, args[4].w, args[5].w); break;
    case 4: res.u64 = ((uint64_t(__cdecl*)(jw_t,double,jw_t,jw_t,jw_t,jw_t)) f)    (args[0].w, args[1].d, args[2].w, args[3].w, args[4].w, args[5].w); break;
    case 5: res.d = ((double(__cdecl*)(jw_t,double,jw_t,jw_t,jw_t,jw_t)) f)  (args[0].w, args[1].d, args[2].w, args[3].w, args[4].w, args[5].w); break;
    case 6: res.u64 = ((uint64_t(__cdecl*)(double,double,jw_t,jw_t,jw_t,jw_t)) f)  (args[0].d, args[1].d, args[2].w, args[3].w, args[4].w, args[5].w); break;
    case 7: res.d = ((double(__cdecl*)(double,double,jw_t,jw_t,jw_t,jw_t)) f)(args[0].d, args[1].d, args[2].w, args[3].w, args[4].w, args[5].w); break;
    default: return js_err(js, "ffi");
  }
  //printf("  TYPE %d RES: %" PRIxPTR " %g %p\n", type, res.v, res.d, res.p);
  // Import return value into JS
  switch (fn[0]) {
    case 'p': return tov(res.w);
    case 'i': return tov((int) res.u64);
    case 'd': return tov(res.d);
    case 'b': return mkval(T_BOOL, res.w ? 1 : 0);
    case 's': return mkstr(js, (char *) (intptr_t) res.w, strlen((char *) (intptr_t) res.w));
    case 'v': return mkval(T_UNDEF, 0);
    case 'j': return (jsval_t) res.u64;
  }
  // clang-format on
  return js_err(js, "bad sig");
}
///////////////////////////////////////////////  C  FFI implementation end

// Call JS function. 'fn' looks like this: "(a,b) { return a + b; }"
static jsval_t call_js(struct js *js, const char *fn, int fnlen) {
  int fnpos = 1;
  mkscope(js);  // Create function call scope
  // Loop over arguments list "(a, b)" and set scope variables
  while (fnpos < fnlen) {
    fnpos = skiptonext(fn, fnlen, fnpos);          // Skip to the identifier
    if (fnpos < fnlen && fn[fnpos] == ')') break;  // Closing paren? break!
    jsoff_t identlen = 0;                          // Identifier length
    uint8_t tok = parseident(&fn[fnpos], fnlen - fnpos, &identlen);
    if (tok != TOK_IDENTIFIER) break;
    // Here we have argument name
    // printf("  [%.*s] -> %u [%.*s] -> ", (int) identlen, &fn[fnpos], js->pos,
    //(int) js->clen, js->code);
    // Calculate argument's value.
    js->pos = skiptonext(js->code, js->clen, js->pos);
    jsval_t v = js->code[js->pos] == ')' ? mkval(T_UNDEF, 0)
                                         : js_expr(js, TOK_COMMA, TOK_RPAREN);
    // printf("[%s]\n", js_str(js, v));
    // Set argument in the function scope
    setprop(js, js->scope, mkstr(js, &fn[fnpos], identlen), v);
    js->pos = skiptonext(js->code, js->clen, js->pos);
    if (js->pos < js->clen && js->code[js->pos] == ',') js->pos++;
    fnpos = skiptonext(fn, fnlen, fnpos + identlen);  // Skip past identifier
    if (fnpos < fnlen && fn[fnpos] == ',') fnpos++;  // And skip comma
  }
  if (fnpos < fnlen && fn[fnpos] == ')') fnpos++;  // Skip to the function body
  fnpos = skiptonext(fn, fnlen, fnpos);            // Up to the opening brace
  if (fnpos < fnlen && fn[fnpos] == '{') fnpos++;  // And skip the brace
  jsoff_t n = fnlen - fnpos - 1;  // Function code with stripped braces
  // printf("  %d. calling, %u [%.*s]\n", js->flags, n, (int) n, &fn[fnpos]);
  js->flags = F_CALL;  // Mark we're in the function call
  jsval_t res = js_eval_nogc(js, &fn[fnpos], n);         // Call function, no GC
  if (!(js->flags & F_RETURN)) res = mkval(T_UNDEF, 0);  // Is return called?
  delscope(js);                                          // Delete call scope
  return res;
}

static jsval_t do_call_op(struct js *js, jsval_t func, jsval_t args) {
  if (vtype(func) != T_FUNC) return js_err(js, "calling non-function");
  if (vtype(args) != T_CODEREF) return js_err(js, "bad call");
  jsoff_t fnlen, fnoff = vstr(js, func, &fnlen);
  const char *fn = (const char *) &js->mem[fnoff];
  const char *code = js->code;              // Save current parser state
  jsoff_t clen = js->clen, pos = js->pos;   // code, position and code length
  js->code = &js->code[coderefoff(args)];   // Point parser to args
  js->clen = codereflen(args);              // Set args length
  js->pos = skiptonext(js->code, js->clen, 0);  // Skip to 1st arg
  // printf("CALL [%.*s] -> %.*s\n", (int) js->clen, js->code, (int) fnlen, fn);
  uint8_t tok = js->tok, flags = js->flags;  // Save flags
  jsval_t res = fn[0] != '(' ? call_c(js, fn, fnlen, fnoff - sizeof(jsoff_t))
                             : call_js(js, fn, fnlen);
  // printf("  -> %s\n", js_str(js, res));
  js->code = code, js->clen = clen, js->pos = pos;  // Restore parser
  js->flags = flags, js->tok = tok;
  return res;
}

static jsval_t do_logical_or(struct js *js, jsval_t l, jsval_t r) {
  if (js_truthy(js, l)) return mkval(T_BOOL, 1);
  return mkval(T_BOOL, js_truthy(js, r) ? 1 : 0);
}

// clang-format off
static jsval_t do_op(struct js *js, uint8_t op, jsval_t lhs, jsval_t rhs) {
  jsval_t l = resolveprop(js, lhs), r = resolveprop(js, rhs);
  //printf("OP %d %d %d\n", op, vtype(lhs), vtype(r));
  if (is_assign(op) && vtype(lhs) != T_PROP) return js_err(js, "bad lhs");
  switch (op) {
    case TOK_LAND:    return mkval(T_BOOL, js_truthy(js, l) && js_truthy(js, r) ? 1 : 0);
    case TOK_LOR:     return do_logical_or(js, l, r);
    case TOK_TYPEOF:  return mkstr(js, typestr(vtype(r)), strlen(typestr(vtype(r))));
    case TOK_CALL:    return do_call_op(js, l, r);
    case TOK_ASSIGN:  return assign(js, lhs, r);
    case TOK_POSTINC: { do_assign_op(js, TOK_PLUS_ASSIGN, lhs, tov(1)); return l; }
    case TOK_POSTDEC: { do_assign_op(js, TOK_MINUS_ASSIGN, lhs, tov(1)); return l; }
    case TOK_NOT:     if (vtype(r) == T_BOOL) return mkval(T_BOOL, !vdata(r)); break;
  }
  if (is_assign(op))    return do_assign_op(js, op, lhs, r);
  if (vtype(l) == T_STR && vtype(r) == T_STR) return do_string_op(js, op, l, r);
  if (is_unary(op) && vtype(r) != T_NUM) return js_err(js, "type mismatch");
  if (!is_unary(op) && op != TOK_DOT && (vtype(l) != T_NUM || vtype(r) != T_NUM)) return js_err(js, "type mismatch");
  double a = tod(l), b = tod(r);
  switch (op) {
    case TOK_EXP:     return tov(pow(a, b));
    case TOK_DIV:     return tod(r) == 0 ? js_err(js, "div by zero") : tov(a / b);
    case TOK_REM:     return tov(a - b * ((long) (a / b)));
    case TOK_MUL:     return tov(a * b);
    case TOK_PLUS:    return tov(a + b);
    case TOK_MINUS:   return tov(a - b);
    case TOK_XOR:     return tov((long) a ^ (long) b);
    case TOK_AND:     return tov((long) a & (long) b);
    case TOK_OR:      return tov((long) a | (long) b);
    case TOK_UMINUS:  return tov(-b);
    case TOK_UPLUS:   return r;
    case TOK_NEG:     return tov(~(long) b);
    case TOK_NOT:     return mkval(T_BOOL, b == 0);
    case TOK_SHL:     return tov((long) a << (long) b);
    case TOK_SHR:     return tov((long) a >> (long) b);
    case TOK_DOT:     return do_dot_op(js, l, r);
    case TOK_EQ:      return mkval(T_BOOL, (long) a == (long) b);
    case TOK_NE:      return mkval(T_BOOL, (long) a != (long) b);
    case TOK_LT:      return mkval(T_BOOL, a < b);
    case TOK_LE:      return mkval(T_BOOL, a <= b);
    case TOK_GT:      return mkval(T_BOOL, a > b);
    case TOK_GE:      return mkval(T_BOOL, a >= b);
    default:          return js_err(js, "unknown op %d", (int) op);  // LCOV_EXCL_LINE
  }
}
// clang-format on

static uint8_t getri(uint32_t mask, uint8_t ri) {
  while (ri > 0 && (mask & (1 << ri))) ri--;
  if (!(mask & (1 << ri))) ri++;
  return ri;
}

static jsval_t js_str_literal(struct js *js) {
  uint8_t *in = (uint8_t *) &js->code[js->toff];
  uint8_t *out = &js->mem[js->brk + sizeof(jsoff_t)];
  int n1 = 0, n2 = 0;
  // printf("STR %u %lu %lu\n", js->brk, js->tlen, js->clen);
  if (js->brk + sizeof(jsoff_t) + js->tlen > js->size) return js_err(js, "oom");
  while (n2++ + 2 < (int) js->tlen) {
    if (in[n2] == '\\') {
      if (in[n2 + 1] == in[0]) {
        out[n1++] = in[0];
      } else if (in[n2 + 1] == 'n') {
        out[n1++] = '\n';
      } else if (in[n2 + 1] == 't') {
        out[n1++] = '\t';
      } else if (in[n2 + 1] == 'r') {
        out[n1++] = '\r';
      } else if (in[n2 + 1] == 'x' && is_xdigit(in[n2 + 2]) &&
                 is_xdigit(in[n2 + 3])) {
        out[n1++] = unhex(in[n2 + 2]) << 4 | unhex(in[n2 + 3]);
        n2 += 2;
      } else {
        return js_err(js, "bad str literal");
      }
      n2++;
    } else {
      out[n1++] = js->code[js->toff + n2];
    }
  }
  return mkstr(js, NULL, n1);
}

static jsval_t js_obj_literal(struct js *js) {
  uint8_t exe = !(js->flags & F_NOEXEC);
  // printf("OLIT1\n");
  jsval_t obj = exe ? mkobj(js, 0) : mkval(T_UNDEF, 0);
  if (is_err(obj)) return obj;
  while (nexttok(js) != TOK_RBRACE) {
    if (js->tok != TOK_IDENTIFIER) return js_err(js, "parse error");
    size_t koff = js->toff, klen = js->tlen;
    if (nexttok(js) != TOK_COLON) return js_err(js, "parse error");
    jsval_t val = js_expr(js, TOK_RBRACE, TOK_COMMA);
    if (exe) {
      // printf("XXXX [%s] scope: %lu\n", js_str(js, val), vdata(js->scope));
      if (is_err(val)) return val;
      jsval_t key = mkstr(js, js->code + koff, klen);
      if (is_err(key)) return key;
      jsval_t res = setprop(js, obj, key, resolveprop(js, val));
      if (is_err(res)) return res;
    }
    if (js->tok == TOK_RBRACE) break;
    if (js->tok != TOK_COMMA) return js_err(js, "parse error");
  }
  return obj;
}

static jsval_t js_func_literal(struct js *js) {
  jsoff_t pos = js->pos;
  uint8_t flags = js->flags;  // Save current flags
  if (nexttok(js) != TOK_LPAREN) return js_err(js, "parse error");
  for (bool expect_ident = false; nexttok(js) != TOK_EOF; expect_ident = true) {
    if (expect_ident && js->tok != TOK_IDENTIFIER)
      return js_err(js, "parse error");
    if (js->tok == TOK_RPAREN) break;
    if (js->tok != TOK_IDENTIFIER) return js_err(js, "parse error");
    if (nexttok(js) == TOK_RPAREN) break;
    if (js->tok != TOK_COMMA) return js_err(js, "parse error");
  }
  if (js->tok != TOK_RPAREN) return js_err(js, "parse error");
  if (nexttok(js) != TOK_LBRACE) return js_err(js, "parse error");
  js->flags |= F_NOEXEC;              // Set no-execution flag to parse the
  jsval_t res = js_block(js, false);  // Skip function body - no exec
  if (is_err(res)) return res;        // But fail short on parse error
  js->flags = flags;                  // Restore flags
  jsval_t str = mkstr(js, &js->code[pos], js->pos - pos);
  // printf("FUNC: %u [%.*s]\n", pos, js->pos - pos, &js->code[pos]);
  return mkval(T_FUNC, vdata(str));
}

static jsval_t js_expr(struct js *js, uint8_t etok, uint8_t etok2) {
  jsval_t stk[JS_EXPR_MAX];                     // parsed values
  uint8_t tok, ops[JS_EXPR_MAX], pt = TOK_ERR;  // operator indices
  uint8_t n = 0, nops = 0, nuops = 0;
  // printf("E1 %d %d %d %u/%u\n", js->tok, etok, etok2, js->pos, js->clen);
  while ((tok = nexttok(js)) != etok && tok != etok2 && tok != TOK_EOF) {
    // printf("E2 %d %d %d %u/%u\n", js->tok, etok, etok2, js->pos, js->clen);
    if (tok == TOK_ERR) return js_err(js, "parse error");
    if (n >= JS_EXPR_MAX) return js_err(js, "expr too deep");
    // Convert TOK_LPAREN to a function call TOK_CALL if required
    if (tok == TOK_LPAREN && (n > 0 && !is_op(pt))) tok = TOK_CALL;
    if (is_op(tok)) {
      // Convert this plus or minus to unary if required
      if (tok == TOK_PLUS || tok == TOK_MINUS) {
        bool convert =
            (n == 0) || (is_op(pt) && (!is_unary(pt) || is_right_assoc(pt)));
        if (convert && tok == TOK_PLUS) tok = TOK_UPLUS;
        if (convert && tok == TOK_MINUS) tok = TOK_UMINUS;
      }
      ops[nops++] = n;
      stk[n++] = mkval(T_ERR, tok);  // Convert op into value and store
      if (!is_unary(tok)) nuops++;   // Count non-unary ops
      // For function calls, store arguments - but don't evaluate just yet
      if (tok == TOK_CALL) {
        stk[n++] = js_call_params(js);
        if (is_err(stk[n - 1])) return stk[n - 1];
      }
    } else {
      // clang-format off
      switch (tok) {
        case TOK_IDENTIFIER:
          // Root level identifiers we lookup and push property: "a" -> lookup("a")
          // Identifiers after dot we push as string tokens: "a.b" -> "b"
          // mkcoderef() returns jsval_t that references an (offset,length)
          // inside the parsed code.
          stk[n] = js->flags & F_NOEXEC ? 0:
                    n > 0 && is_op(vdata(stk[n - 1]) & 255) && vdata(stk[n - 1]) == TOK_DOT
                   ? mkcoderef((jsoff_t) js->toff, (jsoff_t) js->tlen)
                   : lookup(js, js->code + js->toff, js->tlen);
          n++; break;
        case TOK_NUMBER:  stk[n++] = js->tval; break;
        case TOK_LBRACE:  stk[n++] = js_obj_literal(js); break;
        case TOK_STRING:  stk[n++] = js_str_literal(js); break;
        case TOK_FUNC:    stk[n++] = js_func_literal(js); break;
        case TOK_NULL:    stk[n++] = mkval(T_NULL, 0); break;
        case TOK_UNDEF:   stk[n++] = mkval(T_UNDEF, 0); break;
        case TOK_TRUE:    stk[n++] = mkval(T_BOOL, 1); break;
        case TOK_FALSE:   stk[n++] = mkval(T_BOOL, 0); break;
        case TOK_LPAREN:  stk[n++] = js_expr(js, TOK_RPAREN, TOK_EOF); break;
        default:          return js_err(js, "unexpected token '%.*s'", (int) js->tlen, js->code + js->toff);
      }
      // clang-format on
    }
    if (!is_op(tok) && is_err(stk[n - 1])) return stk[n - 1];
    pt = tok;
  }
  // printf("EE toks=%d ops=%d binary=%d\n", n, nops, nuops);
  if (js->flags & F_NOEXEC) return mkval(T_UNDEF, n);  // pass n to the caller
  if (n == 0) return mkval(T_UNDEF, 0);
  if (n != nops + nuops + 1) return js_err(js, "bad expr");
  sortops(ops, nops, stk);  // Sort operations by priority
  uint32_t mask = 0;
  // uint8_t nq = 0;  // Number of `?` ternary operations
  for (int i = 0; i < nops; i++) {
    uint8_t idx = ops[i], op = vdata(stk[idx]) & 255, ri = idx;
    bool unary = is_unary(op), rassoc = is_right_assoc(op);
    bool needleft = unary && rassoc ? false : true;
    bool needright = unary && !rassoc ? false : true;
    jsval_t left = mkval(T_UNDEF, 0), right = mkval(T_UNDEF, 0);
    mask |= 1 << idx;
    // printf("  OP: %d idx %d %d%d\n", op, idx, needleft, needright);
    if (needleft) {
      if (idx < 1) return js_err(js, "bad expr");
      mask |= 1 << (idx - 1);
      ri = getri(mask, idx);
      left = stk[ri];
      if (is_err(left)) return js_err(js, "bad expr");
    }
    if (needright) {
      mask |= 1 << (idx + 1);
      if (idx + 1 >= n) return js_err(js, "bad expr");
      right = stk[idx + 1];
      if (is_err(right)) return js_err(js, "bad expr");
    }
    stk[ri] = do_op(js, op, left, right);       // Perform operation
    if (is_err(stk[ri])) return stk[ri];        // Propagate error
  }
  return stk[0];
}

static jsval_t js_let(struct js *js) {
  uint8_t exe = !(js->flags & F_NOEXEC);
  for (;;) {
    uint8_t tok = nexttok(js);
    if (tok != TOK_IDENTIFIER) return js_err(js, "parse error");
    jsoff_t noff = js->toff, nlen = js->tlen;
    char *name = (char *) &js->code[noff];
    jsval_t v = mkval(T_UNDEF, 0);
    nexttok(js);
    if (js->tok == TOK_ASSIGN) {
      v = js_expr(js, TOK_COMMA, TOK_SEMICOLON);
      if (is_err(v)) break;  // Propagate error if any
    }
    if (exe) {
      if (lkp(js, js->scope, name, nlen) > 0)
        return js_err(js, "'%.*s' already declared", (int) nlen, name);
      jsval_t x =
          setprop(js, js->scope, mkstr(js, name, nlen), resolveprop(js, v));
      if (is_err(x)) return x;
    }
    if (js->tok == TOK_SEMICOLON || js->tok == TOK_EOF) break;  // Stop
    if (js->tok != TOK_COMMA) return js_err(js, "parse error");
  }
  return mkval(T_UNDEF, 0);
}

static jsval_t js_block_or_stmt(struct js *js) {
  js->pos = skiptonext(js->code, js->clen, js->pos);
  if (js->pos < js->clen && js->code[js->pos] == '{') {
    js->pos++;
    return js_block(js, !(js->flags & F_NOEXEC));
  } else {
    return resolveprop(js, js_stmt(js, TOK_SEMICOLON));
  }
}

static jsval_t js_if(struct js *js) {
  if (nexttok(js) != TOK_LPAREN) return js_err(js, "parse error");
  jsval_t cond = js_expr(js, TOK_RPAREN, TOK_EOF);
  if (js->tok != TOK_RPAREN) return js_err(js, "parse error");
  bool noexec = js->flags & F_NOEXEC;
  bool cond_true = js_truthy(js, cond);
  if (!cond_true) js->flags |= F_NOEXEC;
  jsval_t res = js_block_or_stmt(js);
  if (!cond_true && !noexec) js->flags &= ~F_NOEXEC;
  if (lookahead(js) == TOK_ELSE) {
    nexttok(js);
    if (cond_true) js->flags |= F_NOEXEC;
    res = js_block_or_stmt(js);
    if (cond_true && !noexec) js->flags &= ~F_NOEXEC;
  }
  // printf("IF: else %d\n", lookahead(js) == TOK_ELSE);
  return res;
}

static jsval_t js_while(struct js *js) {
  jsoff_t pos = js->pos - js->tlen;  // The beginning of `while` stmt
  if (nexttok(js) != TOK_LPAREN) return js_err(js, "parse error");
  jsval_t cond = js_expr(js, TOK_RPAREN, TOK_EOF);
  if (js->tok != TOK_RPAREN) return js_err(js, "parse error");
  uint8_t flags = js->flags, exe = !(js->flags & F_NOEXEC);
  bool cond_true = js_truthy(js, cond);
  if (exe) js->flags |= F_LOOP | (cond_true ? 0 : F_NOEXEC);
  jsval_t res = js_block_or_stmt(js);
  // printf("WHILE 2 %d %d\n", cond_true, js->flags);
  bool repeat = exe && !is_err(res) && cond_true && !(js->flags & F_BREAK);
  js->flags = flags;          // Restore flags
  if (repeat) js->pos = pos;  // Must loop. Jump back!
  // printf("WHILE %d\n", js_usage(js));
  return mkval(T_UNDEF, 0);
}

static jsval_t js_break(struct js *js) {
  if (!(js->flags & F_LOOP)) return js_err(js, "not in loop");
  if (!(js->flags & F_NOEXEC)) js->flags |= F_BREAK | F_NOEXEC;
  return mkval(T_UNDEF, 0);
}

static jsval_t js_continue(struct js *js) {
  if (!(js->flags & F_LOOP)) return js_err(js, "not in loop");
  js->flags |= F_NOEXEC;
  return mkval(T_UNDEF, 0);
}

static jsval_t js_return(struct js *js) {
  uint8_t exe = !(js->flags & F_NOEXEC);
  // jsoff_t pos = js->pos;
  // printf("RET..\n");
  if (exe && !(js->flags & F_CALL)) return js_err(js, "not in func");
  if (nexttok(js) == TOK_SEMICOLON) return mkval(T_UNDEF, 0);
  js->pos -= js->tlen;  // Go back
  jsval_t result = js_expr(js, TOK_SEMICOLON, TOK_SEMICOLON);
  if (exe) {
    js->pos = js->clen;     // Shift to the end - exit the code snippet
    js->flags |= F_RETURN;  // Tell caller we've executed
  }
  // printf("RR %d [%.*s]\n", js->tok, js->pos - pos, &js->code[pos]);
  return resolveprop(js, result);
}

static jsval_t js_stmt(struct js *js, uint8_t etok) {
  jsval_t res;
  if (js->lev == 0) js_gc(js);  // Before top-level stmt, garbage collect
  js->lev++;
  // clang-format off
  switch (nexttok(js)) {
    case TOK_CASE: case TOK_CATCH: case TOK_CLASS: case TOK_CONST:
    case TOK_DEFAULT: case TOK_DELETE: case TOK_DO: case TOK_FINALLY:
    case TOK_FOR: case TOK_IN: case TOK_INSTANCEOF: case TOK_NEW:
    case TOK_SWITCH: case  TOK_THIS: case TOK_THROW: case TOK_TRY:
    case TOK_VAR: case TOK_VOID: case TOK_WITH: case TOK_YIELD:
      res = js_err(js, "'%.*s' not implemented", (int) js->tlen, js->code + js->toff);
      break;
    case TOK_CONTINUE:  res = js_continue(js); break;
    case TOK_BREAK:     res = js_break(js); break;
    case TOK_LET:       res = js_let(js); break;
    case TOK_IF:        res = js_if(js); break;
    case TOK_LBRACE:    res = js_block(js, !(js->flags & F_NOEXEC)); break;
    case TOK_WHILE:     res = js_while(js); break;
    case TOK_RETURN:    res = js_return(js); break;
    default:
      js->pos -= js->tlen; // Unparse last parsed token
      res = resolveprop(js, js_expr(js, etok, TOK_SEMICOLON));
      break;
  }
  //clang-format on
  js->lev--;
  return res;
}
// clang-format on

struct js *js_create(void *buf, size_t len) {
  struct js *js = NULL;
  if (len < sizeof(*js) + esize(T_OBJ)) return js;
  memset(buf, 0, len);                      // Important!
  js = (struct js *) buf;                   // struct js lives at the beginning
  js->mem = (uint8_t *) (js + 1);           // Then goes memory for JS data
  js->size = (jsoff_t)(len - sizeof(*js));  // JS memory size
  js->scope = mkobj(js, 0);                 // Create global scope
  return js;
}

jsval_t js_mkobj(struct js *js) {
  return mkobj(js, 0);
}

jsval_t js_glob(struct js *js) {
  (void) js;
  return mkval(T_OBJ, 0);
}

void js_set(struct js *js, jsval_t obj, const char *key, jsval_t val) {
  is_err(setprop(js, obj, mkstr(js, key, strlen(key)), val));
}

int js_usage(struct js *js) {
  return js->brk * 100 / js->size;
}

jsval_t js_import(struct js *js, uintptr_t fn, const char *signature) {
  char buf[64];
  size_t n = snprintf(buf, sizeof(buf), "%s@%" PRIxPTR, signature, fn);
  jsval_t str = mkstr(js, buf, n);
  return mkval(T_FUNC, vdata(str));
}

jsval_t js_eval(struct js *js, const char *buf, size_t len) {
  // printf("EVAL: [%.*s]\n", (int) len, buf);
  if (len == (size_t) ~0) len = strlen(buf);
  return js_eval_nogc(js, buf, (jsoff_t) len);
}

#ifdef JS_DUMP
void js_dump(struct js *js) {
  jsoff_t off = 0, v;
  printf("JS size %u, brk %u, callbacks: %u\n", js->size, js->brk, js->ncbs);
  while (off < js->brk) {
    memcpy(&v, &js->mem[off], sizeof(v));
    printf(" %5u: ", off);
    if ((v & 3) == T_OBJ) {
      printf("OBJ %u %u\n", v & ~3, loadoff(js, off + sizeof(off)));
    } else if ((v & 3) == T_PROP) {
      jsoff_t koff = loadoff(js, off + sizeof(v));
      jsval_t val = loadval(js, off + sizeof(v) + sizeof(v));
      printf("PROP next %u, koff %u vtype %d vdata %lu\n", v & ~3, koff,
             vtype(val), vdata(val));
    } else if ((v & 3) == T_STR) {
      jsoff_t len = offtolen(v);
      printf("STR %u [%.*s]\n", len, (int) len, js->mem + off + sizeof(v));
    } else {
      printf("???\n");
      break;
    }
    off += esize(v);
  }

  for (jsoff_t i = 0; i < js->ncbs; i++) {
    jsoff_t base = js->size + i * 3 * sizeof(jsoff_t) + sizeof(jsoff_t);
    jsoff_t o1 = loadoff(js, base), o2 = loadoff(js, base + sizeof(o1));
    printf("FFICB %u %u\n", o1, o2);
  }
}
#endif
