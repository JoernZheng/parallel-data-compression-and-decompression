#include "process.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <zlib.h>

void decompress_chunk(CompressedChunk &chunk, std::ostream &dest) {
    if (chunk.data.empty()) {
        return;
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (inflateInit(&strm) != Z_OK) {
        return;
    }

    strm.avail_in = chunk.data.size();
    strm.next_in = reinterpret_cast<Bytef*>(chunk.data.data());

    unsigned char out[CHUNK_SIZE];
    do {
        strm.avail_out = sizeof(out);
        strm.next_out = out;
        inflate(&strm, Z_NO_FLUSH);

        dest.write(reinterpret_cast<char *>(out), sizeof(out) - strm.avail_out);
    } while (strm.avail_out == 0);

    inflateEnd(&strm);
}

struct CompareChunks {
    bool operator()(const CompressedChunk &a, const CompressedChunk &b) {
        return a.sequence_id > b.sequence_id;
    }
};

void decompress_zwz(const std::string &filename, const std::string &output_dir) {
    std::ifstream source(filename, std::ios::binary);
    if (!source.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::map<std::string, std::priority_queue<CompressedChunk, std::vector<CompressedChunk>, CompareChunks>> pending_chunks;
    std::map<std::string, int> expected_sequence_id;
    std::map<std::string, std::ofstream> output_files;
    std::map<std::string, std::string> file_md5s;

    auto remove_file_info = [&](const std::string &relative_path) {
        output_files[relative_path].close();
        output_files.erase(relative_path);
        expected_sequence_id.erase(relative_path);
        pending_chunks.erase(relative_path);
        file_md5s.erase(relative_path);
    };

    while (!source.eof()) {
        int total_size;
        source.read(reinterpret_cast<char *>(&total_size), sizeof(total_size));
        if (source.eof()) break;    // End of file

        int path_length;
        source.read(reinterpret_cast<char *>(&path_length), sizeof(path_length));

        std::string relative_path(path_length, '\0');
        source.read(&relative_path[0], path_length);

        int sequence_id;
        source.read(reinterpret_cast<char *>(&sequence_id), sizeof(sequence_id));

        bool is_last_chunk;
        source.read(reinterpret_cast<char *>(&is_last_chunk), sizeof(is_last_chunk));

        int compressed_data_size = total_size - (sizeof(path_length) + path_length + sizeof(sequence_id) + sizeof(is_last_chunk));
        std::vector<unsigned char> compressed_data(compressed_data_size);
        source.read(reinterpret_cast<char *>(compressed_data.data()), compressed_data.size());

        if (is_last_chunk) {
            std::string stored_md5(MD5_DATA_SIZE, '\0');
            source.read(&stored_md5[0], MD5_DATA_SIZE);

            std::cout << "Read MD5: " << stored_md5 << std::endl;
            file_md5s[relative_path] = stored_md5;
        }

        // Open the corresponding output file if not opened yet
        if (output_files.find(relative_path) == output_files.end()) {
            std::string file_path = output_dir + "/" + relative_path;

            // Create the directories in the file path if they don't exist
            std::filesystem::path dir = std::filesystem::path(file_path).parent_path();
            if (!std::filesystem::exists(dir)) {
                std::filesystem::create_directories(dir); // Create the directories
            }

            output_files[relative_path].open(file_path, std::ios::binary);
            if (!output_files[relative_path].is_open()) {
                std::cerr << "Error creating output file: " << file_path << std::endl;
                continue;
            }
            expected_sequence_id[relative_path] = 0;
        }

        CompressedChunk chunk;
        chunk.relative_path = relative_path;
        chunk.sequence_id = sequence_id;
        chunk.is_last_chunk = is_last_chunk;
        std::copy(compressed_data.begin(), compressed_data.end(), chunk.data.begin());

        // If this is the next expected chunk, process it immediately
        if (expected_sequence_id[relative_path] == sequence_id) {
            decompress_chunk(chunk, output_files[relative_path]);
            expected_sequence_id[relative_path]++;

            // Check if there are pending chunks that can be processed
            while (!pending_chunks[relative_path].empty() &&
                   pending_chunks[relative_path].top().sequence_id == expected_sequence_id[relative_path]) {
                CompressedChunk next_chunk = pending_chunks[relative_path].top();
                pending_chunks[relative_path].pop();
                decompress_chunk(next_chunk, output_files[relative_path]);
                expected_sequence_id[relative_path]++;
            }

            if (is_last_chunk && expected_sequence_id[relative_path] == sequence_id + 1 && pending_chunks[relative_path].empty()) {
                std::string file_path = output_dir + "/" + relative_path;
                output_files[relative_path].close();

                std::string calculated_md5 = md5_of_file(file_path);
                std::string stored_md5 = file_md5s[relative_path];

                // Compare the calculated MD5 with the stored MD5
                if (calculated_md5 != stored_md5) {
                    std::cerr << "MD5 mismatch for file: " << file_path << std::endl;
                    std::cout << "Expected MD5: " << stored_md5 << std::endl;
                    std::cout << "Calculated MD5: " << calculated_md5 << std::endl;
                } else {
                    std::cout << "MD5 match for file: " << file_path << std::endl;
                }

                remove_file_info(relative_path);
            }
        } else {
            // Else, add the chunk to the waiting queue
            pending_chunks[relative_path].push(chunk);
        }
    }

    // Deal with remaining files
    for (auto &pair: output_files) {
        if (!pending_chunks[pair.first].empty()) {
            std::cerr << "Warning: pending chunks remaining for file: " << pair.first << std::endl;
        }
        pair.second.close();
    }
}

void do_decompression(const std::string &input_dir, const std::string &output_dir) {
    std::vector<std::string> files;

    for (const auto &entry : std::filesystem::directory_iterator(input_dir)) {
        if (entry.path().extension() == ".zwz") {
            files.push_back(entry.path());
        }
    }

    #pragma omp parallel for num_threads(omp_get_num_procs() * 2)
    for (const auto & file : files) {
        decompress_zwz(file, output_dir);
    }
}
