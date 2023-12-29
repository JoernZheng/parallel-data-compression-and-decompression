#include <iostream>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>
#include <mpi.h>
#include <string>
#include <stdexcept>
#include <filesystem>

void _compress(const char *folder_path, const char *output_path) {
//    int world_rank, world_size;
//    char file_record_path[1024];
//
//    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
//    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
//
//    // Step 1: Sort files by size and record them
//    if (world_rank == 0) {
//        cout << "Compressing folder: " << folder_path << endl;
//        char *file_record = sort_files_by_size(folder_path);
//        cout << "File record saved location: " << file_record << endl;
//        strncpy(file_record_path, file_record, sizeof(file_record_path) - 1);
//        file_record_path[sizeof(file_record_path) - 1] = '\0'; // Ensure null-terminated string
//        free(file_record);
//    }
//
//    // Step 2: Broadcast file_record_path to all processes
//    MPI_Bcast(file_record_path, sizeof(file_record_path), MPI_CHAR, 0, MPI_COMM_WORLD);
//
//    int file_count = count_non_empty_lines(file_record_path);
//
//    // Step 3: Compress files
//    if (world_rank < file_count) {
//        do_compression(folder_path, output_path, file_record_path, world_rank);
//    } else {
//        cout << "Rank: " << world_rank << " - No file to compress" << endl;
//    }
//
//    cout << "main.c - Rank: " << world_rank << " - do_compression finished" << endl;
}
//
//void _decompress(const string &source_path, const string &output_path) {
//    int world_rank, world_size;
//    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
//    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
//
//    if (world_rank == 0) {
//        if (world_size > 1) {
//            cout << "Decompression is not supported in MPI parallel mode.\n";
//            cout << "Only use one process to decompress.\n";
//            cout << "Decompression uses multiple threads to parallel decompress files.\n";
//        }
//
//        do_decompression(source_path, output_path);// Assuming this function is converted to C++
//    }
//}
//

bool is_directory_empty(const std::string &path) {
    return std::filesystem::is_empty(path);
}

void remove_trailing_slash(std::string &path) {
    if (!path.empty() && path.back() == '/') {
        path.pop_back();
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    double start_time = MPI_Wtime(); // Start the timer

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
        struct stat path_stat;

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
         _compress(source_path, output_path);
    } else if (operation == "decompress") {
        // _decompress(source_path, output_path);
    } else {
        std::cerr << "Invalid operation: " << operation << ". Please use 'compress' or 'decompress'.\n";
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    double end_time = MPI_Wtime(); // Stop the timer

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
