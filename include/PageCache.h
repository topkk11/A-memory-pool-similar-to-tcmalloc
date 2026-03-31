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
    //通过页地址找到对应Span
    Span* MapObjectToSpan(void* obj);
    //回收CentralCache返回的内存
    void ReceiveFromCentralCache(Span* span);
    //释放内存池管理不了的Span
    void ReleaseSpan(Span* span);
private:
    PageCache() {}
    PageCache(const PageCache& other) = delete;
    PageCache& operator = (const PageCache& other) = delete;
    static PageCache _sInst;

private:
    SpanList _spanLists[MAX_PAGE_NUM]; //第i个Span里面挂的都是i页内存
    std::unordered_map<PageID, Span*> _idSpanMap;  //通过页号来找到对应的Span
    ObjectPool<Span> _spanPool; //span的对象池
public:
    std::mutex _pageMtx; //PageCache整体锁
};