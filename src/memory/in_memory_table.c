#include "in_memory_table.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_TABLE_CAPACITY 16

InMemoryTable* create_in_memory_table(const char *title, size_t col_count, const char **headers) {
    if (col_count == 0 || !headers) {
        return NULL;
    }

    InMemoryTable *table = calloc(1, sizeof(InMemoryTable));
    if (!table) {
        return NULL;
    }

    if (title) {
        table->title = strdup(title);
    }

    table->col_count = col_count;
    table->headers = calloc(col_count, sizeof(char*));
    if (!table->headers) {
        free(table->title);
        free(table);
        return NULL;
    }

    for (size_t i = 0; i < col_count; i++) {
        table->headers[i] = strdup(headers[i]);
        if (!table->headers[i]) {
            // Rollback on failure
            for (size_t j = 0; j < i; j++) {
                free(table->headers[j]);
            }
            free(table->headers);
            free(table->title);
            free(table);
            return NULL;
        }
    }

    table->capacity = INITIAL_TABLE_CAPACITY;
    table->data = calloc(table->capacity, sizeof(char**));
    if (!table->data) {
        for (size_t i = 0; i < col_count; i++) {
            free(table->headers[i]);
        }
        free(table->headers);
        free(table->title);
        free(table);
        return NULL;
    }

    return table;
}

int add_in_memory_table_row(InMemoryTable *table, const char **row_data) {
    if (!table || !row_data) {
        return -1;
    }

    if (table->row_count >= table->capacity) {
        size_t new_capacity = table->capacity * 2;
        char ***new_data = realloc(table->data, new_capacity * sizeof(char**));
        if (!new_data) {
            return -1; // Allocation failure
        }
        table->data = new_data;
        table->capacity = new_capacity;
    }

    char **new_row = calloc(table->col_count, sizeof(char*));
    if (!new_row) {
        return -1;
    }

    for (size_t i = 0; i < table->col_count; i++) {
        new_row[i] = strdup(row_data[i] ? row_data[i] : "");
        if (!new_row[i]) {
            // Rollback
            for (size_t j = 0; j < i; j++) {
                free(new_row[j]);
            }
            free(new_row);
            return -1;
        }
    }

    table->data[table->row_count++] = new_row;
    return 0;
}

void free_in_memory_table(InMemoryTable *table) {
    if (!table) {
        return;
    }

    SAFE_FREE(table->title);

    if (table->headers) {
        for (size_t i = 0; i < table->col_count; i++) {
            free(table->headers[i]);
        }
        free(table->headers);
    }

    if (table->data) {
        for (size_t i = 0; i < table->row_count; i++) {
            if (table->data[i]) {
                for (size_t j = 0; j < table->col_count; j++) {
                    free(table->data[i][j]);
                }
                free(table->data[i]);
            }
        }
        free(table->data);
    }

    free(table);
} 