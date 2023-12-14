#include "process.h"
#include <dirent.h>

struct HashMap* filePathMap;

int createDirectory(const char *path) {
    struct stat st;

    // Check if the directory already exists
    if (stat(path, &st) == -1)
    {
        // Directory doesn't exist, so create it
        if (mkdir(path, 0777) != 0)
        {
            perror("Error creating directory");
            return 1; // Return an error code
        }
    }

    return 0; // Return success
}

int createDirectories(const char *path) {
    char *pathCopy = strdup(path);
    char *token = strtok(pathCopy, "/");
    char currentPath[1024];

    strcpy(currentPath, token);

    while (token != NULL)
    {
        if (createDirectory(currentPath) != 0)
        {
            free(pathCopy);
            return 1; // Return an error code
        }

        token = strtok(NULL, "/");
        if (token != NULL)
        {
            strcat(currentPath, "/");
            strcat(currentPath, token);
        }
    }

    free(pathCopy);
    return 0; // Return success
}

int decompress_file(FILE *fp, FILE *out_fp, ChunkHeader header) {
    // Print "Decompressing file: [filename], is_last: [is_last], size: [size]"
    if (strcmp(header.filename, "sorted_files_by_size.txt") != 0) {
        printf("Hash of this chunk from <%s> is: %s\n", header.filename, header.hash_value);
    }
    int hashMatch = 0;

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
    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];

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

            Chunk decompressedChunk;
            // decompressedChunk.data = out;
            strncpy(decompressedChunk.data, out, have);
            decompressedChunk.data[have] = '\0';
            decompressedChunk.size = have + 1;

            // decompressedChunk.size = have;
            
            char* decompressedChunkHash = get_chunk_hash(&decompressedChunk);
            if (strcmp(decompressedChunkHash, header.hash_value) != 0) {
                printf("decompressedChunkHash: %s\n", decompressedChunkHash);
                printf("header.hash_value: %s\n", header.hash_value);
                hashMatch = -1;
            }

        } while (strm.avail_out == 0);
    } while (ret != Z_STREAM_END && remaining > 0);

    // Clean up zlib stream
    inflateEnd(&strm);
    return hashMatch;
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

    char output_file_path[1024];
    snprintf(output_file_path, sizeof(output_file_path), "%s/%s", output_dir_path, header.filename);

    FILE *out_fp = fopen(output_file_path, "wb");
    if (!out_fp) {
        fprintf(stderr, "Failed to open output file: %s\n", output_file_path);
        fclose(fp);
        return;
    }

    // The header.is_last of the first file must be 1
    int hashMatch = 0;
    decompress_file(fp, out_fp, header);
    fclose(out_fp);
    out_fp = NULL;
    // compare the hash value after decompression to verify the correctness of decompression
    char *hash = get_hash(output_file_path);
    char* bad_dir_name;
    size_t bad_dir_len;
    char *bad_dir;

    // char* bad_dir_name = "bad";
    // size_t bad_dir_len = strlen(fullPath) + 1 + strlen(bad_dir_name) + 1;
    // char *bad_dir = (char*)malloc(bad_dir_len);
    // snprintf(bad_dir, strlen(bad_dir), "%s/%s", output_dir_path, bad_dir_name);
    
    // verify(header.hash_value, hash, header.filename, output_file_path, bad_dir);

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
            if (createDirectories(fullPath) != 0) {
                printf("Failed to create directories: %s\n", fullPath);
                fclose(fp);
                return;
            }

            // get the target file
            snprintf(output_file_path, sizeof(output_file_path), "%s/%s", fullPath, header.filename);
            out_fp = fopen(output_file_path, "wb");
            if (!out_fp) {
                fprintf(stderr, "Failed to open output file: %s\n", output_file_path);
                fclose(fp);
                return;
            }
        }

        int currHashMatch = decompress_file(fp, out_fp, header);
        if (currHashMatch != 0) hashMatch = -1;
        if (header.is_last == 1) {
            fclose(out_fp);
            out_fp = NULL;
            
            // compare the hash value after decompression to verify the correctness of decompression
            if (hashMatch != 0) {
                hash = get_hash(output_file_path);
                bad_dir_name = "bad";
                size_t bad_dir_len = strlen(fullPath) + 1 + strlen(bad_dir_name) + 1;
                bad_dir = (char*)malloc(bad_dir_len);
                snprintf(bad_dir, bad_dir_len, "%s/%s", fullPath, bad_dir_name);
                
                // verify(header.hash_value, hash, header.filename, output_file_path, bad_dir);
                moveToBadDir(header.filename, output_file_path, bad_dir);

                printf("header.hash_value: %s\n", header.hash_value);
                printf("hash: %s\n", hash);
            }
            

            hashMatch = 0;
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