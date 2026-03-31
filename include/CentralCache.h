#pragma once
#include"Common.h"

class CentralCache {
public:
    static CentralCache* GetInstance(){
        return &_CCinstance;
    }
    //返回CentralCache实际提供给ThreadCache的内存大小
    size_t FetchRangeObj(void*& start, void*& end, size_t blockNum, size_t sizePerBlock);
    //获取一个Span
    Span* GetOneSpan(SpanList& list, size_t size);
    //回收ThreadCache返回的内存
    void ReceiveFromThreadCache(void* start, size_t size);

private:
    CentralCache() {}

    CentralCache(const CentralCache& other) = delete;
    CentralCache& operator = (const CentralCache& other) = delete;
private:
    SpanList _spanLists[FREE_LIST_NUM];
    static CentralCache _CCinstance;
};