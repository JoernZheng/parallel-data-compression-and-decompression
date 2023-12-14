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

char *get_chunk_hash(const Chunk *uncompressed_chunk) {
    MD5_CTX md5_context;
    unsigned char digest[MD5_DIGEST_LENGTH];
    char *hash = malloc(2 * MD5_DIGEST_LENGTH + 1);

    MD5_Init(&md5_context);

    MD5_Update(&md5_context, uncompressed_chunk->data, uncompressed_chunk->size);

    MD5_Final(digest, &md5_context);

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

void moveToBadDir(const char *filename, const char *file_path, const char *output_dir_path) {
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