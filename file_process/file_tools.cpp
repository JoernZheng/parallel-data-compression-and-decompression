#include <cctype>
#include <fstream>
#include <iostream>
#include <string>

int count_non_empty_lines(const std::string &file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << file_path << std::endl;
        return -1;
    }

    std::string line;
    int lines = 0;

    while (std::getline(file, line)) {
        if (!line.empty() && std::any_of(line.begin(), line.end(), [](unsigned char ch) { return !std::isspace(ch); })) {
            ++lines;
        }
    }

    return lines;
}


//void extract_filename(char *filename, const char *filepath) {
//    const char *last_slash = strrchr(filepath, '/');
//    if (last_slash) {
//        strcpy(filename, last_slash + 1);
//    } else {
//        strcpy(filename, filepath);
//    }
//}
