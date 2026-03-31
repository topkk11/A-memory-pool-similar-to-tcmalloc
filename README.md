# TCMalloc 内存池

基于 Google tcmalloc 思想实现的自定义高性能内存池，旨在解决传统 `malloc/free` 在多线程高并发场景下的性能瓶颈和内存碎片问题。

## 📖 项目简介

本项目实现了一个三级缓存架构的内存池系统，通过线程本地缓存、中心缓存和页缓存的协同工作，提供高效的内存分配与释放机制。

### 核心特性

- ✅ **三级缓存架构**：ThreadCache → CentralCache → PageCache 分层管理
- ✅ **线程安全**：支持多线程并发访问，锁粒度优化
- ✅ **内存对齐**：智能对齐策略，减少内存碎片
- ✅ **对象池复用**：Span 对象池化，降低元数据开销
- ✅ **自动合并**：Span 自动合并相邻空闲页，提高内存利用率

## 🏗️ 系统架构

```
Application
    ↓
ConcurrentAlloc (统一分配接口)
    ↓
ThreadCache (线程本地缓存) ←→ CentralCache (中心缓存) ←→ PageCache (页缓存)
                                                      ↓
                                                   OS (操作系统)
```

### 组件说明

| 组件 | 职责 | 锁策略 |
|------|------|--------|
| **ConcurrentAlloc** | 统一对外分配接口，根据内存大小路由请求 | 无锁 |
| **ThreadCache** | 每个线程独立的本地缓存，无锁分配小对象 | 无需锁（线程私有） |
| **CentralCache** | 跨线程共享的中心缓存，处理线程间内存平衡 | 桶级锁（208 个哈希桶） |
| **PageCache** | 管理物理内存页，负责与操作系统交互 | 全局锁 |

## 🔧 技术规格

### 环境要求

- **编译器**: 支持 C++11 标准（GCC / Clang / MSVC）
- **构建工具**: CMake 3.10+
- **操作系统**: Windows （需特定配置)

### 内存管理参数

| 参数 | 值 | 说明 |
|------|-----|------|
| `MAX_BYTES` | 256 KB | ThreadCache 管理的最大内存 |
| `MAX_PAGE_NUM` | 129 | PageCache 管理的最大页数 |
| `FREE_LIST_NUM` | 208 | 自由链表数量（哈希桶个数） |
| `PAGE_SHIFT` | 13 | 页大小位移（8KB = 2^13） |

### 内存对齐策略

| 内存范围 | 对齐字节 | 哈希桶下标 |
|----------|----------|------------|
| [1, 128B] | 8B | [0, 16) |
| [128B+1, 1KB] | 16B | [16, 72) |
| [1KB+1, 8KB] | 128B | [72, 128) |
| [8KB+1, 64KB] | 1024B | [128, 184) |
| [64KB+1, 256KB] | 8192B | [184, 208) |

## 📦 安装与构建


### 1. 创建构建目录

```bash
mkdir build
cd build
```

### 3. 配置项目

```bash
cmake ..
```

### 4. 编译项目

```bash
make
```

### 5. 运行测试

```bash
./111
```

## 🧪 测试用例

项目内置了完整的单元测试，涵盖以下场景：

### 测试功能

1. **单线程分配测试** (`TestSingleAlloc`)
   - 测试大内存（1MB）的分配与释放

2. **小内存测试** (`TestSmallAlloc`)
   - 测试不同大小的小内存块（8B, 16B, 128B, 1024B）

3. **大内存测试** (`TestLargeAlloc`)
   - 测试超过 256KB 的大内存（300KB, 1MB, 2MB）

4. **多线程压力测试** (`MultiThreadStressTest`)
   - 默认 4 线程，每线程 10 次随机大小分配（1B ~ 200KB）
   - 统计总耗时，验证并发性能

5. **Span 合并测试** (`TestSpanMerge`)
   - 测试连续分配后逆序释放，验证 Span 自动合并逻辑

### 运行特定测试

编辑 `tests/UnitTest.cpp` 的 `main()` 函数，取消注释对应测试：

```cpp
int main() {
    TestSingleAlloc();      // 单线程大内存测试
    TestSmallAlloc();       // 小内存测试
    TestLargeAlloc();       // 大内存测试
    TestSpanMerge();        // Span 合并测试
    MultiThreadStressTest();// 多线程压力测试（默认开启）
    return 0;
}
```

## 💡 使用示例

### 基本用法

```cpp
#include "ConcurrentAlloc.h"

// 分配内存
void* ptr = ConcurrentAlloc(1024);  // 分配 1KB

// 使用内存
// ... 你的代码 ...

// 释放内存
ConcurrentFree(ptr);
```

### 多线程示例

```cpp
#include "ConcurrentAlloc.h"
#include <thread>
#include <vector>

void worker(int id) {
    // 每个线程独立分配内存
    void* ptr = ConcurrentAlloc(256);
    // 使用内存...
    ConcurrentFree(ptr);
}

int main() {
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back(worker, i);
    }
    for (auto& t : threads) {
        t.join();
    }
    return 0;
}
```

## 🔍 核心实现细节

### 1. 内存分配流程

**小对象（≤ 256KB）:**
1. 检查 ThreadCache 对应哈希桶是否有空闲块
2. 若有，直接返回（无锁操作）
3. 若无，向 CentralCache 批量申请（默认 2~512 块）
4. CentralCache 若无空闲 Span，向 PageCache 申请新页

**大对象（> 256KB）:**
1. 直接向 PageCache 申请整数页
2. PageCache 通过 `SystemAlloc` 向操作系统申请
3. 返回页起始地址

### 2. 内存释放流程

**小对象:**
1. 归还到 ThreadCache 的自由链表
2. 若自由链表块数超过阈值，批量归还给 CentralCache
3. CentralCache 回收 Span 的所有块时，归还给 PageCache

**大对象:**
1. 直接归还给 PageCache
2. PageCache 尝试合并相邻空闲 Span
3. 超过 128 页的 Span 直接释放给操作系统

### 3. Span 管理机制

```cpp
struct Span {
    PageID _pageId;          // 起始页号
    size_t _pageNum;         // 页数
    size_t _objSize;         // 小块大小
    void* _freeList;         // 空闲链表头指针
    size_t _useCount;        // 已使用块数
    bool _isUse;             // 是否在 CentralCache 中
};
```

- **在 PageCache 中**: `_isUse = false`，可与其他 Span 合并
- **在 CentralCache 中**: `_isUse = true`，禁止合并

### 4. 锁优化策略

- **ThreadCache**: 线程本地存储，无需加锁
- **CentralCache**: 208 个哈希桶，每个桶独立自旋锁
- **PageCache**: 全局互斥锁，但锁持有时间最小化

## 📊 性能优势

相比传统 `malloc/free`：

| 场景 | 传统 malloc | TCMalloc |
|------|-------------|----------|
| 小对象分配 | 全局锁竞争 | 线程本地无锁 |
| 大对象分配 | 系统调用频繁 | 页缓存复用 |
| 内存碎片 | 外部碎片严重 | 自动合并 Span |
| 多线程并发 | 性能下降明显 | 线性扩展良好 |

## ⚠️ 注意事项

### 已知限制

1. **Windows 支持**: Windows 需额外配置，目前未实现linux系统下的支持
2. **内存耗尽处理**: 当前通过 `std::bad_alloc` 异常处理
3. **大内存阈值**: 256KB 为界，可根据实际需求调整 `MAX_BYTES`


## 📝 开发计划

- [ ] 增加性能基准测试脚本
- [ ] 支持 Windows 原生 API
- [ ] 添加内存统计监控接口
- [ ] 优化大内存分配策略
- [ ] 增加自定义分配器接口

## 🤝 贡献指南

欢迎提交 Issue 和 Pull Request！

## 📄 许可证

本项目参考 Google tcmalloc 设计思想以及https://gitee.com/yjy_fangzhang/memory-pool-project/blob/master/ConcurrentMemoryPool/ConcurrentMemoryPool/PageCache.cpp，仅供学习研究使用。

## 📧 联系方式

如有问题或建议，请提交 Issue。

---

**最后更新**: 2026-03-31
