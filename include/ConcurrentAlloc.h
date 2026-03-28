#pragma once
#include"ThreadCache.h"

void* ConcurrentAlloc(size_t size) {
    if (pTLSThreadCache == nullptr) {
        pTLSThreadCache = new ThreadCache;
    }

    return pTLSThreadCache->Allocate(size);
}

void ConcurrentFree(void* obj,size_t size) {
    assert(obj);
    pTLSThreadCache->Deallocate(obj, size);
}