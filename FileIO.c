#include "FileIO.h"

int save(const char *fname, const cData * cData) {
    FILE *out_file;
    fopen_s(&out_file, fname, "wb");

    if (cData->txtWidth != 0) {
        if (fText(cData->destBuf, cData->destSize, cData->txtWidth, out_file, fname)
            != cData->destSize ) {
            fprintf(stderr, "An error occurred during file writing!\n");
            return -1;
        }
    }
    else {
        if (fwrite(cData->destBuf, 1, cData->destSize, out_file) != cData->destSize) {
            fprintf(stderr, "An error occurred during file writing!\n");
            return -1;
        }
    }

    fclose(out_file);

    return 0;
}

int fText(const uchar * buf, ulong size, uint width, FILE * fp, const char *name) {
    char *name_buf;
    ulong i;
    ulong line_num;

    width &= 0x07;

    if (fp == NULL || buf == NULL || name == NULL) return -1;

    strcpy_s(name_buf, strlen(name) + 1, name);
    size = (size + width - 1) / width;
    line_num = 1;

    if (width >= 2) line_num = 2;

    for (i = 0; i < size; i++) {
        if ((i * line_num) % 0x10 == 0) {
            if (fprintf(fp, "\n") < 0) {
                return -1;
            }
        }
        switch (width) {
            case 1: { buf++; }
                break;
            case 2: { buf += 2; }
                break;
            case 4: { buf += 4; }
                break;
            default:
                return -1;
        }
    }

    free(name_buf);
    return i * width;
}

int load(const char *file_name, cData * cData) {
    FILE *in_file;
    if (fopen_s(&in_file, file_name, "rb") != 0 ) {
        fprintf(stderr, "ERR: could not open file: %s\n", file_name);
        return -1;
    }

    if ((cData->srcSize = FILE_SIZE(in_file)) < 0) {
        fprintf(stderr, "ERR: file error\n");
        return -1;
    }

    int a = cData->srcSize;
    int roundUp = (a + 3) & ~3;
    if ((cData->srcBuf = (uchar *) malloc(roundUp)) == NULL) {
        fprintf(stderr, "ERR: memory exhausted\n");
        return -1;
    }

    fseek(in_file, 0, SEEK_SET);
    if (fread(cData->srcBuf, 1, cData->srcSize, in_file) != cData->srcSize) {
        fprintf(stderr, "ERR: read error\n");
        return -1;
    }
    fclose(in_file);


    for (ulong i = 0; (cData->srcSize + i) & 3; i++) {
        cData->srcBuf[cData->srcSize + i] = 0;
    }

    return 0;
}