#include"../include/ThreadCache.h"
#include"../include/CentralCache.h"

void* ThreadCache::Allocate(size_t size) {
    assert(size <= MAX_BYTES); //申请的大小不能超过MAX_BYTES
    size_t index = SizeClass::Index(size);
    size_t alignSize = SizeClass::RoundUp(size);
    //从哈希表中对应的自由链表里取出
    if (!_freeLists[index].isEmpty()) {
        return _freeLists[index].Pop();
    }
    else {
        //如果对应自由链表为空则向CentralCache申请
        return FetchFromCentralCache(index, alignSize);
    }
}

void ThreadCache::Deallocate(void* obj, size_t size) {
    assert(obj); //obj不能为空
    assert(size <= MAX_BYTES); //释放的大小不能超过MAX_BYTES
    // std::cout << "underneath 256KB deallocate:" << obj << std::endl;
    size_t index = SizeClass::Index(size);
    _freeLists[index].Push(obj);
    
    //如果threadCache中空闲块数量大于单批次申请块最大值，归还给CentralCache
    if (_freeLists[index].Size() >= _freeLists[index].MaxSize()) {
        ReturnToCentralCache(_freeLists[index], size);
    }

}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize) {
    //慢开始反馈调节算法
    // size_t nummovesize = SizeClass::NumMoveSize(alignSize);
    // size_t maxsize = _freeLists[index].MaxSize();
    size_t blockNum = std::min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));
    if (blockNum == _freeLists[index].MaxSize()) {
        _freeLists[index].MaxSize()++;
    }

    void* start = nullptr;
    void* end = nullptr;

    size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, blockNum, alignSize);

    assert(actualNum >= 1);
    if (actualNum == 1) {
        assert(start == end);
        return start;
    }
    else {
        _freeLists[index].PushRange(Nextobj(start), end, actualNum - 1);
        return start;

    }

}

void ThreadCache::ReturnToCentralCache(FreeList& list, size_t size) {
    void* start = nullptr;
    void* end = nullptr;
    list.PopRange(start, end, list.MaxSize());

    //向CentralCache归还内存
    CentralCache::GetInstance()->ReceiveFromThreadCache(start, size);
}
