#pragma once
#include <stdint.h>

static void* arena_alloc__noabc(void** arena, size_t size) {
    void* result = *arena;
    *arena = ((char*) *arena) + size;
    return result;
}
