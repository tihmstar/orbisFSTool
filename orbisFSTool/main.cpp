//
//  main.cpp
//  orbisFSTool
//
//  Created by tihmstar on 16.12.25.
//

#include "OrbisFSImage.hpp"
#include "OrbisFSFuse.hpp"

#include <libgeneral/macros.h>
#include <libgeneral/Utils.hpp>

#include <functional>

#include <sys/stat.h>

#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

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

    { "inode",              required_argument,  NULL,  0  },
    { "mount",              required_argument,  NULL,  0  },
    { "offset",             required_argument,  NULL,  0  },
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
           "      --inode <num>\t\tspecify file by Inode instead of path\n"
           "      --mount <path>\t\tpath to mount\n"
           "      --offset <cnt>\t\toffset inside image\n"
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
    
    while ((opt = getopt_long(argc, (char* const *)argv, "he::i:l::o:rvw", longopts, &optindex)) >= 0) {
        switch (opt) {
            case 0: //long opts
            {
                std::string curopt = longopts[optindex].name;

                if (curopt == "inode") {
                    iNode = atoi(optarg);
                }else if (curopt == "mount"){
                    mountPath = optarg;
                }else if (curopt == "offset"){
                    offset = parseNum(optarg);
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
    
    if (!imagePath.size() && iNode) imagePath = img->getPathForInode(iNode);
    
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
        uint64_t size = f->size();
        if (bufSize > size) bufSize = size;
        assure(buf = (uint8_t*)calloc(1, bufSize));
        retassure((fd = open(outfile, O_CREAT | O_WRONLY, 0644)) != -1, "Failed to create output file '%s' errno=%d (%s)",outfile,errno,strerror(errno));
        
        while (size) {
            size_t didread = 0;
            retassure(didread = f->read(buf, bufSize), "Failed to read file");
            retassure(write(fd, buf, bufSize) == bufSize, "Failed to write data to file '%s'",outfile);
            size -= didread;
        }
        info("Extracted '%s' to '%s'",imagePath.c_str(),outfile);
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
                    {
                        uint64_t usetime = node.accessOrModDate;
                        time_t t = usetime;
                        struct tm * timeinfo = localtime (&t);
                        strftime(buf, sizeof(buf), "%Y %b %d %H:%M:%S\t", timeinfo);
                        printInfo += buf;
                    }
                }
            }
            
            if (verbosity > 1) {
                char buf[0x100] = {};
                snprintf(buf, sizeof(buf), "(%5d) ", node.inodeNum);
                printInfo += buf;
            }

            printInfo += path;
            if (S_ISDIR(node.fileMode)) {
                printInfo += '/';
            }else if (S_ISLNK(node.fileMode)){
                reterror("Symlinks currently not supported!");
            }
            
            printf("%s\n",printInfo.c_str());
        });
    }else if (mountPath) {
        info("Mounting disk");
        OrbisFSFuse off(img, mountPath);
        off.loopSession();
    }

    info("Done");
    return 0;
}
