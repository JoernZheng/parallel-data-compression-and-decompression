#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mpi.h>
#include <string>
#include <vector>

struct FileEntry {
    std::string relpath;// Relative path
    off_t size;
};

void collect_files(const std::filesystem::path &base_path, const std::filesystem::path &current_path, std::vector<FileEntry> &files) {
    for (const auto &entry : std::filesystem::recursive_directory_iterator(current_path)) {
        if (entry.is_regular_file()) {
            // Use static_cast to convert file_size() to off_t
            files.push_back({std::filesystem::relative(entry.path(), base_path).string(),
                             static_cast<off_t>(entry.file_size())});
        }
    }
}

std::string sort_files_by_size(const std::filesystem::path &path) {
    std::vector<FileEntry> files;

    collect_files(path, path, files);

    // Sort files by size in descending order using a lambda expression
    std::sort(files.begin(), files.end(),
              [](const FileEntry &a, const FileEntry &b) { return a.size > b.size; });

    auto output_filename = path.parent_path() / "sorted_files_by_size.txt";

    std::ofstream file(output_filename);
    if (file.is_open()) {
        for (const auto &entry: files) {
            file << entry.relpath << " (" << entry.size << " bytes)\n";
        }
    }

    return output_filename.string();
}
