#ifndef FINAL_DEMO_PROCESS_H
#define FINAL_DEMO_PROCESS_H

#include <mpi.h>

typedef struct {
    char *relpath;
    off_t size;
} FileEntry;

char *sort_files_by_size(const char *path);
void do_compression(const char *input_dir, const char *output_dir, const char *file_record, int world_rank);

#endif
