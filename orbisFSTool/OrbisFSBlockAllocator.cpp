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
OrbisFSBlockAllocator::OrbisFSBlockAllocator(OrbisFSImage *parent, uint32_t allocatorInfoBlock, bool virtualMode)
: _parent(parent), _blockSize(_parent->getBlocksize())
, _info(NULL)
{
    if (virtualMode) {
        _virtualMem[allocatorInfoBlock] = {(const void *)_parent->getBlock(allocatorInfoBlock),_blockSize};
    }
    
    _info = (OrbisFSAllocatorInfoElem_t*)getBlock(allocatorInfoBlock);
}

OrbisFSBlockAllocator::~OrbisFSBlockAllocator(){
    //
}

#pragma mark OrbisFSBlockAllocator private
uint8_t *OrbisFSBlockAllocator::getBlock(uint32_t blkNum){
    if (_virtualMem.size() == 0){
        return _parent->getBlock(blkNum);
    }else{
        /*
            We're in virtual mode!
            Cache unseed blocks and return everything only from cache
         */
        auto c = _virtualMem.find(blkNum);
        if (c != _virtualMem.end()) return c->second.data();
        
        _virtualMem[blkNum] = {(const void *)_parent->getBlock(blkNum),_blockSize};
        
        try {
            return _virtualMem.at(blkNum).data();
        } catch (...) {
            reterror("OrbisFSBlockAllocator virtual mode error!");
        }
    }
}

#pragma mark OrbisFSBlockAllocator public
uint64_t OrbisFSBlockAllocator::getTotalBlockNum(){
    uint64_t ret = 1;
    uint32_t maxEntries = _blockSize / sizeof(*_info);
    for (uint32_t i=0; i<maxEntries; i++) {
        if (_info[i].bitmapBlk.type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        ret += _info[i].totalBlocks;
    }
    return ret;
}

uint64_t OrbisFSBlockAllocator::getFreeBlocksNum(){
    uint64_t ret = 0;
    uint32_t maxEntries = _blockSize / sizeof(*_info);
    for (uint32_t i=0; i<maxEntries; i++) {
        if (_info[i].bitmapBlk.type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        ret += _info[i].freeBlocks;
    }
    return ret;
}

bool OrbisFSBlockAllocator::isBlockFree(uint32_t blkNum){
    uint32_t maxEntries = _blockSize / sizeof(*_info);
    for (uint32_t i=0; i<maxEntries; i++) {
        OrbisFSAllocatorInfoElem_t ci = _info[i];
        if (ci.bitmapBlk.type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        if (blkNum >= ci.totalBlocks) {
            blkNum -= ci.totalBlocks;
            continue;
        }
        uint8_t *bitmap = getBlock(ci.bitmapBlk.blk);
        uint32_t blkIdx = blkNum >> 3;
        uint32_t blkOff = blkNum & 7;
        return (bitmap[blkIdx] >> blkOff) & 1;
    }
    reterror("failed to find block in bitmap");
}

void OrbisFSBlockAllocator::freeBlock(uint32_t blkNum){
    uint32_t maxEntries = _blockSize / sizeof(*_info);
    for (uint32_t i=0; i<maxEntries; i++) {
        OrbisFSAllocatorInfoElem_t *ci = &_info[i];
        if (ci->bitmapBlk.type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        if (blkNum >= ci->totalBlocks) {
            blkNum -= ci->totalBlocks;
            continue;
        }
        uint8_t *bitmap = getBlock(ci->bitmapBlk.blk);
        uint32_t blkIdx = blkNum >> 3;
        uint32_t blkOff = blkNum & 7;
        retassure(((bitmap[blkIdx] >> blkOff) & 1) == 0, "double free detected!");
        bitmap[blkIdx] |= (1<<blkOff);
        ci->freeBlocks++;
        retassure(ci->freeBlocks <= ci->totalBlocks, "Error: freeBlocks > totalBlocks");
        return;
    }
    reterror("failed to find block in bitmap");
}

uint32_t OrbisFSBlockAllocator::allocateBlock(){
    reterror("TODO");
}
