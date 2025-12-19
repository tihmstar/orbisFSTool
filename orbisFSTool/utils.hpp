//
//  utils.hpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#ifndef utils_hpp
#define utils_hpp

#include <iostream>

#include <stdint.h>
#include <stddef.h>

namespace orbisFSTool {

bool memvcmp(void *memory, size_t size, uint8_t val);
void DumpHex(const void* data, size_t size);

std::string strForDate(time_t date);

}

#endif /* utils_hpp */
