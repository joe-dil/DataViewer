# DataViewer Project Notes

## Recent Changes

### Column Highlighting Fix
**Date**: Current session
**Issue**: Column highlighting was continuing past the end of actual data rows, making the display look sloppy compared to the well-handled row highlighting.

**Root Cause**: The `apply_column_highlight()` function was being called with `display_rows` (total terminal rows minus status line) as the end parameter, which meant it highlighted empty rows below the actual data.

**Solution**: Modified `src/ui/display.c` to calculate the actual end row that contains data before applying column highlighting:

1. **With Header**: Calculate `data_end_row = screen_start_row + remaining_data_rows` where `remaining_data_rows = visible_row_count - start_row`
2. **Without Header**: Calculate `data_end_row = remaining_data_rows` directly
3. **Bounds Check**: Ensure `data_end_row` doesn't exceed `display_rows`

**Code Changes**:
- Updated column highlighting logic in `display_table_view()` function
- Added proper calculation of data boundaries for both header and no-header cases
- Maintained the same logic pattern used by row highlighting (which already worked correctly)

**Result**: Column highlighting now stops at the end of actual data, matching the clean appearance of row highlighting.

### Test Suite Segmentation Fault Fix
**Date**: Current session
**Issue**: `make test` was failing with a segmentation fault in the "File DS | Creation" test.

**Root Cause**: The `DSVViewer` struct in the test fixture was not being properly initialized to zero, containing garbage data that caused crashes during cleanup when the cache system tried to free uninitialized pointers.

**Solution**: Added proper initialization in `tests/unit/test_data_source.c`:
```c
static void setup_file_ds_test(FileDSTestFixture* fixture, const char* content) {
    // Initialize the entire fixture to zero
    memset(fixture, 0, sizeof(FileDSTestFixture));
    // ... rest of setup
}
```

**Result**: All 63 tests now pass successfully, including the previously failing data source tests.

### Unified Sorting Function Refactor
**Date**: Current session
**Issue**: The sorting system had separate comparison functions for numeric and string sorting, creating unnecessary complexity and code duplication.

**Solution**: Refactored `src/core/sorting.c` to use a single unified comparison function:

1. **Simplified Structure**: Added `is_numeric` flag to `DecoratedRow` struct to track data type
2. **Unified Logic**: Single `compare_decorated_rows()` function handles both numeric and string comparisons
3. **Maintained Stability**: Preserved stable sorting behavior with `original_index` tie-breaking
4. **Reduced Code**: Eliminated duplicate comparison logic and simplified the sorting flow

**Code Changes**:
- Replaced `compare_decorated_rows_numeric()` and `compare_decorated_rows_string()` with single `compare_decorated_rows()`
- Updated `DecoratedRow` struct to include `is_numeric` field
- Simplified qsort call to use single comparison function
- Maintained all existing functionality and stability guarantees

**Result**: Cleaner, more maintainable sorting code with identical functionality and better UX for frequency analysis sorting.

## Application Architecture

### Core Components
- **DataViewer**: C-based CSV viewer with ncurses terminal UI
- **DataSource**: Abstraction layer supporting both file-based and in-memory data
- **View System**: Multiple views of the same data with independent cursors, sorting, and selection
- **Search**: Find functionality with wrap-around and status feedback
- **Highlighting**: Row, column, and cell highlighting with proper boundary handling

### Key Features
- File-based and in-memory data sources
- Column sorting (numeric vs string)
- Row selection and view creation from selections
- Search with find next capability ("/", "n" keys)
- Multiple views with Tab/Shift+Tab navigation
- Status bar with comprehensive information
- Frequency analysis functionality
- Extensive test suite (63 passing tests)

### Technical Details
- Uses ncurses for terminal UI
- File-based logging to avoid UI corruption (`dsv_debug.log`)
- Proper memory management with cleanup functions
- Comprehensive error handling and boundary checking
- Unicode support with ASCII fallback

## Code Quality Observations

### ✅ Good Practices Observed

1. **Consistent Boundary Checking**
   - Row highlighting correctly checks data boundaries before applying highlighting
   - Functions like `draw_data_row()` check `view_data_row >= current_view->visible_row_count` before processing
   - Pattern: Always validate data exists before attempting to render or highlight it

2. **Modular Function Design**
   - Functions like `apply_row_highlight()` and `apply_column_highlight()` are focused and single-purpose
   - Display logic is separated into helper functions (`get_column_width()`, `add_separator_if_needed()`)
   - Each function has clear responsibilities

3. **Proper Error Handling**
   - NULL pointer checks with `CHECK_NULL_RET()` macros
   - Graceful handling of edge cases (empty views, out-of-bounds access)
   - Error messages provide helpful context

4. **Memory Safety**
   - Proper cleanup functions for allocated resources
   - Bounds checking for array access
   - Safe string operations with size limits

### ❌ Issues Fixed

1. **Inconsistent Boundary Logic**
   - **Issue**: Column highlighting was applied to all terminal rows regardless of data presence
   - **Fix**: Applied same boundary checking pattern used by row highlighting
   - **Lesson**: When fixing one area (row highlighting), ensure similar areas (column highlighting) use the same pattern

2. **UI Corruption from Logging**
   - **Previous Issue**: Direct `printf()` calls corrupted ncurses display
   - **Fix**: File-based logging with `dsv_debug.log`
   - **Lesson**: Terminal applications must never write directly to stdout/stderr during UI operations

3. **Uninitialized Memory in Tests**
   - **Issue**: Test structs containing `DSVViewer` were not properly initialized, causing segfaults during cleanup
   - **Fix**: Added `memset(fixture, 0, sizeof(FileDSTestFixture))` in test setup
   - **Lesson**: Always initialize structs to zero, especially when they contain pointers that cleanup functions will try to free

## Code Patterns to Follow

### Display Logic Pattern
```c
// 1. Check data boundaries
if (view_data_row >= current_view->visible_row_count) {
    continue; // Skip empty rows
}

// 2. Process actual data
// ... draw/highlight content ...

// 3. Apply highlighting only to rows with data
if (condition_met) {
    apply_highlight(screen_row, start_col, actual_content_width);
}
```

### Error Handling Pattern
```c
// 1. Validate inputs
CHECK_NULL_RET(pointer, default_value);

// 2. Check bounds
if (index >= array_size) {
    return ERROR_VALUE;
}

// 3. Provide helpful error messages
set_error_message(viewer, "Descriptive error: %s", context);
```

### Memory Management Pattern
```c
// 1. Allocate with error checking
ptr = malloc(size);
if (!ptr) {
    // Handle allocation failure
    return ERROR;
}

// 2. Always provide cleanup function
void cleanup_resource(Resource *res) {
    if (res && res->data) {
        free(res->data);
        res->data = NULL;
    }
}
```

## Architecture Strengths

1. **Separation of Concerns**: UI, data processing, and business logic are well separated
2. **Extensible Design**: DataSource abstraction allows multiple data backends
3. **Comprehensive Testing**: 63 passing tests provide good coverage
4. **Consistent Naming**: Functions and variables follow clear naming conventions

## Areas for Improvement

1. **Magic Numbers**: Some constants could be better defined and documented
2. **Error Recovery**: Some error conditions could be handled more gracefully rather than just displaying messages
3. **Secondary Sort Keys**: Could implement more sophisticated tie-breaking for frequency analysis views

## Known Issues
- None currently identified

## TODO Tasks
- Add vi-style navigation keys (hjkl) as alternatives to arrow keys
- Document cache system's memory allocation strategy and ownership model
- Consider refactoring to explicit MVC pattern for better separation of concerns 