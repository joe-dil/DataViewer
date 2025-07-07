#ifndef IN_MEMORY_TABLE_H
#define IN_MEMORY_TABLE_H

#include <stddef.h>

/**
 * @brief A generic structure for holding tabular data in memory.
 *
 * This struct is designed to be self-contained and independent of the UI or
 * any other application component. It stores a header, a 2D array of strings
 * for the data, and the dimensions of the table.
 */
typedef struct {
    char *title;          // Optional title for the data table.
    char **headers;       // Array of strings for column headers.
    char ***data;         // 2D array of strings for row data.
    size_t row_count;     // Number of data rows currently stored.
    size_t col_count;     // Number of columns.
    size_t capacity;      // Allocated row capacity.
} InMemoryTable;

/**
 * @brief Creates a new, empty in-memory table.
 *
 * @param title The title of the table. Can be NULL.
 * @param col_count The number of columns the table will have.
 * @param headers An array of strings for the column headers. The contents are copied.
 * @return A pointer to the newly created InMemoryTable, or NULL on failure.
 */
InMemoryTable* create_in_memory_table(const char *title, size_t col_count, const char **headers);

/**
 * @brief Adds a new row to the in-memory table.
 *
 * The table will automatically resize its internal storage if necessary.
 *
 * @param table The table to add the row to.
 * @param row_data An array of strings representing the data for the new row.
 * @return 0 on success, -1 on failure.
 */
int add_in_memory_table_row(InMemoryTable *table, const char **row_data);

/**
 * @brief Frees all memory associated with an in-memory table.
 *
 * This includes the title, headers, and all row data.
 *
 * @param table The table to free.
 */
void free_in_memory_table(InMemoryTable *table);

#endif // IN_MEMORY_TABLE_H 