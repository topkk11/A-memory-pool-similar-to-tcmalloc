#pragma once
#include"ThreadCache.h"
#include"PageCache.h"

void* ConcurrentAlloc(size_t size) {
    // if (size <= MAX_BYTES) {
    //     if (pTLSThreadCache == nullptr) {
    //         //每个线程拥有一个ThreadCache
    //         static ObjectPool<ThreadCache> objPool;

    //         objPool._poolMtx.lock();
    //         pTLSThreadCache = objPool.New();
    //         objPool._poolMtx.unlock();
    //     }
    //     // void* ptr = pTLSThreadCache->Allocate(size);
    //     // std::cout << ptr << " Allocated" << std::endl;
    //     // return ptr;
    //     return pTLSThreadCache->Allocate(size);
    // }
    // else {
    //     size_t alignSize = SizeClass::RoundUp(size); //按页大小对齐
    //     size_t pageNum = alignSize >> PAGE_SHIFT;    //计算页数

    //     PageCache::GetInstance()->_pageMtx.lock();
    //     Span* span = PageCache::GetInstance()->NewSpan(pageNum);
    //     PageCache::GetInstance()->_pageMtx.unlock();

    //     void* ptr = reinterpret_cast<void*>(span->_pageId << PAGE_SHIFT);
    //     std::cout << ptr << " Allocated" << std::endl;
    //     return ptr;
    // }
    if (size > MAX_BYTES) {
        size_t alignSize = SizeClass::RoundUp(size); //按页大小对齐
        size_t pageNum = alignSize >> PAGE_SHIFT;    //计算页数

        PageCache::GetInstance()->_pageMtx.lock();
        Span* span = PageCache::GetInstance()->NewSpan(pageNum);
        PageCache::GetInstance()->_pageMtx.unlock();

        void* ptr = reinterpret_cast<void*>(span->_pageId << PAGE_SHIFT);
        std::cout << ptr << " Allocated" << std::endl;
        return ptr;
    }
    else {
        if (pTLSThreadCache == nullptr) {
            //每个线程拥有一个ThreadCache
            static ObjectPool<ThreadCache> objPool;

            objPool._poolMutex.lock();
            pTLSThreadCache = objPool.New();
            objPool._poolMutex.unlock();
        }
        // void* ptr = pTLSThreadCache->Allocate(size);
        // std::cout << ptr << " Allocated" << std::endl;
        // return ptr;
        return pTLSThreadCache->Allocate(size);
    }
}

void ConcurrentFree(void* obj) {
    assert(obj);
    
    Span* span = PageCache::GetInstance()->MapObjectToSpan(obj);
    
    size_t spanTotalSize = span->_pageNum << PAGE_SHIFT;
    if (spanTotalSize > (MAX_PAGE_NUM - 1) << PAGE_SHIFT) { //当线程单次内存申请超过PageCache最大Span大小，直接调用系统申请
        std::cout << "systemFree" << std::endl;
        SystemFree(obj);
    } 
    else if (spanTotalSize > MAX_BYTES ) {
        PageCache::GetInstance()->_pageMtx.lock();
        PageCache::GetInstance()->ReleaseSpan(span);
        PageCache::GetInstance()->_pageMtx.unlock();
    }
    else {
        size_t size = span->_objSize;   //span中被划分的小块大小
        pTLSThreadCache->Deallocate(obj, size);
    }
    
}