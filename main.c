#include <stdio.h>
#include <string.h>
#include "process.h"

void compress(const char *folder_path) {
    printf("Compressing folder: %s\n", folder_path);

    sort_files_by_size(folder_path);
}

void decompress(const char *folder_path) {
    // 解压缩操作的实现
    printf("Decompressing folder: %s\n", folder_path);
    // 这里添加解压缩的具体逻辑
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    if (argc < 3) {
        printf("Usage: %s <compress/decompress> <folder path>\n", argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    const char *operation = argv[1];
    const char *folder_path = argv[2];

    if (strcmp(operation, "compress") == 0) {
        compress(folder_path);
    } else if (strcmp(operation, "decompress") == 0) {
        decompress(folder_path);
    } else {
        printf("Invalid operation: %s. Please use 'compress' or 'decompress'.\n", operation);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    MPI_Finalize();
    return 0;
}

