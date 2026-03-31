#pragma once
#include"Common.h"

class ThreadCache {
public:
    //线程申请size大小空间
    void* Allocate(size_t size);
    //回收线程size大小空间
    void Deallocate(void* obj, size_t size);
    //当ThreadCache中Freelist为空时向CentralCache申请内存
    void* FetchFromCentralCache(size_t index, size_t alignSize);
    //当ThreadCache中空闲块太多时向CentralCache归还内存
    void ReturnToCentralCache(FreeList& list, size_t size);
private:
    FreeList _freeLists[FREE_LIST_NUM] ;
    //size范围                    对齐字节                哈希桶下标范围
    //[1,128]                     8B                     [0,16)   
    //[128+1,1024]                16B                    [16,72) 
    //[1024+1,8*1024]             128B                   [72,128) 
    //[8*1024+1,8*8*1024]         1024B                  [128,184) 
    //[8*8*1024+1,8*8*8*1024]     8*1024B                [184,208) 
};


#ifdef _MSC_VER
    // Microsoft Visual C++
    #define THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__)
    // GCC/MinGW
    #define THREAD_LOCAL __thread
#else
    // C++11 标准关键字
    #define THREAD_LOCAL thread_local
#endif
//每个线程拥有一个独立的全局对象
static THREAD_LOCAL ThreadCache* pTLSThreadCache = nullptr;