//
//  OrbisFSImage.hpp
//  orbisFSTool
//
//  Created by tihmstar on 16.12.25.
//

#ifndef OrbisFSImage_hpp
#define OrbisFSImage_hpp

#include "OrbisFSFormat.h"
#include "OrbisFSBlockAllocator.hpp"
#include "OrbisFSInodeDirectory.hpp"

#include <vector>
#include <iostream>

#include <stdint.h>
#include <stddef.h>

namespace orbisFSTool {

class OrbisFSImage{
    bool _writeable;
    int _fd;
    uint8_t *_mem;
    size_t _memsize;
    
    OrbisFSSuperblock_t *_superblock;
    OrbisFSDiskinfoblock_t *_diskinfoblock;
    
    OrbisFSBlockAllocator *_blockAllocator;
    OrbisFSInodeDirectory *_inodeDir;
    
    void init();
    uint8_t *getBlock(uint32_t blknum);
public:
    OrbisFSImage(const char *path, bool writeable, uint64_t offset = 0);
    ~OrbisFSImage();
    
    uint32_t getBlocksize();
    
    std::vector<std::pair<std::string, OrbisFSInode_t>> listFilesInFolder(std::string path);
    std::vector<std::pair<std::string, OrbisFSInode_t>> listFilesInFolder(uint32_t inode);

    void iterateOverFilesInFolder(std::string path, bool recursive, std::function<void(std::string path, OrbisFSInode_t node)> callback);
    
#pragma mark friends
    friend OrbisFSBlockAllocator;
    friend OrbisFSInodeDirectory;
};

}

#endif /* OrbisFSImage_hpp */
