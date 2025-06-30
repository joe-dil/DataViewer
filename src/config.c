#include "config.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// --- Default Values ---

void config_init_defaults(DSVConfig *config) {
    if (!config) return;

    // Display
    config->max_field_len = 1024;
    config->max_cols = 256;
    config->max_column_width = 16;
    config->min_column_width = 4;
    config->buffer_pool_size = 5;

    // Cache
    config->cache_size = 16384;
    config->cache_string_pool_size = 4 * 1024 * 1024; // 4MB
    config->intern_table_size = 4096;
    config->max_truncated_versions = 8;
    config->cache_threshold_lines = 500;
    config->cache_threshold_cols = 20;

    // I/O
    config->buffer_size = 8192;
    config->delimiter_detection_sample_size = 1024;
    config->line_estimation_sample_size = 4096;
    config->default_chars_per_line = 80;
    
    // Analysis
    config->column_analysis_sample_lines = 1000;
}


// --- Helper Functions for Parsing ---

static char* trim_whitespace(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// --- Configuration Loading ---

DSVResult config_load_from_file(DSVConfig *config, const char *filename) {
    if (!config || !filename) {
        LOG_WARN("Cannot load config from NULL filename.");
        return DSV_ERROR_INVALID_ARGS;
    }
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        LOG_WARN("Configuration file not found: %s", filename);
        return DSV_ERROR_FILE_IO;
    }

    char line[256];
    int line_num = 0;
    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char *trimmed_line = trim_whitespace(line);
        if (trimmed_line[0] == '#' || trimmed_line[0] == '\0') continue;
        
        char *key = strtok(trimmed_line, "=");
        char *value = strtok(NULL, "=");
        
        if (!key || !value) {
            LOG_WARN("Invalid format on line %d of %s", line_num, filename);
            continue;
        }
        
        key = trim_whitespace(key);
        value = trim_whitespace(value);
        
        // Use a macro to simplify setting config values
        #define SET_CONFIG(name, format, type) \
            if (strcmp(key, #name) == 0) { sscanf(value, format, (type*)&config->name); }

        // Display
        SET_CONFIG(max_field_len, "%d", int)
        else SET_CONFIG(max_cols, "%d", int)
        else SET_CONFIG(max_column_width, "%d", int)
        else SET_CONFIG(min_column_width, "%d", int)
        else SET_CONFIG(buffer_pool_size, "%d", int)
        // Cache
        else SET_CONFIG(cache_size, "%d", int)
        else SET_CONFIG(cache_string_pool_size, "%zu", size_t)
        else SET_CONFIG(intern_table_size, "%d", int)
        else SET_CONFIG(max_truncated_versions, "%d", int)
        else SET_CONFIG(cache_threshold_lines, "%d", int)
        else SET_CONFIG(cache_threshold_cols, "%d", int)
        // I/O
        else SET_CONFIG(buffer_size, "%zu", size_t)
        else SET_CONFIG(delimiter_detection_sample_size, "%d", int)
        else SET_CONFIG(line_estimation_sample_size, "%d", int)
        else SET_CONFIG(default_chars_per_line, "%d", int)
        // Analysis
        else SET_CONFIG(column_analysis_sample_lines, "%d", int)
        else {
            LOG_WARN("Unknown configuration key '%s' in %s", key, filename);
        }
        
        #undef SET_CONFIG
    }
    
    fclose(file);
    return DSV_OK;
}

// --- Configuration Validation ---

DSVResult config_validate(const DSVConfig *config) {
    if (!config) return DSV_ERROR_INVALID_ARGS;
    
    // Use a macro for simple positive value checks
    #define VALIDATE_POSITIVE(name) \
        if (config->name <= 0) { \
            LOG_ERROR("Invalid config: '%s' must be positive.", #name); \
            return DSV_ERROR; \
        }
    
    // Display
    VALIDATE_POSITIVE(max_field_len)
    VALIDATE_POSITIVE(max_cols)
    VALIDATE_POSITIVE(max_column_width)
    VALIDATE_POSITIVE(min_column_width)
    VALIDATE_POSITIVE(buffer_pool_size)

    // Cache
    VALIDATE_POSITIVE(cache_size)
    VALIDATE_POSITIVE(cache_string_pool_size)
    VALIDATE_POSITIVE(intern_table_size)
    VALIDATE_POSITIVE(max_truncated_versions)
    VALIDATE_POSITIVE(cache_threshold_lines)
    VALIDATE_POSITIVE(cache_threshold_cols)

    // I/O
    VALIDATE_POSITIVE(buffer_size)
    VALIDATE_POSITIVE(delimiter_detection_sample_size)
    VALIDATE_POSITIVE(line_estimation_sample_size)
    VALIDATE_POSITIVE(default_chars_per_line)

    // Analysis
    VALIDATE_POSITIVE(column_analysis_sample_lines)

    #undef VALIDATE_POSITIVE

    if (config->min_column_width > config->max_column_width) {
        LOG_ERROR("Invalid config: 'min_column_width' cannot be greater than 'max_column_width'.");
        return DSV_ERROR;
    }
    
    LOG_DEBUG("Configuration validated successfully.");
    return DSV_OK;
} 