#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

typedef struct {
    char *relpath; // Relative path
    off_t size;
} FileEntry;

int compare_file_size(const void *a, const void *b) {
    FileEntry *file1 = (FileEntry *) a;
    FileEntry *file2 = (FileEntry *) b;
    return (file2->size - file1->size); // Descending order
}

// This function recursively collects file information
void collect_files(const char *base_path, const char *current_path, FileEntry **files, int *count) {
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    if (!(dir = opendir(current_path)))
        return;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[1024];
        sprintf(fullpath, "%s/%s", current_path, entry->d_name);

        if (stat(fullpath, &statbuf) == 0) {
            if (S_ISREG(statbuf.st_mode)) { // If it's a file
                char relpath[1024];
                sprintf(relpath, "%s", fullpath + strlen(base_path) + 1); // +1 to remove leading '/'

                *files = realloc(*files, sizeof(FileEntry) * (*count + 1));
                (*files)[*count].relpath = strdup(relpath);
                (*files)[*count].size = statbuf.st_size;
                (*count)++;
            } else if (S_ISDIR(statbuf.st_mode)) { // If it's a directory
                collect_files(base_path, fullpath, files, count);
            }
        }
    }
    closedir(dir);
}

void sort_files_by_size(const char *path) {
    FileEntry *files = NULL;
    int count = 0;

    char parent_directory[1024];
    // Get the parent directory
    if (strcmp(path, "/") == 0) {
        strcpy(parent_directory, "/");
    } else {
        // Copy the path to parent_directory
        strcpy(parent_directory, path);

        // Find the position of the last slash
        int len = strlen(parent_directory);
        int last_slash = -1;
        for (int i = len - 1; i >= 0; i--) {
            if (parent_directory[i] == '/') {
                last_slash = i;
                break;
            }
        }

        // If a slash is found, replace it with a null terminator
        if (last_slash >= 0) {
            parent_directory[last_slash] = '\0';
        }
    }

    dirname(parent_directory); // Getting the parent directory

    collect_files(path, path, &files, &count); // Pass base path twice initially

    qsort(files, count, sizeof(FileEntry), compare_file_size);

    char output_filename[1024];
    sprintf(output_filename, "%s/sorted_files_by_size.txt", parent_directory);
    FILE *file = fopen(output_filename, "w");
    if (file != NULL) {
        for (int i = 0; i < count; i++) {
            fprintf(file, "%s (%lld bytes)\n", files[i].relpath, files[i].size);
            free(files[i].relpath);
        }
        fclose(file);
    }
    free(files);
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    if (world_rank == 0) {
        if (argc < 2) {
            printf("Usage: %s <directory path>\n", argv[0]);
            MPI_Abort(MPI_COMM_WORLD, 1);
            return 1;
        }
        sort_files_by_size(argv[1]);
    }

    MPI_Barrier(MPI_COMM_WORLD); // Ensuring all processes reach this point
    MPI_Finalize();
    return 0;
}
