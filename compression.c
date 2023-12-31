#include "process.h"

Chunk queue[QUEUE_SIZE];
int queue_head = 0;
int queue_tail = 0;

omp_lock_t queue_lock;

sem_t *sem_queue_not_full;
sem_t *sem_queue_not_empty;

int mpi_proc_size;
int mpi_proc_rank;

int max_record_line_num;

// Consumer's parameters
int final_chunk_processed = 0;
int processed_chunk_count = 0;
int active_consumer_count = NUM_CONSUMERS;

// Compressed file output
int next_output_id = 0;
char *output_filename;
FILE *dest;

// Write controllers
omp_lock_t write_lock;
int next_chunk_to_write = 0;

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

void data_writer(Chunk *chunk, long compressed_size, const unsigned char *compressed_data, FILE *dest) {
    ChunkHeader header;
    strncpy(header.filename, chunk->filename, sizeof(header.filename) - 1);
    header.filename[sizeof(header.filename) - 1] = '\0'; // Ensure null-termination
    header.size = compressed_size;
    header.is_last = chunk->is_last_chunk;
    
    // Calculate the hash value of each chunk and write it into the ChunkHeader
    // char *chunk_hash = get_chunk_hash(chunk);
    if (chunk->is_last_chunk == 1) {
        char *chunk_hash = get_hash(chunk->full_path);
        strncpy(header.hash_value, chunk_hash, sizeof(header.hash_value) - 1);
        header.hash_value[sizeof(header.hash_value) - 1] = '\0';
    }
    
    // printf("Hash of this chunk from %s is: %s\n", chunk->full_path, chunk_hash);

    fwrite(&header, sizeof(header), 1, dest);
    fwrite(compressed_data, sizeof(unsigned char), compressed_size, dest);
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
        // printf("file_number: %d, mpi_proc_size: %d, world_rank: %d\n", file_number, mpi_proc_size, world_rank);
        if (file_number == next_file_number) {
            // Increment next_file_number by mpi_proc_size before processing because we need this parameter as a signal
            // to tell the consumer that there is no more file to process when next_file_number >= max_record_line_num
            next_file_number += mpi_proc_size;
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
                chunk.id = chunk_number++; // chunk.id = chunk_id;
                extract_filename(chunk.filename, full_path);
                chunk.size = bytes_read;
                chunk.is_last_chunk = feof(source);
                if (next_file_number >= max_record_line_num) {
                    if (chunk.is_last_file == 0) {
                        chunk.is_last_file = 1;
                    }
                } else {
                    chunk.is_last_file = 0;
                }
               strncpy(chunk.full_path, full_path, sizeof(chunk.full_path) - 1);
               chunk.full_path[sizeof(chunk.full_path) - 1] = '\0';
                sem_wait(sem_queue_not_full);
                omp_set_lock(&queue_lock);
                queue[queue_tail] = chunk;
                queue_tail = (queue_tail + 1) % QUEUE_SIZE;
                omp_unset_lock(&queue_lock);
                sem_post(sem_queue_not_empty);

                // printf("Rank: %d, Thread: %d - Produced chunk %d\n", world_rank, omp_get_thread_num(), chunk_number - 1);
            }

            fclose(source);
        }

        file_number++;
    }

    printf("Rank: %d - Total processed file: %d\n", world_rank, file_number);
    printf("Rank: %d - Total produced chunk: %d\n", world_rank, chunk_number);
    fclose(record_file);
}

// Compress a chunk of data
void consumer() {
    while (!final_chunk_processed) {
        sem_wait(sem_queue_not_empty);

        // Avoid deadlock
        #pragma omp flush(final_chunk_processed)
        if (final_chunk_processed) {
            break;
        }

        omp_set_lock(&queue_lock);
        Chunk chunk = queue[queue_head];
        // printf("Chunk id: %d, is_last_file: %d, is_last_chunk: %d, filename: %s\n", chunk.id, chunk.is_last_file, chunk.is_last_chunk, chunk.filename);
        if (chunk.is_last_file && chunk.is_last_chunk) {
            final_chunk_processed = 1;
        }
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        omp_unset_lock(&queue_lock);
        sem_post(sem_queue_not_full);

        // Compress the chunk  -> zlib
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

        // deflate(&strm, chunk.is_last_chunk ? Z_FINISH : Z_NO_FLUSH);
        deflate(&strm, Z_FINISH);
        long compressed_size = CHUNK_SIZE + 1000 - strm.avail_out;
        // printf("Chunk id: %d, original_size: %zu, compressed_size: %u\n", chunk.id, chunk.size, CHUNK_SIZE + 1000 - strm.avail_out);
        // printf("compressed_size: %ld\n", compressed_size);

        deflateEnd(&strm);

        // Make sure only one thread is writing to the file according to the chunk id
        while (1) {
            omp_set_lock(&write_lock);
            #pragma omp flush(next_chunk_to_write)
            if (chunk.id == next_chunk_to_write) {
                // char *hash = get_chunk_hash(chunk.data);
                // printf("Chunk id: %d, hash value of this chunk from %s is: %s\n", chunk.id, chunk.filename, hash);
                data_writer(&chunk, compressed_size, out, dest);
                // printf("Chunk id: %d, next_chunk_to_write: %d\n", chunk.id, next_chunk_to_write);
                next_chunk_to_write++;
                // printf("next_chunk_to_write: %d\n", next_chunk_to_write);
                // printf("Write data for chunk %d, filename: %s\n", chunk.id, chunk.filename);
                omp_unset_lock(&write_lock);
                break;
            }
            omp_unset_lock(&write_lock);
        }

        #pragma omp atomic
        ++processed_chunk_count;
        // printf("Rank: %d, Thread: %d - Compressed chunk %d\n", mpi_proc_rank, omp_get_thread_num(), processed_chunk_count);

        if (final_chunk_processed && chunk.is_last_chunk) {
            for (int i = 0; i < omp_get_num_threads() - 1; ++i) {
                // printf("Rank: %d, Thread: %d - Send signals to unlock blocked threads\n", mpi_proc_rank, omp_get_thread_num());
                sem_post(sem_queue_not_empty);
            }
        }
    }
}

void init_output_filename(const char *output_dir, int world_rank) {
    output_filename = malloc(strlen(output_dir) + 256);
    int output_dir_len = strlen(output_dir);
    if (output_dir[output_dir_len - 1] == '/') {
        sprintf(output_filename, "%scompressed_%d.zwz", output_dir, world_rank);
    } else {
        sprintf(output_filename, "%s/compressed_%d.zwz", output_dir, world_rank);
    }
}

void write_file_record_to_dest(const char *file_record, FILE *dest) {
    FILE *record_file = fopen(file_record, "rb");
    if (!record_file) {
        perror("Error opening file record");
        return;
    }

    // Get file size
    fseek(record_file, 0, SEEK_END);
    long file_size = ftell(record_file);
    fseek(record_file, 0, SEEK_SET);

    // Read file content
    char *buffer = malloc(file_size);
    if (!buffer) {
        perror("Memory allocation failed");
        fclose(record_file);
        return;
    }
    fread(buffer, file_size, 1, record_file);
    fclose(record_file);

    // Initialize zlib compression stream
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    if (deflateInit(&strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
        free(buffer);
        return;
    }

    strm.avail_in = file_size;
    strm.next_in = (Bytef *)buffer;

    // Initialize dynamic buffer
    size_t out_capacity = CHUNK_SIZE;  // Initial capacity
    unsigned char *out = malloc(out_capacity);
    if (!out) {
        perror("Memory allocation failed");
        free(buffer);
        deflateEnd(&strm);
        return;
    }

    strm.avail_out = out_capacity;
    strm.next_out = out;

    // Loop to compress and dynamically expand buffer
    int ret;
    do {
        ret = deflate(&strm, Z_FINISH);
        if (strm.avail_out == 0) {
            // Buffer is full, need to expand
            out_capacity *= 2;
            unsigned char *new_out = realloc(out, out_capacity);
            if (!new_out) {
                perror("Memory reallocation failed");
                free(buffer);
                free(out);
                deflateEnd(&strm);
                return;
            }
            out = new_out;
            strm.avail_out = out_capacity - (strm.next_out - out);
            strm.next_out = out + (out_capacity / 2);
        }
    } while (ret == Z_OK);

    deflateEnd(&strm);

    // Record size of compressed data
    size_t compressed_size = strm.next_out - out;

    // Create file header and write it
    ChunkHeader header;
    extract_filename(header.filename, file_record);
    header.size = compressed_size;
    header.is_last = 1;
    fwrite(&header, sizeof(ChunkHeader), 1, dest);

    // Write compressed data
    fwrite(out, 1, compressed_size, dest);

    free(buffer);
    free(out);
}

void do_compression(const char *input_dir, const char *output_dir, const char *file_record, int world_rank) {
    omp_init_lock(&queue_lock);
    omp_init_lock(&write_lock);
    omp_set_num_threads(NUM_CONSUMERS + 1);

    init_semaphores(world_rank);

    MPI_Comm_size(MPI_COMM_WORLD, &mpi_proc_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_proc_rank);

    max_record_line_num = count_non_empty_lines(file_record);

    init_output_filename(output_dir, world_rank);
    dest = fopen(output_filename, "wb");
    write_file_record_to_dest(file_record, dest);

    printf("Initialized semaphores in %d process\n", world_rank);
    printf("Max record line num: %d\n", max_record_line_num);

    #pragma omp parallel sections
    {
        #pragma omp critical
        {
            producer(input_dir, file_record, world_rank);
        }

        #pragma omp section
        {
            for (int i = 0; i < NUM_CONSUMERS; ++i) {
                #pragma omp task
                {
                    consumer();
                }
            }
        }
    }

    omp_destroy_lock(&queue_lock);
    omp_destroy_lock(&write_lock);
    destroy_semaphores(world_rank);
    fclose(dest);
}
