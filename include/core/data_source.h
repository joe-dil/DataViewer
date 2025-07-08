#ifndef DATA_SOURCE_H
#define DATA_SOURCE_H

#include "core/field_desc.h"
#include <stddef.h>

// Forward declarations to avoid circular dependencies.
struct DataSource;
struct InMemoryTable;
struct DSVViewer;

/**
 * @brief Enum to identify the type of the underlying data source.
 */
typedef enum {
    DATA_SOURCE_FILE,
    DATA_SOURCE_MEMORY
} DataSourceType;

/**
 * @brief A struct of function pointers that define the interface for a data source.
 *
 * This allows the application to interact with different kinds of data sources
 * (e.g., a file on disk or an in-memory table) through a common API.
 */
typedef struct {
    size_t (*get_row_count)(void *context);
    size_t (*get_col_count)(void *context);
    FieldDesc (*get_cell)(void *context, size_t row, size_t col);
    FieldDesc (*get_header)(void *context, size_t col);
    int (*get_column_width)(void *context, size_t col);
    void (*destroy)(void *context);
} DataSourceOps;

/**
 * @brief A generic data source that can be backed by a file or in-memory data.
 *
 * This struct is the primary handle for data access throughout the application.
 * It combines a void pointer to a context-specific struct with a table of
 * function pointers (ops) that operate on that context.
 */
typedef struct DataSource {
    void *context;
    const DataSourceOps *ops;
    DataSourceType type;
} DataSource;

/**
 * @brief Creates a new data source backed by a file.
 *
 * @param viewer A pointer to the main viewer struct, which contains file data.
 * @return A pointer to the new DataSource, or NULL on failure.
 */
DataSource* create_file_data_source(struct DSVViewer *viewer);

/**
 * @brief Creates a new data source backed by an in-memory table.
 *
 * @param table A pointer to the InMemoryTable that will serve as the data source.
 * @return A pointer to the new DataSource, or NULL on failure.
 */
DataSource* create_memory_data_source(struct InMemoryTable *table);

/**
 * @brief Frees the resources associated with a data source.
 *
 * @param data_source The data source to destroy.
 */
void destroy_data_source(DataSource *data_source);

#endif // DATA_SOURCE_H 