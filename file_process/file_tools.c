#include <stdio.h>
#include <ctype.h>
#include <string.h>

int count_non_empty_lines(const char *file_path) {
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }

    int lines = 0;
    int ch;
    int line_non_empty = 0;

    while ((ch = fgetc(file)) != EOF) {
        if (!isspace(ch)) {
            line_non_empty = 1;
        }

        if (ch == '\n') {
            lines += line_non_empty;
            line_non_empty = 0;
        }
    }

    // Check if the last line is non-empty
    if (line_non_empty) {
        lines++;
    }

    fclose(file);
    return lines;
}

void extract_filename(char *filename, const char *filepath) {
    const char *last_slash = strrchr(filepath, '/');
    if (last_slash) {
        strcpy(filename, last_slash + 1);
    } else {
        strcpy(filename, filepath);
    }
}
