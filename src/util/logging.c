#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include "logging.h"

static FILE* log_file = NULL;

void logging_init(void) {
    log_file = fopen("dsv_debug.log", "w");
    if (log_file) {
        // Redirect stderr to the log file. This will capture all LOG_* macros.
        dup2(fileno(log_file), STDERR_FILENO);
        setvbuf(stderr, NULL, _IOLBF, 0); // Line-buffer stderr
    }
}

void log_message(LogLevel level, const char *file, int line, const char *format, ...) {
    if (level > LOG_LEVEL || !log_file) {
        return;
    }

    // Get current time
    time_t now = time(0);
    struct tm *local_time = localtime(&now);
    char time_buf[20];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", local_time);

    // Format log message prefix
    const char *level_str = "";
    switch (level) {
        case LOG_LEVEL_DEBUG: level_str = "DEBUG"; break;
        case LOG_LEVEL_INFO:  level_str = "INFO "; break;
        case LOG_LEVEL_WARN:  level_str = "WARN "; break;
        case LOG_LEVEL_ERROR: level_str = "ERROR"; break;
    }

    fprintf(stderr, "%s %s ", time_buf, level_str);
    
    // Print the actual log message
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    // Print source location
    fprintf(stderr, " (%s:%d)\n", file, line);
    fflush(stderr);
} 