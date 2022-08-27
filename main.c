#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "FileIO.h"
#include "LH.h"

static int convert_data(cData * cnv_dat);
static char* make_file_name(int count, ...);

int main(int argc, char **argv) {
    cData conv;
    memset(&conv, 0, sizeof(conv));

    load(argv[1], &conv);
    convert_data(&conv);

    const char* conv_file = make_file_name(2, argv[1], ".LH");
    save(conv_file, &conv);

    free(conv.srcBuf);
    if (conv.destBuf != conv.srcBuf) {
        free(conv.destBuf);
    }

    return 0;
}

char* make_file_name(int count, ...) {
    va_list ap;
    int i;

    int len = 1;
    va_start(ap, count);
    for(i=0 ; i<count ; i++)
        len += strlen(va_arg(ap, char*));
    va_end(ap);

    char *merged = calloc(sizeof(char),len);
    int null_pos = 0;

    va_start(ap, count);
    for(i=0 ; i<count ; i++)
    {
        char *s = va_arg(ap, char*);
        strcpy(merged+null_pos, s);
        null_pos += strlen(s);
    }
    va_end(ap);

    return merged;
}

static int convert_data(cData * cData) {
    cData->destBuf = (u8*)malloc(cData->srcSize * 3 + 512);
    cData->destSize = LH(cData->srcBuf, cData->srcSize, cData->destBuf);
    u32 a = cData->destSize;
    u32 b = (cData->txtWidth & 0x07) - 1;

    if (cData->txtWidth >= 2) {
        cData->destSize =  (a + b) & ~b;
    }

    return 0;
}
