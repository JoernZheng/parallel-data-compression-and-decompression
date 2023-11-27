#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <zlib.h>
#include <string.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mpi.h>

#define CHUNK_SIZE 16384
#define QUEUE_SIZE 100
#define NUM_CONSUMERS 4

typedef struct {
    unsigned char data[CHUNK_SIZE];
    size_t size;
    int is_last;
} Chunk;

typedef struct {
    unsigned char content[CHUNK_SIZE];
    size_t size;
    int written; // 0: not written, 1: written
} CompressedChunk;

Chunk queue[QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;

CompressedChunk compressedQueue[QUEUE_SIZE];
int current_chunk_id = 0; // The id of the chunk that is being compressed

omp_lock_t lock;
omp_lock_t queue_lock;

sem_t *sem_queue_not_full;
sem_t *sem_queue_not_empty;

int mpi_proc_size;

void init_semaphores(int world_rank) {
    char sem_name_full[256];
    char sem_name_empty[256];

    sprintf(sem_name_full, "/sem_queue_not_full_%d", world_rank);
    sprintf(sem_name_empty, "/sem_queue_not_empty_%d", world_rank);

    sem_unlink(sem_name_full);
    sem_unlink(sem_name_empty);

    sem_queue_not_full = sem_open(sem_name_full, O_CREAT, S_IRUSR | S_IWUSR, QUEUE_SIZE);
    sem_queue_not_empty = sem_open(sem_name_empty, O_CREAT, S_IRUSR | S_IWUSR, 0);

    if (sem_queue_not_full == SEM_FAILED || sem_queue_not_empty == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }
}

void destroy_semaphores(int world_rank) {
    char sem_name_full[256];
    char sem_name_empty[256];

    sprintf(sem_name_full, "/sem_queue_not_full_%d", world_rank);
    sprintf(sem_name_empty, "/sem_queue_not_empty_%d", world_rank);

    sem_close(sem_queue_not_full);
    sem_close(sem_queue_not_empty);
    sem_unlink(sem_name_full);
    sem_unlink(sem_name_empty);
}

// Create chunks of data from files
void producer(const char *input_dir, const char *file_record, int world_rank) {
    FILE *record_file = fopen(file_record, "r");
    if (record_file == NULL) {
        printf("Rank: %d - Error opening file record: %s\n", world_rank, file_record);
        return;
    }

    char file_path[1024];
    char format_string[20];
    int file_number = 0;
    int next_file_number = world_rank;
    int input_dir_len = strlen(input_dir);

    // Debug
    int chunk_number = 0;

    // Remove trailing '/' if there is one
    if (input_dir[input_dir_len - 1] == '/') {
        sprintf(format_string, "%%s%%s");
    } else {
        sprintf(format_string, "%%s/%%s");
    }

    while (fgets(file_path, sizeof(file_path), record_file) != NULL) {
        char *parenthesis = strchr(file_path, '(');
        if (parenthesis != NULL && parenthesis != file_path) {
            *(parenthesis - 1) = '\0';
        }

        // Only process files that are assigned to this process
        printf("file_number: %d, mpi_proc_size: %d, world_rank: %d\n", file_number, mpi_proc_size, world_rank);
        if (file_number == next_file_number) {
            char full_path[1024];
            sprintf(full_path, format_string, input_dir, file_path);
            printf("Rank: %d - Processing file: %s\n", world_rank, full_path);
            full_path[strcspn(full_path, "\n")] = 0; // Remove trailing '\n'

            FILE *source = fopen(full_path, "rb");
            if (source == NULL) {
                char error_message[1024];
                snprintf(error_message, sizeof(error_message), "Error opening source file: %s", full_path);
                perror(error_message);
                continue;
            }

            Chunk chunk;
            size_t bytes_read;
            while ((bytes_read = fread(chunk.data, 1, CHUNK_SIZE, source)) > 0) {
                chunk.size = bytes_read;
                chunk.is_last = feof(source);

                sem_wait(sem_queue_not_full);
                omp_set_lock(&queue_lock);
                queue[queue_tail] = chunk;
                queue_tail = (queue_tail + 1) % QUEUE_SIZE;
                omp_unset_lock(&queue_lock);
                sem_post(sem_queue_not_empty);

                printf("Rank: %d - Produced chunk %d\n", world_rank, ++chunk_number);
            }

            fclose(source);
            next_file_number += mpi_proc_size;
        }

        file_number++;
    }

    fclose(record_file);
}


// Compress a chunk of data
void consumer() {
    int last_chunk_processed = 0;
    while (!last_chunk_processed) {
        sem_wait(sem_queue_not_empty);
        omp_set_lock(&queue_lock);

        Chunk chunk = queue[queue_head];
        last_chunk_processed = chunk.is_last; // If this is the last chunk, we will stop after processing it
        queue_head = (queue_head + 1) % QUEUE_SIZE;

        omp_unset_lock(&queue_lock);
        sem_post(sem_queue_not_full);

        // Compress the chunk
        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        deflateInit(&strm, Z_DEFAULT_COMPRESSION);

        strm.avail_in = chunk.size;
        strm.next_in = chunk.data;
        unsigned char out[CHUNK_SIZE];
        strm.avail_out = CHUNK_SIZE;
        strm.next_out = out;

        deflate(&strm, chunk.is_last ? Z_FINISH : Z_NO_FLUSH);
        size_t compressed_size = CHUNK_SIZE - strm.avail_out;

        deflateEnd(&strm);

        // Add the compressed chunk to the Array
        omp_set_lock(&lock);
        int chunk_id = current_chunk_id++;
        CompressedChunk *compressedChunk = &compressedQueue[chunk_id % QUEUE_SIZE];
        memcpy(compressedChunk->content, out, compressed_size);
        compressedChunk->size = compressed_size;
        compressedChunk->written = 0;
        omp_unset_lock(&lock);

        if (last_chunk_processed) {
            omp_set_lock(&lock);
            compressedChunk = &compressedQueue[current_chunk_id % QUEUE_SIZE];
            compressedChunk->size = (size_t)-1; // Mark this chunk as the last chunk
            compressedChunk->written = 0;
            omp_unset_lock(&lock);
        }
    }
}

// Write compressed chunks to file
void writer(const char *output_filename) {
    FILE *dest = fopen(output_filename, "wb");
    int written_chunk_id = 0;

    while (1) {
        CompressedChunk *compressedChunk = &compressedQueue[written_chunk_id % QUEUE_SIZE];
        if (compressedChunk->size == (size_t)-1) { // If this is the last chunk
            omp_unset_lock(&lock);
            break;
        }

        if (!compressedChunk->written && compressedChunk->size > 0) {
            fwrite(compressedChunk->content, 1, compressedChunk->size, dest);
            compressedChunk->written = 1;
            written_chunk_id++;
        }
    }

    fclose(dest);
}

void do_compression(const char *input_dir, const char *output_dir, const char *file_record, int world_rank) {
    omp_init_lock(&lock);
    omp_init_lock(&queue_lock);

    init_semaphores(world_rank);

    MPI_Comm_size(MPI_COMM_WORLD, &mpi_proc_size);

    printf("Initialized semaphores in %d process\n", world_rank);

    #pragma omp parallel sections
    {
        #pragma omp critical
        {
            printf("Producer %d\n", world_rank);
            producer(input_dir, file_record, world_rank);
            printf("Producer %d finished\n", world_rank);
        }

//        #pragma omp section
//        {
//            printf("Consumer %d\n", world_rank);
//            for (int i = 0; i < NUM_CONSUMERS; ++i) {
//                #pragma omp task
//                {
//                    consumer();
//                }
//            }
//        }
//
//        #pragma omp section
//        {
//            char *output_filename = malloc(strlen(output_dir) + 256);
//            int output_dir_len = strlen(output_dir);
//            if (output_dir[output_dir_len - 1] == '/') {
//                sprintf(output_filename, "%scompressed_%d.tar.gz", output_dir, world_rank);
//            } else {
//                sprintf(output_filename, "%s/compressed_%d.tar.gz", output_dir, world_rank);
//            }
//            writer(output_filename);
//        }
    }

    omp_destroy_lock(&lock);
    omp_destroy_lock(&queue_lock);
    destroy_semaphores(world_rank);
}
