#pragma once
#define _ISbit(bit) ((bit) < 8 ? ((1 << (bit)) << 8) : ((1 << (bit)) >> 8))
#define _ISupper _ISbit(0)
#define _ISlower _ISbit(1)
#define _ISalpha (_ISupper | _ISlower)
#define _ISdigit _ISbit(3)
#define _ISxdigit _ISbit(4)
#define _ISspace _ISbit(5)
#define _ISprint _ISbit(6)
#define _ISgraph (_ISbit(7) | _ISpunct)
#define _ISblank _ISbit(8)
#define _IScntrl _ISbit(9)
#define _ISpunct _ISbit(10)
#define _ISalnum (_ISalpha | _ISdigit)
extern "C" {
int isalnum(int);
int isalpha(int);
int iscntrl(int);
int isdigit(int);
int isgraph(int);
int islower(int);
int isprint(int);
int ispunct(int);
int isspace(int);
int isupper(int);
int isxdigit(int);
int isblank(int);
int tolower(int);
int toupper(int);
}
