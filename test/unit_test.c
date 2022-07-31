#include <time.h>
#define JS_DUMP
#include "../elk.c"

static bool ev(struct js *js, const char *expr, const char *expectation) {
  const char *result = js_str(js, js_eval(js, expr, strlen(expr)));
  bool correct = strcmp(result, expectation) == 0;
  if (!correct) printf("[%s] -> [%s] [%s]\n", expr, result, expectation);
  return correct;
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
  // assert(ev(js, "2**3", "8"));
  // assert(ev(js, "1.2**3.4", "1.85873"));
}

static void test_errors(void) {
  char mem[200];
  struct js *js;
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  js_setmaxcss(js, 5000);
  assert(ev(js, "1+(((((((((((((((((1)))))))))))))))))", "ERROR: C stack"));
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "+", "ERROR: bad expr"));
  assert(ev(js, "2+", "ERROR: bad expr"));
  assert(ev(js, "2 * * 2", "ERROR: bad expr"));
  assert(ev(js, "1 2", "ERROR: ; expected"));
  assert(ev(js, "1 2 + 3", "ERROR: ; expected"));
  assert(ev(js, "1 + 2 3", "ERROR: ; expected"));
  assert(ev(js, "1 2 + 3 4", "ERROR: ; expected"));
  assert(ev(js, "1 + 2 3 * 5", "ERROR: ; expected"));
  assert(ev(js, "1 + 2 3 * 5 + 6", "ERROR: ; expected"));

  assert(ev(js, "switch", "ERROR: 'switch' not implemented"));
  assert(ev(js, "with", "ERROR: 'with' not implemented"));
  assert(ev(js, "try", "ERROR: 'try' not implemented"));
  assert(ev(js, "class", "ERROR: 'class' not implemented"));
  assert(ev(js, "const x", "ERROR: 'const' not implemented"));
  assert(ev(js, "var x", "ERROR: 'var' not implemented"));

  assert(ev(js, "1 + yield", "ERROR: bad expr"));
  assert(ev(js, "yield", "ERROR: 'yield' not implemented"));
  assert(ev(js, "@", "ERROR: parse error"));
  assert(ev(js, "$", "ERROR: '$' not found"));
  assert(ev(js, "let a=0; a+=x;a", "ERROR: 'x' not found"));
}

static void test_basic(void) {
  struct js *js;
  char mem[sizeof(*js) + 350];
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
  js_gc(js);
  assert(js->brk < 100);

  assert(ev(js, "1;2", "2"));
  assert(ev(js, "1;2;", "2"));
  assert(ev(js, "let a ;", "undefined"));
  assert(ev(js, "{let a,}", "ERROR: parse error"));
  assert(ev(js, "let ;", "ERROR: parse error"));
  assert(ev(js, "{let a 2}", "ERROR: parse error"));
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
  assert(ev(js, "({a:3}).a", "3"));
  assert(ev(js, "({\"a\":4})", "{\"a\":4}"));
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

  assert(ev(js, "b = 2; a = {x:1,y:b};", "{\"y\":2,\"x\":1}"));

  assert(ev(js, "a=5;a;", "5"));
  assert(ev(js, "a&=3;a;", "1"));
  assert(ev(js, "a|=3;a;", "3"));
  assert(ev(js, "(((((((1)))))))", "1"));
  assert(ev(js, "1+(((((((1)))))))", "2"));

  size_t a = 0, b = 0, c = 0;
  js_stats(js, &a, &b, &c);
  assert(a > 0 && b > 0 && c > 0);
}

static void test_memory(void) {
  char mem[sizeof(struct js) + 8];  // Not enough memory to create an object
  struct js *js;
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "({a:1})", "ERROR: oom"));  // OOM
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
  assert(ev(js, "'aa'.foo", "ERROR: lookup in non-obj"));
  assert(ev(js, "'aa'.length", "2"));
  assert(ev(js, "'Київ'.length", "8"));
  assert(ev(js, "({a:'ї'}).a.length", "2"));
  assert(ev(js, "a=true;a", "true"));
  assert(ev(js, "a=!a;a", "false"));
  assert(ev(js, "!123", "false"));
  assert(ev(js, "!0", "true"));
  assert(ev(js, "let r=''; r+='x'; r+='y'; r", "\"xy\""));
  assert(ev(js, "let i=0;r=''; for(;i<2;) { r+='x'; i++; } r", "\"xx\""));
}

static void test_flow(void) {
  struct js *js;
  char mem[sizeof(*js) + 300];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "let a = 1; a", "1"));
  assert(ev(js, "if (true) a++; a", "2"));
  assert(ev(js, "if (a) a++; a", "3"));
  assert(ev(js, "if (!a) a++; a", "3"));
  assert(ev(js, "a=0;if (a) a++; a", "0"));
  assert(ev(js, "if ('') a--; a", "0"));
  assert(ev(js, "a=2; for(;a;)a--; a", "0"));
  assert(ev(js, "a=2", "2"));
  assert(ev(js, "if (1) 2;", "2"));
  assert(ev(js, "if (0) 2;", "undefined"));
  assert(ev(js, "if (0) { a = 7; a++; }", "undefined"));
  assert(ev(js, "a", "2"));
  assert(ev(js, "if (1) {}", "undefined"));
  assert(ev(js, "if ('boo') { a = 7; a++; }", "7"));
  assert(ev(js, "a", "8"));
  assert(ev(js, "while (false)break;", "ERROR: 'while' not implemented"));
  assert(ev(js, "break;", "ERROR: not in loop"));
  assert(ev(js, "continue;", "ERROR: not in loop"));
  assert(ev(js, "let b = 0; for (;b < 10;) {b++; a--;} a;", "-2"));
  assert(ev(js, "b = 0; for (;b++ < 10;) a += 3;  a;", "28"));
  assert(ev(js, "b = 0; for (;;) break; ", "undefined"));
  assert(ev(js, "b = 0; for (;true;) break; ", "undefined"));
  assert(ev(js, "b = 0; for (;;) break; b", "0"));
  assert(ev(js, "a", "28"));
  assert(ev(js, "b = 0; for (;;) if (a-- < 10) break;", "undefined"));
  assert(ev(js, "a", "8"));
  assert(ev(js, "b = 0; for (;;) if (b++ > 10) break; b;", "12"));
  assert(ev(js, "a = b = 0; for (;b++ < 10;) for (;a < b;) a++; a", "10"));
  assert(ev(js, "a = 0; for (;;) { if (a++ < 10) continue; break;} a", "11"));
  assert(ev(js, "a=b=0; for (;b++<10;) {true;a++;} a", "10"));
  assert(ev(js, "a=b=0; if (false) b++; else b--; b", "-1"));
  assert(ev(js, "a=b=0; if (false) {b++;} else {b--;} b", "-1"));
  assert(ev(js, "a=b=0; if (false) {2;b++;} else {2;b--;} b", "-1"));
  assert(ev(js, "a=b=0; if (true) b++; else b--; b", "1"));
  assert(ev(js, "a=b=0; if (true) {2;b++;} else {2;b--;} b", "1"));
  assert(ev(js, "a=0; if (1) a=1; else if (0) a=2; a;", "1"));
  assert(ev(js, "a=0; if (0) a=1; else if (1) a=2; a;", "2"));
  assert(ev(js, "a=0; if (0){7;a=1;}else if (1){7;a=2;} a;", "2"));
  assert(ev(js, "a=0; if(0){7;a=1;}else if(0){5;a=2;}else{3;a=3;} a;", "3"));
  assert(ev(js, "a=0; for (let i=0;i<5;i++) a++; a", "5"));
  assert(ev(js, "a=0; for (let i=0;x;i++) a++; a", "ERROR: 'x' not found"));
  assert(ev(js, "a=0; for (x;1;i++) a++; a", "ERROR: 'x' not found"));
  assert(ev(js, "a=0; for (1;1;x) a++; a", "ERROR: 'x' not found"));
  assert(ev(js, "a=0; for (1 1;1;1) a++; a", "ERROR: parse error"));
  assert(ev(js, "a=0; for (1;1 1;1) a++; a", "ERROR: parse error"));
  assert(ev(js, "a=0; for (1;1;1 1) a++; a", "ERROR: parse error"));
  assert(ev(js, "a=0; for (;;) break; a", "0"));
  assert(ev(js, "a=0; for (;;) {a++; break;} a", "1"));
  assert(ev(js, "a=0; for (;a<100;) {a++; continue;} a", "100"));
  assert(ev(js, "a=0; for (let i=0;i<10;i++) { continue; a++;} a", "0"));
  assert(ev(js, "a=0; for (let i=0;i++<2;i) a+=x; b", "ERROR: 'x' not found"));
}

static void test_scopes(void) {
  struct js *js;
  char mem[sizeof(*js) + 250];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "let a = 5; { a = 6; let x = 2; } a", "6"));
  assert(ev(js, "let b = 5; { let b = 6; } b", "5"));
  js_gc(js);
  jsoff_t brk = js->brk;
  assert(ev(js, "{}", "undefined"));
  assert(ev(js, "{1}", "ERROR: ; expected"));
  assert(ev(js, "{1;}", "1"));
  assert(ev(js, "{1;2}", "ERROR: ; expected"));
  assert(ev(js, "{1;2;}", "2"));
  assert(ev(js, "{1}2", "ERROR: ; expected"));
  assert(ev(js, "{1;}2", "2"));
  assert(ev(js, "{1;}2;3", "3"));
  assert(ev(js, "{1;}2;3;", "3"));
  assert(ev(js, "{{}}", "undefined"));
  assert(ev(js, "{let a=0; for(;a<5;){a++;}}", "undefined"));
  js_gc(js);
  assert(js->brk == brk);
  assert(ev(js, "{ let b = 6; } 1", "1"));
  js_gc(js);
  assert(js->brk == brk);
  // js_dump(js);
  assert(ev(js, "{ let a = 'hello'; { let a = 'world'; } }", "undefined"));
  js_gc(js);
  assert(js->brk == brk);
}

static void test_funcs(void) {
  struct js *js;
  char mem[sizeof(*js) + 500];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "function(){};1;", "1"));
  assert(ev(js, "let f=function(){};1;", "1"));
  assert(ev(js, "f;", "function(){}"));
  assert(ev(js, "function(){1}", "ERROR: ; expected"));
  assert(ev(js, "function(){1;}", "function(){1;}"));
  assert(ev(js, "function(){1;};", "function(){1;}"));
  assert(js->flags == 0);
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
  assert(ev(js, "return", "ERROR: not in func"));
  assert(ev(js, "(function(){})()", "undefined"));
  assert(ev(js, "(function(){})(1,2,3)", "undefined"));
  assert(ev(js, "(function(){1;})(1,2,3)", "undefined"));
  assert(js->flags == 0);
  assert(ev(js, "(function(x){res+=x;})(1)", "ERROR: 'res' not found"));
  assert(ev(js, "(function(){return 1;})()", "1"));
  assert(ev(js, "(function(){1;2;return 1;})()", "1"));
  assert(ev(js, "(function(){return 1;})(1)", "1"));
  assert(ev(js, "(function(){return 1;})(1,2,3)", "1"));
  assert(ev(js, "(function(){return 1;})(1)", "1"));
  assert(ev(js, "(function(){return 1;})(1,)", "ERROR: parse error"));
  assert(ev(js, "(function(){return 1;2;})()", "1"));
  assert(ev(js, "(function(){return 1;2;return 3;})()", "1"));
  assert(ev(js, "(function(a){return b;})(1)", "ERROR: 'b' not found"));
  assert(ev(js, "(function(a,b){return a + b;})()", "ERROR: type mismatch"));
  assert(ev(js, "(function(a,b){return a + b;})(1)", "ERROR: type mismatch"));
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
  assert(ev(js, "f('a','b')", "\"ab\""));

  assert(ev(js, "let i,a=0; (function(){a++;})(); a", "1"));
  assert(ev(js, "a=0; (function(){ a++; })(); a", "1"));

  assert(ev(js, "a=0; (function(x){a=x;})(2); a", "2"));
  assert(ev(js, "a=0; (function(x){a=x;})('hi'); a", "\"hi\""));
  assert(ev(js, "a=0;(function(x){let z=x;a=typeof z;})('x');a", "\"string\""));
  assert(ev(js, "(function(x){return x;})(1);", "1"));
  assert(ev(js, "(function(x){return {a:x};null;})(1).a;", "1"));
  assert(ev(js, "(function(x){let m= {a:7}; return m;})(1).a;", "7"));
  assert(ev(js, "(function(x){let m=7;return m;})(1);", "7"));
  assert(ev(js, "(function(x){let m='hi';return m;})(1);", "\"hi\""));
  assert(ev(js, "(function(x){let m={a:2};return m;})(1).a;", "2"));
  assert(ev(js, "(function(x){let m={a:x};return m;})(3).a;", "3"));

  assert(ev(js, "i=a=0;f=function(x,y){return x*y;};1;", "1"));
  js_gc(js);
  brk = js->brk;
  assert(ev(js, "i=a=0; for (;i++<99;) a=i;a", "99"));
  js_gc(js);
  assert(js->brk == brk);
  assert(ev(js, "i=a=0; for (;i++ < 9999;) a += i*i; a", "333283335000"));
  js_gc(js);
  assert(js->brk == brk);
  assert(ev(js, "i=a=0; for (;i++ < 9999;) a += f(i,i); a", "333283335000"));
  js_gc(js);
  assert(js->brk == brk);

  js_eval(js, "f=function(){return 1;};", ~0UL);
  assert(ev(js, "f();", "1"));
  assert(ev(js, "f() + 2;", "3"));

  assert(ev(js, "f=function (x){return x+1;}; f(1);", "2"));

  assert(ev(js, "f = function(x){return x;};", "function(x){return x;}"));
  assert(ev(js, "f(2)", "2"));
  assert(ev(js, "f({})", "{}"));
  assert(ev(js, "f({a:5,b:3}).b", "3"));
  assert(ev(js, "f({\"a\":5,\"b\":3}).b", "3"));
}

static void test_bool(void) {
  struct js *js;
  char mem[sizeof(*js) + 200];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(js->size == 200);  // Check 8-byte size align
  assert((js = js_create(mem, sizeof(mem) - 1U)) != NULL);
  assert(js->size == 192);  // Check 8-byte size align
  assert(ev(js, "1 && 2", "2"));
  assert(ev(js, "1 && 'x'", "\"x\""));
  assert(ev(js, "1 && ''", "\"\""));
  assert(ev(js, "1 && false", "false"));
  assert(ev(js, "1 && false || true", "true"));
  assert(ev(js, "1 && false && true", "false"));
  assert(ev(js, "1 === 2", "false"));
  assert(ev(js, "1 !== 2", "true"));
  assert(ev(js, "1 === true", "ERROR: type mismatch"));
  assert(ev(js, "1 <= 2", "true"));
  assert(ev(js, "1 < 2", "true"));
  assert(ev(js, "2 >= 2", "true"));
  assert(ev(js, "let a=true;a", "true"));
  assert(ev(js, "a=!a;a", "false"));
  assert(ev(js, "!123", "false"));
  assert(ev(js, "!0", "true"));
  assert(ev(js, "a=1; 0 || a++; a", "2"));
  assert(ev(js, "a=1; 1 || a++; a", "1"));
  assert(ev(js, "a=1; 0 && a++; a", "1"));
  assert(ev(js, "a=1; 1 && a++; a", "2"));
  assert(ev(js, "a=1; 1 && 2 && a++; a", "2"));
}

void prnt(const char *s) {
  (void) s;
  // printf("%s", s);
}

static void test_gc(void) {
  struct js *js;
  char mem[sizeof(*js) + 1500];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  jsval_t obj = js_mkobj(js);
  js_set(js, js_glob(js), "os", obj);
  js_set(js, obj, "a", mkval(T_BOOL, 0));
  js_set(js, obj, "b", mkval(T_BOOL, 1));
#if 0
  jsoff_t brk = js->brk;
  js_gc(js);
  assert(js->brk == brk);
  js_set(js, js_glob(js), "prnt", js_import(js, (uintptr_t) prnt, "vs"));
  js_set(js, js_glob(js), "str", js_import(js, (uintptr_t) js_str, "smj"));
  assert(ev(js,
            "let f=function(){let n=0; while (n++ < "
            "100){prnt(str(0,n)+'\\n');} return n;}; f()",
            "101"));
#endif

  assert(ev(js, "let a='';", "undefined"));
  assert(ev(js, "(function(x){for(let i=0;i<x;i++)a+='x';})(2);a", "\"xx\""));
  assert(
      ev(js, "(function(x){for(let i=0;i<x;){a+='y';i++;}})(1);a", "\"xxy\""));
}

// Postponed callback invocation. C code stores a callback, then calls later
static void (*s_timer_fn)(int, void *);
static void *s_timer_fn_data;
static void set_timer(void (*fp)(int, void *), void *userdata) {
  s_timer_fn = fp;
  s_timer_fn_data = userdata;
}

struct js_timer_data {
  struct js *js;
  char name[10];
};

static void js_timer_fn(int n, void *userdata) {
  struct js_timer_data *d = (struct js_timer_data *) userdata;
  char buf[20];
  int len = snprintf(buf, sizeof(buf), "%s(%d)", d->name, n);
  // printf("EVEEEEE [%s]\n", buf);
  if (d->js) js_eval(d->js, buf, len > 0 ? (size_t) len : 0);
}

static jsval_t js_set_timer(struct js *js, jsval_t *args, int nargs) {
  static struct js_timer_data d;
  if (nargs != 1) return js_mkerr(js, "1 cb expected");
  d.js = js;
  size_t i, len = 0;
  char *name = js_getstr(js, args[0], &len);
  for (i = 0; i < len && i + 1 < sizeof(d.name); i++) d.name[i] = name[i];
  d.name[i] = '\0';
  set_timer(js_timer_fn, &d);
  return js_mkundef();
}

static jsval_t js_gt(struct js *js, jsval_t *args, int nargs) {
  if (!js_chkargs(args, nargs, "dd")) return js_mkerr(js, "doh");
  return js_getnum(args[0]) > js_getnum(args[1]) ? js_mktrue() : js_mkfalse();
}

static void test_c_funcs(void) {
  struct js *js;
  char mem[sizeof(*js) + 1800];

  assert((js = js_create(mem, sizeof(mem))) != NULL);
  js_set(js, js_glob(js), "gt", js_mkfun(js_gt));
  assert(ev(js, "gt()", "ERROR: doh"));
  assert(ev(js, "gt(1,null)", "ERROR: doh"));
  assert(ev(js, "gt(null, 1)", "ERROR: doh"));
  assert(ev(js, "gt(1,2)", "false"));
  assert(ev(js, "gt(1,1)", "false"));
  assert(ev(js, "gt(2,1)", "true"));
  assert(ev(js, "gt(0.78,-12.5)", "true"));
  // assert(ev(js, "gt(2,2)", "true"));

  js_set(js, js_glob(js), "set_timer", js_mkfun(js_set_timer));
  js_eval(js, "let v = 0, f = function(x) { v+=x; };", ~0UL);
  js_eval(js, "set_timer('f');", ~0UL);
  if (s_timer_fn) s_timer_fn(7, s_timer_fn_data);  // C code calls timer
  assert(ev(js, "v", "7"));
  if (s_timer_fn) s_timer_fn(1, s_timer_fn_data);  // C code calls timer
  assert(ev(js, "v", "8"));
  // printf("--> [%s]\n", js_str(js, js_glob(js)));

  jsval_t args[] = {0, js_mktrue(), js_mkstr(js, "a", 1), js_mknull()};
  assert(js_chkargs(args, 4, "dbsj") == true);
  assert(js_chkargs(args, 4, "dbsjb") == false);
  assert(js_chkargs(args, 4, "bbsj") == false);
  assert(js_chkargs(args, 4, "ddsj") == false);
  assert(js_chkargs(args, 4, "dbdj") == false);
  assert(js_chkargs(args, 4, "dbss") == false);
  assert(js_chkargs(args, 4, "d") == false);
}

static void test_ternary(void) {
  struct js *js;
  char mem[sizeof(*js) + 4500];
  assert((js = js_create(mem, sizeof(mem))) != NULL);
  assert(ev(js, "'aa'; 'cc'; 'bb';", "\"bb\""));
  assert(ev(js, "'aa'; 'cc'; '12345'; 'bb';", "\"bb\""));
  assert(ev(js, "1?2:3", "2"));
  assert(ev(js, "0?2:3", "3"));
  assert(ev(js, "true ? 1 + 2 : 'doh'", "3"));
  assert(ev(js, "false ? 1 + 2 : 'doh'", "\"doh\""));
  assert(ev(js, "1?2:3", "2"));
  assert(ev(js, "0?2:3", "3"));
  assert(ev(js, "0?1+1:1+2", "3"));
  assert(ev(js, "let a,b=0?1+1:1+2; b", "3"));
  assert(ev(js, "a=b=0; a=b=0?1+1:1+2", "3"));
  assert(ev(js, "a=0; a=a?1:2;a", "2"));
  assert(ev(js, "a=0; 0?a++:7; a", "0"));
  assert(ev(js, "a=1?2:0?3:4;a", "2"));
  assert(ev(js, "a=0?2:false?3:4;a", "4"));
  assert(ev(js, "a=0?2:true?3:4;a", "3"));
  assert(ev(js, "a=1;a=a?0:1;a", "0"));
  assert(ev(js, "a=0;a=a?0:1;a", "1"));
  // Calculate factorial using ternary op
  assert(ev(js, "let f=function(n){return n<2?1:n*f(n-1);}; 0", "0"));
  assert(ev(js, "f(0)", "1"));
  assert(ev(js, "f(3)", "6"));
  assert(ev(js, "f(4)", "24"));
  assert(ev(js, "f(5)", "120"));
  assert(ev(js, "f(10)", "3628800"));
}

int main(void) {
  clock_t a = clock();
  test_basic();
  test_bool();
  test_scopes();
  test_arith();
  test_errors();
  test_memory();
  test_strings();
  test_flow();
  test_funcs();
  test_c_funcs();
  test_ternary();
  test_gc();
  double ms = (double) (clock() - a) * 1000 / CLOCKS_PER_SEC;
  printf("SUCCESS. All tests passed in %g ms\n", ms);
  return EXIT_SUCCESS;
}
