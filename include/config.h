#ifndef CONFIG_H
#define CONFIG_H

#include "error_context.h"
#include <stddef.h>

// --- Configuration Structure ---

typedef struct {
    // Display settings
    int max_field_len;
    int max_cols;
    int max_column_width;
    int min_column_width;
    int buffer_pool_size;
    
    // Cache settings  
    int cache_size;
    size_t cache_string_pool_size;
    int intern_table_size;
    int max_truncated_versions;
    int cache_threshold_lines;
    int cache_threshold_cols;
    
    // I/O settings
    size_t buffer_size;
    int delimiter_detection_sample_size;
    int line_estimation_sample_size;
    int default_chars_per_line;
    
    // Analysis settings
    int column_analysis_sample_lines;
} DSVConfig;


// --- Public Function Declarations ---

/**
 * @brief Initializes the configuration with sensible default values.
 * 
 * @param config A pointer to the DSVConfig struct to initialize.
 */
void config_init_defaults(DSVConfig *config);

/**
 * @brief Loads configuration from a file, overriding defaults.
 * 
 * @param config A pointer to the DSVConfig struct to populate.
 * @param filename The path to the configuration file.
 * @return DSV_OK on success, DSV_ERROR_FILE_IO on file error.
 */
DSVResult config_load_from_file(DSVConfig *config, const char *filename);

/**
 * @brief Validates the current configuration settings.
 * 
 * @param config A pointer to the DSVConfig struct to validate.
 * @return DSV_OK if the configuration is valid, DSV_ERROR otherwise.
 */
DSVResult config_validate(const DSVConfig *config);

#endif // CONFIG_H 