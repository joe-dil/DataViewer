#include "delimiter.h"

char detect_delimiter(const char *data, size_t length) {
    int comma_count = 0, pipe_count = 0, tab_count = 0, semicolon_count = 0;
    size_t sample_size = length > 10000 ? 10000 : length;
    
    // Only scan the first line for delimiters
    for (size_t i = 0; i < sample_size && data[i] != '\n'; i++) {
        switch (data[i]) {
            case ',': comma_count++; break;
            case '|': pipe_count++; break;
            case '\t': tab_count++; break;
            case ';': semicolon_count++; break;
        }
    }
    
    if (pipe_count > comma_count && pipe_count > tab_count && pipe_count > semicolon_count) {
        return '|';
    } else if (tab_count > comma_count && tab_count > semicolon_count) {
        return '\t';
    } else if (semicolon_count > comma_count) {
        return ';';
    }
    return ','; // Default to comma
} 