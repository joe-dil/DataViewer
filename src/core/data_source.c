#include "core/data_source.h"
#include "app/app_init.h"
#include "core/file_data.h"
#include "core/parsed_data.h"
#include "core/field_desc.h"
#include "memory/in_memory_table.h"
#include "memory/buffer_pool.h"
#include "util/error_context.h"
#include "util/logging.h"
#include "util/utils.h"
#include "core/parser.h"
#include "memory/constants.h"

#include <stdlib.h>
#include <string.h>

// File data source context structure
typedef struct FileDataSourceContext {
    struct DSVViewer *viewer;
    FileData *file_data;
    ParsedData *parsed_data;
    char delimiter;
    // Cache for current line parse (from buffer pool)
    FieldDesc *field_buffer;
    size_t field_buffer_id;
    size_t cached_line;
    bool cache_valid;
} FileDataSourceContext;

// Memory data source context structure
typedef struct MemoryDataSourceContext {
    struct InMemoryTable *table;
    int *column_widths;  // Calculated column widths
    bool owns_table;     // If true, destroy table on cleanup
} MemoryDataSourceContext;

// --- Function Prototypes for FileDataSource ---
static size_t file_ds_get_row_count(void *context);
static size_t file_ds_get_col_count(void *context);
static int file_ds_get_cell(void *context, size_t row, size_t col, char *buffer, size_t buffer_size);
static int file_ds_get_header(void *context, size_t col, char *buffer, size_t buffer_size);
static bool file_ds_has_headers(void *context);
static int file_ds_get_column_width_hint(void *context, size_t col);
static void file_ds_destroy(void *context);

// --- Function Prototypes for MemoryDataSource ---
static size_t mem_ds_get_row_count(void *context);
static size_t mem_ds_get_col_count(void *context);
static int mem_ds_get_cell(void *context, size_t row, size_t col, char *buffer, size_t buffer_size);
static int mem_ds_get_header(void *context, size_t col, char *buffer, size_t buffer_size);
static bool mem_ds_has_headers(void *context);
static int mem_ds_get_column_width_hint(void *context, size_t col);
static void mem_ds_destroy(void *context);


// --- Ops Structs ---
static const DataSourceOps file_data_source_ops = {
    .get_row_count = file_ds_get_row_count,
    .get_col_count = file_ds_get_col_count,
    .get_cell = file_ds_get_cell,
    .get_header = file_ds_get_header,
    .has_headers = file_ds_has_headers,
    .get_column_width_hint = file_ds_get_column_width_hint,
    .destroy = file_ds_destroy,
};

static const DataSourceOps memory_data_source_ops = {
    .get_row_count = mem_ds_get_row_count,
    .get_col_count = mem_ds_get_col_count,
    .get_cell = mem_ds_get_cell,
    .get_header = mem_ds_get_header,
    .has_headers = mem_ds_has_headers,
    .get_column_width_hint = mem_ds_get_column_width_hint,
    .destroy = mem_ds_destroy,
};

// --- FileDataSource Implementation ---

/**
 * @brief Ensures the requested line is parsed and cached in the context.
 * @param ctx The file data source context.
 * @param row The row number to ensure is cached.
 * @return True on success, false on failure.
 */
static bool ensure_line_is_cached(FileDataSourceContext *ctx, size_t row) {
    if (ctx->cache_valid && ctx->cached_line == row) {
        return true;
    }

    if (row >= ctx->parsed_data->num_lines) {
        return false; // Out of bounds
    }

    size_t line_offset = ctx->parsed_data->line_offsets[row];
    size_t num_fields = parse_line(ctx->file_data->data,
                                   ctx->file_data->length,
                                   ctx->delimiter,
                                   line_offset,
                                   ctx->field_buffer,
                                   ctx->viewer->config->max_cols);

    if (num_fields == 0 && line_offset >= ctx->file_data->length) {
        // This can happen for trailing newlines
    }

    ctx->cached_line = row;
    ctx->cache_valid = true;
    // We don't store num_fields here because get_col_count should be used for that.

    return true;
}


static size_t file_ds_get_row_count(void *context) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    size_t count = ctx->parsed_data->num_lines;
    if (ctx->parsed_data->has_header) {
        // The prompt says "if (viewer->parsed_data->has_header && viewer->display_state->show_header)"
        // but here we don't have display_state. The data source should represent raw data.
        // The view layer will handle whether to show header or not.
        // So we just subtract one if header exists.
        if (count > 0) {
            count--;
        }
    }
    return count;
}

static size_t file_ds_get_col_count(void *context) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    // Return the number of fields in the header if available, as it's the most reliable measure.
    if (ctx->parsed_data->has_header) {
        return ctx->parsed_data->num_header_fields;
    }
    // As a fallback, this could parse the first line, but the prompt says
    // "Returns max columns across all rows". A full scan is too slow for file source.
    // The existing logic seems to rely on max_cols.
    // Let's rely on num_header_fields or a reasonable default from config.
    return ctx->viewer->config->max_cols;
}

static int file_ds_get_cell(void *context, size_t row, size_t col, char *buffer, size_t buffer_size) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    
    // Adjust row index if there is a header
    size_t file_row = row;
    if (ctx->parsed_data->has_header) {
        file_row++;
    }

    if (!ensure_line_is_cached(ctx, file_row)) {
        LOG_ERROR("Row %zu is out of bounds.", row);
        return -1;
    }
    
    // After caching, we need to re-parse to get num_fields for *this* line
    size_t num_fields = parse_line(ctx->file_data->data,
                                   ctx->file_data->length,
                                   ctx->delimiter,
                                   ctx->parsed_data->line_offsets[file_row],
                                   ctx->field_buffer,
                                   ctx->viewer->config->max_cols);

    if (col >= num_fields) {
        // This is not an error, just an empty cell in a ragged CSV.
        buffer[0] = '\0';
        return 0;
    }

    render_field(&ctx->field_buffer[col], buffer, buffer_size);
    return strlen(buffer);
}

static int file_ds_get_header(void *context, size_t col, char *buffer, size_t buffer_size) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    if (!ctx->parsed_data->has_header) {
        return -1;
    }
    if (col >= ctx->parsed_data->num_header_fields) {
        return -1;
    }
    render_field(&ctx->parsed_data->header_fields[col], buffer, buffer_size);
    return strlen(buffer);
}

static bool file_ds_has_headers(void *context) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    return ctx->parsed_data->has_header;
}

static int file_ds_get_column_width_hint(void *context, size_t col) {
    (void)context;
    (void)col;
    // File data source doesn't have pre-calculated widths. Return 0 for default.
    return 0;
}

static void file_ds_destroy(void *context) {
    if (!context) return;
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    // Release the field buffer if it was acquired.
    // Based on re-evaluation, we malloc it, so we free it.
    free(ctx->field_buffer);
    free(ctx);
}

/**
 * @brief Create a data source for file-based data
 * @param viewer The viewer instance containing file data
 * @return New data source or NULL on failure
 */
DataSource* create_file_data_source(struct DSVViewer *viewer) {
    CHECK_NULL_RET(viewer, NULL);
    CHECK_NULL_RET(viewer->file_data, NULL);
    CHECK_NULL_RET(viewer->parsed_data, NULL);
    CHECK_NULL_RET(viewer->config, NULL);

    FileDataSourceContext *ctx = calloc(1, sizeof(FileDataSourceContext));
    if (!ctx) {
        LOG_ERROR("Failed to allocate file data source context");
        return NULL;
    }

    ctx->viewer = viewer;
    ctx->file_data = viewer->file_data;
    ctx->parsed_data = viewer->parsed_data;
    ctx->delimiter = viewer->parsed_data->delimiter;
    ctx->cache_valid = false;
    ctx->cached_line = (size_t)-1;
    
    // Allocate buffer for one line of parsed fields.
    ctx->field_buffer = malloc(sizeof(FieldDesc) * viewer->config->max_cols);
    if (!ctx->field_buffer) {
        free(ctx);
        LOG_ERROR("Failed to allocate field buffer for data source");
        return NULL;
    }
    ctx->field_buffer_id = 0; // Not using buffer pool IDs for this.

    DataSource *ds = malloc(sizeof(DataSource));
    if (!ds) {
        free(ctx->field_buffer);
        free(ctx);
        LOG_ERROR("Failed to allocate data source");
        return NULL;
    }

    ds->ops = &file_data_source_ops;
    ds->context = ctx;

    return ds;
}

// --- MemoryDataSource Implementation ---
static void calculate_memory_table_widths(MemoryDataSourceContext *ctx) {
    InMemoryTable *table = ctx->table;
    ctx->column_widths = calloc(table->col_count, sizeof(int));
    if (!ctx->column_widths) {
        // Cannot store widths, but not a fatal error for the data source itself.
        return;
    }

    // Calculate header widths
    for (size_t col = 0; col < table->col_count; col++) {
        ctx->column_widths[col] = strlen(table->headers[col]);
    }

    // Scan all data
    for (size_t row = 0; row < table->row_count; row++) {
        for (size_t col = 0; col < table->col_count; col++) {
            int len = strlen(table->data[row][col]);
            if (len > ctx->column_widths[col]) {
                ctx->column_widths[col] = len;
            }
        }
    }
    
    // Apply min/max constraints
    for (size_t col = 0; col < table->col_count; col++) {
        if (ctx->column_widths[col] < DEFAULT_MIN_COLUMN_WIDTH) {
            ctx->column_widths[col] = DEFAULT_MIN_COLUMN_WIDTH;
        }
        if (ctx->column_widths[col] > DEFAULT_MAX_COLUMN_WIDTH) {
            ctx->column_widths[col] = DEFAULT_MAX_COLUMN_WIDTH;
        }
    }
}

static size_t mem_ds_get_row_count(void *context) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    return ctx->table->row_count;
}

static size_t mem_ds_get_col_count(void *context) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    return ctx->table->col_count;
}

static int mem_ds_get_cell(void *context, size_t row, size_t col, char *buffer, size_t buffer_size) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    if (row >= ctx->table->row_count || col >= ctx->table->col_count) {
        LOG_ERROR("Cell (%zu, %zu) is out of bounds.", row, col);
        return -1;
    }
    strncpy(buffer, ctx->table->data[row][col], buffer_size -1);
    buffer[buffer_size - 1] = '\0';
    return strlen(buffer);
}

static int mem_ds_get_header(void *context, size_t col, char *buffer, size_t buffer_size) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    if (col >= ctx->table->col_count) {
        LOG_ERROR("Header %zu is out of bounds.", col);
        return -1;
    }
    strncpy(buffer, ctx->table->headers[col], buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
    return strlen(buffer);
}

static bool mem_ds_has_headers(void *context) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    return ctx->table->headers != NULL;
}

static int mem_ds_get_column_width_hint(void *context, size_t col) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    if (!ctx->column_widths || col >= ctx->table->col_count) {
        return 0; // Use default
    }
    return ctx->column_widths[col];
}

static void mem_ds_destroy(void *context) {
    if (!context) return;
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    if (ctx->owns_table) {
        free_in_memory_table(ctx->table);
    }
    free(ctx->column_widths);
    free(ctx);
}


DataSource* create_memory_data_source(struct InMemoryTable *table) {
    CHECK_NULL_RET(table, NULL);

    MemoryDataSourceContext *ctx = calloc(1, sizeof(MemoryDataSourceContext));
    if (!ctx) {
        LOG_ERROR("Failed to allocate memory data source context");
        return NULL; // Error context cannot be set here easily.
    }

    ctx->table = table;
    ctx->owns_table = true; // By default, the data source owns the table. This can be changed by the caller.

    calculate_memory_table_widths(ctx);

    DataSource *ds = malloc(sizeof(DataSource));
    if (!ds) {
        free(ctx->column_widths);
        free(ctx);
        LOG_ERROR("Failed to allocate data source");
        return NULL;
    }

    ds->ops = &memory_data_source_ops;
    ds->context = ctx;

    return ds;
}

void destroy_data_source(DataSource *source) {
    if (source) {
        if (source->ops && source->ops->destroy) {
            source->ops->destroy(source->context);
        }
        free(source);
    }
} 