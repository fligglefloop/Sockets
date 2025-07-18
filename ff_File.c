//
// Created by Nick Norwood on 5/29/25.
//

#include "ff_File.h"
#include <stdio.h>
#include <stdlib.h>

ff_File_t *readFile(const char *fileName)
{
    FILE *fp;                      // Variable to represent open file
    long size;
    size_t bytesRead;
    char *buffer = NULL;

    ff_File_t *ffFile;
    ffFile = (ff_File_t *)malloc(sizeof(ff_File_t));
    ffFile->file_name = fileName;

    fp = fopen(fileName, "rb");  // Open file for reading
    if(fp == NULL)
    {
        printf("Failed to open file.\n");
        return NULL;
    }
    else
    {
        printf("File opened successfully.\n");
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        buffer = malloc(size);
        bytesRead = fread(buffer, 1, size, fp);
        if(bytesRead != size)
        {
            printf("Failed to read all data...");
            free(buffer);
            return NULL;
        }
        else{printf("Bytes read: %lu.\n", bytesRead);}
        fclose(fp);

        ffFile->file_size = size;
        ffFile->file_data = buffer;
        return ffFile;
    }


}
int writefile(const char *fileName, const char *data, size_t len)
{
    FILE *fp;
    size_t bytesWritten;
    int ret;

    fp = fopen(fileName, "w");  // Open file for reading
    if(fp == NULL){printf("Failed to open file.");}
    else{printf("File opened successfully.\n");}

    bytesWritten = fwrite(data, 1, len, fp);
    if(bytesWritten != len)
    {
        printf("Failed to write all data...");
        ret = 1;
    }
    else
    {
        printf("Bytes written: %lu.\n", bytesWritten);
        ret = 0;
    }

    fclose(fp);

    return ret;
}