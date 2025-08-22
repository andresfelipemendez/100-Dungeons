#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <time.h>

// ANSI color codes
#define RESET_COLOR "\033[0m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"

// Macro for logging with file and line number, using printf formatting and
// color
#define print_log(color, fmt, ...)                                             \
	print_log_impl(__FILE__, __LINE__, color, fmt, ##__VA_ARGS__)

// Declaration of print_log_impl (definition will be in a .cpp file)
void print_log_impl(const char *file, int line, const char *color,
					const char *fmt, ...);
