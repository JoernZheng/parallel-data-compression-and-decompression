#include "process.h"

void _compress(const char *folder_path, const char *output_path) {
    int world_rank, world_size;
    char file_record_path[1024];

    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

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

    int file_count = count_non_empty_lines(file_record_path);

    // Step 3: Compress files
    if (world_rank < file_count) {
        do_compression(folder_path, output_path, file_record_path, world_rank);
    } else {
        printf("Rank: %d - No file to compress\n", world_rank);
    }

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
        if (++n > 2) break;
    }
    closedir(dir);
    if (n <= 2) return 1; // Directory Empty
    else return 0; // Directory not empty
}

void remove_trailing_slash(char *path) {
    size_t length = strlen(path);
    if (length > 0 && path[length - 1] == '/') {
        path[length - 1] = '\0';  // Replace the last character with null terminator
    }
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Check for correct usage
    if (argc < 4) {
        printf("Usage: %s <compress/decompress> <source directory path> <output directory path>\n", argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    const char *operation = argv[1];
    char source_path[1024];
    char output_path[1024];

    strncpy(source_path, argv[2], sizeof(source_path) - 1);
    source_path[sizeof(source_path) - 1] = '\0';  // Ensure null-terminated string
    strncpy(output_path, argv[3], sizeof(output_path) - 1);
    output_path[sizeof(output_path) - 1] = '\0';

    // Remove trailing slash from paths
    remove_trailing_slash(source_path);
    remove_trailing_slash(output_path);

    if (world_rank == 0) {
        struct stat path_stat;

        // Check if source path exists
        if (stat(source_path, &path_stat) != 0) {
            printf("Source path does not exist.\n");
            return 1;
        }

        // Check if source path is a directory
        if (S_ISDIR(path_stat.st_mode)) {
            // Check if source directory is empty
            if (is_directory_empty(source_path)) {
                printf("Source directory is empty.\n");
                return 1;
            }
        } else {
            // source_path is not a directory, assume it's a file
            printf("Source path is not a directory.\n");
            printf("Assuming it's a file.\n");

            // Create a new folder named based on the file name
            char new_folder_path[1024];
            snprintf(new_folder_path, sizeof(new_folder_path), "%s_folder", source_path);
            printf("new_folder_path: %s\n", new_folder_path);
            mkdir(new_folder_path, 0777);

            // Construct new file path in the new folder
            char new_file_path[2048];
            char filename[1024];
            extract_filename(filename, source_path);
            snprintf(new_file_path, sizeof(new_file_path), "%s/%s", new_folder_path, filename);
            printf("filename: %s\n", filename);
            printf("new_file_path: %s\n", new_file_path);

            // Move the file to the new folder
            rename(source_path, new_file_path);

            // Update source_path to point to the new folder
            strncpy(source_path, new_folder_path, sizeof(source_path));
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
// - Clean tmp folder after compression + Refactor Code