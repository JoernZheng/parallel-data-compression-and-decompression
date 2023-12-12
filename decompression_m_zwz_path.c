#include "process.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE_LENGTH 256

void decompress_file(FILE *fp, FILE *out_fp, ChunkHeader header) {
    // Print "Decompressing file: [filename], is_last: [is_last], size: [size]"

    // Initialize zlib decompression stream
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (inflateInit(&strm) != Z_OK) {
        fprintf(stderr, "Failed to initialize zlib stream\n");
        return;
    }

    // Allocate input and output buffers
    unsigned char in[CHUNK_SIZE + 1000];
    unsigned char out[CHUNK_SIZE + 1000];
    long remaining = header.size; // Remaining amount of compressed data

    // Read and decompress data
    int ret;
    do {
        long to_read = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        strm.avail_in = fread(in, sizeof(unsigned char), to_read, fp);
        if (strm.avail_in == 0) {
            break;
        }
        strm.next_in = in;
        remaining -= strm.avail_in;

        // Decompress data to output buffer
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret != Z_OK && ret != Z_STREAM_END) {
                inflateEnd(&strm);
                fprintf(stderr, "Zlib decompression error: %d\n", ret);
                return;
            }
            size_t have = CHUNK_SIZE - strm.avail_out;
            fwrite(out, 1, have, out_fp);
        } while (strm.avail_out == 0);
    } while (ret != Z_STREAM_END && remaining > 0);

    // Clean up zlib stream
    inflateEnd(&strm);
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

    // Read sort size file
    FILE* sortSizeFile = fopen(output_file_path, "r");
    if (sortSizeFile == NULL) {
        fprintf(stderr, "Error opening file.\n");
        return 1;
    }

    // Create a hashmap (array of linked lists)
    // struct Node* hashmap[MAX_LINE_LENGTH] = {NULL};
    struct HashMap* filePathMap = createHashMap(10);

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
        printf("%s: %s\n", fileName, filePath);
    }
    fclose(sortSizeFile);

    // Read subsequent headers
    while (fread(&header, sizeof(ChunkHeader), 1, fp) == 1) {
        if (out_fp == NULL) {
            // printf("%s\n", header.filename);
            char* relativePath = search(filePathMap, header.filename);
            
            size_t fullPathSize = 0;
            if (relativePath == NULL) {
                fullPathSize = strlen(output_dir_path) + 1;
            } else {
                fullPathSize = strlen(output_dir_path) + strlen(relativePath) + 1;
                printf("file name: %s, relativePath: %s\n", header.filename, relativePath);
            }
            char* fullPath = (char *)malloc(fullPathSize);
            if (fullPath == NULL) {
                fprintf(stderr, "fullPath Memory allocation failed\n");
            }
            strcpy(fullPath, output_dir_path);
            if (relativePath != NULL) strcat(fullPath, relativePath);

            struct stat st;
            if (stat(fullPath, &st) == -1) {
                // Directory doesn't exist, so create it
                if (mkdir(fullPath, 0777) == 0) {
                    printf("Directory created successfully: %s\n", fullPath);
                } else {
                    perror("Error creating directory");
                    fclose(fp);
                    return 1;
                }
            } else {
                printf("Directory already exists: %s\n", fullPath);
            }

            snprintf(output_file_path, sizeof(output_file_path), "%s/%s", fullPath, header.filename);
            printf("output_file_path: %s\n", output_file_path);
            // snprintf(output_file_path, sizeof(output_file_path), "%s/%s", output_dir_path, header.filename);
            out_fp = fopen(output_file_path, "wb");
            if (!out_fp) {
                fprintf(stderr, "Failed to open output file: %s\n", output_file_path);
                fclose(fp);
                return;
            }
        }

        decompress_file(fp, out_fp, header);
        if (header.is_last == 1) {
            fclose(out_fp);
            out_fp = NULL;
        }
    }
    destroyHashMap(filePathMap);
}

void do_decompression(const char *source_dir_path, const char *output_dir_path) {
    // Step 1: Iterate through all files in the source directory, only deal with .zwz files
    DIR *dir;
    struct dirent *ent;
    int readEnd = 0;
    // char* d_name_copy;

    dir = opendir(source_dir_path);
    if (dir == NULL) perror("Could not open source directory");
    else {
        // multi-threads decompressing .zwz files
        #pragma omp parallel 
        {
            char* d_name_copy;

            // update ent and d_name_copy
            #pragma omp critial 
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
                #pragma omp critial 
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



