#pragma once
#include"Common.h"

class PageCache {
public:
    //饿汉单例
    static PageCache* GetInstance() {
        return &_sInst;
    }

    //从PageCache中拿出一个k页的span
    Span* NewSpan(size_t k);
private:
    PageCache() {}
    PageCache(const PageCache& other) = delete;
    PageCache& operator = (const PageCache& other) = delete;
    static PageCache _sInst;

private:
    SpanList _spanLists[MAX_PAGE_NUM]; //第i个Span里面挂的都是i页内存
public:
    std::mutex _pageMtx; //PageCache整体锁
};