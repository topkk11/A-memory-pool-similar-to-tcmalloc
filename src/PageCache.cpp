#include"../include/PageCache.h"

PageCache PageCache::_sInst;

Span* PageCache::NewSpan(size_t k) {
    assert(k > 0 && k < MAX_PAGE_NUM);
    //1._SpanLists[k]中有Span
    if (!_spanLists[k].Empty()) {
        return _spanLists[k].PopFront();
    }
    //2._SpanLists[k]中没有Span，但后面的有
    for (int i = k + 1;i < MAX_PAGE_NUM;i++) {
        if (!_spanLists[i].Empty()) {
            Span* iSpan = _spanLists[i].PopFront();
            //划分到_SpanLists[k]的Span，需要新建空间
            Span* kSpan = new Span;

            kSpan->_pageId = iSpan->_pageId;
            kSpan->_pageNum = k;

            iSpan->_pageId += k;
            iSpan->_pageNum -= k;

            _spanLists[iSpan->_pageNum].PushFront(iSpan);
            return kSpan;
        }
    }
    //3._SpanLists[]中都没有Span
    //申请128页内存
    void* ptr = SystemAlloc(MAX_PAGE_NUM - 1);
    Span* bigSpan = new Span;
    bigSpan->_pageId = ((PageID)ptr) >> PAGE_SHIFT;
    bigSpan->_pageNum = MAX_PAGE_NUM - 1;

    _spanLists[MAX_PAGE_NUM - 1].PushFront(bigSpan);

    return NewSpan(k);
}