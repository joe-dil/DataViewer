#ifndef FILE_DATA_H
#define FILE_DATA_H

#include <stddef.h>
#include "encoding.h"

// A component to hold file related data.
typedef struct {
    char *data;
    size_t length;
    int fd;
    FileEncoding detected_encoding;
} FileData;

#endif // FILE_DATA_H 