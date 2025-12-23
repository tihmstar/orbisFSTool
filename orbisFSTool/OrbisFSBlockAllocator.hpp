//
//  OrbisFSBlockAllocator.hpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#ifndef OrbisFSBlockAllocator_hpp
#define OrbisFSBlockAllocator_hpp

#include "OrbisFSFormat.h"

#include <libgeneral/Mem.hpp>

#include <map>

#include <stdint.h>

namespace orbisFSTool {
class OrbisFSImage;

class OrbisFSBlockAllocator {
    OrbisFSImage *_parent; //not owned
    const uint32_t _blockSize;
    
    OrbisFSAllocatorInfoElem_t *_info;
    
    std::map<uint32_t,tihmstar::Mem> _virtualMem;
    
    uint8_t *getBlock(uint32_t blkNum);
public:
    OrbisFSBlockAllocator(OrbisFSImage *parent, uint32_t allocatorInfoBlock, bool virtualMode = false);
    ~OrbisFSBlockAllocator();
    
    uint64_t getTotalBlockNum();
    uint64_t getFreeBlocksNum();
    bool isBlockFree(uint32_t blkNum);
    void freeBlock(uint32_t blkNum);
    uint32_t allocateBlock();
};

}

#endif /* OrbisFSBlockAllocator_hpp */
