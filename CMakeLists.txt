cmake_minimum_required(VERSION 3.26)
project(Final_Demo CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(/opt/homebrew/Cellar/open-mpi/5.0.1/include)
include_directories(/opt/homebrew/Cellar/libomp/17.0.6/include)
include_directories(/opt/homebrew/Cellar/openssl@3/3.2.0_1/include)


add_executable(main main.cpp compression.cpp decompression.cpp file_process/file_sort.cpp file_process/file_tools.cpp
        verification.cpp)
