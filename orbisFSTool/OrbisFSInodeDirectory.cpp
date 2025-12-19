//
//  OrbisFSInodeDirectory.cpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#include "OrbisFSInodeDirectory.hpp"
#include "OrbisFSException.hpp"
#include "OrbisFSImage.hpp"
#include "utils.hpp"

#include <libgeneral/macros.h>

#include <algorithm>

#include <sys/stat.h>

#define ARRAYOF(a) (sizeof(a)/sizeof(*a))

using namespace orbisFSTool;

#pragma mark OrbisFSInodeDirectory
OrbisFSInodeDirectory::OrbisFSInodeDirectory(OrbisFSImage *parent, uint32_t inodeRootDirBlock)
: _parent(parent)
, _blockSize(_parent->getBlocksize()), _inodeElemsPerBlock(_parent->getBlocksize() / sizeof(OrbisFSInode_t))
, _inodeRootDir(NULL)
, _self(nullptr)
{
    _inodeRootDir = (OrbisFSInode_t*)_parent->getBlock(inodeRootDirBlock);
    retassure(memvcmp(&_inodeRootDir[0], sizeof(_inodeRootDir[0]), 0x00), "inode 0 is not zero");
    retassure(memvcmp(&_inodeRootDir[1], sizeof(_inodeRootDir[1]), 0x00), "inode 1 is not zero");
}

OrbisFSInodeDirectory::~OrbisFSInodeDirectory(){
    //
}

#pragma mark OrbisFSInodeDirectory private
#pragma mark OrbisFSInodeDirectory public

std::vector<std::pair<std::string, OrbisFSInode_t>> OrbisFSInodeDirectory::listFilesInDir(uint32_t inodeNum, bool includeSelfAndParent){
    std::vector<std::pair<std::string, OrbisFSInode_t>> ret;
    
    OrbisFSInode_t *node = findInode(inodeNum);
    retassure(S_ISDIR(node->fileMode), "inode %d is not a directory!",inodeNum);
    auto df = _parent->openFileNode(node, true);
    
    OrbisFSDirectoryElem_t *elem = NULL;
    for (uint64_t offset = 0; offset + sizeof(*elem) < node->filesize; offset+= elem->elemSize) {
        elem = (OrbisFSDirectoryElem_t *)df->getDataForOffset(offset);
        retassure(elem->inodeNum, "unexpected zero elem");
        retassure(offset + elem->elemSize <= node->filesize, "elemsize goes oob");
        retassure(sizeof(*elem)+elem->namelen <= elem->elemSize, "namelen too long");

        std::string elemName{elem->name,elem->name+elem->namelen};
        if (!includeSelfAndParent && (elemName == "." || elemName == "..")) continue;

        OrbisFSInode_t *curInode = NULL;
        try {
            curInode = findInode(elem->inodeNum);
        } catch (tihmstar::OrbisFSInodeBadMagic &e) {
#ifdef XCODE
            debug("Ignoring vanished elem '%s'",elemName.c_str());
#endif
            continue;
        }
        ret.push_back({
            elemName,
            *curInode
        });
    }
out:
    std::sort(ret.begin(), ret.end(), [](const std::pair<std::string, OrbisFSInode_t> &a, const std::pair<std::string, OrbisFSInode_t> &b)->bool{
        return a.first < b.first;
    });
    return ret;
}

OrbisFSInode_t *OrbisFSInodeDirectory::findChildInDirectory(OrbisFSInode_t *node, std::string childname){
    retassure(S_ISDIR(node->fileMode), "inode %d is not a directory!",node->inodeNum);
    auto df = _parent->openFileNode(node, true);

    OrbisFSDirectoryElem_t *elem = NULL;
    for (uint64_t offset = 0; offset + sizeof(*elem) < node->filesize; offset+= elem->elemSize) {
        elem = (OrbisFSDirectoryElem_t *)df->getDataForOffset(offset);
        retassure(elem->inodeNum, "unexpected zero elem");
        retassure(offset + elem->elemSize <= node->filesize, "elemsize goes oob");
        retassure(sizeof(*elem)+elem->namelen <= elem->elemSize, "namelen too long");

        std::string elemName{elem->name,elem->name+elem->namelen};
        if (elemName == "." || elemName == "..") continue;
        if (elemName != childname) continue;

        OrbisFSInode_t *curInode = NULL;
        try {
            curInode = findInode(elem->inodeNum);
        } catch (tihmstar::OrbisFSInodeBadMagic &e) {
#ifdef XCODE
            debug("Ignoring vanished elem '%s'",elemName.c_str());
#endif
            continue;
        }
        return curInode;
    }
error:
    retcustomerror(OrbisFSFileNotFound,"Failed to find child '%s' in directory",childname.c_str());
}

OrbisFSInode_t *OrbisFSInodeDirectory::findInode(uint32_t inodeNum){
    retassure(inodeNum <= _parent->_diskinfoblock->highestUsedInode, "Trying to access beyond largest used iNode");
    
    OrbisFSInode_t *ret = NULL;
    if (inodeNum < _inodeElemsPerBlock) {
        ret = &_inodeRootDir[inodeNum];
    }else {
        if (!_self) _self = _parent->openFileID(kOrbisFSInodeRootDirID);
        uint32_t inodeBlk = inodeNum/_inodeElemsPerBlock;
        uint32_t inodeElem = inodeNum % _inodeElemsPerBlock;
        ret = (OrbisFSInode_t*)_self->getDataBlock(inodeBlk);
        ret = &ret[inodeElem];
    }
    retcustomassure(OrbisFSInodeBadMagic, ret->magic == ORBIS_FS_INODE_MAGIC, "inode %d entry has bad magic",inodeNum);
    retassure(ret->inodeNum == inodeNum, "inode %d entry has wrong inode num",inodeNum);
    retassure(memvcmp(&ret->_pad0, sizeof(ret->_pad0), 0x00), "inode %d entry _pad0 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad1, sizeof(ret->_pad1), 0x00), "inode %d entry _pad1 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad2, sizeof(ret->_pad2), 0x00), "inode %d entry _pad2 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad3, sizeof(ret->_pad3), 0x00), "inode %d entry _pad3 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad4, sizeof(ret->_pad4), 0x00), "inode %d entry _pad4 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad5, sizeof(ret->_pad5), 0x00), "inode %d entry _pad5 is not zero",inodeNum);
    return ret;
}

uint32_t OrbisFSInodeDirectory::findInodeIDForPath(std::string path){
    if (strncmp(path.c_str(), "iNode", sizeof("iNode")-1) == 0){
        return atoi(path.c_str()+sizeof("iNode")-1);
    }
    retassure(path.front() == '/', "path needs to start with '/'");
    path = path.substr(1);
    bool isLookingForFolder = false;
    if (path.size() && path.back() == '/'){
        isLookingForFolder = true;
        path.pop_back();
    }

    OrbisFSInode_t *rec = findInode(kOrbisFSRootFolderID);
    while (path.size()) {
        ssize_t spos = path.find('/');
        std::string curname;
        if (spos != std::string::npos) {
            curname = path.substr(0,spos);
            path = path.substr(spos+1);
        }else{
            curname = path;
            path.clear();
        }
        
        if (S_ISDIR(rec->fileMode)) {
            rec = findChildInDirectory(rec, curname);
        } else if (S_ISLNK(rec->fileMode)){
            reterror("Symlinks currently not support");
        }else{
            reterror("Unexpected type!");
        }
    }
    
    return rec->inodeNum;
}

OrbisFSInode_t *OrbisFSInodeDirectory::findInodeForPath(std::string path){
    return findInode(findInodeIDForPath(path));
}
