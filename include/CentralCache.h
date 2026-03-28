#pragma once
#include"Common.h"

class CentralCache {
public:
    static CentralCache* GetInstance(){
        return &_CCinstance;
    }
    size_t FetchRangeObj(void*& start, void*& end, size_t blockNum, size_t sizePerBlock);
        //返回CC实际提供给tc的内存大小
        //start为这块内存的开始，end为这块内存的结束
        //blockNum是tc申请的内存块数
        //sizePerBlock是tc需要的单块内存大小
    Span* GetOneSpan(SpanList& list, size_t size);

private:
    CentralCache() {}

    CentralCache(const CentralCache& other) = delete;
    CentralCache& operator = (const CentralCache& other) = delete;
private:
    SpanList _spanLists[FREE_LIST_NUM];
    static CentralCache _CCinstance;
};