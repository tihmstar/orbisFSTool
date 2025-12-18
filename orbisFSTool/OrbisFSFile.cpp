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

using namespace orbisFSTool;

#pragma mark OrbisFSFile
OrbisFSFile::OrbisFSFile(OrbisFSImage *parent, OrbisFSInode_t *node)
: _parent(parent)
, _node(node)
, _blockSize(_parent->getBlocksize())
, _offset(0)
{
    

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
uint8_t *OrbisFSFile::getDataBlock(uint32_t num){
    retassure(_node->dataLnk.type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad dataLnk type 0x%02x",_node->dataLnk.type);

    OrbisFSChainLink_t *fat = NULL;
    
    if (num < _blockSize/sizeof(OrbisFSChainLink_t)) {
        fat = (OrbisFSChainLink_t*)_parent->getBlock(_node->dataLnk.blk);
        fat = &fat[num];
    }else{
        reterror("TODO lookup more fat blocks?");
    }
    
    retassure(fat->type == ORBIS_FS_CHAINLINK_TYPE_LINK, "bad fat type 0x%02x",fat->type);
    return _parent->getBlock(fat->blk);
}

#pragma mark OrbisFSFile public
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
