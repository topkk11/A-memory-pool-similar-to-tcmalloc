#pragma once

#include<iostream>
#include<vector>
#include<cassert>
#include<thread>
#include<mutex>
#ifdef _WIN32
    #include<Windows.h>
#else
#endif


typedef size_t PageID;

static const size_t FREE_LIST_NUM = 208;    //最大自由链表个数
static const size_t MAX_BYTES = 256 * 1024; //ThreadCache单次申请最大字节数
static const size_t MAX_PAGE_NUM = 129; //PageCache管理的最大页数
static const size_t PAGE_SHIFT = 13; //页的大小 2的13次方 8KB


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

struct Span {
    PageID _pageId = 0; //页号
    size_t _pageNum = 0;//页数

    Span* _next = nullptr;
    Span* _prev = nullptr;

    void* _freeList = nullptr;
    size_t _useCount = 0;
};

class SpanList {
public:
    std::mutex _mutexForBucket;
    SpanList() {
        _head = new Span;
        _head->_next = _head;
        _head->_prev = _head;
    }
    bool Empty() {
        return _head == _head->_next;
    }
    Span* Begin() {
        return _head->_next;
    }

    Span* End(){
        return _head;
    }
    void PushFront(Span* span) {
        //头插
        Insert(Begin(), span);
    }
    Span* PopFront() {
        Span* front = _head->_next;
        Erase(front);
        return front;
    }
    void Insert(Span* pos, Span* ptr) {
        //在pos前插入ptr
        assert(pos);
        assert(ptr);

        Span* prev = pos->_prev;
        ptr->_prev = prev;
        prev->_next = ptr;

        ptr->_next = pos;
        pos->_prev = ptr;
    }

    void Erase(Span* pos) {
        assert(pos);
        assert(pos != _head);
        Span* prev = pos->_prev;
        Span* next = pos->_next;

        prev->_next = next;
        next->_prev = prev;

        //TODO:pos回收逻辑
    }
private:
    Span* _head = nullptr;
};

class FreeList {
public:
    bool isEmpty() {
        return _freeList == nullptr;
    }
    void Push(void* obj) {
        assert(obj);
        //头插法
        Nextobj(obj) = _freeList;
        _freeList = obj;
    }

    void PushRange(void* start, void* end) {
        assert(start);
        assert(end);
        Nextobj(end) = nullptr;
        _freeList = start;
    }
    void* Pop() {
        assert(_freeList);
        //从头部取出
        void* obj = _freeList;
        _freeList = Nextobj(obj);
        return obj;
    }

    size_t& MaxSize(){
        return _maxSize;
    }
    
private:
    void* _freeList = nullptr;
    size_t _maxSize = 1;
};

class SizeClass {
public:
    static size_t NumMovePage(size_t size) {
        //计算申请内存的最大大小
        size_t num = NumMoveSize(size);
        size_t npage = num * size;

        //计算有多少页 相当于npage/=8KB
        npage >>= PAGE_SHIFT;

        if (npage == 0) {
            npage = 1;
        }
        return npage;
    }
    static size_t NumMoveSize(size_t size) {
        //计算申请内存块块数上限
        assert(size > 0);
        int num = MAX_BYTES / size;
        if (num > 512) {
            num = 512;
        }
        if (num < 2) {
            num = 2;
        }
        return num;
    }
    static inline size_t Index(size_t size) {
        assert(size < MAX_BYTES);
        //前四个对齐字节的链表个数
        static int list_array[4] = { 16,56,56,56 };
        if (size <= 128) {
            //[1,128] 8B 
            return _Index(size, 8);
        }
        else if (size <= 1024) {
            //[128+1,1024] 16B 
            return _Index(size, 16) + list_array[0];
        }
        else if (size <= 1024*8) {
            //[1024+1,8*1024] 128B
            return _Index(size, 128) + list_array[0] + list_array[1];
        }
        else if (size <= 1024*64) {
            //[8*1024+1,8*8*1024] 1024B   
            return _Index(size, 1024) + list_array[0] + list_array[1] + list_array[2];
        }
        else if (size <= 1024*256) {
            //[8*8*1024+1,8*8*8*1024] 8*1024B
            return _Index(size, 8 * 1024) + list_array[0] + list_array[1] + list_array[2] + list_array[3];
        }
        else {
            assert(false);
            return -1;
        }
    }

    //计算对齐字节数
    static size_t RoundUp(size_t size) {
        if (size <= 128) {
            //[1,128] 8B 
            return _RoundUp(size, 8);
        }
        else if (size <= 1024) {
            //[128+1,1024] 16B 
            return _RoundUp(size, 16);
        }
        else if (size <= 1024*8) {
            //[1024+1,8*1024] 128B
            return _RoundUp(size, 128);
        }
        else if (size <= 1024*64) {
            //[8*1024+1,8*8*1024] 1024B   
            return _RoundUp(size, 1024);
        }
        else if (size <= 1024*256) {
            //[8*8*1024+1,8*8*8*1024] 8*1024B
            return _RoundUp(size, 8*1024);
        }
        else {
            assert(false);
            return -1;
        }
    }
private:
    static inline size_t _Index(size_t size ,size_t align_shift) {
        //计算每个链表在哈希表中的相对位置
        return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
    }

    static size_t _RoundUp(size_t size, size_t alignment) {
        //size:传入的字节大小
        //alignment：需要对齐的字节大小
        return (size + alignment + 1) & ~(alignment - 1);
    
    }
};

