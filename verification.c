// 压缩过程中：
// 1.扫描所有文件，计算文件内容的哈希值
// 2.计算全局唯一的哈希值
// 3.把哈希值写入到压缩包的末尾

// 解压过程中：
// 1.在解压过程中/完成后，计算全局唯一的哈希值  MD5等都行
// 2.和压缩包末尾的哈希值进行比较

#include "process.h"
#include <dirent.h>
#include <sys/stat.h>

char *get_hash(const char *full_path) {
    size_t bytes_read;
    MD5_CTX md5_context;
    unsigned char buffer[BUFFER_SIZE];
    unsigned char digest[MD5_DIGEST_LENGTH];
    char *hash = malloc(2 * MD5_DIGEST_LENGTH + 1);

    FILE *file = fopen(full_path, "rb");
    if (!file) {
        perror("Error opening the file");
        return NULL;
    }

    MD5_Init(&md5_context);

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) != 0) {
        MD5_Update(&md5_context, buffer, bytes_read);
    }

    MD5_Final(digest, &md5_context);

    fclose(file);

    // convert binary digest to a string
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&hash[i * 2], "%02x", digest[i]);
    }

    return hash;
}

void verify(const char *hash_header, const char *hash_decmprs, const char *filename, const char *file_path, const char *output_dir_path) {
    if (strcmp(hash_decmprs, hash_header) != 0) {
        DIR *dir;
        char dest[1024];
        sprintf(dest, "%s/%s", output_dir_path, filename);

        // move bad files into the "bad" directory
        if ((dir = opendir(output_dir_path)) != NULL || mkdir(output_dir_path, 0777) == 0) {
            if (rename(file_path, dest) == 0) {
                printf("Bad file <%s> found. Moved to <%s> directory\n", filename, output_dir_path);
            } else {
                perror("Error moving file");
            }

            closedir(dir);
        }
    }
}

void do_verification(const char *source_path, const char *output_path) {
    // printf("%s\n", source_path);
    DIR *dir;
    struct dirent *ent;
    if ((dir = opendir(source_path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            if (strcmp(ent->d_name, "sorted_files_by_size.txt") != 0 && strstr(ent->d_name, ".txt") != NULL) {
                char full_path[1024];
                snprintf(full_path, sizeof(full_path), "%s/%s", source_path, ent->d_name);
                char *hash = get_hash(full_path);
                // printf("hash of %s: %s\n", full_path, hash);
            } 
        }

        closedir(dir);
    } else {
        perror("Could not open source directory");
    }
}