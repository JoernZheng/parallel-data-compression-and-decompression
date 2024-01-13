#include "process.hpp"
#include <cstring>
#include <iostream>
#include <mpi.h>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

void compress(const std::string &folder_path, const std::string &output_path) {
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    std::string file_record;

    // Step 1: Sort files by size and record them
    if (world_rank == 0) {
        std::cout << "Compressing folder: " << folder_path << std::endl;
        file_record = sort_files_by_size(folder_path);
        std::cout << "File record saved location: " << file_record << std::endl;
    }

    // Step 2: Broadcast file_record to all processes
    // Broadcast the size of file_record to all processes
    size_t buffer_size = file_record.size();
    MPI_Bcast(&buffer_size, 1, MPI_UNSIGNED_LONG_LONG, 0, MPI_COMM_WORLD);

    // Resize the buffer on all processes
    std::vector<char> broadcast_buffer(buffer_size + 1); // +1 for null terminator
    if (world_rank == 0) {
        std::strcpy(broadcast_buffer.data(), file_record.c_str());
    }

    MPI_Bcast(broadcast_buffer.data(), buffer_size + 1, MPI_CHAR, 0, MPI_COMM_WORLD);

    if (world_rank != 0) {
        file_record = std::string(broadcast_buffer.data(), buffer_size);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    std::cout << "file_record: " << file_record << std::endl;

    int file_count = count_non_empty_lines(file_record);

    // Step 3: Compress files
        if (world_rank < file_count) {
            do_compression(folder_path, output_path, file_record, world_rank);
        } else {
            std::cout << "Rank: " << world_rank << " - No file to compress" << std::endl;
        }

    std::cout << "main.c - Rank: " << world_rank << " - do_compression finished" << std::endl;
}

void decompress(const std::string &source_path, const std::string &output_path) {
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_rank == 0) {
        if (world_size > 1) {
            std::cout << "Decompression is not supported in MPI parallel mode.\n";
            std::cout << "Only use one process to decompress.\n";
            std::cout << "Decompression uses multiple threads to parallel decompress files.\n";
        }

        do_decompression(source_path, output_path);
    }
}

void remove_trailing_slash(std::string &path) {
    if (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    double start_time = MPI_Wtime();// Start the timer

    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // Check for correct usage
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <compress/decompress> <source directory path> <output directory path>\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    std::string operation = argv[1];
    std::string source_path = argv[2];
    std::string output_path = argv[3];

    // Remove trailing slash from paths
    remove_trailing_slash(source_path);
    remove_trailing_slash(output_path);

    std::cout << "source_path: " << source_path << '\n';
    std::cout << "output_path: " << output_path << '\n';

    // Master node checks and prepares paths
    if (world_rank == 0) {
        struct stat path_stat{};

        // Check if source path exists
        if (stat(source_path.c_str(), &path_stat) != 0) {
            std::cerr << "Source path does not exist.\n";
            return 1;
        }

        // todo: check if source path is a directory or a file

        // Check if output path exists
        if (stat(output_path.c_str(), &path_stat) != 0) {
            // Output path does not exist, create it
            if (mkdir(output_path.c_str(), 0777) == -1) {
                perror("Failed to create output directory");
                return 1;
            }
        } else if (!S_ISDIR(path_stat.st_mode)) {
            // Output path exists but is not a directory
            std::cerr << "Output path is not a directory.\n";
            return 1;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Execute the specified operation
    if (operation == "compress") {
        compress(source_path, output_path);
    } else if (operation == "decompress") {
        decompress(source_path, output_path);
    } else {
        std::cerr << "Invalid operation: " << operation << ". Please use 'compress' or 'decompress'.\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double end_time = MPI_Wtime();// Stop the timer

    if (world_rank == 0) {
        double total_time = end_time - start_time;
        std::cout << "========================================\n"
                  << "Operation: " << operation << '\n'
                  << "Processor Count: " << world_size << '\n'
                  << "Time Taken: " << total_time << " seconds\n"
                  << "========================================\n";
    }

    MPI_Finalize();
    return 0;
}
