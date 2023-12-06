// 压缩过程中：
// 1.扫描所有文件，计算文件内容的哈希值
// 2.计算全局唯一的哈希值
// 3.把哈希值写入到压缩包的末尾

// 解压过程中：
// 1.在解压过程中/完成后，计算全局唯一的哈希值  MD5等都行
// 2.和压缩包末尾的哈希值进行比较

#include "process.h"

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

void do_verification(const char *source_path, const char *output_path) {
    // printf("%s\n", source_path);
}