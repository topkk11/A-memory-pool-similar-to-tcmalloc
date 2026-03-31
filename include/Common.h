#pragma once

#include"MemoryPool.h"
#include<iostream>
#include<vector>
#include<cassert>
#include<thread>
#include<mutex>
#include<unordered_map>

typedef size_t PageID;

static const size_t FREE_LIST_NUM = 208;    //最大自由链表个数
static const size_t MAX_BYTES = 256 * 1024; //ThreadCache单次申请最大字节数
static const size_t MAX_PAGE_NUM = 129; //PageCache管理的最大页数


struct Span {
    PageID _pageId = 0; //页号
    size_t _pageNum = 0;//页数

    size_t _objSize = 0; //span管理页被划分的块大小

    Span* _next = nullptr;
    Span* _prev = nullptr;

    void* _freeList = nullptr;
    size_t _useCount = 0;

    bool _isUse = false;//判断Span的位置  false在PageCache中 true在CentralCache中  方便PageCache合并相邻的Span
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
        _size++;
    }
    void* Pop() {
        assert(_freeList);
        //从头部取出
        void* obj = _freeList;
        _freeList = Nextobj(obj);
        _size--;
        return obj;
    }

    void PushRange(void* start, void* end,size_t size) {
        assert(start);
        assert(end);
        Nextobj(end) = nullptr;
        _freeList = start;

        _size += size;
    }
    void PopRange(void*& start, void*& end,size_t n) {
        assert(n <= _size);
        start = end = _freeList;
        for (size_t i = 0;i < n - 1;i++) {
            end = Nextobj(end);
        }
        _freeList = Nextobj(end);
        Nextobj(end) = nullptr;
        _size -= n;
    }

    size_t& MaxSize(){
        return _maxSize;
    }
    size_t Size(){
        return _size;
    }
private:
    void* _freeList = nullptr;
    size_t _maxSize = 1;  //当前自由链表申请未达到上限时，能够申请的内存块的最大数量
    size_t  _size = 0;     //当前自由链表有多少块内存块
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
            return _Index(size, 3);
        }
        else if (size <= 1024) {
            //[128+1,1024] 16B 
            return _Index(size - 128, 4) + list_array[0];
        }
        else if (size <= 1024*8) {
            //[1024+1,8*1024] 128B
            return _Index(size - 1024, 7) + list_array[0] + list_array[1];
        }
        else if (size <= 1024*64) {
            //[8*1024+1,8*8*1024] 1024B   
            return _Index(size - 8 * 1024, 10) + list_array[0] + list_array[1] + list_array[2];
        }
        else if (size <= 1024*256) {
            //[8*8*1024+1,8*8*8*1024] 8*1024B
            return _Index(size - 64 * 1024, PAGE_SHIFT) + list_array[0] + list_array[1] + list_array[2] + list_array[3];
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
            //单次申请空间大于256KB，直接按页对齐
            return _RoundUp(size, 1 << PAGE_SHIFT);
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
        return (size + alignment - 1) & ~(alignment - 1);
    
    }
};

