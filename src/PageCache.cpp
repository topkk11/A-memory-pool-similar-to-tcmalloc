#include"../include/PageCache.h"

PageCache PageCache::_sInst;

Span* PageCache::NewSpan(size_t k) {
    assert(k > 0);
    //如果申请的页数超过128页
    if (k > MAX_PAGE_NUM - 1) {
		void* ptr = SystemAlloc(k); 
		Span* span = _spanPool.New(); 
		
		span->_pageId = ((PageID)ptr >> PAGE_SHIFT);
		span->_pageNum = k; 
		

		_idSpanMap[span->_pageId] = span;

		return span;
	}

    //1._SpanLists[k]中有Span
    if (!_spanLists[k].Empty()) {
        Span* span = _spanLists[k].PopFront();
        //记录span管理的每一页的页号和其地址的映射
        for (PageID i = 0; i < span->_pageNum;i++) {
            _idSpanMap[span->_pageId + i] = span;
        }
        return span;
    }
    //2._SpanLists[k]中没有Span，但后面的有
    //将第i个span（iSpan）划分成第k个span 和 第i-k个span
    for (int i = k + 1;i < MAX_PAGE_NUM;i++) {
        if (!_spanLists[i].Empty()) {
            Span* iSpan = _spanLists[i].PopFront();
            //划分到_SpanLists[k]的Span，需要新建空间
            Span* kSpan = _spanPool.New();
            //分出前k页的span
            kSpan->_pageId = iSpan->_pageId;
            kSpan->_pageNum = k;
            //i-k页的span
            iSpan->_pageId += k;
            iSpan->_pageNum -= k;
            //将i-k页的span放回对应哈希桶中
            _spanLists[iSpan->_pageNum].PushFront(iSpan);

            //记录i-k页span的边缘页(第一页和最后一页)的页号和其地址的映射
            _idSpanMap[iSpan->_pageId] = iSpan;
            _idSpanMap[iSpan->_pageId + iSpan->_pageNum - 1] = iSpan;


            //记录kSpan管理的每一页的页号和其地址的映射
            for (PageID i = 0; i < kSpan->_pageNum;i++) {
            _idSpanMap[kSpan->_pageId + i] = kSpan;
        }
            return kSpan;
        }
    }
    //3._SpanLists[]中都没有Span
    //申请128页内存
    void* ptr = SystemAlloc(MAX_PAGE_NUM - 1);
    Span* bigSpan = _spanPool.New();
    bigSpan->_pageId = ((PageID)ptr) >> PAGE_SHIFT;
    bigSpan->_pageNum = MAX_PAGE_NUM - 1;

    _spanLists[MAX_PAGE_NUM - 1].PushFront(bigSpan);

    return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
   
    //通过地址找到页号
    PageID id = (reinterpret_cast<PageID>(obj) >> PAGE_SHIFT);
    std::unique_lock<std::mutex> lock(_pageMutex);
    auto res = _idSpanMap.find(id);
    if (res != _idSpanMap.end()) {
        return res->second;
    }
    else {
        assert(false);
        return nullptr;
    }
}
//管理归还回来的Span
void PageCache::ReceiveFromCentralCache(Span* span) {
    //合并规则：
    //1.没有相邻Span 停止合并
    //2.相邻Span在CentralCache中 停止合并
    //3.相邻Span和当前Span合并后超过128页 停止合并
    //4.当前Span和相邻Span合并
    //向左不断合并
    std::cout << "cur span: " << span 
                  << ", pageId: " << (size_t)span->_pageId 
                  << ", pageNum: " << (size_t)span->_pageNum << std::endl;
    while (1) {
        PageID leftID = span->_pageId - 1;
        auto res = _idSpanMap.find(leftID);

        if (res == _idSpanMap.end()) {
            std::cout << "left break" << std::endl;
            break;
        }

        Span* leftSpan = res->second;
        if (leftSpan->_isUse == true) {
            std::cout << "left break" << std::endl;
            break;
        }
        if (leftSpan->_pageNum + span->_pageNum > MAX_PAGE_NUM - 1) {
            std::cout << "left break" << std::endl;
            break;
        }

        span->_pageId = leftSpan->_pageId;
        span->_pageNum += leftSpan->_pageNum;

        std::cout << "Merging left span: " << leftSpan 
                  << ", pageId: " << (size_t)leftSpan->_pageId 
                  << ", pageNum: " << (size_t)leftSpan->_pageNum << std::endl;

        // // 清理被合并的leftSpan的所有页面映射
        // for (PageID i = 0; i < leftSpan->_pageNum; i++) {
        //     _idSpanMap.erase(leftSpan->_pageId + i);
        // }

        _spanLists[leftSpan->_pageNum].Erase(leftSpan);
        _spanPool.Delete(leftSpan);
        // delete(leftSpan);
    }
    //向右不断合并
    while (1) {
        PageID rightID = span->_pageId + span->_pageNum;
        auto res = _idSpanMap.find(rightID);

        if (res == _idSpanMap.end()) {
            std::cout << "right break" << std::endl;
            break;
        }
        

        Span* rightSpan = res->second;
        if (rightSpan->_isUse == true) {
            std::cout << "right break" << std::endl;
            break;
        }
        if (rightSpan->_pageNum + span->_pageNum > MAX_PAGE_NUM - 1) {
            std::cout << "right break" << std::endl;
            break;
        }

        std::cout << "Merging right span: " << rightSpan 
                  << ", pageId: " << (size_t)rightSpan->_pageId 
                  << ", pageNum: " << (size_t)rightSpan->_pageNum << std::endl;

        span->_pageNum += rightSpan->_pageNum;

        // // 清理被合并的rightSpan的所有页面映射
        // for (PageID i = 0; i < rightSpan->_pageNum; i++) {
        //     _idSpanMap.erase(rightSpan->_pageId + i);
        // }

        _spanLists[rightSpan->_pageNum].Erase(rightSpan);
        _spanPool.Delete(rightSpan);
    }
    //将合并好的Span挂在对应桶中
    _spanLists[span->_pageNum].PushFront(span);
    //更新span边缘页的地址映射
    _idSpanMap[span->_pageId] = span;
    _idSpanMap[span->_pageId + span->_pageNum - 1] = span;

    std::cout << "Final span: " << span 
              << ", pageId: " << (size_t)span->_pageId 
              << ", pageNum: " << (size_t)span->_pageNum << std::endl;
}

void PageCache::ReleaseSpan(Span* span) {
    assert(span->_pageNum <= MAX_PAGE_NUM - 1);
    std::cout <<span<< " span releseed " << std::endl;
    //要释放的内存地址
    void* ptr = reinterpret_cast<void*>(span->_pageId << PAGE_SHIFT);
    // //清理_idSpanMap中span对应的所有页号映射
    // for (PageID i = 0; i < span->_pageNum; i++) {
    //     _idSpanMap.erase(span->_pageId + i);
    // }

    SystemFree(ptr);
    _spanPool.Delete(span);

}