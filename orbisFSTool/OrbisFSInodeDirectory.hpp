//
//  OrbisFSInodeDirectory.hpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#ifndef OrbisFSInodeDirectory_hpp
#define OrbisFSInodeDirectory_hpp

#include "OrbisFSFormat.h"

#include <vector>
#include <iostream>

#include <stdint.h>

namespace orbisFSTool {
class OrbisFSImage;

class OrbisFSInodeDirectory {
    OrbisFSImage *_parent; //not owned
    
    const uint32_t _elemsPerBlock;
    
    OrbisFSInode_t *_inodeRootDir;
    OrbisFSInode_t *_self;
    
public:
    OrbisFSInodeDirectory(OrbisFSImage *parent, uint32_t inodeRootDirBlock);
    ~OrbisFSInodeDirectory();

    std::vector<std::pair<std::string, OrbisFSInode_t>> listFilesInDir(uint32_t inodeNum);

    OrbisFSInode_t *findChildInDirectory(OrbisFSInode_t *node, std::string childname);

    OrbisFSInode_t *findInode(uint32_t inodeNum);
    uint32_t findInodeIDForPath(std::string path);
    OrbisFSInode_t *findInodeForPath(std::string path);
};

}
#endif /* OrbisFSInodeDirectory_hpp */
