#include "process.h"
#include <dirent.h>
// export OMP_STACKSIZE=16M
    // 2 locks: 1 between producer and all consumers, make sure consumer not read when producer write
    // 1 between all consumers, make sure only one consumer get the data
    // If I create and kill threads each time when I read a chunk, the performance would be bad
    // I need to create threads in once, and call them when I need them.
    // so, How to create all threads at once

    // MPI
    // one processor read the files and pass the file to other processors
    // let the consumer processors do their work in parallel

    // OpenMP
    // multiple readers and multiple workers + writers


    // master thread reads data, there is a number assosicated with the data chunk
    

    // each thread check if the chunk number should be processed by it
    // if not, keep waiting
    // if yes, process the data and write to specific position
    

// void decompress_file(FILE *fp, FILE *out_fp, ChunkHeader header, int chunkOffset) {
//     // omp_set_num_threads(5);
//     // Print "Decompressing file: [filename], is_last: [is_last], size: [size]"
//     long orgSize = header.size;
//     long remaining = header.size; // Remaining amount of compressed data
//     unsigned char masterIn[CHUNK_SIZE + 1000];
//     // const size_t stack_size = 16 * 1024 * 1024;
//     // omp_set_stacksize(stack_size);
    

//     // #pragma omp parallel 
//     // {
//         // int localN = 0;
//         long to_read = 0;
//         size_t readSize = 0;
//         int breakFlag = 0;

//         // Initialize zlib decompression stream
//         z_stream strm;
//         strm.zalloc = Z_NULL;
//         strm.zfree = Z_NULL;
//         strm.opaque = Z_NULL;
//         if (inflateInit(&strm) != Z_OK) {
//             fprintf(stderr, "Failed to initialize zlib stream\n");
//             // return;
//             exit(-1);
//         }

//         // Allocate input and output buffers
//         unsigned char localIn[CHUNK_SIZE + 1000];
//         unsigned char localOut[CHUNK_SIZE + 1000];

//         // int ret;
//         size_t ret;
//         long offSet = chunkOffset * CHUNK_SIZE;

//         do {
//             // read file synchronized
//             // #pragma omp critical 
//             // {
//                 to_read = remaining < CHUNK_SIZE ? remaining : CHUNK_SIZE;
//                 readSize = fread(masterIn, sizeof(unsigned char), to_read, fp);
//                 if (readSize == 0) {
//                     // #pragma omp barrier
//                     // break;
//                     breakFlag = 1;
//                     // printf("readSize == 0\n");
//                     // exit(-1);
//                 }

//                 strm.avail_in = readSize;

//                 offSet += orgSize - remaining;

//                 // printf("remaining: %d, offSet: %d, n: %d\n", remaining, offSet);
//                 // offSet = remaining;
//                 remaining -= readSize;
//                 // n++;
//                 // localN = n;
//                 memcpy(localIn, masterIn, CHUNK_SIZE + 1000);
//             // }

//             if (breakFlag == 1) {
//                 // #pragma omp barrier
//                 break;
//             } 

//             strm.next_in = localIn;
//             // Decompress data to output buffer
//             do {
//                 // stacksize(16 * 1024 * 1024);
//                 strm.avail_out = CHUNK_SIZE;
                
//                 strm.next_out = localOut;
                
//                 // #pragma omp critical
//                 // {
//                     ret = inflate(&strm, Z_NO_FLUSH);
                
//                 // ret = inflate(&strm, Z_NO_FLUSH);
//                 // #pragma omp critical 
//                 // {
//                 if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
//                     inflateEnd(&strm);
//                     fprintf(stderr, "Zlib decompression error: %d\n", ret);
//                     printf(ret);
//                     // return;
//                     exit(-1);
//                 }
                
//                 size_t have = CHUNK_SIZE - strm.avail_out;
//                 // fseek(out_fp, offSet, SEEK_SET); 
//                 // int temp = fseek(out_fp, offSet, SEEK_SET);   
//                 // printf("fseek() status: %d\n", temp);
//                 // for (int i = 0; i < CHUNK_SIZE + 1000; i++) {
//                 //     printf("%c", localOut[i]);
//                 // }
//                 // printf("\n");
//                 // printf("localOut: %d\n", localOut);
                
//                     fseek(out_fp, offSet, SEEK_SET); 
//                     fwrite(localOut, 1, have, out_fp);
//                 // }
//                 // }
                
//             } while (strm.avail_out == 0);
//         } while (ret != Z_STREAM_END && remaining > 0);
//         // Clean up zlib stream
//         inflateEnd(&strm);
//     // }
// }

void decompress_file(FILE *fp, FILE *out_fp, ChunkHeader header, int chunkOffset) {
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
            fseek(out_fp, chunkOffset, SEEK_SET); 
            fwrite(out, 1, have, out_fp);
        } while (strm.avail_out == 0);
    } while (ret != Z_STREAM_END && remaining > 0);

    // Clean up zlib stream
    inflateEnd(&strm);
}

// unsigned char[] void read_chunk() {
//     // read data to in
//     // stop reading when the data is taken out by consumer
//     // 
// }

// void decompress_chunk() {
//     // take data and copy it to its own in
//     // release the producer in
//     // 
// }

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
    decompress_file(fp, out_fp, header, 0);
    fclose(out_fp);
    out_fp = NULL;
    int chunkOffset = 0;

    // Read subsequent headers
    // #pragma omp parallel 
    // {
    ChunkHeader localHeader;
    size_t readStatus = 0;
    int localOffSet = 0;

    // #pragma omp critical 
    // {
        readStatus = fread(&localHeader, sizeof(ChunkHeader), 1, fp);
        localOffSet = chunkOffset;
        chunkOffset++;
        if (out_fp == NULL) {
            snprintf(output_file_path, sizeof(output_file_path), "%s/%s", output_dir_path, header.filename);
            out_fp = fopen(output_file_path, "wb");
            if (!out_fp) {
                fprintf(stderr, "Failed to open output file: %s\n", output_file_path);
                fclose(fp);
                // return;
                exit(-1);
            }
        }
    // }

    while (readStatus == 1) {
        decompress_file(fp, out_fp, localHeader, localOffSet);
        // chunkOffset++;
        if (localHeader.is_last == 1) {
            fclose(out_fp);
            out_fp = NULL;
            chunkOffset = 0;
        }
        // #pragma omp critical 
        // {
            readStatus = fread(&header, sizeof(ChunkHeader), 1, fp);
            localOffSet = chunkOffset;
            chunkOffset++;
        // }
    } 
    // }

    // while (fread(&header, sizeof(ChunkHeader), 1, fp) == 1) {
    //     if (out_fp == NULL) {
    //         snprintf(output_file_path, sizeof(output_file_path), "%s/%s", output_dir_path, header.filename);
    //         out_fp = fopen(output_file_path, "wb");
    //         if (!out_fp) {
    //             fprintf(stderr, "Failed to open output file: %s\n", output_file_path);
    //             fclose(fp);
    //             return;
    //         }
    //     }

    //     decompress_file(fp, out_fp, header, chunkOffset);
    //     chunkOffset++;
    //     if (header.is_last == 1) {
    //         fclose(out_fp);
    //         out_fp = NULL;
    //         chunkOffset = 0;
    //     }
    // }
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
                // Step 2: Process each .zwz file
                decompress_zwz(file_path, output_dir_path);
            }
        }
        closedir(dir);
    } else {
        perror("Could not open source directory");
    }
}
