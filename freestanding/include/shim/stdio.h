#pragma once
typedef struct __wasm_shim_file FILE;
typedef long long fpos_t;
#define EOF (-1)
#define BUFSIZ 8192
#define FILENAME_MAX 4096
#define FOPEN_MAX 16
#define TMP_MAX 1024
#define L_tmpnam 256
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define _IOFBF 0
#define _IOLBF 1
#define _IONBF 2
extern "C" {
void clearerr(FILE*);
int fclose(FILE*);
int feof(FILE*);
int ferror(FILE*);
int fflush(FILE*);
int fgetc(FILE*);
int fgetpos(FILE*, fpos_t*);
char* fgets(char*, int, FILE*);
FILE* fopen(const char*, const char*);
int fprintf(FILE*, const char*, ...);
int fputc(int, FILE*);
int fputs(const char*, FILE*);
size_t fread(void*, size_t, size_t, FILE*);
FILE* freopen(const char*, const char*, FILE*);
int fscanf(FILE*, const char*, ...);
int fseek(FILE*, long, int);
int fsetpos(FILE*, const fpos_t*);
long ftell(FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
int getc(FILE*);
int getchar(void);
char* gets(char*);
void perror(const char*);
int putc(int, FILE*);
int putchar(int);
int puts(const char*);
int remove(const char*);
int rename(const char*, const char*);
void rewind(FILE*);
void setbuf(FILE*, char*);
int setvbuf(FILE*, char*, int, size_t);
FILE* tmpfile(void);
char* tmpnam(char*);
int ungetc(int, FILE*);
int vfprintf(FILE*, const char*, __builtin_va_list);
int vfscanf(FILE*, const char*, __builtin_va_list);
int vprintf(const char*, __builtin_va_list);
int vscanf(const char*, __builtin_va_list);
int vsnprintf(char*, size_t, const char*, __builtin_va_list);
int vsprintf(char*, const char*, __builtin_va_list);
int vsscanf(const char*, const char*, __builtin_va_list);
int printf(const char*, ...);
int snprintf(char*, unsigned long, const char*, ...);
int sprintf(char*, const char*, ...);
int sscanf(const char*, const char*, ...);
int scanf(const char*, ...);
}
