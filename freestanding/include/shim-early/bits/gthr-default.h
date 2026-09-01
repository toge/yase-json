#pragma once
typedef int __gthread_key_t;
typedef int __gthread_once_t;
typedef struct { int __x; } __gthread_mutex_t;
typedef struct { int __x; } __gthread_recursive_mutex_t;
typedef struct { int __x; } __gthread_cond_t;
typedef struct { long long tv_sec; long tv_nsec; } __gthread_time_t;
static inline void __gthread_yield(void) {}
