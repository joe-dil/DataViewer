#include "error_context.h"
#include <stddef.h>

const char* dsv_result_to_string(DSVResult result) {
    switch (result) {
        case DSV_OK:                    return "Success";
        case DSV_ERROR:                 return "Generic Error";
        case DSV_ERROR_MEMORY:          return "Memory Allocation Error";
        case DSV_ERROR_FILE_IO:         return "File I/O Error";
        case DSV_ERROR_PARSE:           return "Parsing Error";
        case DSV_ERROR_DISPLAY:         return "Display Error";
        case DSV_ERROR_CACHE:           return "Cache Error";
        case DSV_ERROR_INVALID_ARGS:    return "Invalid Arguments Error";
        case DSV_ERROR_NOT_IMPLEMENTED: return "Functionality Not Implemented";
        default:                        return "Unknown Error Code";
    }
} 