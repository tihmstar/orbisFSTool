//
//  OrbisFSException.hpp
//  orbisFSTool
//
//  Created by tihmstar on 18.12.25.
//

#ifndef OrbisFSException_hpp
#define OrbisFSException_hpp

#include <libgeneral/exception.hpp>

namespace tihmstar {
EASY_BASE_EXCEPTION(OrbisFSException);

//custom exceptions
EASY_EXCEPTION(OrbisFSInodeBadMagic, OrbisFSException);


}

#endif /* OrbisFSException_hpp */
