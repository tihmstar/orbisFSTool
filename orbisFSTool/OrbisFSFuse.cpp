//
//  OrbisFSFuse.cpp
//  orbisFSTool
//
//  Created by tihmstar on 18.12.25.
//

#include "OrbisFSFuse.hpp"
#include "OrbisFSException.hpp"
#include <libgeneral/macros.h>

#ifdef HAVE_FUSE
#   define FUSE_USE_VERSION 28
#   include <fuse/fuse.h>
#endif

using namespace orbisFSTool;

#ifdef HAVE_FUSE
static int fs_getattr(const char *path, struct stat *stbuf) noexcept{
    struct fuse_context *ctx = fuse_get_context();
    OrbisFSImage *img = (OrbisFSImage*)ctx->private_data;
    
    OrbisFSInode_t node = {};
    memset(stbuf, 0, sizeof(*stbuf));
    
    try {
        node = img->getInodeForPath(path);
    } catch(tihmstar::OrbisFSFileNotFound &e){
        return -EEXIST;
    } catch (tihmstar::exception &e) {
        e.dump();
        return -EFAULT;
    }
    
    stbuf->st_ino = node.inodeNum;
    stbuf->st_mode = node.fileMode;
    stbuf->st_size = node.filesize;
    {
#ifdef __APPLE__
    stbuf->st_birthtimespec.tv_sec = node.createDate;
    stbuf->st_birthtimespec.tv_nsec = 0;
    stbuf->st_mtimespec.tv_sec = node.modOrAccessDate;
    stbuf->st_mtimespec.tv_nsec = 0;
    stbuf->st_ctimespec.tv_sec = node.modOrAccessDate;
    stbuf->st_ctimespec.tv_nsec = 0;
    stbuf->st_atimespec.tv_sec = node.accessOrModDate;
    stbuf->st_atimespec.tv_nsec = 0;
#elif defined(__linux__)
    /*
        birthtime is not available on linux stat
     */
    stbuf->st_mtim.tv_sec = node.modOrAccessData;
    stbuf->st_mtim.tv_nsec = 0;
    stbuf->st_ctim.tv_sec = node.modOrAccessDate;
    stbuf->st_ctim.tv_nsec = 0;
    stbuf->st_atim.tv_sec = node.accessOrModDate;
    stbuf->st_atim.tv_nsec = 0;
#else
#error unexpected platform!
#endif
    }
    
    stbuf->st_mode |= 05; //this isn't accurate, but we do want to read the files afterall, right?
    
    return 0;
 }

static int fs_readlink(const char *path, char *buf, size_t bufsize) noexcept{
    /*
        Currently not supported
     */
    return -EFAULT;
}

static int fs_open(const char *path, struct fuse_file_info *fi) noexcept{
    struct fuse_context *ctx = fuse_get_context();
    OrbisFSImage *img = (OrbisFSImage*)ctx->private_data;
    std::shared_ptr<OrbisFSFile> f;

    try {
        f = img->openFilAtPath(path);
    } catch(tihmstar::OrbisFSFileNotFound &e){
        return -EEXIST;
    } catch (tihmstar::exception &e) {
        e.dump();
        return -EFAULT;
    }
    
    fi->fh = (uint64_t)new std::shared_ptr<OrbisFSFile>(f);
    return 0;
}

static int fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) noexcept{
    std::shared_ptr<OrbisFSFile> *f = (std::shared_ptr<OrbisFSFile> *)fi->fh;
    try {
        return (int)(*f)->pread(buf, size, offset);
    } catch (tihmstar::exception &e) {
        return -EFAULT;
    }
}

static int fs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) noexcept{
    std::shared_ptr<OrbisFSFile> *f = (std::shared_ptr<OrbisFSFile> *)fi->fh;
    try {
        return (int)(*f)->pwrite(buf, size, offset);
    } catch (tihmstar::exception &e) {
        return -EFAULT;
    }
}

static int fs_release(const char *path, struct fuse_file_info *fi) noexcept{
    std::shared_ptr<OrbisFSFile> *f = (std::shared_ptr<OrbisFSFile> *)fi->fh; fi->fh = 0;
    safeDelete(f);
    return 0;
}

int fs_opendir(const char *path, struct fuse_file_info *fi) noexcept{
    struct fuse_context *ctx = fuse_get_context();
    OrbisFSImage *img = (OrbisFSImage*)ctx->private_data;

    std::vector<std::pair<std::string, uint64_t>> *files = nullptr;
    cleanup([&]{
        safeDelete(files);
    });
    files = new std::vector<std::pair<std::string, uint64_t>>;
    
    try {
        auto ff = img->listFilesInFolder(path, true);
        for (auto f : ff){
            files->push_back({f.first.c_str(), f.second.inodeNum});
        }
    } catch(tihmstar::OrbisFSFileNotFound &e){
        return -EEXIST;
    } catch (tihmstar::exception &e) {
#ifdef DEBUG
        e.dump();
#endif
        return -EFAULT;
    }

    {
        std::vector<std::pair<std::string, uint64_t>> *old = (std::vector<std::pair<std::string, uint64_t>>*)fi->fh; fi->fh = 0;
        safeDelete(old);
    }
    fi->fh = (uint64_t)files; files = nullptr;
    return 0;
}

int fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info *fi) noexcept{
    std::vector<std::pair<std::string, uint64_t>> *tgt = (std::vector<std::pair<std::string, uint64_t>>*)fi->fh;

    while (off < tgt->size()) {
        auto e = tgt->at(off);
        struct stat stbuf = {};
        stbuf.st_ino = e.second;
        off++;
        if (filler(buf,e.first.c_str(), &stbuf, off)) break;
    }
    return 0;
}

int fs_releasedir(const char *path, struct fuse_file_info *fi) noexcept{
    std::vector<std::pair<std::string, uint64_t>> *old = (std::vector<std::pair<std::string, uint64_t>>*)fi->fh; fi->fh = 0;
    safeDelete(old);
    return 0;
}

static const struct fuse_operations orbisimgFuse_ops = {
    .getattr = fs_getattr,
    .readlink = fs_readlink,
    .open    = fs_open,
    .read    = fs_read,
    .write   = fs_write,
    .release = fs_release,
    .opendir = fs_opendir,
    .readdir = fs_readdir,
    .releasedir = fs_releasedir,
};
#endif


#pragma mark OrbisFSFuse
OrbisFSFuse::OrbisFSFuse(std::shared_ptr<OrbisFSImage> img, const char *mountPath)
: _img(img)
, _fuse(NULL), _ch(NULL)
{
#ifndef HAVE_FUSE
    reterror("Built without FUSE support!");
#else
    retassure(mountPath, "Error no mount point specified!");
    _mountpoint = mountPath;

    int vers = fuse_version();
    if (vers < FUSE_USE_VERSION) {
        reterror("Fuse version too low, expected %d but got %d",FUSE_USE_VERSION,vers);
    }
    info("Got FUSE version %d",vers);

    struct fuse_args args = {};
    cleanup([&]{
        fuse_opt_free_args(&args);
    });

    args = FUSE_ARGS_INIT(0, nullptr);
    assure(!fuse_opt_add_arg(&args, "FIRST_ARG_IS_IGNORED"));

    if (!_img->isWriteable()){
        assure(!fuse_opt_add_arg(&args, "-r"));
    }

    {
        std::string imgNameOpt = "fsname=OrbisFSFuse";
        assure(!fuse_opt_add_arg(&args, "-o"));
        assure(!fuse_opt_add_arg(&args, imgNameOpt.c_str()));
    }

#if !defined(__linux__)
    {
        std::string imgNameOpt = "volname=OrbisFSFuse";
        assure(!fuse_opt_add_arg(&args, "-o"));
        assure(!fuse_opt_add_arg(&args, imgNameOpt.c_str()));
    }
#endif //!defined(__linux__)

#ifdef __APPLE__
    assure(!fuse_opt_add_arg(&args, "-o"));
    if (!_img->isWriteable()){
        assure(!fuse_opt_add_arg(&args, "rdonly,local"));
    }else{
        assure(!fuse_opt_add_arg(&args, "local"));
    }
#endif

    {
        debug("Trying to mount at %s",_mountpoint.c_str());
        retassure(_ch = fuse_mount(_mountpoint.c_str(), &args), "Failed to mount");
        retassure(_fuse = fuse_new(_ch, &args, &orbisimgFuse_ops, sizeof(orbisimgFuse_ops), _img.get()), "Failed to create FUSE session");
    }
    info("Mounted at %s",_mountpoint.c_str());
#endif
}

OrbisFSFuse::~OrbisFSFuse(){
#ifdef HAVE_FUSE
    if (_ch) {
        fuse_unmount(_mountpoint.c_str(), _ch); _ch = NULL;
    }
    safeFreeCustom(_fuse, fuse_destroy);
#endif
}

#pragma mark OrbisFSFuse private

#pragma mark OrbisFSFuse public
void OrbisFSFuse::loopSession(){
#ifndef HAVE_FUSE
    reterror("Built without FUSE support!");
#else
    struct fuse_session *se = NULL;
    cleanup([&]{
        safeFreeCustom(se, fuse_remove_signal_handlers);
    });
    if ((se = fuse_get_session(_fuse))) {
        if (fuse_set_signal_handlers(se)){
            se = NULL;
            error("Failed to set FUSE sighandlers");
        }
    }
    fuse_loop_mt(_fuse);
#endif
}

void OrbisFSFuse::stopSession(){
#ifndef HAVE_FUSE
    reterror("Built without FUSE support!");
#else
    fuse_exit(_fuse);
#endif
}
