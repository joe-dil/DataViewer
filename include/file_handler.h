#ifndef FILE_HANDLER_H
#define FILE_HANDLER_H

// Forward declare DSVViewer to avoid circular dependencies.
struct DSVViewer; 

int load_file(struct DSVViewer *viewer, const char *filename);
void unload_file(struct DSVViewer *viewer);

#endif // FILE_HANDLER_H 