#pragma once
#include <stdint.h>

#define assert(condition) ({\
    if (!(condition)) {\
        __assert_failed(__FILE__, __LINE__, __func__, #condition);\
    }\
    })

#define unimplemented ({\
    __stub(__FILE__, __LINE__, __func__);\
    })

void __assert_failed(const char *file, uint32_t line, const char *func, const char *cond);
void __stub(const char *file, uint32_t line, const char *func);