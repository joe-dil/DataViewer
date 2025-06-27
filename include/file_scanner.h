#ifndef FILE_SCANNER_H
#define FILE_SCANNER_H

// Forward declare DSVViewer to avoid circular dependencies.
struct DSVViewer; 

void scan_file(struct DSVViewer *viewer);

#endif // FILE_SCANNER_H 