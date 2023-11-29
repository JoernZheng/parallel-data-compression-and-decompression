# slurm-data-packer

## How to Run the Program

### Dependencies
- MPI
- OpenMP
- zlib

For MacOS, these can be installed directly using Homebrew. On Linux, they are available through package managers like apt-get.

### IDE Function Suggestions/Resolving Header File Not Found Issues
Modify your dependency paths in `CMakeLists.txt`. Please ensure not to commit this file to your git repository.

### Compilation - Example
```
mpicc -fopenmp file_process/file_sort.c main.c compression.c file_process/file_tools.c decompression.c -o main -lz
```
### Execution - Example
```
mpirun -n 2 main compress /tmp/Cache/temp/data /tmp/Cache/temp/output
mpirun -n 1 main decompress /tmp/Cache/temp/output /tmp/Cache/temp/output2
```
