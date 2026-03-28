#include"../include/CentralCache.h"
#include"../include/PageCache.h"
CentralCache CentralCache::_CCinstance;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t blockNum, size_t sizePerBlock) {
    size_t index = SizeClass::Index(sizePerBlock);
    size_t actualNum;
    _spanLists[index]._mutexForBucket.lock();
    Span* span = GetOneSpan(_spanLists[index], sizePerBlock);
    assert(span);
    assert(span->_freeList);
    start = end = span->_freeList;
    actualNum = 1;
    size_t i = 0;
    while (i < blockNum - 1 && Nextobj(end) != nullptr) {
        end = Nextobj(end);
        actualNum++;
        i++;
    }
    span->_freeList = Nextobj(end);
    span->_useCount += actualNum;
    Nextobj(end) = nullptr;
    _spanLists[index]._mutexForBucket.unlock();
    return actualNum;


}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
    //先找CentralCache中有没有空闲的Span
    Span* span = list.Begin();
    while(span != list.End()) {
        if (span->_freeList != nullptr) {
            return span;
        }
        else {
            span = span->_next;
        }
    }
    //如果CentralCache中没有现成的Span，先将桶锁解锁，以便于ThreadCache向CentralCache归还内存
    list._mutexForBucket.unlock();
    
    //从PageCache中获取没有划分过的newSpan
    size_t k = SizeClass::NumMovePage(size);

    //给PageCache整体加锁
    PageCache::GetInstance()->_pageMtx.lock();
    Span* newSpan = PageCache::GetInstance()->NewSpan(k);
    PageCache::GetInstance()->_pageMtx.unlock();
    
    //划分newSpan使其挂到对应的_freelist上    
    //定义为char*方便移动指针
    char* start = reinterpret_cast<char*>(newSpan->_pageId << PAGE_SHIFT);
    char* end = reinterpret_cast<char*>(start + (newSpan->_pageNum << PAGE_SHIFT));
    newSpan->_freeList = start;
    void* tail = reinterpret_cast<void*>(start);
    //先将start向后移动一个块大小 方便控制循环
    start += size;
    while (start < end) {
        Nextobj(tail) = reinterpret_cast<void*>(start);
        start += size;
        tail = Nextobj(tail);
    }

    //将划分好的newSpan加入到CentralCache中的Spanlist中
    list._mutexForBucket.lock();
    list.PushFront(newSpan);

    return newSpan;
}