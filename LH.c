#include <stdio.h>
#include <stdlib.h>
#include "LH.h"

//-------------------------------------------------------------
// LZ Compression
//-------------------------------------------------------------
static void init_LZ(LZ* lz) {
    for (u16 i = 0; i < TABLE_SIZE; i++) {
        lz->byteTable[i] = -1;
        lz->endTable[i] = -1;
    }
    lz->wPos = 0;
    lz->wLen = 0;
}

static void slide(LZ* lz, const u8 *data) {
    s16 offset;
    u8 input = *data;
    u16 insertOffset;

    const u16 windowPos = lz->wPos;
    const u16 windowLen = lz->wLen;
    const u32 offsetSize = (1 << lz->offsetBits);


    if (windowLen == offsetSize ) {
        u8 outData = *(data - offsetSize);
        if ((lz->byteTable[outData] = lz->offsetTable[lz->byteTable[outData]]) == -1) lz->endTable[outData] = -1;
        insertOffset = windowPos;
    } else insertOffset = windowLen;

    offset = lz->endTable[input];
    if (offset == -1) lz->byteTable[input] = insertOffset;
    else lz->offsetTable[offset] = insertOffset;

    lz->endTable[input] = insertOffset;
    lz->offsetTable[insertOffset] = -1;

    if (windowLen == offsetSize) lz->wPos = (u16)((windowPos + 1) % offsetSize);
    else lz->wLen++;
}

static void n_slide(LZ* info, const u8 *data, u32 n) {
    for (u32 i = 0; i < n; i++) {
        slide(info, data++);
    }
}

static u16 search(const LZ* lz, const u8 *searchIn, u32 size, u16 *offset, u16 minOffset, u32 maxLength) {
    const u8 *search;
    const u8 *head, *searchHead;
    u16 currOffset;
    u16 currLength = 2;
    u16 tmpLength;
    s32 windowOffset;

    if (size < 3) return 0;

    windowOffset = lz->byteTable[*searchIn];
    while (windowOffset != -1) {
        if (windowOffset < lz->wPos) search = searchIn - lz->wPos + windowOffset;
        else search = searchIn - lz->wLen - lz->wPos + windowOffset;

        if (*(search + 1) != *(searchIn + 1) || *(search + 2) != *(searchIn + 2)) {
            windowOffset = lz->offsetTable[windowOffset];
            continue;
        }

        if (searchIn - search < minOffset) break;

        tmpLength = 3;
        searchHead = search + 3;
        head = searchIn + 3;

        while (((u32)(head - searchIn) < size) && (*head == *searchHead)) {
            head++;
            searchHead++;
            tmpLength++;

            if (tmpLength == maxLength) break;
        }
        if (tmpLength > currLength) {
            currLength = tmpLength;
            currOffset = (u16)(searchIn - search);
            if (currLength == maxLength) break;

        }
        windowOffset = lz->offsetTable[windowOffset];
    }

    if (currLength < 3) return 0;
    *offset = currOffset;
    return currLength;
}

static u32 LZCompress(const u8 *uncData, s32 size, u8 *dest, u8 searchOffset, u8 offsetBits) {
    u32 compressedBytes = 0; //Amount of compressed bytes.
    u8 flags; //Flags to determine whether the data is being compressed or not.
    u8* flags_p;
    u16 lastOffset; //Offset to longest matching data so far.
    u16 lastLength; //The length of the longest matching data at the current point.

    const u32 maxLength = 0xFF + 3;

    init_LZ(&lz);
    lz.offsetBits = offsetBits;

    while (size > 0) {
        flags = 0;
        flags_p = dest++;
        compressedBytes++;

        //( times looped because 8-bit data
        for (u8 i = 0; i < 8; i++) {
            flags <<= 1;
            if (size <= 0) continue;

            if ((lastLength = search(&lz, uncData, size, &lastOffset, searchOffset, maxLength)) != 0 ) {
                flags |= 0x1;

                *dest++ = (u8)(lastLength - 3); //Offset is stored in the 4 upper and 8 lower bits.
                *dest++ = (u8)((lastOffset - 1) & 0xff); //Little Endian
                *dest++ = (u8)((lastOffset - 1) >> 8);
                compressedBytes += 3;
                n_slide(&lz, uncData, lastLength);
                uncData += lastLength;
                size -= lastLength;
            }
            else {
                n_slide(&lz, uncData, 1);
                *dest++ = *uncData++;
                size--;
                compressedBytes++;
            }
        }
        *flags_p = flags;
    }

    u8 i = 0;
    while ((compressedBytes + i) & 0x3) {
        *dest++ = 0;
        i++;
    }

    return compressedBytes;
}

//-----------------------------------------------------------
// Huffman Compression
//-----------------------------------------------------------

static void initHuffman(Huffman* h, u8 bitSize) {
    u32 tableSize = (1 << bitSize);

    h->table = (Node*)malloc(sizeof(Node) * tableSize * 2 );
    h->tree = (u16*)malloc(sizeof(u16) * tableSize * 2 );
    h->offsetNeed = (NodeOffsetNeed*)malloc(sizeof(NodeOffsetNeed) * tableSize );

    h->root = 1;
    h->size = bitSize;

    const Node initNode = {0, 0, 0, {-1, -1}, 0, 0, 0, 0, 0 };
    for (u32 i = 0; i < tableSize * 2; i++ ) {
        h->table[i] = initNode;
        h->table[i].id = (u16) i;
    }

    const NodeOffsetNeed initNodeOff = {1, 1, 0, 0 };
    for (u32 i = 0; i < tableSize; i++) {
        h->tree[i * 2] = 0;
        h->tree[i * 2 + 1] = 0;
        h->offsetNeed[i] = initNodeOff;
    }
}

static void LZ_huffMem(const u8* data, u32 size, Huffman* bit8, Huffman* bit16) {
    u32 processed = 0;

    while (processed < size) {
        u8 flags = data[processed++]; //Whether compression is applied or not.
        for (u32 i = 0; i < 8; i++) {
            if (flags & 0x80) {
                u8 length = data[processed++];
                u16 offset = data[processed++]; //Little endian.
                offset |= (data[processed++] << 8);

                bit8->table[length | 0x100].frequency++;

                u32 offset_bit = 0;
                while (offset != 0) {
                    ++offset_bit;
                    offset >>= 1;
                }
                bit16->table[offset_bit].frequency++;
            }
            else {
                u8 b = data[processed++];
                bit8->table[b].frequency++;
            }
            flags <<= 1;
            if (processed >= size) break;
        }
    }
}

static void constructTree(Huffman* h, u8 bitSize) {
    Node* table = h->table;
    u16 root = makeNode(table, bitSize);

    addCodeToTable(table, root, 0x00);
    neededSpaceForTree(table, root);

    makeTree(h, root);
    h->root--;
}

static void makeTree(Huffman* h, u16 root) {
    s16 memSub, tempMemSub;
    s16 sizeOffsetNeed, tempSizeOffsetNeed;
    s16 sizeMaxKey;
    BOOL sizeMaxRightFlag;
    u16 offsetNeedNum;
    BOOL tempRightFlag;
    const u32 maxSize = 1 << (h->size - 2);

    h->root = 1;
    sizeOffsetNeed = 0;

    h->offsetNeed[0].offsetNeedL = 0;
    h->offsetNeed[0].rightNode = root;

    while(1) {
        offsetNeedNum = 0;
        for (s16 i = 0; i < h->root; i++) {
            if (h->offsetNeed[i].offsetNeedL) offsetNeedNum++;
            if (h->offsetNeed[i].offsetNeedR) offsetNeedNum++;
        }

        memSub = -1;
        sizeMaxKey = -1;

        for (s16 i = 0; i < h->root; i++) {
            tempSizeOffsetNeed = (u16)(h->root - i);

            if (h->offsetNeed[i].offsetNeedL) {
                tempMemSub = (s16)h->table[h->offsetNeed[i].leftNode].subTreeMem;

                if ((u32)(tempMemSub + offsetNeedNum) > maxSize) goto leftCostEvaluationEnd;
                if (!subTreeExpansionPossible(h, (u16) tempMemSub)) goto leftCostEvaluationEnd;

                if (tempMemSub > memSub) {
                    sizeMaxKey = i;
                    sizeMaxRightFlag = 0;
                }
                else if ((tempMemSub == memSub) && (tempSizeOffsetNeed > sizeOffsetNeed)) {
                    sizeMaxKey = i;
                    sizeMaxRightFlag = 0;
                }
            }
            leftCostEvaluationEnd:{}

            if (h->offsetNeed[i].offsetNeedR) {
                tempMemSub = (s16)h->table[ h->offsetNeed[i].rightNode ].subTreeMem;

                if ((u32)(tempMemSub + offsetNeedNum) > maxSize) {
                    goto rightCostEvaluationEnd;
                }
                if (!subTreeExpansionPossible(h, (u16) tempMemSub)) {
                    goto rightCostEvaluationEnd;
                }
                if (tempMemSub > memSub) {
                    sizeMaxKey = i;
                    sizeMaxRightFlag = 1;
                }
                else if ((tempMemSub == memSub) && (tempSizeOffsetNeed > sizeOffsetNeed)) {
                    sizeMaxKey = i;
                    sizeMaxRightFlag = 1;
                }
            }
            rightCostEvaluationEnd:{}
        }

        if (sizeMaxKey >= 0) {
            makeSubTree(h, (u16) sizeMaxKey, sizeMaxRightFlag);
            goto nextTreeMaking;
        }
        else {
            for (s16 i = 0; i < h->root; i++) {
                u16 tmp = 0;
                tempRightFlag = 0;
                if (h->offsetNeed[i].offsetNeedL) {
                    tmp = h->table[h->offsetNeed[i].leftNode].subTreeMem;
                }
                if ( h->offsetNeed[i].offsetNeedR ) {
                    if (h->table[h->offsetNeed[i].rightNode].subTreeMem > tmp) {
                        tempRightFlag = 1;
                    }
                }
                if ((tmp != 0) || (tempRightFlag)) {
                    createNoteTable(h, (u16) i, tempRightFlag);
                    goto nextTreeMaking;
                }
            }
        }
        return;
        nextTreeMaking:{}
    }
}

static void makeSubTree(Huffman* h, u16 nodeId, BOOL rightNodeFlag) {
    u16 i= h->root;
    createNoteTable(h, nodeId, rightNodeFlag);

    if (rightNodeFlag) h->offsetNeed[nodeId].offsetNeedR = 0;
    else h->offsetNeed[nodeId].offsetNeedL = 0;


    while (i < h->root) {
        if (h->offsetNeed[ i ].offsetNeedL) {
            createNoteTable(h, i, 0);
            h->offsetNeed[ i ].offsetNeedL = 0;
        }
        if (h->offsetNeed[ i ].offsetNeedR) {
            createNoteTable(h, i, 1);
            h->offsetNeed[ i ].offsetNeedR = 0;
        }
        i++;
    }
}

static BOOL subTreeExpansionPossible(Huffman* h, u16 expandBy) {
    s16 capacity;
    const u32 maxSize = 1 << (h->size - 2);

    capacity = (s16)(maxSize - expandBy);

    for (u16 i = 0; i < h->root; i++ ) {
        if (h->offsetNeed[i].offsetNeedL) {
            if ((h->root - i) <= capacity) capacity--;
            else return 0;
        }
        if (h->offsetNeed[i].offsetNeedR) {
            if ((h->root - i) <= capacity) capacity--;
            else return 0;
        }
    }

    return 1;
}

static void createNoteTable(Huffman* h, u16 nodeId, BOOL rightNodeFlag) {
    u16 nodeNo;
    u16 offsetData = 0;

    if (rightNodeFlag) {
        nodeNo = h->offsetNeed[nodeId].rightNode;
        h->offsetNeed[nodeId].offsetNeedR = 0;
    }
    else {
        nodeNo = h->offsetNeed[nodeId].leftNode;
        h->offsetNeed[nodeId].offsetNeedL = 0;
    }

    if (h->table[h->table[nodeNo].childrenId[0]].leafDepth == 0) {
        offsetData |= 0x8000;
        h->tree[h->root * 2 + 0] = (u16)h->table[nodeNo].childrenId[0];
        h->offsetNeed[h->root].leftNode = (u16)h->table[nodeNo].childrenId[0];
        h->offsetNeed[h->root].offsetNeedL = 0;
    }
    else {
        h->offsetNeed[h->root].leftNode = (u16)h->table[nodeNo].childrenId[0];
    }

    if (h->table[h->table[nodeNo].childrenId[1]].leafDepth == 0) {
        offsetData |= 0x4000;
        h->tree[h->root * 2 + 1] = (u16)h->table[nodeNo].childrenId[1];
        h->offsetNeed[h->root].rightNode = (u16)h->table[nodeNo].childrenId[1];
        h->offsetNeed[h->root].offsetNeedR = 0;
    }
    else {
        h->offsetNeed[h->root].rightNode = (u16)h->table[nodeNo].childrenId[1];
    }

    offsetData |= (u16)(h->root - nodeId - 1 );
    h->tree[nodeId * 2 + (rightNodeFlag ? 1 : 0) ] = offsetData;

    h->root++;
}

static u16 makeNode(Node* table, u8 bitSize ) {
    u16 dataNum  = ( 1 << bitSize );
    u16 tableTop = (u16)dataNum;

    u32       i;
    s32 leftNo, rightNo;
    u16 rootNo;

    leftNo  = -1;
    rightNo = -1;
    while (1) {
        for (u32 i = 0; i < tableTop; i++ ) {
            if ((table[i].frequency == 0) || (table[i].parentId != 0)) {
                continue;
            }
            if (leftNo < 0) {
                leftNo = i;
            }
            else if (table[i].frequency < table[ leftNo].frequency) {
                leftNo = i;
            }
        }

        for (u32 i = 0; i < tableTop; i++) {
            if ((table[i].frequency == 0 ) || (table[i].parentId != 0 ) ||
                (i == leftNo)) {
                continue;
            }

            if (rightNo < 0) {
                rightNo = i;
            }
            else if (table[i].frequency < table[rightNo].frequency) {
                rightNo = i;
            }
        }

        if (rightNo < 0) {
            if (tableTop == dataNum) {
                table[tableTop].frequency = table[ leftNo ].frequency;
                table[tableTop].childrenId[0] = (s16)leftNo;
                table[tableTop].childrenId[1] = (s16)leftNo;
                table[tableTop].leafDepth = 1;
                table[leftNo].parentId = (s16)tableTop;
                table[leftNo].data = 0;
                table[leftNo].parentDepth = 1;
            }
            else {
                tableTop--;
            }
            rootNo  = tableTop;
            return rootNo;
        }

        table[tableTop].frequency = table[leftNo].frequency + table[rightNo].frequency;
        table[tableTop].childrenId[0] = (s16)leftNo;
        table[tableTop].childrenId[1] = (s16)rightNo;
        if (table[leftNo].leafDepth > table[ rightNo ].leafDepth) {
            table[tableTop].leafDepth = (u16)(table[ leftNo ].leafDepth + 1);
        }
        else {
            table[tableTop].leafDepth = (u16)(table[rightNo].leafDepth + 1);
        }

        table[leftNo].parentId = table[rightNo].parentId = (s16)(tableTop);
        table[leftNo].data  = 0;
        table[rightNo].data  = 1;

        addParentDepthToTable(table, (u16) leftNo, (u16) rightNo);

        tableTop++;
        leftNo = rightNo = -1;
    }
}

static void addParentDepthToTable(Node *table, u16 leftNo, u16 rightNo) {
    table[leftNo].parentDepth++;
    table[rightNo].parentDepth++;

    if (table[leftNo].leafDepth != 0 ) {
        addParentDepthToTable(table, (u16) table[leftNo].childrenId[0], (u16) table[leftNo].childrenId[1]);
    }
    if (table[rightNo].leafDepth != 0 ) {
        addParentDepthToTable(table, (u16) table[rightNo].childrenId[0], (u16) table[rightNo].childrenId[1]);
    }
}

static void addCodeToTable(Node* table, u16 node, u32 paHuffCode ) {
    table[node].huffmanCode = (paHuffCode << 1) | table[node].data;

    if (table[node].leafDepth != 0 ) {
        addCodeToTable(table, (u16) table[node].childrenId[0], table[node].huffmanCode);
        addCodeToTable(table, (u16) table[node].childrenId[1], table[node].huffmanCode);
    }
}

static u16 neededSpaceForTree(Node *table, u16 node) {
    u16 leftHWord, rightHWord;

    switch (table[ node].leafDepth) {
        case 0:
            return 0;
        case 1:
            leftHWord = rightHWord = 0;
            break;
        default:
            leftHWord = neededSpaceForTree(table, (u16) table[node].childrenId[0]);
            rightHWord = neededSpaceForTree(table, (u16) table[node].childrenId[1]);
            break;
    }

    table[node].subTreeMem = (u16)(leftHWord + rightHWord + 1);
    return (u16)(leftHWord + rightHWord + 1);
}

static void LZ_makeTree(const u8* data, u32 size, Huffman* bit8, Huffman* bit16) {
    initHuffman(bit8, LENGTH_BITS);
    initHuffman(bit16, LH_OFFSET_TABLE_BITS);

    LZ_huffMem(data, size, bit8, bit16);

    constructTree(bit8, LENGTH_BITS);
    constructTree(bit16, LH_OFFSET_TABLE_BITS);
}

static u32 writeHuffman(u8* dest, Huffman* h, u8 bitSize) {
    BStream stream;
    u8* pSize;
    u32 tblSize;

    initStream(&stream, dest);

    pSize = dest;
    u32 roundUp = (bitSize + 7) & ~7;
    writeStream(&stream, 0, roundUp);

    for (u32 i = 1; i < (u16)((h->root + 1) * 2); i++) {
        u16 flags = (u16)(h->tree[i] & 0xC000);
        u32 data = h->tree[i] | (flags >> (16 - bitSize));
        writeStream(&stream, data, bitSize);
    }
    closeStream(&stream, 4);

    tblSize = (stream.size / 4) - 1;
    if (roundUp == 8) {
        if (tblSize >= 0x100) {
            fprintf(stderr, "Table ended!\n");
        }
        *pSize = (u8)( tblSize );
    }
    else {
        if (tblSize >= 0x10000) {
            fprintf(stderr, "Table ended!\n");
        }
        *(u16*)pSize = (u16)( tblSize );
    }
    return stream.size;
}

static void ConvertHuff(Huffman* h, u16 data, BStream* stream) {
    u16 width = h->table[data].parentDepth;
    u32 code  = h->table[data].huffmanCode;

    writeStream(stream, code, width);
}

static u32 LZConvertHuffData(const u8* data, u32 srcSize, u8* dest, Huffman* bit8, Huffman* bit16 ) {
    u32 srcCnt = 0;

    BStream stream;

    initStream(&stream, dest);

    while (srcCnt < srcSize) {
        u32 i;
        u8 compFlags = data[ srcCnt++];

        for (i = 0; i < 8; i++ ) {
            if (compFlags & 0x80) {
                u8  length = data[ srcCnt++ ];
                u16 offset = data[ srcCnt++ ];
                offset |= data[ srcCnt++] << 8;

                ConvertHuff(bit8, length | 0x100, &stream);

                u16 offset_bit = 0;
                u16 offset_tmp = offset;
                while (offset_tmp > 0) {
                    offset_tmp >>= 1;
                    ++offset_bit;
                }
                ConvertHuff(bit16, offset_bit, &stream);
                writeStream(&stream, offset & ~(1 << (offset_bit - 1)), offset_bit - 1);
            }
            else {
                u8 b = data[srcCnt++];
                ConvertHuff(bit8, b, &stream);
            }
            compFlags <<= 1;
            if (srcCnt >= srcSize) break;

        }
    }

    closeStream(&stream, 4);
    return stream.size;
}

u32 LH(const u8 *data, s32 size, u8 *dest) {
    static Huffman bit8;
    static Huffman bit16;

    u32 tmpSize;
    u32 dstSize;
    u8* tmpBuf = (u8*)malloc(size * 3);
    tmpSize = LZCompress(data, size, tmpBuf, 2, LH_OFFSET_BITS);

    LZ_makeTree(tmpBuf, tmpSize, &bit8, &bit16);

    if (size < 0x1000000 && size > 0) {
        *(u32*)dest = LH_HEADER | ( size << 8);
        dstSize = 4;
    }
    else {
        *(u32*)dest = LH_HEADER;
        *(u32*)&dest[4] = size;
        dstSize = 8;
    }

    dstSize += writeHuffman(&dest[dstSize], &bit8, LENGTH_BITS);
    dstSize += writeHuffman(&dest[dstSize], &bit16, LH_OFFSET_TABLE_BITS);


    dstSize += LZConvertHuffData(tmpBuf, tmpSize, &dest[ dstSize ], &bit8, &bit16);

    return dstSize;
}