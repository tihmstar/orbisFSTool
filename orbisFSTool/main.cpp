//
//  main.cpp
//  orbisFSTool
//
//  Created by tihmstar on 16.12.25.
//

#include "OrbisFSImage.hpp"
#include "OrbisFSFuse.hpp"
#include "utils.hpp"

#include <libgeneral/macros.h>
#include <libgeneral/Utils.hpp>

#include <algorithm>

#include <sys/stat.h>

#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#define ARRAYOF(a) (sizeof(a)/sizeof(*a))

using namespace orbisFSTool;

static struct option longopts[] = {
    { "help",               no_argument,        NULL, 'h' },
    { "extract",            optional_argument,  NULL, 'e' },
    { "input",              required_argument,  NULL, 'i' },
    { "output",             required_argument,  NULL, 'o' },
    { "path",               required_argument,  NULL, 'p' },
    { "list",               optional_argument,  NULL, 'l' },
    { "recursive",          no_argument,        NULL, 'r' },
    { "verbose",            no_argument,        NULL, 'v' },
    { "writeable",          no_argument,        NULL, 'w' },

    { "extract-resource",   no_argument,        NULL,  0  },
    { "inode",              required_argument,  NULL,  0  },
    { "mount",              required_argument,  NULL,  0  },
    { "offset",             required_argument,  NULL,  0  },

    //advanced debugging
    { "dump-inode",         no_argument,        NULL,  0  },
    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf(
           "Usage: orbisFSTool [OPTIONS]\n"
           "Work with orbisFS disk images\n\n"
           "  -h, --help\t\t\tprints usage information\n"
           "  -e, --extract [path]\t\textract path from image)\n"
           "  -i, --input <path>\t\tinput file (or blockdevice)\n"
           "  -l, --list [path]\t\tlist files at path\n"
           "  -o, --output <path>\t\toutput path\n"
           "  -p, --path <path>\t\tpath inside image\n"
           "  -r, --recursive\t\tperform operation recursively\n"
           "  -v, --verbose\t\t\tincrease logging output\n"
           "  -w, --writeable\t\topen image in write mode\n"
           "      --extract-resource\textract file resource instead of file contents\n"
           "      --inode <num>\t\tspecify file by Inode instead of path\n"
           "      --mount <path>\t\tpath to mount\n"
           "      --offset <cnt>\t\toffset inside image\n"
           "\n"
           //advanced debugging
           "      --dump-inode\t\tdump inode structure\n"
           "\n"
           );
}

uint64_t parseNum(const char *num){
    bool isHex = false;
    int64_t ret = 0;
    
    if (strncasecmp(num, "0x", 2) == 0) {
        isHex = true;
    }

    if (isHex) {
        num += 2;
        //parse hex
        while (*num) {
            char c = *num++;
            ret <<= 4;
            if (c >= '0' && c<='9') {
                ret += c - '0';
            }else if (c >= 'a' && c <= 'f'){
                ret += 10 + c - 'a';
            }else if (c >= 'A' && c <= 'F'){
                ret += 10 + c - 'A';
            }else{
                reterror("Got unexpected char '%c' in hex mode",c);
            }
        }
    }else{
        //parse decimal
        while (*num) {
            char c = *num++;
            retassure(c >= '0' && c <= '9', "Got unexpected char '%c' in decimal mode",c);
            ret *= 10;
            ret += c - '0';
        }
    }
    return ret;
}

MAINFUNCTION
int main_r(int argc, const char * argv[]) {
    info("%s",VERSION_STRING);
    
    int optindex = 0;
    int opt = 0;
    
    const char *infile = NULL;
    const char *outfile = NULL;
    const char *mountPath = NULL;
    
    std::string imagePath;

    uint64_t offset = 0;
    uint32_t iNode = 0;
    
    int verbosity = 0;
    
    bool writeable = false;
    bool recursive = false;

    bool doList = false;
    bool doExtract = false;
    bool doExtractResource = false;
    
    bool dumpInode = false;
    
    while ((opt = getopt_long(argc, (char* const *)argv, "he::i:l::o:rvw", longopts, &optindex)) >= 0) {
        switch (opt) {
            case 0: //long opts
            {
                std::string curopt = longopts[optindex].name;

                if (curopt == "extract-resource") {
                    doExtractResource = true;
                }else if (curopt == "inode"){
                    iNode = atoi(optarg);
                }else if (curopt == "mount"){
                    mountPath = optarg;
                }else if (curopt == "offset"){
                    offset = parseNum(optarg);

                }else if (curopt == "dump-inode"){
                    dumpInode = true;
                } else {
                    reterror("unexpected lonopt=%s",curopt.c_str());
                }
                break;
            }
                
            case 'h':
                cmd_help();
                return 0;
                
            case 'e':
                if (optarg) {
                    retassure(!imagePath.size(), "Image path already set");
                    imagePath = optarg;
                }
                doExtract = true;
                break;

            case 'i':
                infile = optarg;
                break;
                
            case 'l':
                if (optarg) {
                    retassure(!imagePath.size(), "Image path already set");
                    imagePath = optarg;
                }
                doList = true;
                break;

            case 'o':
                outfile = optarg;
                break;

            case 'p':
                retassure(!imagePath.size(), "Image path already set");
                imagePath = optarg;
                break;

            case 'r':
                recursive = true;
                break;

            case 'v':
                verbosity++;
                break;
                
            case 'w':
                writeable = true;
                info("Enable writeable mode");
                break;
                
            default:
                cmd_help();
                return -1;
        }
    }
    
    if (!infile){
        error("No inputfile specified");
        cmd_help();
        return -1;
    }
    
    std::shared_ptr<OrbisFSImage> img = std::make_shared<OrbisFSImage>(infile, writeable, offset);
    
    if (!imagePath.size() && iNode) {
        char buf[0x100] = {};
        snprintf(buf, sizeof(buf), "iNode%d",iNode);
        imagePath = buf;
    }
    
    if (doExtract) {
        retassure(outfile, "No outputpath specified");
        retassure(imagePath.size(), "No path for extraction specified");
        uint8_t *buf = NULL;
        int fd = -1;
        cleanup([&]{
            safeClose(fd);
            safeFree(buf);
        });
        size_t bufSize = img->getBlocksize();
        
        auto f = img->openFilAtPath(imagePath);

        retassure((fd = open(outfile, O_CREAT | O_WRONLY, 0644)) != -1, "Failed to create output file '%s' errno=%d (%s)",outfile,errno,strerror(errno));

        uint64_t size = doExtractResource ? f->resource_size() : f->size();
        if (size) {
            if (bufSize > size) bufSize = size;
            assure(buf = (uint8_t*)calloc(1, bufSize));
            
            
            if (doExtractResource){
                uint64_t offset = 0;
                while (size) {
                    size_t didread = 0;
                    retassure(didread = f->resource_pread(buf, bufSize, offset), "Failed to pread file resource");
                    retassure(write(fd, buf, bufSize) == bufSize, "Failed to write data to file '%s'",outfile);
                    offset += didread;
                    size -= didread;
                }
                info("Extracted resource of '%s' to '%s'",imagePath.c_str(),outfile);
            }else{
                while (size) {
                    size_t didread = 0;
                    retassure(didread = f->read(buf, bufSize), "Failed to read file");
                    retassure(write(fd, buf, bufSize) == bufSize, "Failed to write data to file '%s'",outfile);
                    size -= didread;
                }
                info("Extracted '%s' to '%s'",imagePath.c_str(),outfile);
            }
        }
    } else if (doList) {
        if (!imagePath.size()) imagePath = "/";
        img->iterateOverFilesInFolder(imagePath, recursive, [&](std::string path, OrbisFSInode_t node){
            if (path.back() == '/') path.pop_back();
            std::string printInfo;
            
            if (verbosity > 0) {
                if (S_ISDIR(node.fileMode)) printInfo+='d';
                else if (S_ISLNK(node.fileMode)) printInfo+='l';
                else printInfo+='-';

                std::string perm;
                int permNum = node.fileMode & 0777;
                for (int i=0; i<3; i++) {
                    int c = permNum & 7;
                    if (c & 1) perm += 'x';
                    else perm += '-';
                    if (c & 2) perm += 'w';
                    else perm += '-';
                    if (c & 4) perm += 'r';
                    else perm += '-';
                    permNum /= 8;
                }
                std::reverse(perm.begin(), perm.end());
                printInfo += perm;
                {
                    char buf[0x400];
                    uint64_t fsize = S_ISDIR(node.fileMode) ? 0 : node.filesize;
                    snprintf(buf, sizeof(buf), "%10llu ",fsize);
                    printInfo += buf;
                    printInfo += strForDate(node.accessOrModDate)+"\t";
                }
            }
            
            if (verbosity > 1) {
                char buf[0x100] = {};
                snprintf(buf, sizeof(buf), "(%6d) ", node.inodeNum);
                printInfo += buf;
            }

            printInfo += path;
            if (S_ISDIR(node.fileMode)) {
                printInfo += '/';
            }else if (S_ISLNK(node.fileMode)){
                reterror("Symlinks currently not supported!");
            }
            
            printf("%s\n",printInfo.c_str());
            if (dumpInode) {
                printf("Inode:\n");
                printf("\tmagic             : 0x%08x\n",node.magic);
                printf("\tfatStages         : 0x%08x\n",node.fatStages);
                printf("\tinodeNum          : 0x%08x\n",node.inodeNum);
                printf("\tfileMode          :  o0%o\n",node.fileMode);
                printf("\tfilesize          : 0x%016llx (%lld)\n",node.filesize,node.filesize);
                printf("\tcreateDate        : %10lld (%s)\n",node.createDate,strForDate(node.createDate).c_str());
                printf("\taccessOrModDate   : %10lld (%s)\n",node.accessOrModDate,strForDate(node.accessOrModDate).c_str());
                printf("\tmodOrAccessData   : %10lld (%s)\n",node.modOrAccessData,strForDate(node.modOrAccessData).c_str());
                for (int i=0; i<ARRAYOF(node.resourceLnk); i++) {
                    if (node.resourceLnk[i].type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
                    printf("\tresourceLnk[%2d]   : type: 0x%02x blk: %d\n",i,node.resourceLnk[i].type,node.resourceLnk[i].blk);
                }
                for (int i=0; i<ARRAYOF(node.dataLnk); i++) {
                    if (node.dataLnk[i].type != ORBIS_FS_CHAINLINK_TYPE_LINK) break;
                    printf("\tdataLnk[%2d]       : type: 0x%02x blk: %d\n",i,node.dataLnk[i].type,node.dataLnk[i].blk);
                }
                if (verbosity >= 2) {
                    DumpHex(&node, sizeof(node));
                }
            }
        });
    }else if (mountPath) {
        info("Mounting disk");
        OrbisFSFuse off(img, mountPath);
        off.loopSession();
    }

    info("Done");
    return 0;
}
