#pragma once
#include<iostream>
#include<mutex>

#ifdef _WIN32 
#include<Windows.h>
#else
//linux
#endif

static const size_t PAGE_SHIFT = 13;

inline static void*& Nextobj(void* obj) {
    return *reinterpret_cast<void**>(obj);
}
inline static void* SystemAlloc(size_t kpage) {
#ifdef _WIN32
    void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else

#endif
    if (ptr == nullptr) {
        throw std::bad_alloc();
    }
    return ptr;
}

inline static void SystemFree(void* ptr) {
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else

#endif
}


template<class T>
class ObjectPool {
public:
    T* New() {
        T* obj = nullptr;
        //优先使用freelist中的内存
        if (_freeList) {
            //从头部取出
            void* next = *(void**)_freeList;
            obj = (T*)_freeList;
            _freeList = next;
        }else {
            if (_restBytes < sizeof(T)) {
                _restBytes = 128 * 1024;
                _blockPtr = (char*)SystemAlloc(_restBytes>>PAGE_SHIFT);
                if (_blockPtr == nullptr) {
                    throw std::bad_alloc();
                }
            }
            obj = (T*)_blockPtr;
            //将T的大小调整至至少一个指针大小
            size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
            _blockPtr += objSize;
            _restBytes -= objSize;
        }
        //以上只是为T分配了空间
        //调用T的构造函数
        obj = (T*)_blockPtr;
        new(obj)T;
        return obj;
    }

    void Delete(T* obj) {
        if (obj == nullptr)return;
        //调用T的析构函数
        obj->~T();
        
        *(void**)obj = _freeList;
        _freeList = obj;
        
    }
private:
    char* _blockPtr = nullptr;  //指向内存池的指针
    size_t _restBytes = 0;
    void* _freeList = nullptr;

public:
    std::mutex _poolMtx;
};