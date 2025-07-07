#include "core/data_source.h"
#include "core/parser.h"
#include "app/app_init.h"
#include "core/file_data.h"
#include "core/parsed_data.h"
#include "memory/in_memory_table.h"
#include "util/logging.h"
#include "memory/constants.h"
#include <stdlib.h>
#include <string.h>

// --- File Data Source ---

typedef struct {
    struct DSVViewer *viewer;
    size_t cached_line_index;
    FieldDesc *cached_fields;
    size_t field_count;
    size_t max_fields;
} FileDataSourceContext;

static size_t file_get_row_count(void *context);
static size_t file_get_col_count(void *context);
static FieldDesc file_get_cell(void *context, size_t row, size_t col);
static FieldDesc file_get_header(void *context, size_t col);
static int file_get_column_width(void *context, size_t col);
static void file_destroy(void *context);

static const DataSourceOps file_ops = {
    .get_row_count = file_get_row_count,
    .get_col_count = file_get_col_count,
    .get_cell = file_get_cell,
    .get_header = file_get_header,
    .get_column_width = file_get_column_width,
    .destroy = file_destroy,
};

static void ensure_file_line_cached(FileDataSourceContext *ctx, size_t row_index) {
    if (ctx->cached_line_index == row_index) {
        return;
    }

    FileData *fd = ctx->viewer->file_data;
    ParsedData *pd = ctx->viewer->parsed_data;
    if (row_index >= pd->num_lines) {
        ctx->field_count = 0;
        ctx->cached_line_index = (size_t)-1;
        return;
    }

    size_t line_offset = pd->line_offsets[row_index];
    ctx->field_count = parse_line(fd->data, fd->length, pd->delimiter, line_offset, ctx->cached_fields, ctx->max_fields);
    ctx->cached_line_index = row_index;
}

// --- Memory Data Source ---

typedef struct {
    struct InMemoryTable *table;
    int *column_widths;
} MemoryDataSourceContext;

static size_t mem_get_row_count(void *context);
static size_t mem_get_col_count(void *context);
static FieldDesc mem_get_cell(void *context, size_t row, size_t col);
static FieldDesc mem_get_header(void *context, size_t col);
static int mem_get_column_width(void *context, size_t col);
static void mem_destroy(void *context);

static const DataSourceOps mem_ops = {
    .get_row_count = mem_get_row_count,
    .get_col_count = mem_get_col_count,
    .get_cell = mem_get_cell,
    .get_header = mem_get_header,
    .get_column_width = mem_get_column_width,
    .destroy = mem_destroy,
};

static void calculate_memory_table_widths(MemoryDataSourceContext *ctx) {
    InMemoryTable *table = ctx->table;
    ctx->column_widths = calloc(table->col_count, sizeof(int));
    if (!ctx->column_widths) {
        return;
    }

    for (size_t col = 0; col < table->col_count; col++) {
        ctx->column_widths[col] = strlen(table->headers[col]);
    }

    for (size_t row = 0; row < table->row_count; row++) {
        for (size_t col = 0; col < table->col_count; col++) {
            int len = strlen(table->data[row][col]);
            if (len > ctx->column_widths[col]) {
                ctx->column_widths[col] = len;
            }
        }
    }
}

// --- Constructors / Destructors ---

DataSource* create_file_data_source(struct DSVViewer *viewer) {
    FileDataSourceContext *ctx = calloc(1, sizeof(FileDataSourceContext));
    if (!ctx) return NULL;

    ctx->viewer = viewer;
    ctx->cached_line_index = (size_t)-1; // -1 indicates no line is cached
    ctx->max_fields = viewer->config->max_cols;
    ctx->cached_fields = malloc(sizeof(FieldDesc) * ctx->max_fields);
    if (!ctx->cached_fields) {
        free(ctx);
        return NULL;
    }

    DataSource *ds = malloc(sizeof(DataSource));
    if (!ds) {
        free(ctx->cached_fields);
        free(ctx);
        return NULL;
    }

    ds->context = ctx;
    ds->ops = &file_ops;
    ds->type = DATA_SOURCE_FILE;

    return ds;
}

DataSource* create_memory_data_source(struct InMemoryTable *table) {
    MemoryDataSourceContext *ctx = calloc(1, sizeof(MemoryDataSourceContext));
    if (!ctx) return NULL;
    ctx->table = table;
    calculate_memory_table_widths(ctx);

    DataSource *ds = malloc(sizeof(DataSource));
    if (!ds) {
        free(ctx->column_widths);
        free(ctx);
        return NULL;
    }
    ds->context = ctx;
    ds->ops = &mem_ops;
    ds->type = DATA_SOURCE_MEMORY;
    return ds;
}

void destroy_data_source(DataSource *data_source) {
    if (data_source) {
        if (data_source->ops && data_source->ops->destroy) {
            data_source->ops->destroy(data_source->context);
        }
        free(data_source);
    }
}

// --- File Data Source Ops Implementation ---

static size_t file_get_row_count(void *context) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    // The view is responsible for deciding whether to skip the header row,
    // so the data source should report the total number of data rows.
    size_t count = ctx->viewer->parsed_data->num_lines;
    if (ctx->viewer->parsed_data->has_header && count > 0) {
        return count - 1;
    }
    return count;
}

static size_t file_get_col_count(void *context) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    return ctx->viewer->parsed_data->num_header_fields;
}

static FieldDesc file_get_cell(void *context, size_t row, size_t col) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    size_t actual_row = row;
    if (ctx->viewer->parsed_data->has_header) {
        actual_row++;
    }
    
    ensure_file_line_cached(ctx, actual_row);

    if (col < ctx->field_count) {
        return ctx->cached_fields[col];
    }
    return (FieldDesc){ .start = NULL, .length = 0, .needs_unescaping = 0 };
}

static FieldDesc file_get_header(void *context, size_t col) {
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    if (ctx->viewer->parsed_data->has_header && col < ctx->viewer->parsed_data->num_header_fields) {
        return ctx->viewer->parsed_data->header_fields[col];
    }
    return (FieldDesc){ .start = NULL, .length = 0, .needs_unescaping = 0 };
}

static int file_get_column_width(void *context, size_t col) {
    (void)context;
    (void)col;
    return DEFAULT_COL_WIDTH; // File source does not pre-calculate widths
}

static void file_destroy(void *context) {
    if (!context) return;
    FileDataSourceContext *ctx = (FileDataSourceContext *)context;
    free(ctx->cached_fields);
    free(ctx);
}

// --- Memory Data Source Ops Implementation ---

static size_t mem_get_row_count(void *context) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    return ctx->table->row_count;
}

static size_t mem_get_col_count(void *context) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    return ctx->table->col_count;
}

static FieldDesc mem_get_cell(void *context, size_t row, size_t col) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    if (row < ctx->table->row_count && col < ctx->table->col_count) {
        const char *cell_data = ctx->table->data[row][col];
        return (FieldDesc){ .start = cell_data, .length = strlen(cell_data), .needs_unescaping = 0 };
    }
    return (FieldDesc){ .start = NULL, .length = 0, .needs_unescaping = 0 };
}

static FieldDesc mem_get_header(void *context, size_t col) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    if (col < ctx->table->col_count) {
        const char *header_data = ctx->table->headers[col];
        return (FieldDesc){ .start = header_data, .length = strlen(header_data), .needs_unescaping = 0 };
    }
    return (FieldDesc){ .start = NULL, .length = 0, .needs_unescaping = 0 };
}

static int mem_get_column_width(void *context, size_t col) {
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    if (ctx->column_widths && col < ctx->table->col_count) {
        return ctx->column_widths[col];
    }
    return DEFAULT_COL_WIDTH;
}

static void mem_destroy(void *context) {
    if (!context) return;
    MemoryDataSourceContext *ctx = (MemoryDataSourceContext *)context;
    // The InMemoryTable is owned by the analysis that created it, which should
    // be cleaned up when the view is closed.
    free_in_memory_table(ctx->table);
    free(ctx->column_widths);
    free(ctx);
} 