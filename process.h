#ifndef FINAL_DEMO_PROCESS_H
#define FINAL_DEMO_PROCESS_H

#include <array>
#include <filesystem>
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <zlib.h>

constexpr std::size_t CHUNK_SIZE = 65535;
#define NUM_CONSUMERS 1

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

struct CompressedChunk {
    std::string relative_path;
    int sequence_id;
    bool is_last_chunk;
    std::array<unsigned char, CHUNK_SIZE> data;
};

std::string sort_files_by_size(const std::filesystem::path &path);
int count_non_empty_lines(const std::string &file_path);
void do_compression(const std::string &input_dir, const std::string &output_dir, const std::string &file_record, int world_rank);
void do_decompression(const std::string &input_dir, const std::string &output_dir);

#endif
