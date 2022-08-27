#ifndef LH_C_FILEIO_H
#define LH_C_FILEIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

#define uint    unsigned int
#define ulong   unsigned long
#define uchar   unsigned char

typedef struct {
    ulong       srcSize;
    ulong       destSize;
    uchar       *srcBuf;
    uchar       *destBuf;
    uchar       txtWidth;
    uchar       alignment;
} cData;

int load(const char *fname, cData * cData);
int save(const char *fname, const cData * cData);
int fText(const uchar * buf, ulong size, uint width, FILE * fp, const char *name);


static int FILE_SIZE(FILE* file) {
    fseek(file, 0, SEEK_END);
    int size = ftell(file);
    fseek(file, 0, SEEK_SET);

    return size;
}

#endif //LH_C_FILEIO_H