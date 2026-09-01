#pragma once
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 2147483647
#define MB_CUR_MAX 1
typedef struct { int quot; int rem; } div_t;
typedef struct { long quot; long rem; } ldiv_t;
typedef struct { long long quot; long long rem; } lldiv_t;
extern "C" {
void abort(void) __attribute__((noreturn));
void exit(int) __attribute__((noreturn));
void _Exit(int) __attribute__((noreturn));
int atexit(void (*)(void));
int at_quick_exit(void (*)(void));
void quick_exit(int) __attribute__((noreturn));
void* malloc(unsigned long);
void* calloc(unsigned long, unsigned long);
void* realloc(void*, unsigned long);
void free(void*);
void* aligned_alloc(unsigned long, unsigned long);
char* getenv(const char*);
int system(const char*);
int rand(void);
void srand(unsigned);
int abs(int) __attribute__((const));
long labs(long) __attribute__((const));
long long llabs(long long) __attribute__((const));
div_t div(int, int) __attribute__((const));
ldiv_t ldiv(long, long) __attribute__((const));
lldiv_t lldiv(long long, long long) __attribute__((const));
int atoi(const char*);
long atol(const char*);
long long atoll(const char*);
double atof(const char*);
long strtol(const char*, char**, int);
long long strtoll(const char*, char**, int);
unsigned long strtoul(const char*, char**, int);
unsigned long long strtoull(const char*, char**, int);
double strtod(const char*, char**);
float strtof(const char*, char**);
long double strtold(const char*, char**);
void qsort(void*, unsigned long, unsigned long, int (*)(const void*, const void*));
void* bsearch(const void*, const void*, unsigned long, unsigned long, int (*)(const void*, const void*));
int mblen(const char*, unsigned long);
int mbtowc(wchar_t*, const char*, unsigned long);
int wctomb(char*, wchar_t);
unsigned long mbstowcs(wchar_t*, const char*, unsigned long);
unsigned long wcstombs(char*, const wchar_t*, unsigned long);
}
