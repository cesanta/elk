#include "elk.c"

#define ASSERT(expr)                                    \
  do {                                                  \
    if (!(expr)) {                                      \
      printf("%s:%d: %s\n", __FILE__, __LINE__, #expr); \
    }                                                   \
  } while (0)

static void test_stack() {
  struct point {
    int x;
    int y;
  } p;
  stack(int, 3) ints;
  stack(struct point, 10) points;

  stack_reset(ints);
  stack_reset(points);

  ASSERT(stack_cap(ints) == 3 && stack_len(ints) == 0);
  ASSERT(stack_push(ints, 1) == 0 && stack_len(ints) == 1);
  ASSERT(stack_peek(ints, 0) == 1);
  ASSERT(stack_push(ints, 4) == 0 && stack_len(ints) == 2);
  ASSERT(stack_peek(ints, 0) == 4 && stack_peek(ints, 1) == 1);
  ASSERT(stack_push(ints, 7) == 0 && stack_len(ints) == 3);
  ASSERT(stack_push(ints, 11) == -1 && stack_len(ints) == 3);
  ASSERT(stack_peek(ints, 0) == 7 && stack_peek(ints, 1) == 4 &&
         stack_peek(ints, 2) == 1);
  ASSERT(stack_pop(ints) == 7 && stack_len(ints) == 2);
  ASSERT(stack_peek(ints, 0) == 4 && stack_peek(ints, 1) == 1);
  ASSERT(stack_pop(ints) == 4 && stack_len(ints) == 1);
  ASSERT(stack_peek(ints, 0) == 1);
  ASSERT(stack_pop(ints) == 1 && stack_len(ints) == 0);

  ASSERT(stack_cap(points) == 10);
  p.x = p.y = 1;
  stack_push(points, p);
  p.x = p.y = 2;
  stack_push(points, p);
  ASSERT(stack_len(points) == 2);
  ASSERT(stack_peek(points, 0).x == 2);
  ASSERT(stack_peek(points, 1).x == 1);
  ASSERT(stack_pop(points).x == 2);
  ASSERT(stack_len(points) == 1);
  ASSERT(stack_pop(points).x == 1);
  ASSERT(stack_len(points) == 0);

  p.x = p.y = 3;
  stack_push(points, p);
  ASSERT(stack_peek(points, 0).x == 3);
  ASSERT(stack_len(points) == 1);
  stack_reset(points);
  ASSERT(stack_len(points) == 0);
}

static void assert_lexer(const char *input, ...) {
  va_list ap;
  struct lexer lex;
  va_start(ap, input);
  lexer_init(&lex, input, strlen(input));
  for (;;) {
    int token_type = lexer_next(&lex);
    int expected_type = va_arg(ap, int);
    char *expected_token = va_arg(ap, char *);
    printf("et: %d, tt: %d, tv: %s %.*s\n", expected_type, token_type,
           expected_token, (int) (lex.s - lex.token), lex.token);
    ASSERT(token_type == expected_type);
    printf("  %d %d\n", lex.s - lex.token, strlen(expected_token));
    ASSERT(lex.s - lex.token == (int) strlen(expected_token));
    ASSERT(strncmp(lex.token, expected_token, strlen(expected_token)) == 0);
    if (expected_type == T_EOF || token_type == T_EOF) {
      break;
    }
  }
  va_end(ap);
}

static void test_lexer() {
  /* Spaces */
  assert_lexer("", T_EOF, "");
  assert_lexer(" /* hello */", T_EOF, "");
  assert_lexer(" // hello \n\n  ", T_EOF, "");
  assert_lexer(" /* \n//\n */ ", T_EOF, "");
  /* Numbers */
  assert_lexer("123", T_NUM, "123", T_EOF, "");
  assert_lexer("1.23", T_NUM, "1.23", T_EOF, "");
  assert_lexer("  1.23  ", T_NUM, "1.23", T_EOF, "");
  assert_lexer("0x123", T_NUM, "0x123", T_EOF, "");
  /* Identifiers */
  assert_lexer("a", T_IDNT, "a", T_EOF, "");
  assert_lexer("_a$3 x", T_IDNT, "_a$3", T_IDNT, "x", T_EOF, "");
  /* Strings */
  assert_lexer("'a b'", T_STR, "'a b'", T_EOF, "");
  assert_lexer("\"a'b\"", T_STR, "\"a'b\"", T_EOF, "");
  /* Function literals */
  assert_lexer("function(a,b) { {} }", T_FUNC, "function(a,b) { {} }", T_EOF,
               "");
  /* Brackets and semicolons */
  assert_lexer("foo(x);", T_IDNT, "foo", T_BRKT, "(", T_IDNT, "x", T_BRKT, ")",
               T_SEMI, ";", T_EOF, "");
  /* Operators */
  assert_lexer("2 << 2 < 3", T_NUM, "2", T_OP, "<<", T_NUM, "2", T_OP, "<",
               T_NUM, "3", T_EOF, "");
  assert_lexer("typeof x", T_OP, "typeof", T_IDNT, "x", T_EOF, "");
}

static void test_js_values() {
  jsval_t s, v;
  struct js js;
  uint8_t buf[16];
  js_init(&js, buf, sizeof(buf));

  v = JS_NULL;
  ASSERT(JS_TYPE(v) == JS_TYPE_NULL && JS_PAYLOAD(v) == 0);
  v = JS_TRUE;
  ASSERT(JS_TYPE(v) == JS_TYPE_BOOLEAN && JS_PAYLOAD(v) == 1);
  v = JS_FALSE;
  ASSERT(JS_TYPE(v) == JS_TYPE_BOOLEAN && JS_PAYLOAD(v) == 0);
  v = JS_UNDEFINED;
  ASSERT(JS_TYPE(v) == JS_TYPE_UNDEFINED && JS_PAYLOAD(v) == 0);
  v = js_number(1.23f);
  ASSERT(JS_TYPE(v) == JS_TYPE_NUMBER && js_float(v) == 1.23f);
  v = js_string(&js, "hello", 5);
  s = v;
  ASSERT(JS_TYPE(v) == JS_TYPE_STRING);
  ASSERT(js_strlen(&js, v) == 5);
  ASSERT(strcmp(js_strptr(&js, v), "hello") == 0);
  v = js_string(&js, "out of memory", 13);
  ASSERT(JS_TYPE(v) == JS_TYPE_ERROR);

  ASSERT(js.brk > 0);
  js_gc(&js, s);
  ASSERT(js.brk == 0);

  v = js_string(&js, "out of memory", 13);
  ASSERT(JS_TYPE(v) == JS_TYPE_STRING && js_strlen(&js, v) == 13);
  js_gc(&js, v);

  v = js_object(&js);
  ASSERT(JS_TYPE(v) == JS_TYPE_OBJECT);
  s = js_string(&js, "foo", 3);
  js_object_set(&js, v, s, js_number(42));
  /*p = js_object_get(&js, v, "foo", 3);*/
  /*ASSERT(p != NULL && JS_TYPE(*p) == JS_TYPE_NUMBER && js_float(*p) == 42);*/
  ASSERT(js.brk > 0);
  ASSERT(js.objs[1].flags > 0);
  js_gc(&js, s);
  ASSERT(js.brk > 0);
  js_gc(&js, v);
  ASSERT(js.brk == 0);
  ASSERT(js.objs[1].flags == 0);
}

static void assert_js(const char *code, jsval_t v) {
  struct js js;
  uint8_t buf[1024] = {0};
  jsval_t r;
  js_init(&js, buf, sizeof(buf));
  r = js_eval(&js, code, strlen(code));
  if (r != v) {
    printf("FAILED: %s\n", code);
    printf("   result = %s\n", js_stringify(&js, r));
    printf("   expect = %s\n", js_stringify(&js, v));
    printf("   types = %d %d\n", JS_TYPE(r), JS_TYPE(v));
  }
}

static void test_elk() {
  /* Primitives */
  assert_js("", JS_UNDEFINED);
  assert_js("1", js_number(1));
  assert_js("1.2", js_number(1.2));
  assert_js("0xf", js_number(15));
  assert_js("null", JS_NULL);
  assert_js("undefined", JS_UNDEFINED);
  assert_js("true", JS_TRUE);
  assert_js("false", JS_FALSE);
  /* Expressions */
  assert_js("1+2", js_number(3));
  assert_js("3-1", js_number(2));
  assert_js("1+2+3", js_number(6));
  assert_js("1+2*3", js_number(7));
  assert_js("1*2+3", js_number(5));
  assert_js("2*(3+4)", js_number(14));
  assert_js("(2*3)+4)", js_number(10));
  assert_js("1 === 1", JS_TRUE);
  assert_js("1 !== 1", JS_FALSE);
  assert_js("1 === 2", JS_FALSE);
  assert_js("1 === '1'", JS_FALSE);
  /* Multiple statements */
  assert_js("1; 2", js_number(2));
  assert_js("1; 2;", js_number(2));
  /* Let */
  assert_js("let a, b; a=1; b=2; a+b", js_number(3));
  assert_js("let a=1, b=a+1; a+b", js_number(3));
  /* Short-circuit */
  assert_js("let a = 1; 1 && (a = 2) && (a = 3); a", js_number(3));
  assert_js("let a = 1; 0 && (a = 2) && (a = 3); a", js_number(1));
  assert_js("let a = 1; 1 || (a = 2) || (a = 3); a", js_number(1));
  assert_js("let a = 1; 0 || (a = 2) || (a = 3); a", js_number(2));
  /* Strings */
  assert_js("'hello'.length", js_number(5));
  assert_js("let s = 'hello'; s.length", js_number(5));
  assert_js("let s = 'hello'; s[1]", js_number(101));
  /* Objects */
  assert_js("{a:1,b:2}.a", js_number(1));
  assert_js("{'a':1,\"b\":2}.a", js_number(1));
  assert_js("let x = {a:3,b:4}; x.a+x.b", js_number(7));
  assert_js("let x = {a:3,b:4}; x.b=5; x.a+x.b", js_number(8));
  assert_js("let x = {a:3,b:4}; x.c=5; x.a+x.c", js_number(8));
  assert_js("let x = {a:3,b:{c:4}}; x.b.c = 5; x.b.c + x.a", js_number(8));
  assert_js("let x = {a:3,b:{c:4}}; x['b'].c = 5; x.b.c + x.a", js_number(8));
  /* Return */
  assert_js("return 1+2; return 3+4;", js_number(3));
  /* Scopes */
  assert_js("let a = 1, b = 2; {let a = 3; b = 4 } a+b", js_number(5));
  /* If */
  assert_js("if (1) 2", js_number(2));
  assert_js("if (0) 2", JS_UNDEFINED);
  assert_js("let a = 4, b; b = 2; a + b", js_number(6));
  assert_js("let a = 4; if (1) a = 3; if (0) { a = 2 }; a", js_number(3));
  assert_js("let a=4; if(1)a=3;a", js_number(3));
  assert_js("let a=4; if(0)a=3;a", js_number(4));
  assert_js("let a=4; if(1)if(0)a=3;a", js_number(4));
  assert_js("let a=4; if(0)if(1)a=3;a", js_number(4));
  assert_js("let a=4; if(1){if(0){a=3}}a", js_number(4));
  assert_js("let a=4; if(0){if(1){a=3}}a", js_number(4));
  /* While */
  assert_js("let i=3, j=0; while (0 < i) { i = i - 1; j = j + 2; }; j",
            js_number(6));
  assert_js("let i=12;while(5<i){while(10<i){i=i-2}i=i-3}i", js_number(4));
  /* Functions */
  assert_js("let f=function(){return 5};f()", js_number(5));
  assert_js("let a, f=function(){a=2;if(1){return 2+6}a=4};f()+a",
            js_number(10));
  assert_js("1 + (function(){return 2;})() + 3", js_number(6));
  assert_js("let a = 1 + (function(){return 2;})() + 3; a", js_number(6));
  assert_js("let g={f:function(){return 5;}}; g.f()", js_number(5));
  assert_js("let f=function(a,b){return a+b};f(2,f(3,4)+5)", js_number(14));
  assert_js("let f=function(a,b){return a+b};f(f(2,3),4)", js_number(9));
  assert_js("(function(a) { return a + 1; })(3)", js_number(4));
  assert_js("let x = 3; (function(a) { return a+1; })(x)", js_number(4));
}

static int test_ffi_cb(int a, int b, int (*cb)(int, void *userdata),
                       void *userdata) {
  return cb(a + b, userdata);
}

static void test_ffi() {
  struct js js;
  uint8_t buf[1024] = {0};
  jsval_t r;
  const char *atoi_code = "atoi('42') + 1";
  const char *cb_code =
      "let a = 1; let f = function(x) { a = x; return 42; }; "
      "test_ffi_cb(3, 4, f, 0); a";
  js_init(&js, buf, sizeof(buf));
  js_import(&js, atoi, "is");
  js_import(&js, test_ffi_cb, "iii[iiu]u");
  r = js_eval(&js, atoi_code, strlen(atoi_code));
  ASSERT(js_float(r) == 43);
  r = js_eval(&js, cb_code, strlen(cb_code));
  ASSERT(js_float(r) == 7);
}

int main() {
  test_stack();
  test_lexer();
  test_js_values();
  test_elk();
  test_ffi();
  return 0;
}
