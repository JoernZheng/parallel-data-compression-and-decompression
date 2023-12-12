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

#define CHUNK_SIZE 65535
#define QUEUE_SIZE 100
#define NUM_CONSUMERS 3
#define MAX_LINE_LENGTH 256
#define HASHMAP_INIT_SIZE 100
#define HASHMAP_RESIZE_FACTOR 2

typedef struct {
    char *relpath;
    off_t size;
} FileEntry;

typedef struct {
    char filename[255];
    long size;    // Size of the compressed chunk
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

/*
    Decompress Define
*/
// Node structure for the linked list in each hash table entry
struct Node
{
    char *key;
    char *value;
    struct Node *next;
};

// Hash table structure
struct HashMap
{
    int size;
    int usedSize;
    struct Node **array;
};

char *sort_files_by_size(const char *path);
void do_compression(const char *input_dir, const char *output_dir, const char *file_record, int world_rank);
int count_non_empty_lines(const char *file_path);
void extract_filename(char *filename, const char *filepath);
void do_decompression(const char *source_path, const char *output_path);

int calculateHash(char *key, int table_size);
struct Node *createNode(char *key, char *value);
struct HashMap *createHashMap(int size);
void insert(struct HashMap *map, char *key, char *value);
char *search(struct HashMap *map, char *key);
void resizeHashMap(struct HashMap *map, int new_size);
void destroyHashMap(struct HashMap *map);

#endif
