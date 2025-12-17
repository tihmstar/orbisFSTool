//
//  OrbisFSBlockAllocator.hpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#ifndef OrbisFSBlockAllocator_hpp
#define OrbisFSBlockAllocator_hpp

#include "OrbisFSFormat.h"

#include <stdint.h>

namespace orbisFSTool {
class OrbisFSImage;

class OrbisFSBlockAllocator {
    OrbisFSImage *_parent; //not owned
    
    OrbisFSAllocatorInfoElem_t *_info;
public:
    OrbisFSBlockAllocator(OrbisFSImage *parent, uint32_t allocatorInfoBlock);
    ~OrbisFSBlockAllocator();
    
    uint64_t getTotalBlockNum();
    uint64_t getFreeBlocksNum();
    bool isBlockFree(uint32_t blkNum);
};

}

#endif /* OrbisFSBlockAllocator_hpp */
