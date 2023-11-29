#ifndef FINAL_DEMO_PROCESS_H
#define FINAL_DEMO_PROCESS_H

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <zlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>

#define CHUNK_SIZE 16384
#define QUEUE_SIZE 100
#define NUM_CONSUMERS 1

typedef struct {
    char *relpath;
    off_t size;
} FileEntry;

//typedef struct {
//    char filename[255];
//    size_t size;
//} FileHeader;

typedef struct {
    char filename[255];
    size_t size;    // Size of the compressed file
    int is_last;    // If this is the last chunk of the file
} ChunkHeader;

typedef struct {
    int id;
    char filename[255];
    unsigned char data[CHUNK_SIZE];
    size_t size;
    int is_last_chunk;
    int is_last_file;
} Chunk;

typedef struct {
    char filename[255];
    unsigned char content[CHUNK_SIZE];
    size_t size;
    int written; // 0: not written, 1: written
} CompressedChunk;

char *sort_files_by_size(const char *path);
void do_compression(const char *input_dir, const char *output_dir, const char *file_record, int world_rank);
int count_non_empty_lines(const char *file_path);
void extract_filename(char *filename, const char *filepath);

#endif
