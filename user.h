#pragma once
#include "common.h"

void putchar(char ch);
int getchar(void);
int readfile(const char *filename, char *buf, size_t len);
int writefile(const char *filename, const char *buf, size_t len);
__attribute__((noreturn)) void exit(void);
