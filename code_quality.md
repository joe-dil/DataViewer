# Code Quality and Patterns

This document outlines the observed code quality standards and recommended patterns to follow when contributing to the DataViewer project.

## ✅ Good Practices Observed

1.  **Consistent Boundary Checking**
    *   The row highlighting logic correctly checks data boundaries before applying highlighting.
    *   Functions like `draw_data_row()` validate that a row contains data before attempting to render it.
    *   **Pattern**: Always validate that data exists before attempting to render or highlight it.

2.  **Modular Function Design**
    *   Functions are generally focused and have a single purpose (e.g., `apply_row_highlight()`, `get_column_width()`).
    *   Display logic is separated into helper functions to improve readability and reuse.

3.  **Proper Error Handling**
    *   The codebase uses `CHECK_NULL_RET()` macros for validating pointers.
    *   Edge cases like empty views and out-of-bounds access are handled gracefully.
    *   Error messages provide helpful context to the user.

4.  **Memory Safety**
    *   The application has a clear pattern of providing cleanup functions for allocated resources.
    *   Bounds checking is used for array access.
    *   String operations are generally performed with size limits to prevent buffer overflows.

## ❌ Issues Fixed

1.  **Inconsistent Boundary Logic**
    *   **Issue**: Column highlighting was previously applied to all terminal rows, regardless of whether they contained data.
    *   **Fix**: The same boundary-checking logic used by row highlighting was applied to column highlighting.
    *   **Lesson**: When fixing a bug in one area, ensure that similar areas of the code follow the same correct pattern.

2.  **UI Corruption from Logging**
    *   **Issue**: Previous versions of the application used `printf()` for logging, which corrupted the ncurses display.
    *   **Fix**: A file-based logging system was implemented to prevent direct writes to stdout/stderr during UI operations.
    *   **Lesson**: Terminal applications with complex UIs must not write directly to standard output or error streams.

3.  **Uninitialized Memory in Tests**
    *   **Issue**: Test fixtures containing the main `DSVViewer` struct were not properly zero-initialized, causing segmentation faults during cleanup.
    *   **Fix**: `memset()` was added to the test setup to ensure all test fixtures are properly zero-initialized.
    *   **Lesson**: Always zero-initialize structs, especially when they contain pointers that will be freed by cleanup functions.

## 🎨 Code Patterns to Follow

### Display Logic Pattern

```c
// 1. Check data boundaries before processing
if (view_data_row >= current_view->visible_row_count) {
    continue; // Skip empty rows
}

// 2. Process and render the actual data
// ... draw/highlight content ...

// 3. Apply highlighting only to rows that contain data
if (condition_met) {
    apply_highlight(screen_row, start_col, actual_content_width);
}
```

### Error Handling Pattern

```c
// 1. Validate inputs, especially pointers
CHECK_NULL_RET(pointer, default_value);

// 2. Check array bounds before access
if (index >= array_size) {
    return ERROR_VALUE;
}

// 3. Provide helpful and descriptive error messages
set_error_message(viewer, "Descriptive error: %s", context);
```

### Memory Management Pattern

```c
// 1. Always check the return value of memory allocation functions
ptr = malloc(size);
if (!ptr) {
    // Handle allocation failure
    return ERROR;
}

// 2. For every `malloc`, there should be a corresponding `free`.
//    Provide cleanup functions for complex data structures.
void cleanup_resource(Resource *res) {
    if (res && res->data) {
        free(res->data);
        res->data = NULL;
    }
}
``` 