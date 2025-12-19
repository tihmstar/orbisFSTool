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
#include "OrbisFSFile.hpp"

#include <libgeneral/Event.hpp>

#include <vector>
#include <iostream>
#include <memory>
#include <mutex>
#include <functional>

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
    
    uint32_t _references;
    std::mutex _referencesLck;
    tihmstar::Event _unrefEvent;
    
    void init();
    uint8_t *getBlock(uint32_t blknum);
    std::shared_ptr<OrbisFSFile> openFileNode(OrbisFSInode_t *node);
public:
    OrbisFSImage(const char *path, bool writeable, uint64_t offset = 0);
    ~OrbisFSImage();
    
    bool isWriteable();
    uint32_t getBlocksize();
    
    std::vector<std::pair<std::string, OrbisFSInode_t>> listFilesInFolder(std::string path, bool includeSelfAndParent = false);
    std::vector<std::pair<std::string, OrbisFSInode_t>> listFilesInFolder(uint32_t inode, bool includeSelfAndParent = false);
    
    OrbisFSInode_t getInodeForID(uint32_t inode);
    OrbisFSInode_t getInodeForPath(std::string path);

    void iterateOverFilesInFolder(std::string path, bool recursive, std::function<void(std::string path, OrbisFSInode_t node)> callback);
    
#pragma mark files
    std::shared_ptr<OrbisFSFile> openFileID(uint32_t inode);
    std::shared_ptr<OrbisFSFile> openFilAtPath(std::string path);

    
#pragma mark friends
    friend OrbisFSBlockAllocator;
    friend OrbisFSFile;
    friend OrbisFSInodeDirectory;
};

}

#endif /* OrbisFSImage_hpp */
