#pragma once
#include <cstdio>
#include <cstdarg>
#include <cstring>

// ANSI color codes
#define RESET_COLOR    "\033[0m"
#define COLOR_RED      "\033[31m"
#define COLOR_GREEN    "\033[32m"
#define COLOR_YELLOW   "\033[33m"
#define COLOR_BLUE     "\033[34m"

// Macro for logging with file and line number, using printf formatting and color
#define print_log(color, fmt, ...) print_log_impl(__FILE__, __LINE__, color, fmt, ##__VA_ARGS__)

// Function implementation with color parameter
void print_log_impl(const char* file, int line, const char* color, const char* fmt, ...) {
    // Strip the file path to show only after "src/"
    const char* relative_file = strstr(file, "src/");
    if (!relative_file) {
        relative_file = strstr(file, "src\\"); // Try Windows-style separator if not found
    }
    if (relative_file) {
        file = relative_file; // Use the relative path from "src/"
    }

    // Buffer to hold the formatted message
    char message[1024];

    // Handle printf-style formatting for the main message
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    // Print the formatted message with file, line, and specified color
    printf("%s[%s:%d] %s%s\n", color, file, line, message, RESET_COLOR);
}
