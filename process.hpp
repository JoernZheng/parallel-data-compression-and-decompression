#ifndef FINAL_DEMO_PROCESS_HPP
#define FINAL_DEMO_PROCESS_HPP

#include <array>
#include <filesystem>
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <zlib.h>
#include <openssl/md5.h>

constexpr std::size_t CHUNK_SIZE = 65535;
#define NUM_CONSUMERS 1
#define MD5_DATA_SIZE 32

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
std::string md5_of_file(const std::string &file_path);
bool is_md5_match(const std::string &file_path, const std::string &expected_md5);

#endif
