#include "process.h"
#include <dirent.h>

void decompress_file(FILE *fp, FILE *out_fp, ChunkHeader header) {
    // 初始化 zlib 解压缩流
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (inflateInit(&strm) != Z_OK) {
        fprintf(stderr, "Failed to initialize zlib stream\n");
        return;
    }

    // 分配输入和输出缓冲区
    unsigned char in[CHUNK_SIZE];
    unsigned char out[CHUNK_SIZE];
    size_t remaining = header.size; // 压缩数据的剩余量

    // 读取和解压缩数据
    int ret;
    do {
        size_t to_read = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
        strm.avail_in = fread(in, 1, to_read, fp);
        if (strm.avail_in == 0) {
            break;
        }
        strm.next_in = in;
        remaining -= strm.avail_in;

        // 解压缩数据到输出缓冲区
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

    // 清理 zlib 流
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

    // 第一个文件的header.is_last一定为1。
    decompress_file(fp, out_fp, header);
    fclose(out_fp);
    out_fp = NULL;

    // 读取后续的header
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
    if ((dir = opendir(source_dir_path)) != NULL) {
        while ((ent = readdir(dir)) != NULL) {
            // Check if the file extension is .zwz
            if (strstr(ent->d_name, ".zwz") != NULL) {
                char file_path[1024];
                snprintf(file_path, sizeof(file_path), "%s/%s", source_dir_path, ent->d_name);
                // Step2: Process each .zwz file
                decompress_zwz(file_path, output_dir_path);
            }
        }
        closedir(dir);
    } else {
        perror("Could not open source directory");
    }
}
