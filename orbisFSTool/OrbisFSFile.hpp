//
//  OrbisFSFile.hpp
//  orbisFSTool
//
//  Created by tihmstar on 18.12.25.
//

#ifndef OrbisFSFile_hpp
#define OrbisFSFile_hpp

#include "OrbisFSFormat.h"

#include <stdint.h>
#include <stddef.h>

namespace orbisFSTool {
class OrbisFSImage;

class OrbisFSFile {
    OrbisFSImage *_parent; //not owned
    OrbisFSInode_t *_node;
    
    const uint32_t _blockSize;
    
    uint64_t _offset;
    
    uint8_t *getDataBlock(uint32_t num);
public:
    OrbisFSFile(OrbisFSImage *parent, OrbisFSInode_t *node);
    ~OrbisFSFile();
    
#pragma IO operations
    uint64_t size();
    size_t read(void *buf, size_t len);
    size_t write(const void *buf, size_t len);
    
    size_t pread(void *buf, size_t len, uint64_t offset);
    size_t pwrite(const void *buf, size_t len, uint64_t offset);
};

}
#endif /* OrbisFSFile_hpp */
