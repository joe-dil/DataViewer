#include "app_init.h"
#include "input_router.h"
#include "view_state.h"
#include "navigation.h"
#include "parsed_data.h"
#include "view_manager.h"
#include "core/data_source.h"
#include <ncurses.h>
#include <stdbool.h>

void run_viewer(DSVViewer *viewer) {
    // The global viewer state is already initialized by init_viewer.
    // We just need to set up the row selection for the main view.
    init_row_selection(&viewer->view_state, viewer->parsed_data->num_lines);

    // Create file data source for main view
    DataSource *file_ds = create_file_data_source(viewer);
    if (!file_ds) {
        // Error is logged in create function
        return;
    }

    // Store in viewer for cleanup
    viewer->main_data_source = file_ds;
    
    // Create main view with data source
    View *main_view = create_main_view(file_ds);
    if (!main_view) {
        return; // Failed to create main view
    }

    // Set the main view as the first and current view
    viewer->view_manager->views = main_view;
    viewer->view_manager->current = main_view;
    viewer->view_manager->view_count = 1;

    while (1) {
        ViewState *current_state = &viewer->view_state;
        current_state->current_view = viewer->view_manager->current;

        // Only redraw when needed
        if (current_state->needs_redraw) {
            display_data(viewer, current_state);
            current_state->needs_redraw = false;
        }

        int ch = getch();
        
        // Handle mouse events and other special cases that can cause busy loops
        if (ch == ERR || ch == KEY_MOUSE || ch == KEY_RESIZE) {
            continue; // Skip processing, just continue the main loop
        }
        
        // Route input through the new input router
        InputResult result = route_input(ch, viewer, current_state);
        
        switch (result) {
            case INPUT_CONSUMED:
                // Panel handled the input, redraw flag already set if needed
                break;
                
            case INPUT_IGNORED:
                // Panel didn't recognize the input, could log or handle specially
                break;
                
            case INPUT_GLOBAL: {
                // Handle global commands
                GlobalResult global_result = handle_global_input(ch, current_state);
                switch (global_result) {
                    case GLOBAL_QUIT:
                        return;  // Exit the viewer
                        
                    case GLOBAL_SHOW_HELP:
                        show_help();
                        current_state->needs_redraw = true;  // Redraw after modal
                        break;
                        
                    case GLOBAL_SWITCH_PANEL:
                        // Future: Handle panel switching
                        // For now, just force redraw
                        current_state->needs_redraw = true;
                        break;
                        
                    case GLOBAL_CONTINUE:
                        // Should not happen when result is INPUT_GLOBAL
                        break;
                }
                break;
            }
        }
    }
} 