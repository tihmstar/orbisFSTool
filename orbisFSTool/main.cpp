//
//  main.cpp
//  orbisFSTool
//
//  Created by tihmstar on 16.12.25.
//

#include "OrbisFSImage.hpp"

#include <libgeneral/macros.h>
#include <libgeneral/Utils.hpp>

#include <getopt.h>

#include <sys/stat.h>

using namespace orbisFSTool;

static struct option longopts[] = {
    { "help",               no_argument,        NULL, 'h' },
    { "input",              required_argument,  NULL, 'i' },
    { "list",               required_argument,  NULL, 'l' },
    { "recursive",          no_argument,        NULL, 'r' },
    { "verbose",            no_argument,        NULL, 'v' },
    { "writeable",          no_argument,        NULL, 'w' },

    { "mount",              required_argument,  NULL,  0  },
    { "offset",             required_argument,  NULL,  0  },
    { NULL, 0, NULL, 0 }
};

void cmd_help(){
    printf(
           "Usage: orbisFSTool [OPTIONS]\n"
           "Work with orbisFS disk images\n\n"
           "  -h, --help\t\t\t\tprints usage information\n"
           "  -i, --input <path>\t\t\tinput file (or blockdevice)\n"
           "  -l, --list <path>\t\t\tlist files at path\n"
           "  -r, --recursive\t\t\tperform operation recursively\n"
           "  -v, --verbose\t\t\tincrease logging output\n"
           "  -w, --writeable\t\t\topen image in write mode\n"
           "      --mount <path>\t\t\tpath to mount\n"
           "      --offset <cnt>\t\t\toffset inside image\n"
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
    const char *mountPath = NULL;
    
    const char *listPath = NULL;

    uint64_t offset = 0;
    
    int verbosity = 0;
    
    bool writeable = false;
    bool recursive = false;
    
    while ((opt = getopt_long(argc, (char* const *)argv, "hi:l:rvw", longopts, &optindex)) >= 0) {
        switch (opt) {
            case 0: //long opts
            {
                std::string curopt = longopts[optindex].name;

                if (curopt == "mount") {
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
                
            case 'i':
                infile = optarg;
                break;
                
            case 'l':
                listPath = optarg;
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
    
    OrbisFSImage img(infile, writeable, offset);
    
    if (listPath) {
        img.iterateOverFilesInFolder(listPath, recursive, [&](std::string path, OrbisFSInode_t node){
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
                snprintf(buf, sizeof(buf), "(%4d) ", node.inodeNum);
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
    }

    info("Done");
    return 0;
}
