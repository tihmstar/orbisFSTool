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

#include <sys/stat.h>

using namespace orbisFSTool;

#pragma mark OrbisFSInodeDirectory
OrbisFSInodeDirectory::OrbisFSInodeDirectory(OrbisFSImage *parent, uint32_t inodeRootDirBlock)
: _parent(parent)
, _elemsPerBlock(_parent->getBlocksize() / sizeof(OrbisFSDirectoryElem_t))
, _inodeRootDir(NULL)
, _self(NULL)
{
    _inodeRootDir = (OrbisFSInode_t*)_parent->getBlock(inodeRootDirBlock);
    retassure(memvcmp(&_inodeRootDir[0], sizeof(_inodeRootDir[0]), 0x00), "inode 0 is not zero");
    retassure(memvcmp(&_inodeRootDir[1], sizeof(_inodeRootDir[1]), 0x00), "inode 1 is not zero");
    
    _self = findInode(kOrbisFSInodeRootDirID);
}

OrbisFSInodeDirectory::~OrbisFSInodeDirectory(){
    //
}

#pragma mark OrbisFSInodeDirectory private
#pragma mark OrbisFSInodeDirectory public

std::vector<std::pair<std::string, OrbisFSInode_t>> OrbisFSInodeDirectory::listFilesInDir(uint32_t inodeNum){
    std::vector<std::pair<std::string, OrbisFSInode_t>> ret;
    
    OrbisFSInode_t *node = findInode(inodeNum);
    retassure(S_ISDIR(node->fileMode), "inode %d is not a directory!",inodeNum);
    if (node->entryType == ORBIS_FS_INODE_ENTRY_TYPE_EMPTY) return {};
    retassure(node->dataLnk.type == ORBIS_FS_CHAINLINK_TYPE_LINK, "inode %d has invalid data link",inodeNum);
    
    OrbisFSDirectoryElem_t *elems = (OrbisFSDirectoryElem_t*)_parent->getBlock(node->dataLnk.blk);
    while (true) {
        for (int i=0; i<_elemsPerBlock; i++) {
            OrbisFSDirectoryElem_t *curElem = &elems[i];
            if (curElem->inodeNum == 0) goto out;
            
            if (curElem->namelen > sizeof(curElem->name)){
                uint32_t extrablocks = curElem->namelen - sizeof(curElem->name);
                extrablocks /= sizeof(*curElem);
                extrablocks++;
                i+=extrablocks;
                warning("namelen too large, figure this out!");
            }
            std::string elemName{curElem->name,curElem->name+curElem->namelen};
            if (elemName == "." || elemName == "..") continue;

            OrbisFSInode_t *curInode = NULL;
            try {
                curInode = findInode(curElem->inodeNum);
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
        reterror("TODO: move to neext elems block!");
    }
out:
    std::sort(ret.begin(), ret.end(), [](const std::pair<std::string, OrbisFSInode_t> &a, const std::pair<std::string, OrbisFSInode_t> &b)->bool{
        return a.first < b.first;
    });
    return ret;
}

OrbisFSInode_t *OrbisFSInodeDirectory::findChildInDirectory(OrbisFSInode_t *node, std::string childname){
    retassure(S_ISDIR(node->fileMode), "inode %d is not a directory!",node->inodeNum);
    retassure(node->entryType != ORBIS_FS_INODE_ENTRY_TYPE_EMPTY, "directory is empty");
    retassure(node->dataLnk.type == ORBIS_FS_CHAINLINK_TYPE_LINK, "inode %d has invalid data link",node->inodeNum);
    
    OrbisFSDirectoryElem_t *elems = (OrbisFSDirectoryElem_t*)_parent->getBlock(node->dataLnk.blk);
    while (true) {
        for (int i=0; i<_elemsPerBlock; i++) {
            OrbisFSDirectoryElem_t *curElem = &elems[i];
            if (curElem->inodeNum == 0) goto error;
            
            if (curElem->namelen > sizeof(curElem->name)){
                uint32_t extrablocks = curElem->namelen - sizeof(curElem->name);
                extrablocks /= sizeof(*curElem);
                extrablocks++;
                i+=extrablocks;
                warning("namelen too large, figure this out!");
            }
            std::string elemName{curElem->name,curElem->name+curElem->namelen};
            if (elemName == "." || elemName == "..") continue;
            if (elemName != childname) continue;
            
            OrbisFSInode_t *curInode = NULL;
            try {
                curInode = findInode(curElem->inodeNum);
            } catch (tihmstar::OrbisFSInodeBadMagic &e) {
#ifdef XCODE
                debug("Ignoring vanished elem '%s'",elemName.c_str());
#endif
                continue;
            }
            return curInode;
        }
        reterror("TODO: move to next elems block!");
    }
error:
    reterror("Failed to find child in directory");
}

OrbisFSInode_t *OrbisFSInodeDirectory::findInode(uint32_t inodeNum){
    OrbisFSInode_t *ret = NULL;
    if (inodeNum < _parent->getBlocksize() / sizeof(*ret)) {
        ret = &_inodeRootDir[inodeNum];
    }else {
        reterror("TODO: implement 2 stage inode lookup");
    }
    retcustomassure(OrbisFSInodeBadMagic, ret->magic == ORBIS_FS_INODE_MAGIC, "inode %d entry has bad magic",inodeNum);
    retassure(ret->entryType == ORBIS_FS_INODE_ENTRY_TYPE_EMPTY
              || ret->entryType == ORBIS_FS_INODE_ENTRY_TYPE_SINGLEBLOCK
              || ret->entryType == ORBIS_FS_INODE_ENTRY_TYPE_MULTIBLOCK, "inode %d entry has unknown entry type %d",inodeNum,ret->entryType);
    retassure(ret->inodeNum == inodeNum, "inode %d entry has wrong inode num",inodeNum);
    retassure(memvcmp(&ret->_pad0, sizeof(ret->_pad0), 0x00), "inode %d entry _pad0 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad1, sizeof(ret->_pad1), 0x00), "inode %d entry _pad1 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad2, sizeof(ret->_pad2), 0x00), "inode %d entry _pad2 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad3, sizeof(ret->_pad3), 0x00), "inode %d entry _pad3 is not zero",inodeNum);
    retassure(memvcmp(&ret->_pad4, sizeof(ret->_pad4), 0x00), "inode %d entry _pad4 is not zero",inodeNum);
    return ret;
}

uint32_t OrbisFSInodeDirectory::findInodeIDForPath(std::string path){
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
