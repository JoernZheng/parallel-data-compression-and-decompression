#include <fstream>
#include <iostream>
#include <openssl/md5.h>
#include <sstream>

std::string md5_of_file(const std::string &file_path) {
    std::ifstream file(file_path, std::ifstream::binary);
    if (!file) {
        std::cerr << "Cannot open file: " << file_path << std::endl;
        return "";
    }

    MD5_CTX md5Context;
    MD5_Init(&md5Context);

    char buffer[1024];
    while (file.read(buffer, sizeof(buffer)) || file.gcount()) {
        MD5_Update(&md5Context, buffer, file.gcount());
    }

    unsigned char result[MD5_DIGEST_LENGTH];
    MD5_Final(result, &md5Context);

    std::stringstream md5StringStream;
    for (unsigned char i: result) {
        md5StringStream << std::hex << std::setw(2) << std::setfill('0') << (int) i;
    }

    return md5StringStream.str();
}

bool is_md5_match(const std::string &file_path, const std::string &expected_md5) {
    std::string file_md5 = md5_of_file(file_path);

    return file_md5 == expected_md5;
}
