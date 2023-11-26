#ifndef FINAL_DEMO_PROCESS_H
#define FINAL_DEMO_PROCESS_H

#include <mpi.h>

typedef struct {
    char *relpath;
    off_t size;
} FileEntry;

void sort_files_by_size(const char *path);

#endif
