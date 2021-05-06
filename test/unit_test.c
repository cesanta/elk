#define JS_DUMP
#include "../elk.c"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool ev(struct js *js, const char *expr, const char *expectation) {
  const char *result = js_str(js, js_eval(js, expr, strlen(expr)));
#ifdef VERBOSE
  printf("[%s] -> [%s] [%s]\n", expr, result, expectation);
#endif
  return strcmp(result, expectation) == 0;
}

static void test_arith(void) {
  char mem[200];
  struct js *js;
  assert((js = js_create(NULL, 0)) == NULL);
  assert((js = js_create(mem, 0)) == NULL);
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "", "undefined"));
  assert(ev(js, "1.23", "1.23"));
  assert(ev(js, "3 + 4", "7"));
  assert(ev(js, " + 1", "1"));
  assert(ev(js, "+ + 1", "1"));
  assert(ev(js, "+ + + 1", "1"));
  assert(ev(js, "1 + + + 1", "2"));
  assert(ev(js, "-1.23", "-1.23"));
  assert(ev(js, "1/2/4", "0.125"));
  assert(ev(js, "1.23 + 2.1 * 3.7 - 2.5", "6.5"));
  assert(ev(js, "2 * (3 + 4)", "14"));
  assert(ev(js, "2 * (3 + 4 * (2 +5))", "62"));
  assert(ev(js, "5.5 % 2", "1.5"));
  assert(ev(js, "5%2", "1"));
  assert(ev(js, "5 % - 2", "1"));
  assert(ev(js, "-5 % 2", "-1"));
  assert(ev(js, "- 5 % 2", "-1"));
  assert(ev(js, " - 5 % - 2", "-1"));
  assert(ev(js, "24 / 3 % 2", "0"));
  assert(ev(js, "4 / 5 % 3", "0.8"));
  assert(ev(js, "1 + 4 / 5 % 3", "1.8"));
  assert(ev(js, "7^9", "14"));
  assert(ev(js, "1+2*3+4*5+6", "33"));
  assert(ev(js, "1+2*3+4/5+6", "13.8"));
  assert(ev(js, "1+2*3+4/5%3+6", "13.8"));
  assert(ev(js, "1 - - - 2", "-1"));
  assert(ev(js, "1 + + + 2", "3"));
  assert(ev(js, "~5", "-6"));
  assert(ev(js, "6 / - - 2", "3"));
  assert(ev(js, "7+~5", "1"));
  assert(ev(js, "5/3", "1.66667"));
  assert(ev(js, "0x64", "100"));
#ifndef JS32
  assert(ev(js, "0x7fffffff", "2147483647"));
  assert(ev(js, "0xffffffff", "4294967295"));
#endif
  assert(ev(js, "100 << 3", "800"));
  assert(ev(js, "(0-14) >> 2", "-4"));
  assert(ev(js, "6 & 3", "2"));
  assert(ev(js, "6 | 3", "7"));
  assert(ev(js, "6 ^ 3", "5"));
  assert(ev(js, "0.1 + 0.2", "0.3"));
  assert(ev(js, "123.4 + 0.1", "123.5"));
  assert(ev(js, "2**3", "8"));
  assert(ev(js, "1.2**3.4", "1.85873"));
}

static void test_errors(void) {
  char mem[200];
  struct js *js;
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "~~~~~~~~~~~~~~~~~~~~~~", "ERROR: expr too deep"));
  assert(ev(js, "+", "ERROR: bad expr"));
  assert(ev(js, "2+", "ERROR: bad expr"));
  assert(ev(js, "2 * * 2", "ERROR: bad expr"));
  assert(ev(js, "1 2", "ERROR: bad expr"));
  assert(ev(js, "1 2 + 3", "ERROR: bad expr"));
  assert(ev(js, "1 + 2 3", "ERROR: bad expr"));
  assert(ev(js, "1 2 + 3 4", "ERROR: bad expr"));
  assert(ev(js, "1 + 2 3 * 5", "ERROR: bad expr"));
  assert(ev(js, "1 + 2 3 * 5 + 6", "ERROR: bad expr"));

  assert(ev(js, "switch", "ERROR: 'switch' not implemented"));
  assert(ev(js, "with", "ERROR: 'with' not implemented"));
  assert(ev(js, "try", "ERROR: 'try' not implemented"));
  assert(ev(js, "class", "ERROR: 'class' not implemented"));
  assert(ev(js, "const x", "ERROR: 'const' not implemented"));
  assert(ev(js, "var x", "ERROR: 'var' not implemented"));

  assert(ev(js, "1 + yield", "ERROR: unexpected token 'yield'"));
  assert(ev(js, "yield", "ERROR: 'yield' not implemented"));
  assert(ev(js, "@", "ERROR: parse error"));
  assert(ev(js, "$", "ERROR: '$' not found"));
  assert(ev(js, "1?2:3", "ERROR: unknown op 130"));
}

static void test_basic(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "null", "null"));
  assert(ev(js, "null", "null"));
  assert(ev(js, "undefined", "undefined"));
  assert(ev(js, "true", "true"));
  assert(ev(js, "false", "false"));
  assert(ev(js, "({})", "{}"));
  assert(ev(js, "({a:1})", "{\"a\":1}"));
  assert(ev(js, "({a:1,b:true})", "{\"b\":true,\"a\":1}"));
  assert(ev(js, "({a:1,b:{c:2}})", "{\"b\":{\"c\":2},\"a\":1}"));
  assert(js->brk < 100);

  assert(ev(js, "1;2", "2"));
  assert(ev(js, "1;2;", "2"));
  assert(ev(js, "let a ;", "undefined"));
  assert(ev(js, "let a,", "ERROR: parse error"));
  assert(ev(js, "let ;", "ERROR: parse error"));
  assert(ev(js, "let a 2", "ERROR: parse error"));
  assert(ev(js, "let a = 123", "undefined"));
  assert(ev(js, "let a = 123;", "ERROR: 'a' already declared"));
  assert(ev(js, "let b = 123; 1; b", "123"));
  assert(ev(js, "let c = 2, d = 3; c", "2"));
  assert(ev(js, "1 = 7", "ERROR: bad lhs"));
  assert(ev(js, "a = 7", "7"));
  assert(ev(js, "a", "7"));
  assert(ev(js, "d = 1+2-3", "0"));
  assert(ev(js, "1 + d = 3", "ERROR: bad lhs"));
  assert(ev(js, "a = {b:2}", "{\"b\":2}"));
  assert(ev(js, "a", "{\"b\":2}"));
  assert(ev(js, "a.b", "2"));
  assert(ev(js, "a.b = {c:3}", "{\"c\":3}"));
  assert(ev(js, "a", "{\"b\":{\"c\":3}}"));
  assert(ev(js, "a.b.c", "3"));
  assert(ev(js, "a.b.c.", "ERROR: bad expr"));
  assert(ev(js, "a=1;1;", "1"));
  assert(ev(js, "a+=1;a;", "2"));
  assert(ev(js, "a-=3;a;", "-1"));
  assert(ev(js, "a*=8;a;", "-8"));
  assert(ev(js, "a/=2;a;", "-4"));
  assert(ev(js, "a%=3;a;", "-1"));
  assert(ev(js, "a^=5;a;", "-6"));
  assert(ev(js, "a>>=2;a;", "-2"));
  assert(ev(js, "a=3;a<<=2;a;", "12"));
  assert(ev(js, "a=b=7", "7"));
  assert(ev(js, "a", "7"));
  assert(ev(js, "a+", "ERROR: bad expr"));
  assert(ev(js, "a++", "7"));
  assert(ev(js, "a", "8"));
  assert(ev(js, "a--; a", "7"));
  assert(ev(js, "b", "7"));
  assert(ev(js, "~null", "ERROR: type mismatch"));
  assert(ev(js, "1 + ''", "ERROR: type mismatch"));
  assert(ev(js, "1 + true", "ERROR: type mismatch"));
  assert(ev(js, "1 === false", "ERROR: type mismatch"));
  assert(ev(js, "1 === 2", "false"));
  assert(ev(js, "13 + 4 === 17", "true"));
  assert(ev(js, "let o = {a: 1}; o.a += 1; o;", "{\"a\":2}"));

  assert(ev(js, "a= 0; 2 * (3 + a++)", "6"));
  assert(ev(js, "a", "1"));
  assert(ev(js, "a = 0; a++", "0"));
  assert(ev(js, "a = 0; a++ - a++", "-1"));
  assert(ev(js, ",", "ERROR: bad expr"));
  assert(ev(js, "a = 0; 1 + a++ + 2", "3"));
  assert(ev(js, "a", "1"));
  assert(ev(js, "a = 0; 3 * (1 + a++ + (2 + a++))", "12"));

  assert(ev(js, "1+2;", "3"));
  assert(ev(js, "1+2; ", "3"));
  assert(ev(js, "1+2;//9", "3"));
  assert(ev(js, "1+2;//", "3"));
  assert(ev(js, "1/**/+2;//9", "3"));
  assert(ev(js, "1/**/+2;/**///9", "3"));
  assert(ev(js, "1/**/+ /* some comment*/2;/**///9", "3"));
  assert(ev(js, "1/**/+ /* */2;/**///9", "3"));
  assert(ev(js, "1/**/+ /* \n*/2;/**///9", "3"));
  assert(ev(js, "1 + /* * */ 2;", "3"));
  assert(ev(js, "1 + /* **/ 2;", "3"));
  assert(ev(js, "1 + /* ///**/ 2;", "3"));
  assert(ev(js, "1 + /*\n//*/ 2;", "3"));
  assert(ev(js, "1 + /*\n//\n*/ 2;", "3"));
}

static void test_memory(void) {
  char mem[sizeof(struct js) + 8];  // Not enough memory to create an object
  struct js *js;
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "({a:1})", "ERROR: oom"));  // OOM
  assert(js_usage(js) > 0);
  js_dump(js);
}

static void test_strings(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "''", "\"\""));
  assert(ev(js, "\"\"", "\"\""));
  assert(ev(js, "'foo'", "\"foo\""));
  assert(ev(js, "'foo\\'", "ERROR: parse error"));
  assert(ev(js, "'foo\\q", "ERROR: parse error"));
  assert(ev(js, "'f\\x", "ERROR: parse error"));
  assert(ev(js, "'f\\xx", "ERROR: parse error"));
  assert(ev(js, "'f\\xxx", "ERROR: parse error"));
  assert(ev(js, "'foo\\q'", "ERROR: bad str literal"));
  assert(ev(js, "'f\\xrr'", "ERROR: bad str literal"));
  assert(ev(js, "'f\\x61'", "\"fa\""));
  assert(ev(js, "'x\\x61\\t\\r\\n\\''", "\"xa\t\r\n'\""));
  assert(ev(js, "'a'+'b'", "\"ab\""));
  assert(ev(js, "'hi'+' ' + 'there'", "\"hi there\""));
  assert(ev(js, "'a' == 'b'", "ERROR: bad expr"));
  assert(ev(js, "'a' === 'b'", "false"));
  assert(ev(js, "'a' !== 'b'", "true"));
  assert(ev(js, "let a = 'b'; a === 'b'", "true"));
  assert(ev(js, "let b = 'c'; b === 'c'", "true"));
  assert(ev(js, "a === b", "false"));
  assert(ev(js, "a = b = 'hi'", "\"hi\""));
  assert(ev(js, "a", "\"hi\""));
  assert(ev(js, "b", "\"hi\""));
  assert(ev(js, "a = b = 1", "1"));
  assert(ev(js, "'x' * 'y'", "ERROR: bad str op"));
}

static void test_flow(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "let a = 1; a", "1"));
  assert(ev(js, "if (true) a++; a", "2"));
  assert(ev(js, "if ('') a--; a", "2"));
  assert(ev(js, "if (1) 2;", "2"));
  assert(ev(js, "if (0) 2;", "undefined"));
  assert(ev(js, "if (0) { a = 7; a++; }", "undefined"));
  assert(ev(js, "a", "2"));
  assert(ev(js, "if (1) {}", "undefined"));
  assert(ev(js, "if ('boo') { a = 7; a++; }", "7"));
  assert(ev(js, "a", "8"));
  assert(ev(js, "for (;;);", "ERROR: 'for' not implemented"));
  assert(ev(js, "break;", "ERROR: not in loop"));
  assert(ev(js, "continue;", "ERROR: not in loop"));
  assert(ev(js, "let b = 0; while (b < 10) {b++; a--;} a;", "-2"));
  assert(ev(js, "b = 0; while (b++ < 10) a += 3;  a;", "28"));
  assert(ev(js, "b = 0; while (true) break; ", "undefined"));
  assert(ev(js, "b = 0; while (true) break;", "undefined"));
  assert(ev(js, "b = 0; while (true) { break; }", "undefined"));
  assert(ev(js, "b = 0; while (true) break; b", "0"));
  assert(ev(js, "b = 0; while (true) if (a-- < 10) break;", "undefined"));
  assert(ev(js, "a", "8"));
  assert(ev(js, "b = 0; while (true) if (b++ > 10) break; b;", "12"));
  assert(ev(js, "a = b = 0; while (b++ < 10) while (a < b) a++; a", "10"));
  assert(ev(js, "a = 0; while (1) { if (a++ < 10) continue; break;} a", "11"));
  assert(ev(js, "a=b=0; while (b++<10) {true;a++;} a", "10"));
  assert(ev(js, "a=b=0; if (false) b++; else b--; b", "-1"));
  assert(ev(js, "a=b=0; if (false) {b++;} else {b--;} b", "-1"));
  assert(ev(js, "a=b=0; if (false) {2;b++;} else {2;b--;} b", "-1"));
  assert(ev(js, "a=b=0; if (true) b++; else b--; b", "1"));
  assert(ev(js, "a=b=0; if (true) {2;b++;} else {2;b--;} b", "1"));
  assert(ev(js, "a=0; if (1) a=1; else if (0) a=2; a;", "1"));
  assert(ev(js, "a=0; if (0) a=1; else if (1) a=2; a;", "2"));
  assert(ev(js, "a=0; if (0){7;a=1;}else if (1){7;a=2;} a;", "2"));
  assert(ev(js, "a=0; if(0){7;a=1;}else if(0){5;a=2;}else{3;a=3;} a;", "3"));
#if 0
  // Ternary operator
  assert(ev(js, "1?2:3", "2"));
  assert(ev(js, "0?2:3", "3"));
  assert(ev(js, "0?1+1:1+2", "3"));
  assert(ev(js, "a=0?1+1:1+2", "3"));
  assert(ev(js, "a", "3"));
  assert(ev(js, "a=b=0; a=b=0?1+1:1+2", "3"));
  assert(ev(js, "a=0?1+1:1+2; a++; a", "4"));
  assert(ev(js, "a=0; 0?a++:a--; a", "-1"));
  assert(ev(js, "a=1?2:0?3:4", "2"));
#endif
}

static void test_scopes(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "let a = 5; { a = 6; let x = 2; } a", "6"));
  assert(ev(js, "let b = 5; { let b = 6; } b", "5"));
  js_gc(js);
  jsoff_t brk = js->brk;
  assert(ev(js, "{ let b = 6; } 1", "1"));
  js_gc(js);
  assert(js->brk == brk);
  assert(ev(js, "{}", "undefined"));
  js_gc(js);
  assert(js->brk == brk);
  assert(ev(js, "{{}}", "undefined"));
  js_gc(js);
  assert(js->brk == brk);
  assert(ev(js, "{ let a = 'hello'; { let a = 'world'; } }", "undefined"));
  js_gc(js);
  assert(js->brk == brk);
}

static void test_funcs(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "function(){};1;", "1"));
  assert(ev(js, "let f=function(){};1;", "1"));
  assert(ev(js, "f;", "function(){}"));
  assert(ev(js, "typeof 1", "\"number\""));
  assert(ev(js, "typeof(1)", "\"number\""));
  assert(ev(js, "typeof('hello')", "\"string\""));
  assert(ev(js, "typeof {}", "\"object\""));
  assert(ev(js, "typeof f", "\"function\""));
  assert(ev(js, "function(,){};", "ERROR: parse error"));
  assert(ev(js, "function(a,){};", "ERROR: parse error"));
  assert(ev(js, "function(a b){};", "ERROR: parse error"));
  assert(ev(js, "function(a,b){};", "function(a,b){}"));
  assert(ev(js, "1 + f", "ERROR: type mismatch"));
  assert(ev(js, "f = function(a){return 17;}; 1", "1"));
  assert(ev(js, "1()", "ERROR: calling non-function"));
  assert(ev(js, "f(,)", "ERROR: parse error"));
  assert(ev(js, "f(1,)", "ERROR: parse error"));
  assert(ev(js, "f(,2)", "ERROR: parse error"));
  assert(ev(js, "return", "ERROR: not in func"));
  assert(ev(js, "return 2;", "ERROR: not in func"));
  assert(ev(js, "{ return } ", "ERROR: not in func"));
  assert(ev(js, "f(3,4)", "17"));
  assert(ev(js, "(function(){})()", "undefined"));
  assert(ev(js, "(function(){})(1,2,3)", "undefined"));
  assert(ev(js, "(function(){1})(1,2,3)", "undefined"));
  assert(ev(js, "(function(){1;})(1,2,3)", "undefined"));
  assert(ev(js, "(function(){return 1;})(1,2,3)", "1"));
  assert(ev(js, "(function(){return 1;})(1)", "1"));
  assert(ev(js, "(function(){return 1;})(1,)", "ERROR: parse error"));
  assert(ev(js, "(function(){return 1;2;})()", "1"));
  assert(ev(js, "(function(){return 1;2;return 3;})()", "1"));
  assert(ev(js, "(function(a,b){return a + b;})()", "ERROR: type mismatch"));
  assert(ev(js, "(function(a,b){return a + b;})(1,2)", "3"));
  assert(ev(js, "(function(a,b){return a + b;})('foo','bar')", "\"foobar\""));
  assert(ev(js, "(function(a,b){return a + b;})(1,2,3,4)", "3"));
  assert(ev(js, "f = function(a,b){return a + b;}; 1", "1"));
  js_gc(js);
  jsoff_t brk = js->brk;
  assert(ev(js, "f(3, 4 )", "7"));
  assert(ev(js, "f(3,4)", "7"));
  assert(ev(js, "f(1+2,4)", "7"));
  assert(ev(js, "f(1+2,f(2,3))", "8"));
  js_gc(js);
  assert(js->brk == brk);
  assert(ev(js, "let a=0; (function(){a++;})(); a", "1"));
  assert(ev(js, "a=0; (function(){ a++; })(); a", "1"));
}

static void test_bool(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "1 && 2", "true"));
  assert(ev(js, "1 && 'x'", "true"));
  assert(ev(js, "1 && ''", "false"));
  assert(ev(js, "1 && false || true", "true"));
  assert(ev(js, "1 && false && true", "false"));
  assert(ev(js, "1 === 2", "false"));
  assert(ev(js, "1 !== 2", "true"));
  assert(ev(js, "1 === true", "ERROR: type mismatch"));
  assert(ev(js, "1 <= 2", "true"));
  assert(ev(js, "1 < 2", "true"));
  assert(ev(js, "2 >= 2", "true"));
}

static void test_gc(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  jsval_t obj = js_mkobj(js);
  js_set(js, js_glob(js), "os", obj);
  js_set(js, obj, "a", mkval(T_BOOL, 0));
  js_set(js, obj, "b", mkval(T_BOOL, 1));
  jsoff_t brk = js->brk;
  js_gc(js);
  assert(js->brk == brk);
}

static int sum1(int a, int b) {
  return a + b;
}
static double sum2(double a, double b) {
  return a + b;
}
static double sum3(int a, double b) {
  return a + b;
}
static const char *fmt(double d) {
  static char buf[50];
  snprintf(buf, sizeof(buf), "n->%g", d);
  return buf;
}
static int op(int (*fp)(int, void *), int a, int b, void *userdata) {
  return fp(a + b, userdata);
}
// Simulate timer callback. Save callback address & param and call later
static void (*s_op2fp)(int, void *);
static void *s_op2fp_param;
static void op2(void (*fp)(int, void *), void *userdata) {
  s_op2fp = fp;
  s_op2fp_param = userdata;
}

static void test_ffi(void) {
  struct js *js;
  char mem[sizeof(*js) + 1500];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  jsval_t obj = js_mkobj(js);
  js_set(js, js_glob(js), "os", obj);
  js_set(js, obj, "bad1", js_import(js, (uintptr_t) 0, "oy"));
  js_set(js, obj, "atoi", js_import(js, (uintptr_t) atoi, "is"));
  js_set(js, obj, "sum1", js_import(js, (uintptr_t) sum1, "iii"));
  js_set(js, obj, "sum2", js_import(js, (uintptr_t) sum2, "ddd"));
  js_set(js, obj, "sum3", js_import(js, (uintptr_t) sum3, "did"));
  js_set(js, obj, "fmt", js_import(js, (uintptr_t) fmt, "sd"));
  js_set(js, obj, "op", js_import(js, (uintptr_t) op, "i[iiu]iiu"));
  js_set(js, obj, "op2", js_import(js, (uintptr_t) op2, "v[viu]u"));
  js_set(js, js_glob(js), "eval", js_import(js, (uintptr_t) js_eval, "jmsi"));
  assert(ev(js, "os.atoi()", "ERROR: bad arg 1"));
  assert(ev(js, "os.bad1(1)", "ERROR: bad sig"));
  assert(ev(js, "os.sum1(1)", "ERROR: bad arg 2"));
  assert(ev(js, "os.sum1(1,'x')", "ERROR: bad arg 2"));
  assert(ev(js, "os.sum1(1,2,3)", "ERROR: num args"));
  assert(ev(js, "os.sum1(1,-3)", "-2"));
  assert(ev(js, "os.sum1(1.2,-2.3)", "-1"));
  assert(ev(js, "os.sum2(1.2,-2.3)", "-1.1"));
  assert(ev(js, "os.sum3(1.2,-2.3)", "-1.3"));
  assert(ev(js, "os.fmt(3.1416)", "\"n->3.1416\""));
  assert(ev(js, "os.atoi('752')", "752"));
  assert(ev(js, "os.op(function(x){return x;}, 12, 5, null)", "17"));
  assert(ev(js, "os.op(function(x){return x*x;}, 2, 3, null)", "25"));
  assert(ev(js, "let a = 3, b = 4; os.sum1(a, b)", "7"));
  assert(ev(js, "let f = function(){return 1;}; 7;", "7"));
  assert(ev(js, "a=b=0; while(a++<1){os.sum1(1,2);b++;};b", "1"));
  assert(ev(js, "a=b=0; while(a++<1){f();f();b++;};b", "1"));
  assert(ev(js, "eval(null, '3+4',3)", "7"));

  // Test that C can trigger JS callback even after GC
  assert(ev(js, "'foo'; 'bar'; 1", "1"));  // This will be GC-ed
  assert(ev(js, "a=0; os.op2(function(x){a=x;},null); 1", "1"));
  jsoff_t brk = js->brk;
  assert(ev(js, "", "undefined"));  // Trigger GC
  assert(js->brk < brk);            // Make sure we've deleted some stuff
  assert(s_op2fp != NULL);
  s_op2fp(992, s_op2fp_param);
  assert(ev(js, "a", "992"));
}

int main(void) {
  clock_t a = clock();
  test_basic();
  test_ffi();
  test_bool();
  test_gc();
  test_funcs();
  test_scopes();
  test_arith();
  test_errors();
  test_memory();
  test_strings();
  test_flow();
  double ms = (double) (clock() - a) * 1000 / CLOCKS_PER_SEC;
  printf("SUCCESS. All tests passed in %g ms\n", ms);
  return EXIT_SUCCESS;
}
