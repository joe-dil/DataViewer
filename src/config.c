#include "constants.h"
#include "config.h"
#include "logging.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdint.h>

// --- Default Values ---

void config_init_defaults(DSVConfig *config) {
    if (!config) return;

    // Display
    config->max_field_len = DEFAULT_MAX_FIELD_LEN;
    config->max_cols = MAX_COLS;
    config->max_column_width = DEFAULT_MAX_COLUMN_WIDTH;
    config->min_column_width = DEFAULT_MIN_COLUMN_WIDTH;
    config->buffer_pool_size = 5;

    // Cache
    config->cache_size = DEFAULT_CACHE_SIZE;
    config->cache_string_pool_size = DEFAULT_CACHE_STRING_POOL_SIZE;
    config->intern_table_size = DEFAULT_INTERN_TABLE_SIZE;
    config->max_truncated_versions = DEFAULT_MAX_TRUNCATED_VERSIONS;
    config->cache_threshold_lines = DEFAULT_CACHE_THRESHOLD_LINES;
    config->cache_threshold_cols = DEFAULT_CACHE_THRESHOLD_COLS;

    // I/O
    config->buffer_size = DEFAULT_BUFFER_SIZE;
    config->delimiter_detection_sample_size = DEFAULT_DELIMITER_SAMPLE_SIZE;
    config->line_estimation_sample_size = DEFAULT_LINE_SAMPLE_SIZE;
    config->default_chars_per_line = DEFAULT_CHARS_PER_LINE;
    
    // Analysis
    config->column_analysis_sample_lines = DEFAULT_COLUMN_ANALYSIS_LINES;
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

    char line[CONFIG_LINE_BUFFER_SIZE];
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
        
        // Strip comments from value
        char *comment = strchr(value, '#');
        if (comment) {
            *comment = '\0';
            value = trim_whitespace(value);
        }
        
        // Safe config parsing with bounds checking and validation
        #define SET_CONFIG_INT(name) \
            if (strcmp(key, #name) == 0) { \
                char *endptr; \
                long val = strtol(value, &endptr, 10); \
                if (*endptr == '\0' && val >= 0 && val <= INT_MAX) { \
                    config->name = (int)val; \
                } else { \
                    LOG_WARN("Invalid integer value '%s' for '%s' in %s", value, key, filename); \
                } \
            }

        #define SET_CONFIG_SIZE_T(name) \
            if (strcmp(key, #name) == 0) { \
                char *endptr; \
                unsigned long long val = strtoull(value, &endptr, 10); \
                if (*endptr == '\0' && val <= SIZE_MAX) { \
                    config->name = (size_t)val; \
                } else { \
                    LOG_WARN("Invalid size_t value '%s' for '%s' in %s", value, key, filename); \
                } \
            }

        // Display
        SET_CONFIG_INT(max_field_len)
        else SET_CONFIG_INT(max_cols)
        else SET_CONFIG_INT(max_column_width)
        else SET_CONFIG_INT(min_column_width)
        else SET_CONFIG_INT(buffer_pool_size)
        // Cache
        else SET_CONFIG_INT(cache_size)
        else SET_CONFIG_SIZE_T(cache_string_pool_size)
        else SET_CONFIG_INT(intern_table_size)
        else SET_CONFIG_INT(max_truncated_versions)
        else SET_CONFIG_INT(cache_threshold_lines)
        else SET_CONFIG_INT(cache_threshold_cols)
        // I/O
        else SET_CONFIG_SIZE_T(buffer_size)
        else SET_CONFIG_INT(delimiter_detection_sample_size)
        else SET_CONFIG_INT(line_estimation_sample_size)
        else SET_CONFIG_INT(default_chars_per_line)
        // Analysis
        else SET_CONFIG_INT(column_analysis_sample_lines)
        else {
            LOG_WARN("Unknown configuration key '%s' in %s", key, filename);
        }
        
        #undef SET_CONFIG_INT
        #undef SET_CONFIG_SIZE_T
    }
    
    fclose(file);
    return DSV_OK;
}

// --- Configuration Validation ---

DSVResult config_validate(const DSVConfig *config) {
    if (!config) return DSV_ERROR_INVALID_ARGS;
    
    // Use separate macros for int and size_t validation to avoid unsigned comparison errors
    #define VALIDATE_POSITIVE_INT(name) \
        if (config->name <= 0) { \
            LOG_ERROR("Invalid config: '%s' must be positive.", #name); \
            return DSV_ERROR; \
        }

    #define VALIDATE_POSITIVE_SIZE_T(name) \
        if (config->name == 0) { \
            LOG_ERROR("Invalid config: '%s' must be positive.", #name); \
            return DSV_ERROR; \
        }
    
    // Display
    VALIDATE_POSITIVE_INT(max_field_len)
    VALIDATE_POSITIVE_INT(max_cols)
    VALIDATE_POSITIVE_INT(max_column_width)
    VALIDATE_POSITIVE_INT(min_column_width)
    VALIDATE_POSITIVE_INT(buffer_pool_size)

    // Cache
    VALIDATE_POSITIVE_INT(cache_size)
    VALIDATE_POSITIVE_SIZE_T(cache_string_pool_size)
    VALIDATE_POSITIVE_INT(intern_table_size)
    VALIDATE_POSITIVE_INT(max_truncated_versions)
    VALIDATE_POSITIVE_INT(cache_threshold_lines)
    VALIDATE_POSITIVE_INT(cache_threshold_cols)

    // I/O
    VALIDATE_POSITIVE_SIZE_T(buffer_size)
    VALIDATE_POSITIVE_INT(delimiter_detection_sample_size)
    VALIDATE_POSITIVE_INT(line_estimation_sample_size)
    VALIDATE_POSITIVE_INT(default_chars_per_line)

    // Analysis
    VALIDATE_POSITIVE_INT(column_analysis_sample_lines)

    #undef VALIDATE_POSITIVE_INT
    #undef VALIDATE_POSITIVE_SIZE_T

    if (config->min_column_width > config->max_column_width) {
        LOG_ERROR("Invalid config: 'min_column_width' cannot be greater than 'max_column_width'.");
        return DSV_ERROR;
    }
    
    LOG_DEBUG("Configuration validated successfully.");
    return DSV_OK;
} 