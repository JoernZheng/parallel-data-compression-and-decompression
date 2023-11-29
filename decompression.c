// 1. 读取文件
// 压缩包里的结构：
// |filename, size| content | filename, size, is_last | content | filename, size, is_last | content | ...
// sorted file by size text file
// 2. 恢复文件的过程中，可以把文件直接恢复到对应的位置上
// 3. 先单线程，后多线程OpenMP
// 多分片，多进程处理每个分片。（MPI）
// 在每个进程里，一个线程负责在txt file里寻找对应的位置（搬运文件到对应的位置），另一个文件负责解压缩。（OpenMP）


// decompression dir : /tmp/decompression （输入参数）
// data/file_73059.txt (204795 bytes)
// full path: /tmp/decompression/data/file_73059.txt


// file_73059.txt, 100, 0, content, file_73059.txt, 50, 1, content2.
// fopen(file_73059.txt)  fwrite(content)... fclose()
// zlib -> decompress


// txt file, file1, file2, file3 ...... fileN


// typedef struct {
//    char filename[255];
//    size_t size;
//} FileHeader;
//
//typedef struct {
//    char filename[255];
//    size_t size;    // Size of the compressed file
//    int is_last;    // If this is the last chunk of the file
//} ChunkHeader;
