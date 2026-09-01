#pragma once
extern "C" {
void* memcpy(void*, const void*, unsigned long);
void* memmove(void*, const void*, unsigned long);
void* memset(void*, int, unsigned long);
int memcmp(const void*, const void*, unsigned long);
unsigned long strlen(const char*);
char* strcpy(char*, const char*);
char* strncpy(char*, const char*, unsigned long);
char* strcat(char*, const char*);
char* strncat(char*, const char*, unsigned long);
int strcmp(const char*, const char*);
int strncmp(const char*, const char*, unsigned long);
char* strchr(const char*, int);
char* strrchr(const char*, int);
char* strstr(const char*, const char*);
size_t strspn(const char*, const char*);
size_t strcspn(const char*, const char*);
char* strerror(int);
int strcoll(const char*, const char*);
size_t strxfrm(char*, const char*, size_t);
char* strtok(char*, const char*);
char* strpbrk(const char*, const char*);
char* strdup(const char*);
void* memchr(const void*, int, unsigned long);
}
