//
//  OrbisFSInodeDirectory.cpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#include "OrbisFSInodeDirectory.hpp"
#include "OrbisFSImage.hpp"
#include "utils.hpp"

#include <libgeneral/macros.h>

#include <sys/stat.h>

using namespace orbisFSTool;

#pragma mark OrbisFSInodeDirectory
OrbisFSInodeDirectory::OrbisFSInodeDirectory(OrbisFSImage *parent, uint32_t inodeRootDirBlock)
: _parent(parent)
, _inodeRootDir(NULL)
, _self(NULL)
{
    _inodeRootDir = (OrbisFSInode_t*)_parent->getBlock(inodeRootDirBlock);
    retassure(memvcmp(&_inodeRootDir[0], sizeof(_inodeRootDir[0]), 0x00), "inode 0 is not zero");
    retassure(memvcmp(&_inodeRootDir[1], sizeof(_inodeRootDir[1]), 0x00), "inode 1 is not zero");
    
    _self = getInode(kOrbisFSInodeRootDirID);
}

OrbisFSInodeDirectory::~OrbisFSInodeDirectory(){
    //
}

#pragma mark OrbisFSInodeDirectory private
OrbisFSInode_t *OrbisFSInodeDirectory::getInode(uint64_t inodeNum){
    OrbisFSInode_t *ret = NULL;
    if (inodeNum < _parent->getBlocksize() / sizeof(*ret)) {
        ret = &_inodeRootDir[inodeNum];
    }else {
        reterror("TODO: implement 2 stage inode lookup");
    }
    retassure(ret->magic == ORBIS_FS_INODE_MAGIC, "inode %d entry has bad magic",inodeNum);
    retassure(ret->entryType == ORBIS_FS_INODE_ENTRY_TYPE_SINGLEBLOCK
              || ret->entryType == ORBIS_FS_INODE_ENTRY_TYPE_MULTIBLOCK, "inode %d entry has unknown entry type %d",inodeNum,_self->entryType);
    retassure(ret->inodeNum == inodeNum, "inode %d entry has wrong inode num",inodeNum);
    retassure(memvcmp(&ret->_pad1, sizeof(ret->_pad1), 0x00), "inode %d entry _pad1 is not zero",inodeNum);
    return ret;
}

#pragma mark OrbisFSInodeDirectory public

std::vector<std::pair<std::string, OrbisFSInode_t>> OrbisFSInodeDirectory::listFilesInDir(uint64_t inodeNum){
    OrbisFSInode_t *node = getInode(inodeNum);
    retassure(S_ISDIR(node->fileMode), "inode %d is not a directory!",inodeNum);
    retassure(node->resourceLnk.type == ORBIS_FS_CHAINLINK_TYPE_LINK, "inode %d has invalid resource link",inodeNum);
    
    
    reterror("TODO");
}
