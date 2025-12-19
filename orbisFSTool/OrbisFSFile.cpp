//
//  OrbisFSFile.cpp
//  orbisFSTool
//
//  Created by tihmstar on 18.12.25.
//

#include "OrbisFSFile.hpp"
#include "OrbisFSException.hpp"
#include "OrbisFSImage.hpp"
#include "utils.hpp"

#include <libgeneral/macros.h>

#include <sys/stat.h>

#define ARRAYOF(a) (sizeof(a)/sizeof(*a))

using namespace orbisFSTool;

#pragma mark OrbisFSFile
OrbisFSFile::OrbisFSFile(OrbisFSImage *parent, OrbisFSInode_t *node, bool noFilemodeChecks)
: _parent(parent)
, _node(node)
, _blockSize(_parent->getBlocksize())
, _offset(0)
{
    retassure(noFilemodeChecks || S_ISREG(node->fileMode), "Can't open node %d, which not a regular file",_node->inodeNum);
    {
        /*
            Notify parent, that we exist now
         */
        std::unique_lock<std::mutex> ul(_parent->_referencesLck);
        ++_parent->_references;
    }
}

OrbisFSFile::~OrbisFSFile(){
    {
        /*
            Tell parent we are leaving
         */
        std::unique_lock<std::mutex> ul(_parent->_referencesLck);
        --_parent->_references;
        _parent->_unrefEvent.notifyAll();
    }
}

#pragma mark OrbisFSFile private
uint8_t *OrbisFSFile::getDataBlock(uint64_t num){
    retassure(_node->fatStages, "File has no data");
    const uint32_t linkElemsPerPage = _blockSize/sizeof(OrbisFSChainLink_t);

    retassure(_node->dataLnk[0].type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad dataLnk type 0x%02x",_node->dataLnk[0].type);
    if (_node->fatStages == 1){
        retassure(num < ARRAYOF(_node->dataLnk), "1 level stage lookup out of bounds");
        return _parent->getBlock(_node->dataLnk[num].blk);
    }

    /*
        Multistage lookup here
     */
    OrbisFSChainLink_t *fat = (OrbisFSChainLink_t*)_parent->getBlock(_node->dataLnk[0].blk);
    if (_node->fatStages >= 3) {
        retassure(_node->fatStages < 4, "4 level stage lookup not supported");
        /*
            Stage 3 lookup
         */
        uint64_t stage3Idx = num / linkElemsPerPage;
        retassure(stage3Idx < linkElemsPerPage, "Trying to access out of bounds block on stage 3 lookup");
        num %= linkElemsPerPage;
        fat = &fat[stage3Idx];
        retassure(fat->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad dataLnk type 0x%02x in stage 3 lookup",fat->type);
        fat = (OrbisFSChainLink_t*)_parent->getBlock(fat->blk);
    }
    
    /*
        Stage 2 lookup
     */
    fat = &fat[num];
    retassure(fat->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad fat type 0x%02x",fat->type);
    return _parent->getBlock(fat->blk);
}

uint8_t *OrbisFSFile::getDataForOffset(uint64_t offset){
    retassure(offset < _node->filesize, "trying to access data beyond filesize");
    uint64_t blkIdx = offset/_blockSize;
    uint64_t blkOffset = offset & (_blockSize-1); //expected to be a power of two
    
    uint8_t *blk = getDataBlock(blkIdx);
    
    return &blk[blkOffset];
}

#pragma mark OrbisFSFile public
#pragma mark IO operations
uint64_t OrbisFSFile::size(){
    return _node->filesize;
}

size_t OrbisFSFile::read(void *buf, size_t len){
    size_t didRead = pread(buf, len, _offset);
    _offset += didRead;
    return didRead;
}

size_t OrbisFSFile::write(const void *buf, size_t len){
    size_t didwrite = pwrite(buf, len, _offset);
    _offset += didwrite;
    return didwrite;
}

size_t OrbisFSFile::pread(void *buf, size_t len, uint64_t offset){
    if (_offset >= _node->filesize) return 0;
    if (_offset + len >= _node->filesize) len = _node->filesize-_offset;
        
    uint32_t dataBlock = (uint32_t)(offset/_blockSize);
    offset &= (_blockSize-1); //always a power of 2
    
    if (offset + len > _blockSize) len = _blockSize-offset;
    
    uint8_t *block = getDataBlock(dataBlock);
    memcpy(buf, &block[offset], len);
    return len;
}

size_t OrbisFSFile::pwrite(const void *buf, size_t len, uint64_t offset){
    reterror("TODO");
}

#pragma mark resource IO
uint64_t OrbisFSFile::resource_size(){
    /*
        This is wrong, but idk how to get the actual size :(
     */
    uint64_t ret = 0;
    for (int i=0; i<ARRAYOF(_node->resourceLnk); i++) {
        if (_node->resourceLnk[i].type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        ret += _blockSize;
    }
    return ret;
}

size_t OrbisFSFile::resource_pread(void *buf, size_t len, uint64_t offset){
    size_t rs = resource_size();

    if (offset >= rs) return 0;
    if (offset + len > rs) len = rs - offset;
    if (!len) return 0;

    uint32_t resouceBlockIdx = (uint32_t)(offset / _blockSize);
    uint32_t resouceBlockOffset = offset & (_blockSize-1); //expected to be a power of 2

    retassure(resouceBlockIdx < ARRAYOF(_node->resourceLnk), "resouceBlockIdx is out of bounds");

    OrbisFSChainLink_t *rl = &_node->resourceLnk[resouceBlockIdx];
    retassure(rl->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "tgt resouce chain link has bad type 0x%02x",rl->type);

    uint8_t *block = _parent->getBlock(rl->blk);
    memcpy(buf, &block[resouceBlockOffset], len);
    return len;
}

