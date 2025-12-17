//
//  utils.cpp
//  orbisFSTool
//
//  Created by tihmstar on 17.12.25.
//

#include "utils.hpp"

#include <string.h>

using namespace orbisFSTool;

bool orbisFSTool::memvcmp(void *memory, size_t size, uint8_t val){
    uint8_t *mm = (uint8_t*)memory;
    return (*mm == val) && memcmp(mm, mm + 1, size - 1) == 0;
}
