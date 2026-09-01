#pragma once
#define CLOCKS_PER_SEC 1000000
#define TIME_UTC 1
typedef long clock_t;
typedef long long time_t;
struct tm { int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year, tm_wday, tm_yday, tm_isdst; long tm_gmtoff; const char* tm_zone; };
struct timespec { long long tv_sec; long tv_nsec; };
extern "C" {
char* asctime(const struct tm*);
clock_t clock(void);
char* ctime(const time_t*);
double difftime(time_t, time_t);
struct tm* gmtime(const time_t*);
struct tm* localtime(const time_t*);
time_t mktime(struct tm*);
size_t strftime(char*, size_t, const char*, const struct tm*);
time_t time(time_t*);
int timespec_get(struct timespec*, int);
void tzset(void);
}
