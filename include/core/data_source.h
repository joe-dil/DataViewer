#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H

#include <stddef.h>
#include <stdbool.h>

// Forward declarations
struct DSVViewer;
struct InMemoryTable;

// Function pointer types for data source operations
typedef struct DataSourceOps {
    // Core data access
    size_t (*get_row_count)(void *context);
    size_t (*get_col_count)(void *context); // Returns max columns across all rows
    
    // Cell data retrieval - returns bytes written or -1 on error
    int (*get_cell)(void *context, size_t row, size_t col, 
                    char *buffer, size_t buffer_size);
    
    // Header retrieval - returns bytes written or -1 on error
    int (*get_header)(void *context, size_t col, 
                      char *buffer, size_t buffer_size);
    
    // Metadata
    bool (*has_headers)(void *context);
    
    // Column width hint (0 = use default)
    int (*get_column_width_hint)(void *context, size_t col);
    
    // Cleanup
    void (*destroy)(void *context);
} DataSourceOps;

typedef struct DataSource {
    const DataSourceOps *ops;
    void *context;
} DataSource;

// Constructors
DataSource* create_file_data_source(struct DSVViewer *viewer);
DataSource* create_memory_data_source(struct InMemoryTable *table);
void destroy_data_source(DataSource *source);

#endif // DATA_SOURCE_H 