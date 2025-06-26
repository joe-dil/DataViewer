#include "viewer.h"

// Global variable definitions
int show_header = 1; // Default to showing header
int supports_unicode = 0;
const char* separator = " | "; // Default to ASCII

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <csv_file> [delimiter] [options]\n", argv[0]);
        printf("       delimiter: optional (auto-detected if not specified)\n");
        printf("       Common delimiters: ',' '|' ';' '\\t'\n");
        printf("       Options:\n");
        printf("         --no-header    Don't show persistent header\n");
        printf("         -h false       Don't show persistent header\n");
        return 1;
    }
    
    char delimiter = 0;
    char *filename = argv[1];
    
    // Parse arguments
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--no-header") == 0) {
            show_header = 0;
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc && strcmp(argv[i + 1], "false") == 0) {
            show_header = 0;
            i++; // Skip the "false" argument
        } else if (strlen(argv[i]) == 1 || strcmp(argv[i], "\\t") == 0) {
            // This is a delimiter
            if (strcmp(argv[i], "\\t") == 0) {
                delimiter = '\t';
            } else {
                delimiter = argv[i][0];
            }
        }
    }
    
    // Set UTF-8 locale and determine separator
    setlocale(LC_ALL, "");
    char *locale = setlocale(LC_CTYPE, NULL);
    if (locale && (strstr(locale, "UTF-8") || strstr(locale, "utf8"))) {
        supports_unicode = 1;
    }
    separator = supports_unicode ? " │ " : " | ";
    
    CSVViewer viewer;
    if (init_viewer(&viewer, filename, delimiter) != 0) {
        return 1;
    }
    
    printf("Loaded file with %d lines, delimiter: '%c'\n", 
           viewer.num_lines, 
           viewer.delimiter == '\t' ? 'T' : viewer.delimiter);
    printf("Header mode: %s\n", show_header ? "Persistent header" : "Headerless");
    printf("Unicode support: %s (using '%s' separators)\n", 
           supports_unicode ? "YES" : "NO",
           separator);
    printf("Detected %d columns with widths: ", viewer.num_cols);
    for (int i = 0; i < viewer.num_cols && i < 10; i++) {
        printf("%d ", viewer.col_widths[i]);
    }
    if (viewer.num_cols > 10) printf("...");
    printf("\n");
    printf("Press any key to start viewer...\n");
    getchar();
    
    run_viewer(&viewer);
    cleanup_viewer(&viewer);
    
    return 0;
} 