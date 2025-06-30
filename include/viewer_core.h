#ifndef VIEWER_CORE_H
#define VIEWER_CORE_H

#include "error_context.h"

// Forward declaration
struct DSVViewer;

// Component lifecycle management
DSVResult init_viewer_components(struct DSVViewer *viewer);
void cleanup_viewer_components(struct DSVViewer *viewer);

// Configuration management
void configure_viewer_settings(struct DSVViewer *viewer);
void initialize_viewer_cache(struct DSVViewer *viewer);

// Resource management
void cleanup_viewer_resources(struct DSVViewer *viewer);

#endif // VIEWER_CORE_H 