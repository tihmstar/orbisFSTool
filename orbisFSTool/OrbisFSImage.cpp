//
//  OrbisFSImage.cpp
//  orbisFSTool
//
//  Created by tihmstar on 16.12.25.
//

#include "OrbisFSImage.hpp"
#include "utils.hpp"

#include <libgeneral/macros.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <sys/ioctl.h>

#ifdef HAVE_SYS_DISK_H
#   include <sys/disk.h>
#endif //HAVE_SYS_DISK_H

#ifdef HAVE_LINUX_FS_H
#   include <linux/fs.h>
#endif //HAVE_LINUX_FS_H

#define BLOCK_SIZE 0x10000

#define ARRAYOF(a) (sizeof(a)/sizeof(*a))

using namespace orbisFSTool;

#pragma mark helper
#pragma mark OrbisFSImage
OrbisFSImage::OrbisFSImage(const char *path, bool writeable, uint64_t offset)
: _writeable(writeable)
, _fd(-1)
, _mem(NULL), _memsize(0)
, _superblock(NULL), _diskinfoblock(NULL)
, _blockAllocator(nullptr)
, _inodeDir(nullptr)
, _references(0)
{
    retassure((_fd = open(path, writeable ? O_RDWR : O_RDONLY)) != -1, "Failed to open=%s",path);
    {
        struct stat st = {};
        retassure(!fstat(_fd, &st), "Failed to stat file");
        
        if (S_ISBLK(st.st_mode)){
            uint64_t count = 0;
            uint64_t bsize = 0;
            {
                /*
                    macOS
                 */
#ifdef DKIOCGETBLOCKCOUNT
                retassure(!ioctl(_fd, DKIOCGETBLOCKCOUNT, &count), "Failed to get blk count");
#endif //DKIOCGETBLOCKCOUNT
#ifdef DKIOCGETBLOCKSIZE
                retassure(!ioctl(_fd, DKIOCGETBLOCKSIZE, &bsize), "Failed to get blk size");
#endif //DKIOCGETBLOCKSIZE
                debug("Got blkcnt=0x%llx",count);
                debug("Got blksize=0x%llx",bsize);
                _memsize = count * bsize;
            }
            
            {
                /*
                    Linux
                 */
                uint64_t devsize = 0;
#ifdef BLKGETSIZE64
                retassure(!ioctl(_fd, BLKGETSIZE64, &devsize), "Failed to get devsize size");
#endif //BLKGETSIZE64
                if (!_memsize) _memsize = devsize;
            }
            
        }else{
            _memsize = st.st_size;
        }
    }
    retassure(_memsize, "Failed to detect image size!");
    retassure(_memsize > offset, "offset beyond image size");
    _memsize -= offset;
    if ((_mem = (uint8_t*)mmap(NULL, _memsize, PROT_READ | (writeable ? PROT_WRITE : 0), MAP_FILE | (writeable ? MAP_SHARED : MAP_PRIVATE), _fd, offset)) == MAP_FAILED){
        reterror("Failed to mmap '%s' errno=%d (%s)",path,errno,strerror(errno));
    }
    init();
}

OrbisFSImage::~OrbisFSImage(){
    /*
        Close this before other files, because it might contain an open file
     */
    safeDelete(_inodeDir);
    
    {
        std::unique_lock<std::mutex> ul(_referencesLck);
        while (_references) {
            debug("Waiting for open files to close, %d remaining",_references);
            uint64_t wevent = _unrefEvent.getNextEvent();
            ul.unlock();
            _unrefEvent.waitForEvent(wevent);
            ul.lock();
        }
    }
    
    safeDelete(_blockAllocator);
    if (_mem){
        munmap(_mem, _memsize); _mem = NULL;
    }
    safeClose(_fd);
}

#pragma mark OrbisFSImage private
void OrbisFSImage::init(){
    /*
        Init Superblock
     */
    _superblock = (OrbisFSSuperblock_t*)getBlock(0);

    printf("Superblock:\n");
    printf("\tmagic             : 0x%llx\n",_superblock->magic);
    printf("\tunk0              : 0x%llx\n",_superblock->unk0);
    printf("\treserve           : '%.*s'\n",(int)sizeof(_superblock->reserve),_superblock->reserve);
    printf("\tversion           : 0x%llx\n",_superblock->version);
    printf("\tunk2              : 0x%llx\n",_superblock->unk2);
    printf("\tblockAllocatorLnk : type: 0x%02x blk: %d\n",_superblock->blockAllocatorLnk.type,_superblock->blockAllocatorLnk.blk);
    printf("\tunk4              : 0x%08x\n",_superblock->unk4);
    printf("\tunk5              : 0x%08x\n",_superblock->unk5);
    printf("\tdiskinfoLnk       : type: 0x%02x blk: %d\n",_superblock->diskinfoLnk.type,_superblock->diskinfoLnk.blk);
    
    retassure(_superblock->magic == ORBIS_FS_SUPERBLOCK_MAGIC, "Bad superblock magic");
    retassure(memvcmp(_superblock->_pad1, sizeof(_superblock->_pad1), 0x00), "_pad1 is not zero");
    retassure(!strcmp(_superblock->reserve, ORBIS_FS_SUPERBLOCK_RESERVE_STR), "unexpected reserve value '%.*s'",sizeof(_superblock->reserve),_superblock->reserve);
    retassure(memvcmp(_superblock->_pad2, sizeof(_superblock->_pad2), 0x00), "_pad2 is not zero");
    retassure(_superblock->version == ORBIS_FS_SUPERBLOCK_VERSION, "Unexpected superblock version!");
    
    retassure(memvcmp(_superblock+1, BLOCK_SIZE-sizeof(*_superblock), 0x00), "Bytes beyong superblock are not zero");
    
    retassure(_superblock->blockAllocatorLnk.type == ORBIS_FS_CHAINLINK_TYPE_LINK, "Unexpected blockAllocatorLnk.type");
    retassure(_superblock->diskinfoLnk.type == ORBIS_FS_CHAINLINK_TYPE_LINK, "Unexpected diskinfoLnk.type");

    /*
        Init BlockAllocator
     */
    _blockAllocator = new OrbisFSBlockAllocator(this, _superblock->blockAllocatorLnk.blk);
    {
        uint64_t totalBlocks = _blockAllocator->getTotalBlockNum();
        uint64_t totalSize = totalBlocks * BLOCK_SIZE;
        printf("\ttotalFSBlocks     : 0x%llx\n",totalBlocks);
        {
            const char *unit = "B";
            float s = 0;
            if (totalSize >= 1e12) {
                s = totalSize / 1e12;
                unit = "TB";
            }else if (totalSize >= 1e9) {
                s = totalSize / 1e9;
                unit = "GB";
            }else if (totalSize >= 1e6) {
                s = totalSize / 1e6;
                unit = "MB";
            }else if (totalSize >= 1e3) {
                s = totalSize / 1e3;
                unit = "KB";
            }
            printf("\ttotalFSSize       : 0x%llx (%.2f %s)\n",totalSize,s,unit);
        }
        retassure(totalSize <= _memsize, "FS claims to use more block than the image has");
        uint64_t freeBlocks = _blockAllocator->getFreeBlocksNum();
        printf("\tfreeBlocks        : 0x%llx\n",freeBlocks);
    }
    
    /*
        Init DiskinfoBlock
     */
    _diskinfoblock = (OrbisFSDiskinfoblock_t*)getBlock(_superblock->diskinfoLnk.blk);

    printf("Diskinfoblock:\n");
    printf("\tmagic             : 0x%llx\n",_diskinfoblock->magic);
    printf("\tunk1              : 0x%llx\n",_diskinfoblock->unk1_is_2);
    printf("\tunk2              : 0x%llx\n",_diskinfoblock->unk2_is_0x40);
    printf("\tunk3              : 0x%llx\n",_diskinfoblock->unk3_is_0);
    printf("\tdevpath           : '%.*s'\n",(int)sizeof(_diskinfoblock->devpath),_diskinfoblock->devpath);
    printf("\tinodesInRootFolder: 0x%x (%d)\n",_diskinfoblock->inodesInRootFolder,_diskinfoblock->inodesInRootFolder);
    printf("\tunk5              : 0x%x\n",_diskinfoblock->unk5_is_0xffffffff);
    printf("\tnumInodeSlots     : 0x%x (%d)\n",_diskinfoblock->numInodeSlots,_diskinfoblock->numInodeSlots);
    printf("\tblocksUsed        : 0x%llx\n",_diskinfoblock->blocksUsed);
    printf("\tblocksAvailable   : 0x%llx\n",_diskinfoblock->blocksAvailable);
#ifdef DEBUG
    printf("\tunk7:\n");
    DumpHex(_diskinfoblock->unk7,sizeof(_diskinfoblock->unk7));
#endif
    printf("\tinodedirLnk       : type: 0x%02x blk: %d\n",_diskinfoblock->inodedirLnk.type,_diskinfoblock->inodedirLnk.blk);
    printf("\tdiskinfoLnk       : type: 0x%02x blk: %d\n",_diskinfoblock->diskinfoLnk.type,_diskinfoblock->diskinfoLnk.blk);

    retassure(_diskinfoblock->magic == ORBIS_FS_DISKINFOBLOCK_MAGIC, "Bad diskinfoblock magic");
    retassure(_diskinfoblock->unk1_is_2 == 2, "unexpected value for unk1");
    retassure(_diskinfoblock->unk2_is_0x40 == 0x40, "unexpected value for unk2");
    retassure(_diskinfoblock->unk3_is_0 == 0, "unexpected value for unk3");
    retassure(_diskinfoblock->unk5_is_0xffffffff == 0xFFFFFFFF, "unexpected value for unk5");
    retassure(memvcmp(_diskinfoblock->_pad2, sizeof(_diskinfoblock->_pad2), 0x00), "_pad2 is not zero");
    retassure(!memcmp(&_diskinfoblock->diskinfoLnk, &_superblock->diskinfoLnk, sizeof(_superblock->diskinfoLnk)), "diskinfoLnk mismatch between superblock and diskinfoblock");
    retassure(_diskinfoblock->inodedirLnk.type == ORBIS_FS_CHAINLINK_TYPE_LINK, "Unexpected inodedirLnk.type");

    /*
        Init inode root dir block
     */
    _inodeDir = new OrbisFSInodeDirectory(this, _diskinfoblock->inodedirLnk.blk);
}

uint8_t *OrbisFSImage::getBlock(uint32_t blknum){
    size_t offset = (size_t)blknum * BLOCK_SIZE;
    retassure(offset+BLOCK_SIZE <= _memsize, "trying to access out of bounds block");
    return &_mem[offset];
}

std::shared_ptr<OrbisFSFile> OrbisFSImage::openFileNode(OrbisFSInode_t *node){
    return std::make_shared<OrbisFSFile>(this, node);
}

#pragma mark OrbisFSImage public
bool OrbisFSImage::isWriteable(){
    return _writeable;
}

uint32_t OrbisFSImage::getBlocksize(){
    return BLOCK_SIZE;
}

std::vector<std::pair<std::string, OrbisFSInode_t>> OrbisFSImage::listFilesInFolder(std::string path, bool includeSelfAndParent){
    return _inodeDir->listFilesInDir(_inodeDir->findInodeIDForPath(path), includeSelfAndParent);
}

std::vector<std::pair<std::string, OrbisFSInode_t>> OrbisFSImage::listFilesInFolder(uint32_t inode, bool includeSelfAndParent){
    return _inodeDir->listFilesInDir(inode, includeSelfAndParent);
}

OrbisFSInode_t OrbisFSImage::getInodeForID(uint32_t inode){
    return *_inodeDir->findInode(inode);
}

OrbisFSInode_t OrbisFSImage::getInodeForPath(std::string path){
    return *_inodeDir->findInodeForPath(path);
}

std::shared_ptr<OrbisFSFile> OrbisFSImage::openFileID(uint32_t inode){
    return openFileNode(_inodeDir->findInode(inode));
}

std::shared_ptr<OrbisFSFile> OrbisFSImage::openFilAtPath(std::string path){
    return openFileNode(_inodeDir->findInodeForPath(path));
}

void OrbisFSImage::iterateOverFilesInFolder(std::string path, bool recursive, std::function<void(std::string path, OrbisFSInode_t node)> callback){
    std::vector<std::pair<std::string, OrbisFSInode_t>> files;
    std::pair<std::string, OrbisFSInode_t> curpath = {path,{}};
    bool scanFolder = true;
    curpath.second.inodeNum = _inodeDir->findInodeIDForPath(path);
    do{
    loopStart:
        std::vector<std::pair<std::string, OrbisFSInode_t>> curfiles;

        {
            auto r = _inodeDir->findInode(curpath.second.inodeNum);
            curpath = {curpath.first,*r};
        }

        if (S_ISDIR(curpath.second.fileMode)) {
            if (scanFolder) curfiles = listFilesInFolder(curpath.second.inodeNum);
            callback(curpath.first,curpath.second);
        }else{
            scanFolder = false;
            files.push_back(curpath);
        }

        if (scanFolder) {
            for (auto cp = curfiles.rbegin(); cp != curfiles.rend(); ++cp) {
                std::string p = curpath.first;
                if (p.back() != '/') {
                    p+='/';
                }
                p += cp->first;
                files.push_back({p,cp->second});
            }
            scanFolder = recursive;
        }
        while (files.size()) {
            curpath = files.back();
            files.pop_back();
            if (S_ISDIR(curpath.second.fileMode)) {
                goto loopStart; //skip check at the end in case files.size() == 0
            }else{
                callback(curpath.first,curpath.second);
            }
        }
    }while(files.size());
}
