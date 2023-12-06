#include "process.h"

void _compress(const char *folder_path, const char *output_path) {
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    char file_record_path[1024];

    // Step 1: Sort files by size and record them
    if (world_rank == 0) {
        printf("Compressing folder: %s\n", folder_path);
        char *file_record = sort_files_by_size(folder_path);
        printf("File record saved location: %s\n", file_record);
        strncpy(file_record_path, file_record, sizeof(file_record_path) - 1);
        file_record_path[sizeof(file_record_path) - 1] = '\0';  // Ensure null-terminated string
        free(file_record);
    }

    // Step 2: Broadcast file_record_path to all processes
    MPI_Bcast(file_record_path, sizeof(file_record_path), MPI_CHAR, 0, MPI_COMM_WORLD);

    // Step 3: Compress files
    do_compression(folder_path, output_path, file_record_path, world_rank);
    printf("main.c - Rank: %d - do_compression finished\n", world_rank);
}

void _decompress(const char *source_path, const char *output_path) {
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (world_rank == 0) {
        if (world_size > 1) {
            printf("Decompression is not supported in MPI parallel mode.\n");
            printf("Only use one process to decompress.\n");
            printf("Decompression uses multiple threads to parallel decompress files.\n");
        }

        do_decompression(source_path, output_path);
    }
}

void _verify(const char *source_path, const char *output_path) {
    int world_rank, world_size;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    do_verification(source_path, output_path);
}

// Main function handling compression and decompression
// todo: 确保输入输出文件夹存在。另外确保文件夹格式正确
int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    // Check for correct usage
    if (argc < 4) {
        printf("Usage: %s <compress/decompress> <folder path> <output_dir path>\n", argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    const char *operation = argv[1];
    const char *source_path = argv[2];
    const char *output_path = argv[3];

    // Execute the specified operation
    if (strcmp(operation, "compress") == 0) {
        _compress(source_path, output_path);
    } else if (strcmp(operation, "decompress") == 0) {
        _decompress(source_path, output_path);
    } else if (strcmp(operation, "verify") == 0){
        _verify(source_path, output_path);
    } else {
        printf("Invalid operation: %s. Please use 'compress' or 'decompress'.\n", operation);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    MPI_Finalize();
    return 0;
}

// TODOs:
// - Add file compression branch
// - Add folder existence check
// - Add decompression entrance
// - Fix the issue when the count of files is less than the process count