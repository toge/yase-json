#pragma once
#include <stdint.h>
typedef struct { long long quot; long long rem; } imaxdiv_t;
extern "C" {
long long imaxabs(long long) __attribute__((const));
imaxdiv_t imaxdiv(long long, long long) __attribute__((const));
long long strtoimax(const char*, char**, int);
unsigned long long strtoumax(const char*, char**, int);
long long wcstoimax(const wchar_t*, wchar_t**, int);
unsigned long long wcstoumax(const wchar_t*, wchar_t**, int);
}
