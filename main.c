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

int is_directory_empty(const char *path) {
    int n = 0;
    struct dirent *d;
    DIR *dir = opendir(path);

    while ((d = readdir(dir)) != NULL) {
        if(++n > 2) break;
    }
    closedir(dir);
    if (n <= 2) return 1; // Directory Empty
    else return 0; // Directory not empty
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Check for correct usage
    if (argc < 4) {
        printf("Usage: %s <compress/decompress> <folder path> <output_dir path>\n", argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    const char *operation = argv[1];
    const char *source_path = argv[2];
    const char *output_path = argv[3];

    if (world_rank == 0) {
        struct stat path_stat;
        // Check if source path is a directory
        if (stat(source_path, &path_stat) != 0 || !S_ISDIR(path_stat.st_mode)) {
            printf("Source path is not a directory.\n");
            return 1;
        }

        // Check if source directory is empty
        if (is_directory_empty(source_path)) {
            printf("Source directory is empty.\n");
            return 1;
        }

        // Check if output path exists
        if (stat(output_path, &path_stat) != 0) {
            // Output path does not exist, create it
            if (mkdir(output_path, 0777) == -1) {
                perror("Failed to create output directory");
                return 1;
            }
        } else if (!S_ISDIR(path_stat.st_mode)) {
            // Output path exists but is not a directory
            printf("Output path is not a directory.\n");
            return 1;
        }
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Execute the specified operation
    if (strcmp(operation, "compress") == 0) {
        _compress(source_path, output_path);
    } else if (strcmp(operation, "decompress") == 0) {
        _decompress(source_path, output_path);
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
// - Fix the issue when the count of files is less than the process count