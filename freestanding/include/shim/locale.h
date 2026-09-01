#pragma once
#define LC_ALL 0
#define LC_COLLATE 1
#define LC_CTYPE 2
#define LC_MONETARY 3
#define LC_NUMERIC 4
#define LC_TIME 5
struct lconv { char* decimal_point; char* thousands_sep; };
typedef struct __locale_struct { int __mask; }* __locale_t;
extern "C" {
char* setlocale(int, const char*);
struct lconv* localeconv(void);
}
