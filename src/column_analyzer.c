#include "viewer.h"
#include "column_analyzer.h"
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h> // For sysconf

// Struct to pass data to each worker thread
typedef struct {
    DSVViewer *viewer;
    size_t start_line;
    size_t end_line;
    int *thread_widths;
    size_t thread_max_cols;
    pthread_mutex_t *mutex;
} ThreadData;

// Worker function to analyze a slice of the file
static void* analyze_columns_worker(void *arg) {
    ThreadData *data = (ThreadData*)arg;
    DSVViewer *viewer = data->viewer;
    
    // Each thread needs its own field descriptor buffer
    FieldDesc thread_fields[MAX_COLS];

    for (size_t i = data->start_line; i < data->end_line; i++) {
        size_t num_fields = parse_line(viewer, viewer->line_offsets[i],
                                  thread_fields, MAX_COLS);

        if (num_fields > data->thread_max_cols) {
            data->thread_max_cols = num_fields;
        }

        for (size_t col = 0; col < num_fields; col++) {
            if (data->thread_widths[col] >= 16) continue;
            
            int display_width = calculate_field_display_width(viewer, &thread_fields[col]);
            if (display_width > data->thread_widths[col]) {
                data->thread_widths[col] = display_width;
            }
        }
    }
    return NULL;
}

void analyze_columns(struct DSVViewer *viewer) {
    long num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    viewer->num_threads = (num_cores > 1) ? num_cores : 1;
    
    // No benefit from threading small files
    if (viewer->num_lines < 2000 || viewer->num_threads == 1) {
        viewer->num_threads = 1; // Fallback to single-threaded
    }

    size_t lines_per_thread = viewer->num_lines / viewer->num_threads;
    pthread_t threads[viewer->num_threads];
    ThreadData thread_data[viewer->num_threads];

    // --- Create and run threads ---
    for (int i = 0; i < viewer->num_threads; i++) {
        thread_data[i].viewer = viewer;
        thread_data[i].start_line = i * lines_per_thread;
        thread_data[i].end_line = (i == viewer->num_threads - 1) 
                                 ? viewer->num_lines 
                                 : (i + 1) * lines_per_thread;
        thread_data[i].thread_widths = calloc(MAX_COLS, sizeof(int));
        thread_data[i].thread_max_cols = 0;
        
        pthread_create(&threads[i], NULL, analyze_columns_worker, &thread_data[i]);
    }

    // --- Join threads and aggregate results ---
    size_t max_cols = 0;
    int *final_widths = calloc(MAX_COLS, sizeof(int));

    for (int i = 0; i < viewer->num_threads; i++) {
        pthread_join(threads[i], NULL);
        
        if (thread_data[i].thread_max_cols > max_cols) {
            max_cols = thread_data[i].thread_max_cols;
        }
        for (size_t j = 0; j < thread_data[i].thread_max_cols; j++) {
            if (thread_data[i].thread_widths[j] > final_widths[j]) {
                final_widths[j] = thread_data[i].thread_widths[j];
            }
        }
        free(thread_data[i].thread_widths);
    }
    
    viewer->num_cols = max_cols > MAX_COLS ? MAX_COLS : max_cols;
    viewer->col_widths = malloc(viewer->num_cols * sizeof(int));
    for (size_t i = 0; i < viewer->num_cols; i++) {
        viewer->col_widths[i] = final_widths[i] > 16 ? 16 :
                               (final_widths[i] < 4 ? 4 : final_widths[i]);
    }
    free(final_widths);
} 