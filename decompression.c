#include "process.h"
#include <dirent.h>
#include <errno.h>

struct HashMap* filePathMap;

int create_directory(const char *path) {
    return mkdir(path, 0777); // Using 0777 permission
}

// Recursive function to create directories
void createDirectories(const char *path) {
    char temp[1024];
    strncpy(temp, path, sizeof(temp));
    temp[sizeof(temp) - 1] = '\0';

    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            // Temporarily end the string here
            *p = '\0';

            // Create the directory and check for error
            if (create_directory(temp) != 0 && errno != EEXIST) {
                perror("create_directory failed");
                exit(EXIT_FAILURE);
            }

            // Restore the slash for the next iteration
            *p = '/';
        }
    }

    // Create the last directory in the path
    if (create_directory(temp) != 0 && errno != EEXIST) {
        perror("create_directory failed");
        exit(EXIT_FAILURE);
    }
}

void decompress_file(FILE *fp, FILE *out_fp, ChunkHeader header) {
    // Initialize zlib decompression stream
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (inflateInit(&strm) != Z_OK) {
        fprintf(stderr, "Failed to initialize zlib stream\n");
        return;
    }

    long remaining = header.size; // Remaining amount of compressed data

    // Dynamically allocate input and output buffers based on the chunk size
    unsigned char *in = (unsigned char *)malloc(CHUNK_SIZE);
    unsigned char *out = (unsigned char *)malloc(CHUNK_SIZE);
    if (in == NULL || out == NULL) {
        fprintf(stderr, "Memory allocation for buffers failed\n");
        if (in != NULL) free(in);
        if (out != NULL) free(out);
        inflateEnd(&strm);
        return;
    }

    int ret;
    do {
        long to_read = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        strm.avail_in = fread(in, sizeof(unsigned char), to_read, fp);
        if (strm.avail_in == 0) {
            break;
        }
        strm.next_in = in;
        remaining -= strm.avail_in;

        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                fprintf(stderr, "Zlib decompression error: %d\n", ret);
                break;
            }
            size_t have = CHUNK_SIZE - strm.avail_out;
            fwrite(out, 1, have, out_fp);
        } while (strm.avail_out == 0);

    } while (ret != Z_STREAM_END && remaining > 0);

    // Free the allocated memory
    free(in);
    free(out);

    // Clean up zlib stream
    inflateEnd(&strm);
}

struct HashMap* generateFileNamePathMap(char* output_file_path) {
    FILE* sortSizeFile = fopen(output_file_path, "r");
    if (sortSizeFile == NULL) {
        fprintf(stderr, "Error opening sorted txt file.\n");
        return NULL;
    }

    struct HashMap* filePathMap = createHashMap(HASHMAP_INIT_SIZE);

    char line[MAX_LINE_LENGTH];
    while (fgets(line, sizeof(line), sortSizeFile) != NULL) {
        // Remove trailing newline character
        line[strcspn(line, "\n")] = '\0';

        // Split the line into path and file name
        *(strrchr(line, '(') - 1) = '\0';
        char* fileName;
        char* filePath;
        if (strrchr(line, '/') != NULL) {
            fileName = strrchr(line, '/') + 1;
            *strrchr(line, '/') = '\0';
            filePath = (char *)malloc(strlen(line) + 1);
            filePath[0] = '/';
            strcpy(filePath + 1, line);
        } else {
            fileName = line;
            filePath = "";
        }
        
        insert(filePathMap, fileName, filePath);
    }
    fclose(sortSizeFile);
    return filePathMap;
}

char* generateFullPath(const char *output_dir_path, struct HashMap* filePathMap, ChunkHeader header) {
    char* relativePath = search(filePathMap, header.filename);
            
    size_t fullPathSize = 0;
    if (relativePath == NULL) {
        fullPathSize = strlen(output_dir_path) + 1;
    } else {
        fullPathSize = strlen(output_dir_path) + strlen(relativePath) + 1;
    }
    char* fullPath = (char *)malloc(fullPathSize);
    if (fullPath == NULL) {
        fprintf(stderr, "fullPath Memory allocation failed\n");
        return NULL;
    }
    strcpy(fullPath, output_dir_path);
    if (relativePath != NULL) strcat(fullPath, relativePath);
    return fullPath;
}

void decompress_zwz(const char *file_path, const char *output_dir_path) {
    FILE *fp = fopen(file_path, "rb");
    if (!fp) {
        fprintf(stderr, "Failed to open compressed file: %s\n", file_path);
        return;
    }

    // Read the first header
    ChunkHeader header;
    if (fread(&header, sizeof(ChunkHeader), 1, fp) != 1) {
        fprintf(stderr, "Failed to read first header from: %s\n", file_path);
        fclose(fp);
        return;
    }

    printf("ChunkHeader: filename: %s, size: %ld, is_last: %d\n", header.filename, header.size, header.is_last);

    char output_file_path[1024];
    snprintf(output_file_path, sizeof(output_file_path), "%s/%s", output_dir_path, header.filename);

    FILE *out_fp = fopen(output_file_path, "wb");
    if (!out_fp) {
        fprintf(stderr, "Failed to open output file: %s\n", output_file_path);
        fclose(fp);
        return;
    }

    // The header.is_last of the first file must be 1
    decompress_file(fp, out_fp, header);
    fclose(out_fp);
    out_fp = NULL;
    // compare the hash value after decompression to verify the correctness of decompression
    char *hash = get_hash(output_file_path);
    char* bad_dir_name;
    size_t bad_dir_len;
    char *bad_dir;

    // Read sort size file
    filePathMap = generateFileNamePathMap(output_file_path);
    if (filePathMap == NULL) {
        printf("filePathMap is NULL\n");
        exit(-1);
    }

    char* fullPath;
    // Read subsequent headers
    while (fread(&header, sizeof(ChunkHeader), 1, fp) == 1) {
        
        if (out_fp == NULL) {
            fullPath = generateFullPath(output_dir_path, filePathMap, header);
            if (fullPath == NULL){
                fclose(fp);
                return;
            }

            // create non-existing directories
            createDirectories(fullPath);

            // get the target file
            snprintf(output_file_path, sizeof(output_file_path), "%s/%s", fullPath, header.filename);
            out_fp = fopen(output_file_path, "wb");
            if (!out_fp) {
                fprintf(stderr, "Failed to open output file: %s\n", output_file_path);
                fclose(fp);
                return;
            }
        }

        decompress_file(fp, out_fp, header);
        if (header.is_last == 1) {
            if (strcmp(header.filename, "sorted_files_by_size.txt") != 0) {
                printf("Decompressing file: %s, is_last: %d, size: %ld\n", header.filename, header.is_last, header.size);
                // printf("Hash: %s\n", header.hash_value);
            }
            fclose(out_fp);
            out_fp = NULL;
            
            // compare the hash value after decompression to verify the correctness of decompression
            hash = get_hash(output_file_path);
            bad_dir_name = "bad";
            size_t bad_dir_len = strlen(fullPath) + 1 + strlen(bad_dir_name) + 1;
            bad_dir = (char*)malloc(bad_dir_len);
            snprintf(bad_dir, bad_dir_len, "%s/%s", fullPath, bad_dir_name);
            verify(header.hash_value, hash, header.filename, output_file_path, bad_dir);
        }
    }
    free(bad_dir);
    destroyHashMap(filePathMap);
}

void do_decompression(const char *source_dir_path, const char *output_dir_path) {
    // Step 1: Iterate through all files in the source directory, only deal with .zwz files
    DIR *dir;
    struct dirent *ent;
    int readEnd = 0;

    dir = opendir(source_dir_path);
    if (dir == NULL) perror("Could not open source directory");
    else {
        // multi-threads decompressing .zwz files
        #pragma omp parallel 
        {
            char* d_name_copy;

            // update ent and d_name_copy
            #pragma omp critical 
            {
                ent = readdir(dir);
                if (ent == NULL) {
                    readEnd = 1;
                } else {
                    d_name_copy = (char *)malloc(strlen(ent->d_name) + 1);
                    strcpy(d_name_copy, ent->d_name);
                }
            }

            while (!readEnd){
                if (strstr(d_name_copy, ".zwz") != NULL) {
                    char file_path[1024];
                    snprintf(file_path, sizeof(file_path), "%s/%s", source_dir_path, d_name_copy);
                    // Step 2: Process each .zwz file
                    decompress_zwz(file_path, output_dir_path);
                }
                free(d_name_copy);
                // update ent and d_name_copy
                #pragma omp critical 
                {
                    ent = readdir(dir);
                    if (ent == NULL) {
                        readEnd = 1;
                    } else {
                        d_name_copy = (char *)malloc(strlen(ent->d_name) + 1);
                        strcpy(d_name_copy, ent->d_name);
                    }
                }
            }
        }
        closedir(dir);
    }
}