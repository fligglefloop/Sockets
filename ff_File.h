//
// Created by Nick Norwood on 5/29/25.
//

#ifndef F_FILE_H
#define F_FILE_H
#include <stdio.h>

typedef struct {
    const char *file_name;
    char *file_data;
    int file_size;
} ff_File_t;

ff_File_t *readFile(const char *fileName);
int writefile(const char *fileName, const char *data, size_t len);
#endif //F_FILE_H
