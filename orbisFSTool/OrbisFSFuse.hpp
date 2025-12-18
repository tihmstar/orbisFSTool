//
//  OrbisFSFuse.hpp
//  orbisFSTool
//
//  Created by tihmstar on 18.12.25.
//

#ifndef OrbisFSFuse_hpp
#define OrbisFSFuse_hpp

#include "OrbisFSImage.hpp"

#include <memory>
#include <iostream>

struct fuse;
struct fuse_chan;

namespace orbisFSTool {

class OrbisFSFuse {
    std::shared_ptr<OrbisFSImage> _img;
    std::string _mountpoint;
    
    struct fuse *_fuse;
    struct fuse_chan *_ch;
public:
    OrbisFSFuse(std::shared_ptr<OrbisFSImage> disk, const char *mountPath);
    ~OrbisFSFuse();
    
    void loopSession();
    void stopSession();
};

}
#endif /* OrbisFSFuse_hpp */
