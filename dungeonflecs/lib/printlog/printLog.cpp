#include "printLog.h"

void print_log_impl(const char *file, int line, const char *color,
                    const char *fmt, ...) {

  time_t now = time(NULL);
  struct tm local_time;

  // Buffer to hold the time string
  char time_str[10];

  // Use localtime_s for safer local time conversion
  if (localtime_s(&local_time, &now) == 0) {
    // Format the time as [HH:MM:SS]
    strftime(time_str, sizeof(time_str), "%H:%M:%S", &local_time);
  } else {
    // If localtime_s fails, set time_str to a default error string
    strncpy_s(time_str, sizeof(time_str), "??:??:??", _TRUNCATE);
  }

  const char *relative_file = strstr(file, "src/");
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
  printf("%s[%s] [%s:%d] %s%s\n", color, time_str, file, line, message,
         RESET_COLOR);
}
