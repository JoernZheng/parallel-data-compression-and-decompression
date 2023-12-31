#include "concurrence_queue.h"
#include "process.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <omp.h>
#include <string>

int mpi_proc_size;
int mpi_proc_rank;

int max_record_line_num;

ConcurrenceQueue<Chunk> queue;

void producer(const std::string &input_dir, const std::string &file_record, int world_rank) {
    std::ifstream record_file(file_record);
    if (!record_file.is_open()) {
        std::cerr << "Rank: " << world_rank << " - Error opening file record: " << file_record << std::endl;
        return;
    }

    int file_number = 0;
    int next_file_number = world_rank;
    std::string file_path;

    while (std::getline(record_file, file_path)) {
        // Only process files that are assigned to this process
        // printf("file_number: %d, mpi_proc_size: %d, world_rank: %d\n", file_number, mpi_proc_size, world_rank);
        if (file_number == next_file_number) {
            // Increment next_file_number by mpi_proc_size before processing because we need this parameter as a signal
            // to tell the consumer that there is no more file to process when next_file_number >= max_record_line_num
            next_file_number += mpi_proc_size;
            std::filesystem::path full_path = std::filesystem::path(input_dir) / file_path;

            std::ifstream source(full_path, std::ios::binary);
            if (!source.is_open()) {
                std::cerr << "Error opening source file: " << full_path << std::endl;
                continue;
            }

            int sequence_id = 0;// sequence_id is used to identify the order of the chunk in the file

            while (!source.eof()) {
                Chunk chunk;
                chunk.sequence_id = sequence_id++;
                chunk.relative_path = file_path;
                source.read(reinterpret_cast<char *>(chunk.data.data()), CHUNK_SIZE);
                chunk.size = source.gcount();
                chunk.is_last_chunk = source.eof();
                chunk.is_last_file = (next_file_number >= max_record_line_num) && chunk.is_last_chunk;

                std::cout << "Rank: " << world_rank << " - Pushing chunk: " << chunk.sequence_id << " - " << chunk.relative_path << "-" << chunk.size << std::endl;

                queue.push(chunk);
            }
        }

        file_number++;
    }

    std::cout << "Rank: " << world_rank << " - Total processed file: " << file_number << std::endl;
}


std::string generate_output_filename(const std::string& output_dir, int world_rank) {
    std::filesystem::path dir(output_dir);

    // Ensure the directory ends with a slash
    dir = dir / "";

    std::filesystem::path filename = dir / ("compressed_" + std::to_string(world_rank) + ".zwz");
    return filename.string();
}

void do_compression(const std::string &input_dir, const std::string &output_dir, const std::string &file_record, int world_rank) {
    //    omp_init_lock(&write_lock);
    omp_set_num_threads(NUM_CONSUMERS + 1);

    MPI_Comm_size(MPI_COMM_WORLD, &mpi_proc_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpi_proc_rank);

    max_record_line_num = count_non_empty_lines(file_record);

    std::string output_filename = generate_output_filename(output_dir, world_rank);
    std::ofstream dest(output_filename, std::ios::binary);
    //    write_file_record_to_dest(file_record, dest);

    std::cout << "Max record line num: " << max_record_line_num << std::endl;

    #pragma omp parallel sections
    {
        #pragma omp critical
        {
            producer(input_dir, file_record, world_rank);
        }

//        #pragma omp section
//        {
//            for (int i = 0; i < NUM_CONSUMERS; ++i) {
//                #pragma omp task
//                {
//                    consumer();
//                }
//            }
//        }
    }

    //    omp_destroy_lock(&write_lock);
    dest.close();
}