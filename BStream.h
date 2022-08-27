#ifndef LH_C_BSTREAM_H
#define LH_C_BSTREAM_H

//-----------------------------------------------
// DEFINITIONS
//-----------------------------------------------

typedef struct {
    u8* dest;
    u32 size;
    u32 stream;
    u32 streamSize;
} BStream;

static void initStream(BStream* b, u8* dest) {
    b->dest = dest;
    b->size = 0;
    b->stream = 0;
    b->streamSize = 0;
}

static void writeStream(BStream* b, u32 data, u32 width ) {
    u32 stream = b->stream;
    u32 size = b->size;
    u32 streamSize = b->streamSize;
    u32 mask = (1 << width) - 1;

    if (width == 0) return;

    stream = (stream << width) | ( data & mask );
    streamSize += width;

    for (u32 i = 0; i < streamSize / 8; i++) {
        b->dest[ size++ ] = (u8)( stream >> (streamSize - (i + 1 ) * 8 ) );
    }
    streamSize %= 8;

    b->stream = stream;
    b->size = size;
    b->streamSize = streamSize;
}

static void closeStream(BStream* b, u32 align) {
    u32 stream = b->stream;
    u32 size = b->size;
    u32 streamSize = b->streamSize;

    if (streamSize > 0 ) {
        stream <<= 8 - streamSize;

        if (b->streamSize != 0) {
            b->dest[size++] = (u8)(stream);
        }
    }

    while (size % align) {
        b->dest[size++] = 0;
    }
    b->size  = size;
    b->streamSize = 0;
}

#endif //LH_C_BSTREAM_H