#ifndef FINAL_DEMO_PROCESS_H
#define FINAL_DEMO_PROCESS_H

#include <fcntl.h>
#include <mpi.h>
#include <omp.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <zlib.h>
//#include <openssl/md5.h>
#include <array>
#include <cstddef>
#include <dirent.h>
#include <filesystem>// Include this for std::filesystem
#include <iostream>
#include <string>


constexpr std::size_t CHUNK_SIZE = 65535;
//#define QUEUE_SIZE 100
#define NUM_CONSUMERS 1
//
//#define MAX_LINE_LENGTH 256
//#define HASHMAP_INIT_SIZE 100
//#define HASHMAP_RESIZE_FACTOR 2
//#define BUFFER_SIZE 1024


struct FileEntry {
    std::string relpath;// Relative path
    off_t size;
};

struct Chunk {
    int sequence_id;
    std::string relative_path;
    std::array<unsigned char, CHUNK_SIZE> data;
    size_t size;
    bool is_last_chunk;
    bool is_last_file;
};


//typedef struct {
//    char filename[255];
//    long size;    // Size of the compressed chunk
//    int is_last;    // If this is the last chunk of the file
//    char hash_value[2 * MD5_DIGEST_LENGTH + 1]; // hash value of the file before compressed
//} ChunkHeader;

//struct ChunkHeader {
//    long size;
//};


//
struct CompressedChunk {
    std::string relative_path;
    int sequence_id;
    bool is_last_chunk;
    std::array<unsigned char, CHUNK_SIZE> data;
};

/*
    Decompress Define
*/
// Node structure for the linked list in each hash table entry
//struct Node
//{
//    char *key;
//    char *value;
//    struct Node *next;
//};
//
//// Hash table structure
//struct HashMap
//{
//    int size;
//    int usedSize;
//    struct Node **array;
//};

//char *sort_files_by_size(const char *path);
//void do_compression(const char *input_dir, const char *output_dir, const char *file_record, int world_rank);
//int count_non_empty_lines(const char *file_path);
//void extract_filename(char *filename, const char *filepath);
//void do_decompression(const char *source_path, const char *output_path);
//char *get_hash(const char *full_path);
//char *get_chunk_hash(const Chunk *uncompressed_chunk);
//void verify(const char *hash_header, const char *hash_decmprs, const char *filename, const char *file_path, const char *output_dir_path);
//
//int calculateHash(char *key, int table_size);
//struct Node *createNode(char *key, char *value);
//struct HashMap *createHashMap(int size);
//void insert(struct HashMap *map, char *key, char *value);
//char *search(struct HashMap *map, char *key);
//void resizeHashMap(struct HashMap *map);
//void destroyHashMap(struct HashMap *map);

std::string sort_files_by_size(const std::filesystem::path &path);
int count_non_empty_lines(const std::string &file_path);
void do_compression(const std::string &input_dir, const std::string &output_dir, const std::string &file_record, int world_rank);

#endif
