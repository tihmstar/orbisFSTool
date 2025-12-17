//
//  OrbisFSBlockAllocator.cpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#include "OrbisFSBlockAllocator.hpp"
#include "OrbisFSImage.hpp"

#include <libgeneral/macros.h>

using namespace orbisFSTool;

#pragma mark OrbisFSBlockAllocator
OrbisFSBlockAllocator::OrbisFSBlockAllocator(OrbisFSImage *parent, uint32_t allocatorInfoBlock)
: _parent(parent)
, _info(NULL)
{
    _info = (OrbisFSAllocatorInfoElem_t*)_parent->getBlock(allocatorInfoBlock);
}

OrbisFSBlockAllocator::~OrbisFSBlockAllocator(){
    //
}

#pragma mark OrbisFSBlockAllocator private
#pragma mark OrbisFSBlockAllocator public
uint64_t OrbisFSBlockAllocator::getTotalBlockNum(){
    uint64_t ret = 1;
    uint32_t maxEntries = _parent->getBlocksize() / sizeof(*_info);
    for (uint32_t i=0; i<maxEntries; i++) {
        if (_info[i].bitmapBlk.type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        ret += _info[i].totalBlocks;
    }
    return ret;
}

uint64_t OrbisFSBlockAllocator::getFreeBlocksNum(){
    uint64_t ret = 0;
    uint32_t maxEntries = _parent->getBlocksize() / sizeof(*_info);
    for (uint32_t i=0; i<maxEntries; i++) {
        if (_info[i].bitmapBlk.type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        ret += _info[i].freeBlocks;
    }
    return ret;
}

bool OrbisFSBlockAllocator::isBlockFree(uint32_t blkNum){
    uint32_t maxEntries = _parent->getBlocksize() / sizeof(*_info);
    for (uint32_t i=0; i<maxEntries; i++) {
        OrbisFSAllocatorInfoElem_t ci = _info[i];
        if (ci.bitmapBlk.type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        if (blkNum >= ci.totalBlocks) {
            blkNum -= ci.totalBlocks;
            continue;
        }
        uint8_t *bitmap = _parent->getBlock(ci.bitmapBlk.blk);        
        return (bitmap[blkNum/8] >> (blkNum & 3)) & 1;
    }
    reterror("failed to find block in bitmap");
}
