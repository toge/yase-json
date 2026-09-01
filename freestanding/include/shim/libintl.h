#pragma once
extern "C" {
char* gettext(const char*);
char* dgettext(const char*, const char*);
char* dcgettext(const char*, const char*, int);
char* ngettext(const char*, const char*, unsigned long);
char* dngettext(const char*, const char*, const char*, unsigned long);
char* textdomain(const char*);
char* bindtextdomain(const char*, const char*);
}
