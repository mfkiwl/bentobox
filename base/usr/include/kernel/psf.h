#pragma once
#include <stdint.h>

struct psf1_header {
    uint8_t magic[2];
    uint8_t mode;
    uint8_t glyph_size;
};