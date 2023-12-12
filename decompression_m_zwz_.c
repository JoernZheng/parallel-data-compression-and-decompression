#include "process.h"
#include <dirent.h>
#include <string.h>

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

    // Read subsequent headers
    while (fread(&header, sizeof(ChunkHeader), 1, fp) == 1) {
        if (out_fp == NULL) {
            snprintf(output_file_path, sizeof(output_file_path), "%s/%s", output_dir_path, header.filename);
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


