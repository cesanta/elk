#include "../elk.c"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define FAIL(str, line)                                              \
  do {                                                               \
    printf("%s:%d:1 [%s] (in %s)\n", __FILE__, line, str, __func__); \
    return str;                                                      \
  } while (0)

#define ASSERT(expr)                    \
  do {                                  \
    g_num_checks++;                     \
    if (!(expr)) FAIL(#expr, __LINE__); \
  } while (0)

#define RUN_TEST(fn)                                             \
  do {                                                           \
    clock_t started = clock();                                   \
    int n = g_num_checks;                                        \
    const char *msg = fn();                                      \
    double took = (double) (clock() - started) / CLOCKS_PER_SEC; \
    printf("  [%.3f %3d] %s\n", took, g_num_checks - n, #fn);    \
    fflush(stdout);                                              \
    if (msg) return msg;                                         \
  } while (0)

#define CHECK_NUMERIC(x, y) ASSERT(numexpr(js, (x), (y)))

static int g_num_checks;

static bool check_num(struct js *vm, jsval_t v, double expected) {
  // printf("%s: %g %g\n", __func__, tof(v), expected);
  if (JSTYPE(v) == JS_TYPE_ERROR) printf("ERROR: %s\n", js_stringify(vm, v));
  return JSTYPE(v) == JS_TYPE_NUMBER && fabs(js_to_num(v) - expected) < 0.0001;
}

static bool numexpr(struct js *vm, const char *code, double expected) {
  return check_num(vm, js_eval(vm, code, strlen(code)), expected);
}

#if 0
static int check_str(struct js *vm, jsval_t v, const char *expected) {
  jslen_t len;
  const char *p = js_to_str(vm, v, &len);
  int result =
      (js_type(v) == JS_TYPE_STRING && len == (jslen_t) strlen(expected) &&
       memcmp(p, expected, len) == 0);
  if (!result) {
    printf("[%.*s] != [%s]\n", len, p, expected);
  }
  return result;
}

static int strexpr(struct js *vm, const char *code, const char *expected) {
  jsval_t v = js_eval(vm, code, strlen(code));
  // printf("%s: %s\n", __func__, js_stringify(vm, v));
  return js_type(v) != JS_TYPE_STRING ? 0 : check_str(vm, v, expected);
}

static int typeexpr(struct js *vm, const char *code, enum jstype t) {
  jsval_t v = js_eval(vm, code, strlen(code));
  return js_type(v) == t;
}

static const char *test_expr(void) {
  struct js *vm = js_create(1500);

  ASSERT(js_eval(vm, ";;;", -1) == JS_UNDEFINED);
  ASSERT(js_eval(vm, "let a", -1) == JS_UNDEFINED);  // define a var
  ASSERT(js_eval(vm, "let a", -1) == JS_ERROR);      // error: already defined

  ASSERT(numexpr(vm, "let af1 = {x:1}, af2 = 7.1; af3 = true; af2", 7.1f));
  ASSERT(numexpr(vm, "let aa1 = 1, aa2 = 2; aa1 + aa2", 3));
  ASSERT(numexpr(vm, "let ab1 = 1, ab2 = 2; let ab3 = 4; ab1 + ab3", 5));

  ASSERT(typeexpr(vm, "let ax, bx = function(x){}", JS_TYPE_FUNCTION));
  ASSERT(typeexpr(vm, "let ay, by = function(x){}, c", JS_TYPE_UNDEF));

  ASSERT(numexpr(vm, "let aq = 1;", 1.0f));
  ASSERT(numexpr(vm, "let aw = 1, be = 2;", 2.0f));
  ASSERT(numexpr(vm, "123", 123.0f));
  ASSERT(numexpr(vm, "123;", 123.0f));
  ASSERT(numexpr(vm, "{123}", 123.0f));
  ASSERT(numexpr(vm, "1 + 2 * 3.7 - 7 % 3", 7.4f));
  ASSERT(numexpr(vm, "let ag = 1.23, bg = 5.3;", 5.3f));
  ASSERT(numexpr(vm, "ag;", 1.23f));
  ASSERT(numexpr(vm, "ag - 2 * 3.1;", -4.97f));
  ASSERT(numexpr(vm,
                 "let az = 1.23; az + 1; let fz = function(a) "
                 "{ return az + 1; }; 1;",
                 1));
  ASSERT(numexpr(vm, "let at = 9; while (at) at--;", 0.0f));
  ASSERT(numexpr(vm, "let a2 = 9, b2 = 0; while (a2) { a2--; } ", 0.0f));
  ASSERT(numexpr(vm, "let a3 = 9, b3 = 0; while (a3) a3--; b3++; ", 0.0f));
  ASSERT(numexpr(vm, "b3", 1.0f));
  ASSERT(numexpr(vm, "let a4 = 9, b4 = 7; while (a4){a4--;b4++;} b4", 16.0f));

  ASSERT(numexpr(vm, "let q = 1; q++;", 1.0f));
  ASSERT(numexpr(vm, "q;", 2.0f));
  ASSERT(numexpr(vm, "q--;", 2.0f));
  ASSERT(numexpr(vm, "q;", 1.0f));
  ASSERT(numexpr(vm, "q - 7;", -6));

  // printf("--> %s\n", js_stringify(vm, js_eval(vm, "~10", -1)));
  ASSERT(numexpr(vm, "{let a = 200; a += 50; a}", 250));
  ASSERT(numexpr(vm, "{let a = 200; a -= 50; a}", 150));
  ASSERT(numexpr(vm, "{let a = 200; a *= 50; a}", 10000));
  ASSERT(numexpr(vm, "{let a = 200; a /= 50; a}", 4));
  ASSERT(numexpr(vm, "{let a = 200; a %= 21; a}", 11));
  ASSERT(numexpr(vm, "{let a = 100; a <<= 3; a}", 800));
  ASSERT(numexpr(vm, "{let a = 0-14; a >>= 2; a}", -4));
  // ASSERT(numexpr(vm, "{let a = 0-14; a >>>= 2; a}", 1073741820));
  ASSERT(numexpr(vm, "{let a = 6; a &= 3; a}", 2));
  ASSERT(numexpr(vm, "{let a = 6; a |= 3; a}", 7));
  ASSERT(numexpr(vm, "{let a = 6; a ^= 3; a}", 5));

  ASSERT(js_eval(vm, "true", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "!0", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "!1", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "!''", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "!false", -1) == JS_TRUE);
  ASSERT(numexpr(vm, "~10", -11));
  ASSERT(numexpr(vm, "-100", -100));
  ASSERT(numexpr(vm, "+100", 100));

  ASSERT(numexpr(vm, "2 * (3 + 4)", 14));
  ASSERT(numexpr(vm, "2 * (3 + 4 / 2 * 3)", 18));

  CHECK_NUMERIC("false ? 4 : 5;", 5);
  CHECK_NUMERIC("false ? 4 : '' ? 6 : 7;", 7);
  CHECK_NUMERIC("77 ? 4 : '' ? 6 : 7;", 4);

  ASSERT(js_eval(vm, "1 + '1'", -1) == JS_ERROR);
  ASSERT(js_eval(vm, "'1'++", -1) == JS_ERROR);
  ASSERT(js_eval(vm, "'1'--", -1) == JS_ERROR);
  ASSERT(js_eval(vm, "true--", -1) == JS_ERROR);

  // TODO
  // CHECK_NUMERIC("1, 2;", 2);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN", &res));
  // ASSERT_EQ(!!isnan(js_get_double(vm, res)), 1);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === NaN", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN !== NaN", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 1);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === 0", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === 1", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === null", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === undefined", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === ''", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === {}", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "NaN === []", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "isNaN(NaN)", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 1);

  // ASSERT_EXEC_OK(js_exec(vm, "isNaN(NaN * 10)", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 1);

  // ASSERT_EXEC_OK(js_exec(vm, "isNaN(0)", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  // ASSERT_EXEC_OK(js_exec(vm, "isNaN('')", &res));
  // ASSERT_EQ(js_get_bool(vm, res), 0);

  js_destroy(vm);
  return NULL;
}

#define CHECK_STR(vm, expr, ebrk, str) \
  do {                                 \
    jsval_t v = js_eval(vm, expr, -1); \
    ASSERT(check_str(vm, v, str));     \
    if (ebrk) ASSERT(ebrk == vm->brk); \
    js_gc(vm, v);                      \
    ASSERT(vm->brk == 0);              \
  } while (0)

static const char *test_strings(void) {
  struct js *vm = js_create(1000);

  CHECK_STR(vm, "'a'", 4, "a");
  CHECK_STR(vm, "'b'", 4, "b");
  ASSERT(numexpr(vm, "1", 1.0f));
  // ASSERT(numexpr(vm, "{let a = 1;}", 1.0f));
  // ASSERT(vm->brk == 0);
  // ASSERT(numexpr(vm, "{let a = 'abc';} 1;", 1.0f));
  // ASSERT(vm->brk == 0);
  // ASSERT(strexpr(vm, "'a' + 'b'", "ab"));
  CHECK_STR(vm, "'a' + 'b'", 5, "ab");
  CHECK_STR(vm, "'vb'", 5, "vb");
  CHECK_STR(vm, "'a' + 'b' + 'c'", 6, "abc");

  CHECK_STR(vm, "'a' + 'b' + 'c' + 'd' + 'e' + 'f'", 9, "abcdef");
  CHECK_STR(vm, "'a' + 'b' + 'c' + 'd' + 'e' + 'f' + 'g' + 'h' + 'i'", 12,
            "abcdefghi");

  // Make sure strings are GC-ed
  CHECK_NUMERIC("1;", 1);
  ASSERT(vm->brk == 0);

  // ASSERT(strexpr(vm, "let a, b = function(x){}, c = 'aa'", "aa"));
  // ASSERT(strexpr(vm, "let a2, b2 = function(){}, cv = 'aa'", "aa"));
  // CHECK_NUMERIC("'abc'.length", 3);
  // CHECK_NUMERIC("('abc' + 'xy').length", 5);
  // CHECK_NUMERIC("'ы'.length", 2);
  // CHECK_NUMERIC("('ы').length", 2);
  js_destroy(vm);
  return NULL;
}

static const char *test_scopes(void) {
  struct js *vm = js_create(1000);
#if 0
  ASSERT(numexpr(vm, "1.23", 1.23f));
  ASSERT(vm->csp == 1);
  ASSERT(vm->objs[0].flags & OBJ_ALLOCATED);
  ASSERT(!(vm->objs[1].flags & OBJ_ALLOCATED));
  ASSERT(!(vm->props[0].flags & PROP_ALLOCATED));
  ASSERT(numexpr(vm, "{let a = 1.23;}", 1.23f));
  ASSERT(!(vm->objs[1].flags & OBJ_ALLOCATED));
  ASSERT(!(vm->props[0].flags & PROP_ALLOCATED));
  CHECK_NUMERIC("if (1) 2", 2);
  ASSERT(js_eval(vm, "if (0) 2;", -1) == JS_UNDEFINED);
  CHECK_NUMERIC("{let a = 42; }", 42);
  CHECK_NUMERIC("let a = 1, b = 2; { let a = 3; b += a; } b;", 5);
  ASSERT(js_eval(vm, "{}", -1) == JS_UNDEFINED);
#endif
  js_destroy(vm);
  return NULL;
}

static const char *test_if(void) {
  struct js *vm = js_create(1000);
  // printf("---> %s\n", js_stringify(vm, js_eval(vm, "if (true) 1", -1)));
  ASSERT(numexpr(vm, "if (true) 1;", 1.0f));
  ASSERT(js_eval(vm, "if (0) 1;", -1) == JS_UNDEFINED);
  ASSERT(js_eval(vm, "true", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "false", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "null", -1) == JS_NULL);
  ASSERT(js_eval(vm, "undefined", -1) == JS_UNDEFINED);
  CHECK_NUMERIC("if (1) {2;}", 2);
  CHECK_NUMERIC("let a = 0, b = 3; if (a === 0) b = 7; b;", 7);
  // CHECK_NUMERIC("a = 0, b = 1; if (a === 0) b = 2; else b = 3", 2); // TODO
  js_destroy(vm);
  return NULL;
}

static const char *test_function(void) {
  struct js *vm = js_create(1500);
  unsigned len;
  ASSERT(js_eval(vm, "let a = function(x){ return; }; a();", -1) ==
         JS_UNDEFINED);
  CHECK_NUMERIC("let f = function(){ 1; }; 1;", 1);
  CHECK_NUMERIC("let fx = function(a){ return a; }; 1;", 1);
  CHECK_NUMERIC("let fy = function(a){ return a; }; fy(5);", 5);
  // TODO
  // CHECK_NUMERIC("(function(a){ return a; })(5);", 5);
  CHECK_NUMERIC("let f1 = function(a){ 1; }; 1;", 1);
  CHECK_NUMERIC("let f2 = function(a,b){ 1; }; 1;", 1);
  CHECK_NUMERIC("let f3 = function(a,b){ return a; }; f3(7,2);", 7);
  CHECK_NUMERIC("let f4 = function(a,b){ return b; }; f4(1,2);", 2);
  CHECK_NUMERIC("let f5 = function(a,b){ return b; }; f5(1,2);", 2);
  // TODO
  // ASSERT(js_eval(vm, "(function(a,b){return b;})(1);", -1) ==
  // JS_UNDEFINED);
  ASSERT(strexpr(vm, "let f6 = function(x){return typeof(x);}; f6(f5);",
                 "function"));

  // Test that the function's string args get garbage collected
  js_eval(vm, "let f7 = function(s){return s.length;};", -1);
  len = vm->brk;
  CHECK_NUMERIC("f7('abc')", 3);
  ASSERT(vm->brk == len);

  // Test that the function's function args get garbage collected
  js_eval(vm, "let f8 = function(s){return s()};", -1);
  len = vm->brk;
  CHECK_NUMERIC("f8(function(){return 3;})", 3);
  ASSERT(vm->brk == len);

  js_destroy(vm);
  return NULL;
}

static const char *test_objects(void) {
  struct js *vm = js_create(1000);
  ASSERT(typeexpr(vm, "let o = {}; o", JS_TYPE_OBJECT));
  ASSERT(typeexpr(vm, "let o2 = {a:1}; o2", JS_TYPE_OBJECT));
  ASSERT(js_eval(vm, "let o3 = {}; o3.b", -1) == JS_UNDEFINED);
  CHECK_NUMERIC("let o4 = {a:1,b:2}; o4.a", 1);
  CHECK_NUMERIC("o = {a:2}; o.a;", 2);
  CHECK_NUMERIC("o.a = 3;", 3);
  // CHECK_NUMERIC("o.a = 4; o.a;", 4);
  ASSERT(js_eval(vm, "delete o.a;", -1) == JS_ERROR);
  js_destroy(vm);
  return NULL;
}

static void jslog(const char *s) {
  // printf("%s\n", s);
  (void) s;
}

static bool ffi_bb(bool arg) {
  return !arg;
}

static int ffi_iii(int a, int b) {
  // printf("%s --> %d %d\n", __func__, a, b);
  return a + b;
}

static int ffi_ii(int a) {
  return a * 2;
}

static int ffi_i(void) {
  return 42;
}

static bool ffi_bd(double x) {
  // printf("%s --> %lf\n", __func__, x);
  return x > 3.14;
}

static double ffi_dii(int a, int b) {
  // printf("%s --> %d %d\n", __func__, a, b);
  return (double) a / (double) b;
}

static double ffi_d(void) {
  return 3.1415926f;
}

static double ffi_ddd(double a, double b) {
  // printf("%s %g %g\n", __func__, a, b);
  return a * b;
}

static double ffi_ddi(double a, int b) {
  return a * b;
}

static double ffi_did(int b, double a) {
  return a * b;
}

static double ffi_dd(double a) {
  // printf("%s %g \n", __func__, a);
  return a * 3.14;
}

static int ffi_idi(double a, int b) {
  return (int) (a / b);
}

static int ffi_idd(double a, double b) {
  return (int) (a * b);
}

static int ffi_iid(int a, double b) {
  return (int) (a * b);
}

static char *fmt(const char *fmt, double f) {  // Format float value
  static char buf[20];
  snprintf(buf, sizeof(buf), fmt, f);
  // printf("%s %g [%s]\n", __func__, f, buf);
  return buf;
}

static int ffi_cb1(int (*cb)(int, int, void *), void *arg) {
  // printf("calling %p, arg %p\n", cb, arg);
  return cb(2, 3, arg);
}

#if 0
static bool fbiiiii(int n1, int n2, int n3, int n4, int n5) {
  return n1 + n2 + n3 + n4 + n5;
}
static bool fb(void) {
  return true;
}
#endif

struct foo {
  int n;
  unsigned char x;
  char *data;
  int len;
};

static int gi(void *base, int offset) {
  return *(int *) ((char *) base + offset);
}

static void *gp(void *base, int offset) {
  return *(void **) ((char *) base + offset);
}

static int gu8(void *base, int offset) {
  // printf("%s --> %p %d\n", __func__, base, offset);
  return *((unsigned char *) base + offset);
}

static int ffi_cb2(int (*cb)(struct foo *, void *), void *arg) {
  struct foo foo = {1, 4, (char *) "some data", 4};
  // printf("%s --> %p\n", __func__, foo.data);
  return cb(&foo, arg);
}

static int ffi_iiiiiii(int a, int b, int c, int d, int e, int f) {
  return a + b + c + d + e + f;
}

static int ffi_iiid(int a, int b, double c) {
  return (int) (a + b + c);
}

static const char *test_ffi(void) {
  struct js *vm = js_create(2000);

  js_import(vm, js_stringify, "smj");

  js_import(vm, ffi_iii, "iii");
  CHECK_NUMERIC("ffi_iii(1,2)", 3);
  CHECK_NUMERIC("ffi_iii(19,-7)", 12);
  CHECK_NUMERIC("ffi_iii(2,-3)", -1);

  js_import(vm, ffi_ii, "ii");
  CHECK_NUMERIC("ffi_ii(5)", 10);

  js_import(vm, ffi_i, "i");
  CHECK_NUMERIC("ffi_i()", 42);
  ASSERT(js_eval(vm, "ffi_i(1)", -1) == JS_ERROR);

  js_import(vm, memcmp, "ik");
  ASSERT(js_eval(vm, "memcmp()", -1) == JS_ERROR);

  js_import(vm, memcpy, "ki");
  ASSERT(js_eval(vm, "memcpy()", -1) == JS_ERROR);

  js_import(vm, ffi_bb, "bb");
  CHECK_NUMERIC("ffi_bb(true) ? 2 : 3;", 3);
  CHECK_NUMERIC("ffi_bb(false) ? 2 : 3;", 2);

  js_import(vm, ffi_bd, "bd");
  ASSERT(js_eval(vm, "ffi_bd(3.15);", -1) == JS_TRUE);
  // TODO
  // ASSERT(js_eval(vm, "ffi_bd(3.13);", -1) == JS_FALSE);

  js_import(vm, ffi_dii, "dii");
  CHECK_NUMERIC("ffi_dii(2, 3)", 0.66667);

  js_import(vm, jslog, "vs");
  ASSERT(js_eval(vm, "jslog('ffi js/c ok');", -1) == JS_UNDEFINED);

  js_import(vm, ffi_d, "d");
  CHECK_NUMERIC("ffi_d() * 2;", 6.2831852);

  js_import(vm, ffi_ddd, "ddd");
  CHECK_NUMERIC("ffi_ddd(1.323, 7.321)", 9.685683);

  js_import(vm, ffi_ddi, "ddi");
  CHECK_NUMERIC("ffi_ddi(1.323, 2)", 2.646);

  js_import(vm, ffi_did, "did");
  CHECK_NUMERIC("ffi_did(2, 1.323)", 2.646);

  js_import(vm, ffi_dd, "dd");
  CHECK_NUMERIC("ffi_dd(1.323)", 4.15422);

  js_import(vm, ffi_idi, "idi");
  CHECK_NUMERIC("ffi_idi(4.15, 4)", 1);

  js_import(vm, ffi_idd, "idd");
  CHECK_NUMERIC("ffi_idd(4.76, 8.92)", 42);

  js_import(vm, ffi_iid, "iid");
  CHECK_NUMERIC("ffi_iid(3, 3.14)", 9);

  js_import(vm, fmt, "ssd");
  ASSERT(strexpr(vm, "fmt('%.2f', ffi_d());", "3.14"));

  js_import(vm, gi, "ipi");
  js_import(vm, gu8, "ipi");
  js_import(vm, gp, "ppi");
  js_import(vm, ffi_cb2, "i[ipu]u");
  CHECK_NUMERIC(
      "ffi_cb2(function(a,b){"
      "let p = gp(a,0); return gi(a,0) + gu8(a,4);},0);",
      5);
  CHECK_NUMERIC(
      "ffi_cb2(function(a){let x = gp(a,8); "
      "return gi(a,0) + gu8(a,4) + gu8(x, 0); },0)",
      120);

  js_import(vm, ffi_iiiiiii, "iiiiiii");
  CHECK_NUMERIC("ffi_iiiiiii(1,-2,3,-4,5,-6);", -3);

  js_import(vm, ffi_iiid, "iiid");
  ASSERT(js_eval(vm, "ffi_iiid(1,2,3);", -1) == JS_ERROR);

  js_import(vm, ffi_cb1, "i[iiiu]u");
  ASSERT(numexpr(vm, "ffi_cb1(function(a,b,c){return a+b;}, 123);", 5));

  js_import(vm, strlen, "is");
  ASSERT(numexpr(vm, "strlen('abc')", 3));

  js_destroy(vm);
  return NULL;
}

static const char *test_subscript(void) {
  struct js *vm = js_create(1000);
  ASSERT(js_eval(vm, "123[0]", -1) == JS_ERROR);
  ASSERT(js_eval(vm, "'abc'[-1]", -1) == JS_UNDEFINED);
  ASSERT(js_eval(vm, "'abc'[3]", -1) == JS_UNDEFINED);
  ASSERT(strexpr(vm, "'abc'[0]", "a"));
  ASSERT(strexpr(vm, "'abc'[1]", "b"));
  ASSERT(strexpr(vm, "'abc'[2]", "c"));
  js_destroy(vm);
  return NULL;
}

static const char *test_stringify(void) {
  struct js *vm = js_create(1500);
  const char *expected;
  js_import(vm, js_stringify, "smj");
  expected = "{\"a\":1,\"b\":3.14}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:1,b:3.14});", expected));
  expected = "{\"a\":true,\"b\":false}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:true,b:false});", expected));
  expected = "{\"a\":\"function(){}\"}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:function(){}});", expected));
  expected = "{\"a\":cfunc}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:js_stringify});", expected));
  expected = "{\"a\":null}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:null});", expected));
  expected = "{\"a\":undefined}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:undefined});", expected));
  expected = "{\"a\":\"b\"}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:'b'});", expected));
  expected = "{\"a\":1,\"b\":{}}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:1,b:{}});", expected));
  expected = "{\"a\":1,\"b\":{\"c\":2}}";
  ASSERT(strexpr(vm, "js_stringify(0,{a:1,b:{c:2}});", expected));
  js_destroy(vm);
  return NULL;
}

static const char *test_comparison(void) {
  struct js *vm = js_create(500);
  ASSERT(js_eval(vm, "1 === 1", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "1 !== 1", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "1 !== 2", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "'a' === 'a'", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "false === false", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "true === true", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "true !== true", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "1 === 'a'", -1) == JS_FALSE);
  // ASSERT(js_eval(vm, "1 === {}", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "1 < 2", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "2 < 2", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "2 <= 2", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "2 > 1", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "2 > 2", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "2 >= 2", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "'a' < 'b'", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "'aa' < 'b'", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "'a' < 'a'", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "'a' <= 'a'", -1) == JS_TRUE);

  ASSERT(js_eval(vm, "null === null", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "undefined === undefined", -1) == JS_TRUE);
  // ASSERT(js_eval(vm, "undefined === {}", -1) == JS_FALSE);
  ASSERT(js_eval(vm, "undefined === null", -1) == JS_FALSE);

  ASSERT(js_eval(vm, "true > false", -1) == JS_TRUE);
  ASSERT(js_eval(vm, "2 > '2'", -1) == JS_TRUE);  // TODO
  js_destroy(vm);
  return NULL;
}

static const char *test_typeof(void) {
  struct js *vm = js_create(500);
  ASSERT(strexpr(vm, "typeof 1", "number"));
  ASSERT(strexpr(vm, "typeof(2)", "number"));
  ASSERT(strexpr(vm, "typeof('aa')", "string"));
  ASSERT(strexpr(vm, "typeof true", "boolean"));
  ASSERT(strexpr(vm, "typeof false", "boolean"));
  // ASSERT(strexpr(vm, "typeof(bx)", "function"));
  js_destroy(vm);
  return NULL;
}

static const char *test_gc(void) {
  struct js *vm = js_create(500);
  jsval_t v2, v1 = js_eval(vm, "'a'", -1);  // Allocate ID 1
  // printf("%x %x\n", vm->brk, VAL_PAYLOAD(v1));
  ASSERT(VAL_PAYLOAD(v1) == 0 << 1);
  v2 = js_eval(vm, "'b'", -1);  // Allocate ID 2
  // printf("%x %x\n", vm->brk, VAL_PAYLOAD(v2));
  ASSERT(VAL_PAYLOAD(v2) == 1 << 1);
  js_gc(vm, v2);                  // Free ID 1
  v2 = js_eval(vm, "'123'", -1);  // Make sure we use it again
  // printf("%hhu %hhu %x\n", vm->mem[0], vm->mem[1], VAL_PAYLOAD(v2));
  ASSERT(VAL_PAYLOAD(v2) == 1 << 1);
  js_gc(vm, v1);                    // Free ID 0
  v1 = js_eval(vm, "'hello'", -1);  // Reuse it
  ASSERT(VAL_PAYLOAD(v1) == 0 << 1);
  ASSERT(vm->mem[vm->size] == 3);  // Two IDs allocated
  js_gc(vm, v1);
  js_gc(vm, v2);
  ASSERT(vm->mem[vm->size] == 0);  // All deallocated, all IDs are free

  ASSERT(vm->brk == 0);
  PUTINT(vm, 127);
  ASSERT(vm->brk == 1);
  PUTINT(vm, 128);
  ASSERT(vm->brk == 3);
  PUTINT(vm, 16384);
  ASSERT(vm->brk == 6);
  {
    unsigned i = 0, x = 123456;
    GETINT(vm, i, x);
    ASSERT(i == 1 && x == 127);
    GETINT(vm, i, x);
    ASSERT(i == 3 && x == 128);
    GETINT(vm, i, x);
    ASSERT(i == 6 && x == 16384);
    PUTINT(vm, 777);
    ASSERT(vm->brk == 8);
    GETINT(vm, i, x);
    ASSERT(i == 8 && x == 777);
    // printf("%hhu %hhu %x\n", vm->mem[6], vm->mem[7], JS_TRUE);
  }
  js_destroy(vm);
  return NULL;
}
#endif

static const char *test_notsupported(void) {
  struct js *js = js_create(1000);
  ASSERT(js_eval(js, "void", -1) == JS_ERROR);
  js_destroy(js);
  return NULL;
}

static const char *test_comments(void) {
  struct js *js = js_create(1000);
  CHECK_NUMERIC("// hi there!!\n/*\n\n fooo */\n\n   \t\t1", 1);
  CHECK_NUMERIC("1 /* foo */ + /* 3 bar */ 2", 3);
  js_destroy(js);
  return NULL;
}

static const char *test_arith(void) {
  struct js *js = js_create(1000);
  ASSERT(js_eval(js, "^1", -1) == JS_ERROR);
  ASSERT(js_eval(js, "1 1", -1) == JS_ERROR);
  CHECK_NUMERIC("(1+2)*2", 6.0f);
  CHECK_NUMERIC("1.2+3.4", 4.6);
  CHECK_NUMERIC("2 * (1 + 2)", 6.0f);
  CHECK_NUMERIC("0x64", 100);
  // CHECK_NUMERIC("0x7fffffff", 0x7fffffff));
  // CHECK_NUMERIC("0xffffffff", 0xffffffff));
  CHECK_NUMERIC("123.4", 123.4);
  CHECK_NUMERIC("200+50", 250);
  CHECK_NUMERIC("1-2*3", -5);
  CHECK_NUMERIC("1-2+3", 2);
  CHECK_NUMERIC("200-50", 150);
  CHECK_NUMERIC("200*50", 10000);
  CHECK_NUMERIC("200/50", 4);
  CHECK_NUMERIC("200 % 21", 11);
  CHECK_NUMERIC("~2+7", 4);
  CHECK_NUMERIC("~7", -8);
  CHECK_NUMERIC("~~7", 7);
  CHECK_NUMERIC("~~~~7", 7);
  ASSERT(js_eval(js, "200 % 0.9", -1) == JS_NAN);
  CHECK_NUMERIC("5 % 2", 1);
  ASSERT(js_eval(js, "2++", -1) == JS_ERROR);
  ASSERT(js_eval(js, "++2", -1) == JS_ERROR);
  ASSERT(js_eval(js, "2++3", -1) == JS_ERROR);
  CHECK_NUMERIC("2+ +3", 5);
  CHECK_NUMERIC("5 % -2", 1);
  CHECK_NUMERIC("-5 % 2", -1);
  CHECK_NUMERIC("-5 % -2", -1);
  CHECK_NUMERIC("100 << 3", 800);
  CHECK_NUMERIC("(0-14) >> 2", -4);
  CHECK_NUMERIC("31 >>> 2", 7);
  // CHECK_NUMERIC("(0-14) >>> 2", 1073741820));
  CHECK_NUMERIC("6 & 3", 2);
  CHECK_NUMERIC("6 | 3", 7);
  CHECK_NUMERIC("6 ^ 3", 5);
  CHECK_NUMERIC("0.1 + 0.2", 0.3);
  CHECK_NUMERIC("123.4 + 0.1", 123.5);
  js_destroy(js);
  return NULL;
}

static const char *run_all_tests(void) {
  RUN_TEST(test_comments);
  RUN_TEST(test_notsupported);
  RUN_TEST(test_arith);
#if 0
  RUN_TEST(test_gc);
  RUN_TEST(test_typeof);
  RUN_TEST(test_comparison);
  RUN_TEST(test_strings);
  RUN_TEST(test_expr);
  RUN_TEST(test_objects);
  RUN_TEST(test_stringify);
  RUN_TEST(test_ffi);
  RUN_TEST(test_if);
  RUN_TEST(test_subscript);
  RUN_TEST(test_scopes);
  RUN_TEST(test_function);
#endif
  return NULL;
}

int main(void) {
  clock_t started = clock();
  const char *fail_msg = run_all_tests();
  printf("%s, ran %d tests in %.3lfs\n", fail_msg ? "FAIL" : "PASS",
         g_num_checks, (double) (clock() - started) / CLOCKS_PER_SEC);
  return 0;
}
