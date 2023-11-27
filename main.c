#include <stdio.h>
#include <string.h>
#include "process.h"
#include <stdlib.h>

void compress(const char *folder_path, const char *output_path) {
    int world_rank;
    char *file_record = NULL;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    char file_record_path[1024];  // 假设路径长度不超过1023个字符

    // 1.Sort and record files by size
    if (world_rank == 0) {
        printf("Compressing folder: %s\n", folder_path);
        char *file_record = sort_files_by_size(folder_path);  // 创建文件记录
        printf("File record saved location: %s\n", file_record);
        strncpy(file_record_path, file_record, sizeof(file_record_path) - 1);  // 复制路径到 file_record_path
        file_record_path[sizeof(file_record_path) - 1] = '\0';  // 确保字符串以空字符结束
        free(file_record);  // 释放原始 file_record 指针
    }

    // 2.Broadcast file_record_path
    MPI_Bcast(file_record_path, sizeof(file_record_path), MPI_CHAR, 0, MPI_COMM_WORLD);

    // 3.Compress files
    do_compression(folder_path, output_path, file_record_path, world_rank);
    printf("main.c - Rank: %d - do_compression finished\n", world_rank);

    // 4.Clean up
    if (file_record != NULL) {
        free(file_record);
    }
}

void decompress(const char *folder_path) {
    // 解压缩操作的实现
    printf("Decompressing folder: %s\n", folder_path);
    // 这里添加解压缩的具体逻辑
}

int main(int argc, char *argv[]) {
    MPI_Init(&argc, &argv);

    if (argc < 4) {
        printf("Usage: %s <compress/decompress> <folder path> <output_dir path>\n", argv[0]);
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }

    const char *operation = argv[1];
    const char *folder_path = argv[2];
    const char *output_path = argv[3];

    if (strcmp(operation, "compress") == 0) {
        compress(folder_path, output_path);
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

