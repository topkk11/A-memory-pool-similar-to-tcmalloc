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

    size_t index = SizeClass::Index(size);
    _freeLists[index].Push(obj);

    //TODO：
    //如果threadCache中空闲空间过多，归还给CentralCache
    
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize) {
    //慢开始反馈调节算法
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
        _freeLists[index].PushRange(Nextobj(start), end);
        return start;

    }

}