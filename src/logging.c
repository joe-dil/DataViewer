#include "logging.h"
#include <time.h>
#include <string.h>

// --- Color Codes for Terminal Output ---
#define LOG_COLOR_RED     "\x1b[31m"
#define LOG_COLOR_YELLOW  "\x1b[33m"
#define LOG_COLOR_BLUE    "\x1b[34m"
#define LOG_COLOR_GRAY    "\x1b[90m"
#define LOG_COLOR_RESET   "\x1b[0m"

// --- Global State ---
static LogLevel g_current_log_level = LOG_LEVEL_INFO;
static FILE* g_log_output = NULL;

// --- Internal Helper ---
static const char* level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_DEBUG: return "DEBUG";
    }
    return "UNKNOWN";
}

static const char* level_to_color(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_ERROR: return LOG_COLOR_RED;
        case LOG_LEVEL_WARN:  return LOG_COLOR_YELLOW;
        case LOG_LEVEL_INFO:  return LOG_COLOR_BLUE;
        case LOG_LEVEL_DEBUG: return LOG_COLOR_GRAY;
    }
    return LOG_COLOR_RESET;
}

// --- Public Function Implementations ---

void init_logging(LogLevel level, const char* log_file) {
    g_current_log_level = level;
    if (log_file && strcmp(log_file, "-") != 0) {
        g_log_output = fopen(log_file, "a");
        if (!g_log_output) {
            perror("Failed to open log file");
            g_log_output = stderr;
        }
    } else {
        g_log_output = stderr;
    }
}

void log_message(LogLevel level, const char* file, int line, const char* fmt, ...) {
    if (level > g_current_log_level || !g_log_output) {
        return;
    }

    // Timestamp
    time_t timer;
    char time_buffer[26];
    struct tm* tm_info;
    time(&timer);
    tm_info = localtime(&timer);
    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    // Terminal colors
    const char* color_str = "";
    const char* reset_str = "";
    if (g_log_output == stderr) {
        color_str = level_to_color(level);
        reset_str = LOG_COLOR_RESET;
    }
    
    // Log prefix
    fprintf(g_log_output, "%s %s%-5s%s ", time_buffer, color_str, level_to_string(level), reset_str);

    // Message
    va_list args;
    va_start(args, fmt);
    vfprintf(g_log_output, fmt, args);
    va_end(args);

    // Source location
    if (level <= LOG_LEVEL_WARN) {
        fprintf(g_log_output, " %s(%s:%d)%s", LOG_COLOR_GRAY, file, line, reset_str);
    }

    fprintf(g_log_output, "\n");
    fflush(g_log_output);
} 