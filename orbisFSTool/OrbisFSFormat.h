//
//  OrbisFSFormat.h
//  orbisFSTool
//
//  Created by tihmstar on 16.12.25.
//

#ifndef OrbisFSFormat_h
#define OrbisFSFormat_h

#include <stdint.h>

#ifdef _WIN32
#define ATTRIBUTE_PACKED
#pragma pack(push)
#pragma pack(1)
#else
#define ATTRIBUTE_PACKED __attribute__ ((packed))
#endif


#define ORBIS_FS_CHAINLINK_TYPE_LINK        0x40
typedef struct {
    uint32_t blk  : 24;
    uint32_t type : 8;
} ATTRIBUTE_PACKED OrbisFSChainLink_t;

enum {
    kOrbisFSReserved0ID         = 0,    //unused
    kOrbisFSReserved1ID         = 1,    //unused
    kOrbisFSRootFolderID        = 2,
    kOrbisFSInodeRootDirID      = 3,
    kOrbisFSLostAndFoundDirID   = 4,
    kOrbisFSReservedBlockID     = 5, //this is simply reserverd and not touched
    kOrbisFSFirstUserNodeID     = 32
};

#define ORBIS_FS_SUPERBLOCK_MAGIC           0x10f50bf520180705
#define ORBIS_FS_SUPERBLOCK_RESERVE_STR     "reserve"
#define ORBIS_FS_SUPERBLOCK_VERSION         1
typedef struct {
    uint64_t magic;             //should be ORBIS_FS_SUPERBLOCK_MAGIC
    uint8_t _pad1[0x38];        //expected to be zero
    uint64_t unk0;
    char reserve[0x8];          //should be equal to "reserve"
    uint8_t _pad2[0x10];        //expected to be zero
    uint64_t version;
    uint64_t unk2;
    OrbisFSChainLink_t blockAllocatorLnk;
    uint32_t unk4;
    uint32_t unk5;
    OrbisFSChainLink_t diskinfoLnk;
} ATTRIBUTE_PACKED OrbisFSSuperblock_t;


#define ORBIS_FS_DISKINFOBLOCK_MAGIC        0x20f50bf520190705
typedef struct {
    uint64_t magic;             //should be ORBIS_FS_ROOTDIRBLOCK_MAGIC
    uint64_t unk1_is_2;         //is expected to be 2
    uint64_t unk2_is_0x40;      //is expected to be 0x40
    uint64_t unk3_is_0;         //is expected to be 0
    char     devpath[0x100];
    uint32_t inodesInRootFolder;//all the child iNodes inside /
    uint32_t unk5_is_0xffffffff;//is expected to be 0xFFFFFFFF
    uint32_t highestUsedInode;  //not neccessarily in use now, but was in use at some point
    uint8_t _pad2[0x34];
    uint64_t blocksUsed;
    uint64_t blocksAvailable;
    uint8_t unk7[0xb0];
    OrbisFSChainLink_t inodedirLnk;
    OrbisFSChainLink_t diskinfoLnk;
} ATTRIBUTE_PACKED OrbisFSDiskinfoblock_t;


#define ORBIS_FS_INODE_MAGIC                        0xbf10
typedef struct {
    uint32_t magic;         //should be ORBIS_FS_INODE_MAGIC
    uint32_t fatStages;
    uint32_t inodeNum;
    uint32_t _pad0;
    uint16_t fileMode;
    uint8_t _pad1[6];     //should be zero
    uint64_t unk1;
    uint32_t unk2;
    uint32_t unk3;
    uint64_t filesize;
    uint32_t unk4;
    uint32_t unk5;
    uint64_t unk6;
    uint64_t createDate;
    uint64_t _pad2;         //should be zero
    uint64_t accessOrModDate;
    uint64_t _pad3;         //should be zero
    uint64_t modOrAccessData;
    uint64_t _pad4;         //should be zero
    OrbisFSChainLink_t resourceLnkMaybe;
    uint32_t unk13;
    uint64_t unk14;
    OrbisFSChainLink_t dataLnk[0x20];
} ATTRIBUTE_PACKED OrbisFSInode_t;

typedef struct {
    OrbisFSChainLink_t bitmapBlk;
    uint32_t freeBlocks;
    uint32_t totalBlocks;
    uint32_t _pad;
} ATTRIBUTE_PACKED OrbisFSAllocatorInfoElem_t;

typedef struct {
    uint32_t inodeNum;
    uint32_t unk0;
    uint32_t elemSize;
    uint16_t namelen;
    uint16_t unk2;
    char name[];
} ATTRIBUTE_PACKED OrbisFSDirectoryElem_t;

#endif /* OrbisFSFormat_h */
