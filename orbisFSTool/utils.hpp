//
//  utils.hpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#ifndef utils_hpp
#define utils_hpp

#include <stdint.h>
#include <stddef.h>

namespace orbisFSTool {

bool memvcmp(void *memory, size_t size, uint8_t val);

}
#endif /* utils_hpp */
