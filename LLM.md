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
- Extensive test suite (59 passing tests)

### Technical Details
- Uses ncurses for terminal UI
- File-based logging to avoid UI corruption (`dsv_debug.log`)
- Proper memory management with cleanup functions
- Comprehensive error handling and boundary checking
- Unicode support with ASCII fallback

## Known Issues
- None currently identified

## TODO Tasks
- Add vi-style navigation keys (hjkl) as alternatives to arrow keys
- Document cache system's memory allocation strategy and ownership model
- Consider refactoring to explicit MVC pattern for better separation of concerns 