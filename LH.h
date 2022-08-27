#ifndef LH_C_LH_H
#define LH_C_LH_H

//-----------------------------------------------
// INCLUDES
//-----------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include "Types.h"
#include "BStream.h"

//-----------------------------------------------
// DEFINITIONS
//-----------------------------------------------

#define LH_HEADER 0x40

#define LH_OFFSET_BITS 15
#define LH_OFFSET_TABLE_BITS 5

#define LENGTH_BITS 9
#define OFFSET_SIZE_MAX (1 << 15)

#define TABLE_SIZE 256

//-------------------------------------------------------------
//LZ structures
//-------------------------------------------------------------

typedef struct {
    u16 wPos;
    u16 wLen;

    s16 offsetTable[OFFSET_SIZE_MAX];

    s16 byteTable[TABLE_SIZE];
    s16 endTable[TABLE_SIZE];
    u8 offsetBits;
} LZ;
static LZ lz;

//-------------------------------------------------------------
//Huffman structures
//-------------------------------------------------------------

typedef struct {
    u16 id;
    s16 parentId;
    u32 frequency; //The frequency of the node's appearance.
    s16 childrenId[2]; //The ids of the children.
    u16 parentDepth; //Depth of the parent node.
    u16 leafDepth; //Depth of the leaf.
    u32 huffmanCode;
    u16 data;
    u16 subTreeMem; //Amount of memory required to store the subtree rooted at this node.
} Node;

typedef struct {
    u8 offsetNeedL;
    u8 offsetNeedR;
    u16 leftNode;
    u16 rightNode;
} NodeOffsetNeed;

typedef struct {
    Node* table;
    u16* tree;
    NodeOffsetNeed* offsetNeed;
    u16 root;
    u8 size;
    u8 padding_[1];
} Huffman;

//-------------------------------------------------------------
//LZ Compression
//-------------------------------------------------------------

static void init_LZ(LZ* lz);
static void slide(LZ* lz, const u8 *data);
static void n_slide(LZ* info, const u8 *data, u32 n);
static u16 search(const LZ* lz, const u8 *searchIn, u32 size, u16 *offset, u16 minOffset, u32 maxLength);
static u32 LZCompress(const u8 *uncData, s32 size, u8 *dest, u8 searchOffset, u8 offsetBits);

//-------------------------------------------------------------
//Huffman Compression
//-------------------------------------------------------------

static void initHuffman(Huffman* h, u8 bitSize);
static void makeTree(Huffman* h, u16 root);
static void makeSubTree(Huffman* h, u16 nodeId, BOOL rightNodeFlag);
static BOOL subTreeExpansionPossible(Huffman* h, u16 expandBy);
static void createNoteTable(Huffman* h, u16 nodeId, BOOL rightNodeFlag);
static u16 makeNode(Node* table, u8 bitSize);

static void addParentDepthToTable(Node *table, u16 leftNo, u16 rightNo);
static void addCodeToTable(Node* table, u16 node, u32 paHuffCode);
static u16 neededSpaceForTree(Node *table, u16 node);
static void LZ_huffMem(const u8* data, u32 size, Huffman* bit8, Huffman* bit16);
static void constructTree(Huffman* h, u8 bitSize);
static void LZ_makeTree(const u8* data, u32 size, Huffman* bit8, Huffman* bit16);
static u32 writeHuffman(u8* dest, Huffman* h, u8 bitSize);
static void ConvertHuff(Huffman* h, u16 data, BStream* stream);
static u32 LZConvertHuffData(const u8* data, u32 srcSize, u8* dest, Huffman* bit8, Huffman* bit16);

//-------------------------------------------------------------
//LH Compression
//-------------------------------------------------------------

u32 LH(const u8 *data, s32 size, u8 *dest);

#endif //LH_C_LH_H