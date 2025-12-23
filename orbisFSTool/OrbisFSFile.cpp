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
        retassure(_node->dataLnk[num].type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad dataLnk type 0x%02x",_node->dataLnk[num].type);
        return _parent->getBlock(_node->dataLnk[num].blk);
    }
    
    OrbisFSChainLink_t *fat = NULL;
    for (int i=_node->fatStages-1; i>=0; i--) {
        uint64_t elemsInThisStage = 1;
        for (int z=0; z<i; z++) elemsInThisStage *= linkElemsPerPage;
        uint32_t curIdx = (uint32_t)(num / elemsInThisStage);
        num %= elemsInThisStage;
        
        if (!fat) {
            //this is the first level lookup
            retassure(curIdx < ARRAYOF(_node->dataLnk), "1 level lookup out of bounds");
            retassure(_node->dataLnk[curIdx].type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad dataLnk type 0x%02x",_node->dataLnk[curIdx].type);
            fat = (OrbisFSChainLink_t*)_parent->getBlock(_node->dataLnk[curIdx].blk);
        }else{
            //this is further level lookup
            retassure(curIdx < linkElemsPerPage, "Trying to access out of bounds block on stage %d lookup",_node->fatStages-(i-1));
            fat = &fat[curIdx];
            retassure(fat->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad fat type 0x%02x",fat->type);
            fat = (OrbisFSChainLink_t*)_parent->getBlock(fat->blk);
        }
    }
    return (uint8_t*)fat;
}

uint8_t *OrbisFSFile::getDataForOffset(uint64_t offset){
    retassure(offset < _node->filesize, "trying to access data beyond filesize");
    uint64_t blkIdx = offset/_blockSize;
    uint64_t blkOffset = offset & (_blockSize-1); //expected to be a power of two
    
    uint8_t *blk = getDataBlock(blkIdx);
    
    return &blk[blkOffset];
}

std::vector<uint32_t> OrbisFSFile::getAllAllocatedBlocks(){
    const uint32_t linkElemsPerPage = _blockSize/sizeof(OrbisFSChainLink_t);
    std::vector<uint32_t> ret;
    if (!_node->fatStages){
        return {};
    }else if (_node->fatStages == 1) {
        for (int i=0; i<ARRAYOF(_node->dataLnk); i++) {
            if (_node->dataLnk[i].type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
            ret.push_back(_node->dataLnk[i].blk);
        }
    }else if (_node->fatStages == 2){
        reterror("TODO");

    }else if (_node->fatStages == 3){
        
        reterror("TODO");
    }else{
        reterror("%d fat stages currently not supported",_node->fatStages);
    }
    
    //also count resource blocks
    for (int i=0; i<ARRAYOF(_node->resourceLnk); i++) {
        if (_node->resourceLnk[i].type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        ret.push_back(_node->resourceLnk[i].blk);
    }
    
    return ret;
}

void OrbisFSFile::popLastAllocatedBlock(){
    const uint32_t linkElemsPerPage = _blockSize/sizeof(OrbisFSChainLink_t);
    OrbisFSChainLink_t *tgt = NULL;
    uint32_t curIdx = 0;
    int fatEndStage = 0;
    uint32_t usedResourceBlocks = 0;
    
    for (int i=0; i<ARRAYOF(_node->resourceLnk); i++) {
        if (_node->resourceLnk[i].type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
        usedResourceBlocks++;
    }
    
    if (!_node->fatStages){
        reterror("No stages available");
    }else if (_node->fatStages == 1) {
        for (int i=ARRAYOF(_node->dataLnk)-1; i>=0; i--) {
            if (_node->dataLnk[i].type == ORBIS_FS_CHAINLINK_TYPE_LINK){
                tgt = &_node->dataLnk[i];
                if (i==0) _node->fatStages = 0;
                break;
            }
        }
    }else{
    rePopBlock:
        retassure(_node->usedBlocks, "Why does this node not use any blocks??");
        uint64_t num = _node->filesize / _blockSize;
        if ((_node->filesize & (_blockSize-1)) == 0) num--; //num is actually blockIndex not blockNumber
        
        OrbisFSChainLink_t *fat = NULL;
        for (int i=_node->fatStages-1; i>=fatEndStage; i--) {
            uint64_t elemsInThisStage = 1;
            for (int z=0; z<i; z++) elemsInThisStage *= linkElemsPerPage;
            curIdx = (uint32_t)(num / elemsInThisStage);
            num %= elemsInThisStage;
            
            if (!fat) {
                //this is the first level lookup
                retassure(curIdx < ARRAYOF(_node->dataLnk), "1 level lookup out of bounds");
                tgt = &_node->dataLnk[curIdx];
                retassure(tgt->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad dataLnk type 0x%02x",tgt->type);
                fat = (OrbisFSChainLink_t*)_parent->getBlock(tgt->blk);
            }else{
                //this is further level lookup
                retassure(curIdx < linkElemsPerPage, "Trying to access out of bounds block on stage %d lookup",_node->fatStages-(i-1));
                tgt = &fat[curIdx];
                retassure(tgt->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad fat type 0x%02x",tgt->type);
                fat = (OrbisFSChainLink_t*)_parent->getBlock(tgt->blk);
            }
        }
    }
       
    retassure(_node->fatStages < 4, "4 level stages currently not supported!");
    
    {
        uint32_t blk = tgt->blk;
        memset(tgt, 0xFF, sizeof(*tgt));
        _parent->freeBlock(blk);
        _node->usedBlocks--;
        if (curIdx == 0 && (_node->fatStages-fatEndStage > 1)) {
            fatEndStage++;
            goto rePopBlock;
        } else if (curIdx == 1 && (
                                   (_node->fatStages == 2 && _node->usedBlocks == 2+usedResourceBlocks)
                                   || (_node->fatStages == 3 && _node->usedBlocks == linkElemsPerPage+2+usedResourceBlocks)
                                   )){
            /*
                We are at fatStages>=2 but we can downgrage to fatStages>=1 here
             */
            retassure(_node->dataLnk[0].type == ORBIS_FS_CHAINLINK_TYPE_LINK, "unexpecte invalid dataLnk[0] when attepmting to downgrade fatStages 2 -> 1");
            blk = _node->dataLnk[0].blk;
            tgt = (OrbisFSChainLink_t*)_parent->getBlock(blk);
            retassure(tgt->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "unexpecte invalid tgt when attepmting to downgrade fatStages 2 -> 1");
            _node->dataLnk[0] = *tgt;
            _parent->freeBlock(blk);
            _node->usedBlocks--;
            _node->fatStages--;
        }
    }
}

void OrbisFSFile::shrink(uint64_t subBytes){
    retassure(_node->filesize >= subBytes, "trying to shrink more bytes than available");
    while (subBytes) {
        uint64_t lastBlockFill = _node->filesize & (_blockSize-1);
        if (!lastBlockFill && _node->filesize >= _blockSize) lastBlockFill = _blockSize;
        if (subBytes >= lastBlockFill) {
            popLastAllocatedBlock();
        }else{
            lastBlockFill = subBytes;
        }
        _node->filesize -= lastBlockFill;
        subBytes -= lastBlockFill;
    }
}

void OrbisFSFile::grow(uint64_t addBytes){
    reterror("TODO");
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

void OrbisFSFile::resize(uint64_t size){
    if (size < _node->filesize) {
        shrink(_node->filesize-size);
    }else if (size > _node->filesize){
        grow(size-_node->filesize);
    }
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

