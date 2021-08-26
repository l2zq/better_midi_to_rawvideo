#pragma once
#include "util.h"

struct file_global {
  byte *mem;
  size_t len;
};
extern struct file_global file; // clang-format off
bool file_load(const char *filename); // true on success, see errno for error
int  file_free(void);                 // shouldn't be "free", but for beautiful code