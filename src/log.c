// Copyright 2026 Jannik Laugmand Bülow

#include "log.h"

#include <stdarg.h>
#include <stdio.h>

void log_info(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    vfprintf(stdout, fmt, vargs);
    putc('\n', stdout);
    va_end(vargs);
}

void log_error(const char* fmt, ...) {
    va_list vargs;
    va_start(vargs, fmt);
    vfprintf(stderr, fmt, vargs);
    putc('\n', stderr);
    va_end(vargs);
}
