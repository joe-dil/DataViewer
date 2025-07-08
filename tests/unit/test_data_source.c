#include "framework/test_runner.h"
#include "core/data_source.h"
#include "memory/in_memory_table.h"
#include "core/parser.h"
#include "app_init.h"
#include "config.h"
#include "file_io.h"
#include <string.h>
#include <unistd.h>
#include <stdio.h>

// --- Test Fixture for File-based DataSource ---

typedef struct {
    DSVViewer viewer;
    DSVConfig config;
    const char* test_filename;
} FileDSTestFixture;

static void setup_file_ds_test(FileDSTestFixture* fixture, const char* content) {
    // Initialize the entire fixture to zero
    memset(fixture, 0, sizeof(FileDSTestFixture));
    
    config_init_defaults(&fixture->config);
    fixture->test_filename = "test_file_ds.csv";
    
    FILE* f = fopen(fixture->test_filename, "w");
    if (f) {
        fputs(content, f);
        fclose(f);
    }

    // Use the public init_viewer function
    init_viewer(&fixture->viewer, fixture->test_filename, 0, &fixture->config);
    fixture->viewer.main_data_source = create_file_data_source(&fixture->viewer);
}

static void teardown_file_ds_test(FileDSTestFixture* fixture) {
    cleanup_viewer(&fixture->viewer);
    unlink(fixture->test_filename);
}

// --- Test Cases for File DataSource ---

static void test_file_ds_creation() {
    FileDSTestFixture fixture;
    setup_file_ds_test(&fixture, "h1,h2\na,b\nc,d");
    
    ASSERT_NOT_NULL(fixture.viewer.main_data_source);
    ASSERT_EQ(fixture.viewer.main_data_source->type, DATA_SOURCE_FILE);
    
    teardown_file_ds_test(&fixture);
}

static void test_file_ds_counts() {
    FileDSTestFixture fixture;
    setup_file_ds_test(&fixture, "h1,h2,h3\na,b,c\nd,e,f\ng,h,i");
    
    DataSource* ds = fixture.viewer.main_data_source;
    ASSERT_EQ(ds->ops->get_row_count(ds->context), 3);
    ASSERT_EQ(ds->ops->get_col_count(ds->context), 3);
    
    teardown_file_ds_test(&fixture);
}

static void test_file_ds_get_cell() {
    FileDSTestFixture fixture;
    setup_file_ds_test(&fixture, "h1,h2\na,b\nc,d");
    
    DataSource* ds = fixture.viewer.main_data_source;
    FieldDesc fd = ds->ops->get_cell(ds->context, 1, 0); // row 2 ("c"), col 1 ("h1")
    
    char buffer[20];
    render_field(&fd, buffer, sizeof(buffer));
    
    ASSERT_EQ(strcmp(buffer, "c"), 0);
    
    teardown_file_ds_test(&fixture);
}

static void test_file_ds_get_header() {
    FileDSTestFixture fixture;
    setup_file_ds_test(&fixture, "header1,header2\na,b\nc,d");
    
    DataSource* ds = fixture.viewer.main_data_source;
    FieldDesc fd = ds->ops->get_header(ds->context, 1); // col 2 ("header2")
    
    char buffer[20];
    render_field(&fd, buffer, sizeof(buffer));
    
    ASSERT_EQ(strcmp(buffer, "header2"), 0);
    
    teardown_file_ds_test(&fixture);
}


// --- Test Cases for In-Memory DataSource ---

static InMemoryTable* create_test_mem_table() {
    const char *headers[] = {"id", "name", "value"};
    InMemoryTable* table = create_in_memory_table("Test Table", 3, headers);
    
    const char *row1[] = {"1", "alpha", "100"};
    const char *row2[] = {"2", "beta", "200"};
    add_in_memory_table_row(table, row1);
    add_in_memory_table_row(table, row2);
    
    return table;
}

static void test_memory_ds_creation() {
    InMemoryTable* table = create_test_mem_table();
    DataSource* ds = create_memory_data_source(table);

    ASSERT_NOT_NULL(ds);
    ASSERT_EQ(ds->type, DATA_SOURCE_MEMORY);
    ASSERT_NOT_NULL(ds->ops);
    
    destroy_data_source(ds); // This should also free the table
}

static void test_memory_ds_counts() {
    InMemoryTable* table = create_test_mem_table();
    DataSource* ds = create_memory_data_source(table);

    ASSERT_EQ(ds->ops->get_row_count(ds->context), 2);
    ASSERT_EQ(ds->ops->get_col_count(ds->context), 3);
    
    destroy_data_source(ds);
}

static void test_memory_ds_get_cell() {
    InMemoryTable* table = create_test_mem_table();
    DataSource* ds = create_memory_data_source(table);

    FieldDesc fd = ds->ops->get_cell(ds->context, 1, 1); // Row 2, "name" column
    
    char buffer[20];
    render_field(&fd, buffer, sizeof(buffer));
    
    ASSERT_EQ(strcmp(buffer, "beta"), 0);

    destroy_data_source(ds);
}

static void test_memory_ds_get_header() {
    InMemoryTable* table = create_test_mem_table();
    DataSource* ds = create_memory_data_source(table);

    FieldDesc fd = ds->ops->get_header(ds->context, 2); // "value" column
    
    char buffer[20];
    render_field(&fd, buffer, sizeof(buffer));
    
    ASSERT_EQ(strcmp(buffer, "value"), 0);

    destroy_data_source(ds);
}

// --- Test Suite Definition ---

TestCase data_source_tests[] = {
    {"Memory DS | Creation", test_memory_ds_creation},
    {"Memory DS | Row/Col Counts", test_memory_ds_counts},
    {"Memory DS | Get Cell", test_memory_ds_get_cell},
    {"Memory DS | Get Header", test_memory_ds_get_header},
    {"File DS | Creation", test_file_ds_creation},
    {"File DS | Row/Col Counts", test_file_ds_counts},
    {"File DS | Get Cell", test_file_ds_get_cell},
    {"File DS | Get Header", test_file_ds_get_header},
};

int data_source_suite_size = sizeof(data_source_tests) / sizeof(TestCase); 