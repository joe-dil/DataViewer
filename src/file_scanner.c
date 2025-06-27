#include "viewer.h"
#include "file_scanner.h"

#include <stdlib.h>
#include <stdio.h>

// Estimate the number of lines in the file by sampling the first 100 lines or 64KB
static size_t estimate_line_count(DSVViewer *viewer) {
    const size_t sample_size = (viewer->length < 65536) ? viewer->length : 65536;
    if (sample_size == 0) return 1;

    int sample_lines = 0;
    size_t bytes_in_sample = 0;
    char *p = (char*)viewer->data;
    char *end = p + sample_size;
    int in_quotes = 0;

    while (p < end && sample_lines < 100) {
        if (*p == '"') {
            in_quotes = !in_quotes;
        } else if (*p == '\n' && !in_quotes) {
            sample_lines++;
            bytes_in_sample = (p - (char*)viewer->data);
        }
        p++;
    }

    if (sample_lines == 0) {
        // If no newlines are found in the sample, fallback to a simple heuristic
        return (viewer->length / 80) + 1;
    }

    double avg_line_len = (double)bytes_in_sample / sample_lines;
    size_t estimated_total_lines = viewer->length / avg_line_len;
    
    // Return the estimate with a 20% safety margin
    return (size_t)(estimated_total_lines * 1.2) + 1;
}

void scan_file(DSVViewer *viewer) {
    // Get an intelligent estimate of the line count to pre-allocate a buffer.
    viewer->capacity = estimate_line_count(viewer);
    viewer->line_offsets = malloc(viewer->capacity * sizeof(size_t));
    if (!viewer->line_offsets) {
        perror("Failed to allocate initial line_offsets buffer");
        exit(1);
    }

    viewer->num_lines = 0;
    viewer->line_offsets[viewer->num_lines++] = 0;

    char *p = (char*)viewer->data;
    char *end = p + viewer->length;
    int in_quotes = 0;

    // This is a true single-pass scan. It inspects each character only once,
    // which is the most performant and correct way to handle this task.
    for (char *current = p; current < end; current++) {
        if (*current == '"') {
            in_quotes = !in_quotes;
        } else if (*current == '\n' && !in_quotes) {
            // This is a true row delimiter.
            if (viewer->num_lines >= viewer->capacity) {
                // Our estimate was too low. Fallback to doubling the capacity.
                size_t new_capacity = viewer->capacity * 2;
                if (new_capacity == 0) new_capacity = 8192;
                size_t *new_offsets = realloc(viewer->line_offsets, new_capacity * sizeof(size_t));
                if (!new_offsets) {
                    perror("Failed to expand line buffer; file may be truncated");
                    break;
                }
                viewer->line_offsets = new_offsets;
                viewer->capacity = new_capacity;
            }
            // Store the offset of the character *after* the newline.
            if (current + 1 < end) {
                viewer->line_offsets[viewer->num_lines++] = (current - (char*)viewer->data) + 1;
            }
        }
    }

    // Trim the allocated memory to the exact size needed.
    if (viewer->capacity > viewer->num_lines) {
        size_t *trimmed_offsets = realloc(viewer->line_offsets, viewer->num_lines * sizeof(size_t));
        if (trimmed_offsets) {
            viewer->line_offsets = trimmed_offsets;
            viewer->capacity = viewer->num_lines;
        }
    }
} 