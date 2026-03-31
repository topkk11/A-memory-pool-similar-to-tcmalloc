#include"../include/ConcurrentAlloc.h"
#include<iostream>
#include<thread>
#include<vector>
#include<chrono>
#include<random>

void TestSingleAlloc() {

    void* ptr = ConcurrentAlloc(1024*1024);
    std::cout << "Allocated: " << ptr << std::endl;
    if (ptr) {
        ConcurrentFree(ptr);
        std::cout << "Freed: " << ptr << std::endl;
    }
}

//单线程小对象分配与释放
void TestSmallAlloc() {
    std::cout << "单线程小对象分配测试" << std::endl;
    std::vector<void*> objects;
    // 申请不同大小的小内存块 (8B, 16B, 128B, 1024B)
    size_t sizes[] = { 8, 16, 128, 1024 };
    
    for (size_t size : sizes) {
        void* ptr = ConcurrentAlloc(size);
        if (ptr) {
            std::cout << "为 " << ptr << " 分配 " << size << "B" << std::endl;
            objects.push_back(ptr);
        }
        
    }
    for (auto ptr : objects) {
        ConcurrentFree(ptr);
    }
}

//单线程大对象分配与释放
void TestLargeAlloc() {
    std::cout << "单线程大对象分配测试" << std::endl;
    // 申请超过 MAX_BYTES (256KB) 的内存，例如 300KB, 1MB ,2MB
    size_t sizes[] = { 300 * 1024, 1024 * 1024 ,1024 * 2048 };
    std::vector<void*> objects;

    for (size_t size : sizes) {
        void* ptr = ConcurrentAlloc(size);
        if (ptr) {
            std::cout << "为" << ptr << "分配" << size / 1024 << "KB" << std::endl;
            objects.push_back(ptr);
        }
    }
    // 释放内存
    for (auto ptr : objects) {
        ConcurrentFree(ptr);
    }
    
    std::cout << std::endl;
}

//多线程并发分配与释放
void MultiThreadStressTest(size_t threadCount = 4, size_t opsPerThread = 10) {
    //threadCount:线程个数，默认为4
    //opsPerThread:每个线程操作次数,默认为1000
    std::cout << "多线程测试" << std::endl;
    std::vector<std::thread> threads;
    std::vector<std::vector<void*>> threadData(threadCount);

    auto worker = [&](int id) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dist(1, 200 * 1024); // 随机大小 1B ~ 200KB

        for (size_t i = 0; i < opsPerThread; ++i) {
            size_t size = dist(gen);
            void* ptr = ConcurrentAlloc(size);
            if (ptr) {
                threadData[id].push_back(ptr);
            }
        }

        // 释放该线程申请的所有内存
        for (auto p : threadData[id]) {
            ConcurrentFree(p);
        }
        std::cout << "线程： " << id << "完成" << std::endl;
    };

    auto start = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < threadCount; ++i) {
        threads.emplace_back(worker, i);
    }

    for (auto& t : threads) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;
    std::cout << "总共用时：" << diff.count() << "s" << std::endl << std::endl;
}

// 触发 Span 合并逻辑
void TestSpanMerge() {
    std::cout << "Span合并逻辑测试" << std::endl;
    std::vector<void*> v;
    // 申请多个刚好占用 1 页或几页的内存块 (假设 8KB 一页，申请 8KB 数据)
    // 具体页数取决于 SizeClass 的对齐规则，这里尝试申请能凑成整页的大小
    size_t pageSize = 1024; 
    int count = 17;

    for (int i = 0; i < count; ++i) {
        void* ptr = ConcurrentAlloc(pageSize);
        v.push_back(ptr);
    }


    // 逆序释放
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        ConcurrentFree(*it);
    }
}

int main() {
    // TestSingleAlloc();
    // TestSmallAlloc();        // 测试小内存分配
    // TestLargeAlloc();        // 测试大内存分配
    //TestSpanMerge();         // 测试 Span 合并逻辑
    MultiThreadStressTest(); // 测试多线程并发安全性
    

    return 0;
}