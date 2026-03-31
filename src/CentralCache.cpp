#include"../include/CentralCache.h"
#include"../include/PageCache.h"
CentralCache CentralCache::_CCinstance;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t blockNum, size_t sizePerBlock) {
    //start为这块内存的开始，end为这块内存的结束
    //blockNum是tc申请的内存块数
    //sizePerBlock是tc需要的单块内存大小
    size_t index = SizeClass::Index(sizePerBlock);

    _spanLists[index]._mutexForBucket.lock();
    Span* span = GetOneSpan(_spanLists[index], sizePerBlock);
    assert(span);
    assert(span->_freeList);
    start = end = span->_freeList;
    size_t actualNum = 1;
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
    //接着向PageCache申请内存，并将申请到的Span内部进行划分
    list._mutexForBucket.unlock();
    
    //从PageCache中获取没有划分过的newSpan
    size_t k = SizeClass::NumMovePage(size);

    //给PageCache整体加锁
    PageCache::GetInstance()->_pageMutex.lock();
    Span* newSpan = PageCache::GetInstance()->NewSpan(k);
    newSpan->_isUse = true;
    
    PageCache::GetInstance()->_pageMutex.unlock();
    
    //划分newSpan使其挂到对应的_freelist上    
    //定义为char*方便移动指针
    char* start = reinterpret_cast<char*>(newSpan->_pageId << PAGE_SHIFT);
    char* end = reinterpret_cast<char*>(start + (newSpan->_pageNum << PAGE_SHIFT));
    newSpan->_objSize = size;
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

void CentralCache::ReceiveFromThreadCache(void* start, size_t size) {
    //ThreadCache返回的内存空间可能不是仅仅一个Span里的
    size_t index = SizeClass::Index(size);

    _spanLists[index]._mutexForBucket.lock();

    //遍历一个线程返回的内存块，将其放入对应页的Span管理的_freeLists中
    while (start) {
        void* next = Nextobj(start);
        //找到其对应的span
        Span* span = PageCache::GetInstance()->MapObjectToSpan(start);
        //将当前内存块插入到span中,插入后Span的顺序不确定
        Nextobj(start) = span->_freeList;
        span->_freeList = start;
        //更新_useCount
        span->_useCount--;
        if (span->_useCount == 0) {
            //代表当前span管理的所有内存块都回到CentralCache中，则交给PageCache管理
            //将span从CentralCache中分离出来
            _spanLists[index].Erase(span);
            span->_freeList = nullptr;
            span->_next = nullptr;
            span->_prev = nullptr;
            //向PageCache归还内存之前先解锁桶锁
            _spanLists[index]._mutexForBucket.unlock();
            //归还时给PageCache整体上锁
            PageCache::GetInstance()->_pageMutex.lock();
            PageCache::GetInstance()->ReceiveFromCentralCache(span);
            PageCache::GetInstance()->_pageMutex.unlock();
            //归还内存之后重新给桶锁上锁
            _spanLists[index]._mutexForBucket.lock();
        }
        //更新为下一个内存块
        start = next;
    }

    _spanLists[index]._mutexForBucket.unlock();
}