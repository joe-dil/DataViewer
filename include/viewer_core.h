#ifndef VIEWER_CORE_H
#define VIEWER_CORE_H

#include "config.h"

// Forward declaration
struct DSVViewer;
struct DSVConfig;

// Component lifecycle management
DSVResult init_viewer_components(struct DSVViewer *viewer, const DSVConfig *config);
void cleanup_viewer_components(struct DSVViewer *viewer);

// Configuration management
void configure_viewer_settings(struct DSVViewer *viewer, const DSVConfig *config);
void initialize_viewer_cache(struct DSVViewer *viewer, const DSVConfig *config);

// Resource management
void cleanup_viewer_resources(struct DSVViewer *viewer);

#endif // VIEWER_CORE_H 