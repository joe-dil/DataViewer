#ifndef ERROR_CONTEXT_H
#define ERROR_CONTEXT_H

// Return structured errors instead of -1
typedef enum {
    DSV_OK = 0,
    DSV_ERROR, // Generic error, avoid using
    DSV_ERROR_MEMORY,
    DSV_ERROR_FILE_IO, 
    DSV_ERROR_PARSE,
    DSV_ERROR_DISPLAY,
    DSV_ERROR_CACHE,
    DSV_ERROR_INVALID_ARGS,
    DSV_ERROR_NOT_IMPLEMENTED
} DSVResult;

// Converts a DSVResult code into a human-readable string.
const char* dsv_result_to_string(DSVResult result);

#endif // ERROR_CONTEXT_H 