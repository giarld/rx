# Rx

A C++ ReactiveX library built on the design concepts of RxJava

## 简介

`rx` 是一个基于 RxJava 设计理念实现的 C++ ReactiveX 库，提供了强大的异步编程和事件流处理能力。它采用 C++20 标准，支持链式调用、操作符组合和多种调度器，让异步编程变得更加优雅和高效。

## 依赖

本项目依赖以下库，通过 CMake 自动拉取：

- [gany](https://github.com/giarld/gany) - 通用类型系统
- [gx](https://github.com/giarld/gx) - 基础工具库

## 构建

### 要求

- CMake 3.20+
- C++20 兼容的编译器
- Git

## 快速开始

### 基础示例

```cpp
#define USE_GANY_CORE
#include <gx/gany.h>
#include <rx/rx.h>

using namespace rx;

int main() {
    initGAnyCore();
    
    // 创建一个简单的 Observable
    Observable::just(1, 2, 3, 4, 5)
        ->map([](const GAny &x) {
            return x.toInt32() * 2;
        })
        ->filter([](const GAny &x) {
            return x.toInt32() % 4 == 0;
        })
        ->subscribe([](const GAny &v) {
            std::cout << "Value: " << v.toString() << std::endl;
        });
    
    return 0;
}
```

### 使用调度器

```cpp
// 注意：使用 MainThreadScheduler 前需要先创建全局 Timer 调度器
auto mainScheduler = GTimerScheduler::create("MainScheduler");
mainScheduler->start();
GTimerScheduler::makeGlobal(mainScheduler);  // 设置为全局调度器

auto threadScheduler = NewThreadScheduler::create();
auto timerScheduler = MainThreadScheduler::create();  // 依赖全局 Timer 调度器

Observable::create([](const ObservableEmitterPtr &emitter) {
        // 在新线程中执行
        emitter->onNext("Hello");
        emitter->onNext("World");
        emitter->onComplete();
    })
    ->subscribeOn(threadScheduler)  // 在新线程中订阅
    ->observeOn(timerScheduler)     // 在主线程中观察
    ->subscribe([](const GAny &v) {
        std::cout << "Received: " << v.toString() << std::endl;
    });

// 运行主调度器事件循环
mainScheduler->run();
```

### 异步操作

```cpp
// 定时器
Observable::timer(1000)  // 延迟 1000ms
    ->subscribe([](const GAny &v) {
        std::cout << "Timer fired!" << std::endl;
    });

// 间隔发射
Observable::interval(0, 500)  // 初始延迟 0ms，间隔 500ms
    ->subscribe([](const GAny &v) {
        std::cout << "Tick: " << v.toString() << std::endl;
    });
```

## 核心概念

### Observable

`Observable` 是数据流的源头，可以发射零个或多个数据项，然后成功完成或失败。

**创建方式：**

```cpp
// 1. 使用 create 自定义创建逻辑
Observable::create([](const ObservableEmitterPtr &emitter) {
    emitter->onNext(1);
    emitter->onNext(2);
    emitter->onComplete();
});

// 2. 使用 just 发射固定值
Observable::just("Hello");
Observable::just(1, 2, 3);

// 3. 使用 fromArray 从数组创建
std::vector<GAny> items = {1, 2, 3, 4, 5};
Observable::fromArray(items);

// 4. 使用 range 创建范围
Observable::range(0, 10);  // 0 到 9

// 5. 空 Observable
Observable::empty();

// 6. 永不完成的 Observable
Observable::never();

// 7. 错误 Observable
Observable::error(GAnyException("Error message"));

// 8. 延迟创建
Observable::defer(source);
```

### Observer

`Observer` 是数据的消费者，定义了如何响应 Observable 发射的数据。

```cpp
struct Observer {
    virtual void onSubscribe(const DisposablePtr &d) = 0;  // 订阅时调用
    virtual void onNext(const GAny &value) = 0;             // 接收数据
    virtual void onError(const GAnyException &e) = 0;       // 错误处理
    virtual void onComplete() = 0;                          // 完成时调用
};
```

### Disposable

`Disposable` 用于管理订阅的生命周期，可以取消订阅以释放资源。

```cpp
auto disposable = Observable::just(1, 2, 3)
    ->subscribe([](const GAny &v) {
        // ...
    });

// 取消订阅
disposable->dispose();

// 检查是否已取消
if (disposable->isDisposed()) {
    // ...
}
```

### Scheduler

`Scheduler` 控制任务执行的线程和时机。

**内置调度器：**

- `NewThreadScheduler` - 为每个任务创建新线程
- `MainThreadScheduler` - 在主线程中执行（需要先创建全局 GTimerScheduler）
- `TaskSystemScheduler` - 使用任务系统
- `TimerScheduler` - 定时调度器

**重要提示：**

使用 `MainThreadScheduler` 之前，必须先创建并启动全局 `GTimerScheduler`：

```cpp
// 1. 创建并启动全局 Timer 调度器
auto mainScheduler = GTimerScheduler::create("MainScheduler");
mainScheduler->start();
GTimerScheduler::makeGlobal(mainScheduler);

// 2. 然后才能使用 MainThreadScheduler
auto timerScheduler = MainThreadScheduler::create();

// 3. 在程序结束前运行事件循环
mainScheduler->run();
```

## 操作符

### 创建操作符

创建操作符用于生成新的 Observable。

**静态方法：**

- `create(ObservableOnSubscribe)` - 使用自定义逻辑创建
- `just(T...)` - 发射 1-10 个指定的值
- `fromArray(vector<GAny>)` - 从数组创建
- `range(start, count)` - 发射一个整数范围
- `interval(delay, interval)` - 定期发射递增的整数
- `timer(delay)` - 延迟后发射单个值
- `empty()` - 创建立即完成的 Observable
- `never()` - 创建永不发射也不完成的 Observable
- `error(Exception)` - 创建立即发送错误的 Observable
- `defer(ObservableSource)` - 延迟创建 Observable
- `merge(sources...)` - 合并多个 Observable（并发）
- `concat(sources...)` - 顺序连接多个 Observable

详见 [Observable 静态方法](#observable-静态方法) 部分的代码示例。

### 转换操作符

#### map

将每个数据项转换为另一个值。

```cpp
Observable::just(1, 2, 3)
    ->map([](const GAny &x) {
        return x.toInt32() * 2;
    });
// 输出: 2, 4, 6
```

#### flatMap

将每个数据项转换为 Observable，然后合并所有 Observable 的输出。

```cpp
Observable::just(1, 2, 3)
    ->flatMap([](const GAny &v) {
        return Observable::range(0, v.toInt32());
    });
// 输出: 0, 0, 1, 0, 1, 2
```

#### concatMap

将每个数据项转换为 Observable，并按顺序连接它们（等待前一个完成才订阅下一个）。

```cpp
Observable::just(1, 2, 3)
    ->concatMap([](const GAny &v) {
        return Observable::just(v)->delay(100);
    });
// 输出: 1, 2, 3 (顺序执行，确保次序)
```

#### switchMap

当源 Observable 发射新数据时，取消订阅前一个内部 Observable 并订阅新的。

```cpp
Observable::create(...)
    ->switchMap([](const GAny &v) {
        // 如果新数据到来，上一个网络请求会被取消
        return Observable::fromCallable(searchNetwork(v));
    });
```

### 过滤操作符

#### filter

只发射满足条件的数据项。

```cpp
Observable::range(0, 10)
    ->filter([](const GAny &v) {
        return v.toInt32() % 2 == 0;
    });
// 输出: 0, 2, 4, 6, 8
```

#### elementAt

只发射第 N 个数据项（从 0 开始）。

```cpp
// 获取索引为 2 的元素
Observable::just("A", "B", "C", "D", "E")
    ->elementAt(2);
// 输出: C

// 索引越界时使用默认值
Observable::just(1, 2, 3)
    ->elementAt(10, 999);
// 输出: 999
```

#### first

只发射第一个数据项。

```cpp
// 获取第一个元素
Observable::just("First", "Second", "Third")
    ->first();
// 输出: First

// 空序列时使用默认值
Observable::empty()
    ->first("Default");
// 输出: Default
```

#### last

只发射最后一个数据项。

```cpp
// 获取最后一个元素
Observable::just("A", "B", "C")
    ->last();
// 输出: C

// 空序列时使用默认值
Observable::empty()
    ->last("Default");
// 输出: Default
```

#### ignoreElements

忽略所有数据项，只传递 onComplete 或 onError 事件。

```cpp
// 忽略所有元素，只等待完成
Observable::just(1, 2, 3, 4, 5)
    ->ignoreElements()
    ->subscribe(
        [](const GAny &v) {
            // 不会被调用
        },
        [](const GAnyException &e) {
            // 错误仍会传递
        },
        []() {
            std::cout << "Completed!" << std::endl;
        }
    );
// 输出: Completed!
```

#### skip

跳过前 N 个数据项，只发射后续的数据项。

```cpp
// 跳过前 3 个元素
Observable::range(0, 10)
    ->skip(3);
// 输出: 3, 4, 5, 6, 7, 8, 9

// 跳过 0 个元素（返回原 Observable）
Observable::just(1, 2, 3)
    ->skip(0);
// 输出: 1, 2, 3

// 跳过数量大于总数（不发射任何数据，直接完成）
Observable::just(1, 2, 3)
    ->skip(10);
// 输出: (无数据，仅完成)
```

#### skipLast

跳过最后 N 个数据项，只发射前面的数据项。

```cpp
// 跳过最后 2 个元素
Observable::range(0, 10)
    ->skipLast(2);
// 输出: 0, 1, 2, 3, 4, 5, 6, 7

// 跳过最后 0 个元素（返回原 Observable）
Observable::just(1, 2, 3)
    ->skipLast(0);
// 输出: 1, 2, 3

// 跳过数量大于总数（不发射任何数据，直接完成）
Observable::just(1, 2, 3)
    ->skipLast(10);
// 输出: (无数据，仅完成)
```

#### take

只发射前 N 个数据项，然后完成。

```cpp
// 只取前 3 个元素
Observable::range(0, 10)
    ->take(3);
// 输出: 0, 1, 2

// 取 0 个元素（不发射任何数据，直接完成）
Observable::just(1, 2, 3)
    ->take(0);
// 输出: (无数据，仅完成)

// 取的数量大于总数（发射所有数据）
Observable::just(1, 2, 3)
    ->take(10);
// 输出: 1, 2, 3
```

#### takeLast

只发射最后 N 个数据项。

```cpp
// 只取最后 3 个元素
Observable::range(0, 10)
    ->takeLast(3);
// 输出: 7, 8, 9

// 取最后 0 个元素（不发射任何数据，直接完成）
Observable::just(1, 2, 3)
    ->takeLast(0);
// 输出: (无数据，仅完成)

// 取的数量大于总数（发射所有数据）
Observable::just(1, 2, 3)
    ->takeLast(10);
// 输出: 1, 2, 3
```

### 组合操作符

#### combineLatest

组合多个 Observable 的最新值，当任一 Observable 发射数据时，将所有 Observable 的最新值组合后发射。

```cpp
// 组合两个 Observable
auto obs1 = Observable::just(1, 2, 3);
auto obs2 = Observable::just("A", "B", "C");

Observable::combineLatest(obs1, obs2, 
    [](const GAny &v1, const GAny &v2) {
        return v1.toString() + v2.toString();
    });
// 当两个都有值时开始发射组合结果
// 输出: 3A, 3B, 3C (假设obs1先完成)

// 组合多个数据流
auto timer1 = Observable::interval(0, 100);
auto timer2 = Observable::interval(0, 150);

Observable::combineLatest(timer1, timer2,
    [](const GAny &v1, const GAny &v2) {
        return GAny::object({
            {"timer1", v1},
            {"timer2", v2}
        });
    });
// 每当任一定时器触发时，发射两个定时器的最新值组合
```

#### zip

组合多个 Observable 的数据项，按照顺序一对一组合。只有当所有 Observable 都发射了第 N 个数据项时，才会组合并发射第 N 个结果。

```cpp
// 组合两个 Observable
auto obs1 = Observable::just(1, 2, 3);
auto obs2 = Observable::just("A", "B");

Observable::zip(obs1, obs2, [](const GAny &v1, const GAny &v2) {
    return v1.toString() + v2.toString();
});
// 输出: 1A, 2B
// 注意：因为 obs2 只有两个元素，导致 zip 提前结束，3 被忽略
```

#### merge

将多个 Observable 合并为一个，并发执行。

```cpp
auto obs1 = Observable::just(1, 2);
auto obs2 = Observable::just(3, 4);

Observable::merge(obs1, obs2)
    ->subscribe([](const GAny &v) {
        std::cout << v.toString() << " ";
    });
// 输出: 1 2 3 4
```

#### concat

将多个 Observable 顺序连接，等待前一个完成后才订阅下一个。

```cpp
auto obs1 = Observable::just(1, 2);
auto obs2 = Observable::just(3, 4);
auto obs3 = Observable::just(5, 6);

Observable::concat(obs1, obs2, obs3)
    ->subscribe([](const GAny &v) {
        std::cout << v.toString() << " ";
    });
// 输出: 1 2 3 4 5 6 (严格按顺序)

// 与 merge 的区别：concat 保证顺序，merge 可能并发
```

#### buffer

将多个数据项缓存为一个数组后再发射。

```cpp
// buffer(count) - 每 count 个元素缓存一次
Observable::range(1, 10)
    ->buffer(3);
// 输出: [1,2,3], [4,5,6], [7,8,9], [10]

// buffer(count, skip) - count 个元素一组，跳过 skip 个
Observable::range(1, 10)
    ->buffer(3, 2);
// 输出: [1,2,3], [3,4,5], [5,6,7], [7,8,9], [9,10]
```

#### startWith

在数据流开始之前插入指定的数据项。

```cpp
Observable::just(1, 2, 3)
    ->startWith(0)
    ->subscribe([](const GAny &v) {
        std::cout << v.toString() << " ";
    });
// 输出: 0 1 2 3

Observable::just("C", "D")
    ->startWith("A", "B")
    ->subscribe([](const GAny &v) {
        std::cout << v.toString() << " ";
    });
// 输出: A B C D
```

### 聚合操作符

#### scan

对数据流中的每个数据项应用累加器函数，并发射每次累加的结果。

```cpp
// 累乘示例
Observable::just(1, 2, 3, 4)
    ->scan([](const GAny &last, const GAny &item) {
        return last.toInt32() * item.toInt32();
    });
// 输出: 1, 2, 6, 24

// 累加示例
Observable::just(1, 2, 3, 4)
    ->scan([](const GAny &last, const GAny &item) {
        return last.toInt32() + item.toInt32();
    });
// 输出: 1, 3, 6, 10
```

### 时间操作符

#### delay

延迟一段时间后再发射数据项。

```cpp
// 延迟 1000ms 后发射所有数据
Observable::just(1, 2, 3)
    ->delay(1000);
// 输出: (1秒后) 1, 2, 3

// 配合调度器使用
auto scheduler = MainThreadScheduler::create();
Observable::range(0, 5)
    ->delay(500, scheduler)
    ->subscribe([](const GAny &v) {
        std::cout << "Delayed: " << v.toString() << std::endl;
    });
// 在指定调度器上延迟发射
```

#### debounce

防抖动操作符，只有在指定的时间窗口内没有新数据项发射时，才会发射最新的数据项。常用于处理高频事件（如搜索输入、按钮点击）。

```cpp
// 基本用法：500ms 防抖
auto scheduler = MainThreadScheduler::create();
Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext("a");     // 0ms
        GThread::mSleep(100);     // 100ms
        emitter->onNext("ab");    // 100ms - 被新值覆盖
        GThread::mSleep(100);     // 200ms
        emitter->onNext("abc");   // 200ms - 被新值覆盖
        GThread::mSleep(600);     // 800ms
        // 此时 "abc" 在 800ms 时发射（200ms + 600ms > 500ms）
        emitter->onNext("abcd");  // 800ms
        GThread::mSleep(600);     // 1400ms
        // "abcd" 在 1400ms 时发射
        emitter->onComplete();
        // 完成时发射最后的值 "abcd"（如果未发射）
    })
    ->debounce(500, scheduler)
    ->subscribe([](const GAny &v) {
        std::cout << "Debounced: " << v.toString() << std::endl;
    });
// 输出:
// Debounced: abc   (在 800ms 时)
// Debounced: abcd  (在 1400ms 时或完成时)

// 实际应用场景：搜索输入
Observable::create([](const ObservableEmitterPtr &emitter) {
        // 模拟用户快速输入
        emitter->onNext("r");
        emitter->onNext("re");
        emitter->onNext("rea");
        emitter->onNext("reac");
        emitter->onNext("react");  // 用户停止输入
        GThread::mSleep(600);       // 等待超过 debounce 时间
        emitter->onComplete();
    })
    ->debounce(300, scheduler)  // 300ms 防抖
    ->subscribe([](const GAny &v) {
        // 只会触发一次搜索请求
        std::cout << "Searching for: " << v.toString() << std::endl;
    });
// 输出: Searching for: react
```

#### timeout

如果在指定的时间段内没有任何数据项发射，则以错误（Timeout）终止 Observable，或切换到备用的 Observable。

```cpp
// 1. 超时后报错
Observable::never()
    ->timeout(1000)  // 1000ms 后超时
    ->subscribe(
        [](const GAny &v) { ... },
        [](const GAnyException &e) {
            std::cout << "Timeout: " << e.toString() << std::endl;
        }
    );

// 2. 超时后切换到备用 Observable
Observable::never()
    ->timeout(1000, Observable::just("Fallback"))
    ->subscribe([](const GAny &v) {
        std::cout << "Value: " << v.toString() << std::endl;
    });
// 输出: Value: Fallback
```

### 辅助操作符

#### repeat

重复发射数据流指定次数。

```cpp
Observable::just(1, 2, 3)
    ->repeat(2);
// 输出: 1, 2, 3, 1, 2, 3
```

#### reduce

对数据流应用累加器函数，最终只发射一个结果（空序列则报错）。

```cpp
Observable::just(1, 2, 3, 4)
    ->reduce([](const GAny &acc, const GAny &v) {
        return acc.toInt32() + v.toInt32();
    });
// 输出: 10
```

#### retry

当发生错误时重新订阅数据流。

```cpp
// 1. 无限重试
Observable::create(...)
    ->retry();

// 2. 指定重试次数
Observable::create(...)
    ->retry(3); // 最多重试 3 次
```

#### doOnNext / doOnError / doOnComplete

在 Observable 的生命周期事件发生时注册回调，不改变数据流。

```cpp
Observable::just(1, 2, 3)
    ->doOnNext([](const GAny &v) {
        std::cout << "Emitting: " << v.toString() << std::endl;
    })
    ->doOnError([](const GAnyException &e) {
        std::cerr << "Error occurred: " << e.toString() << std::endl;
    })
    ->doOnComplete([]() {
        std::cout << "Stream completed" << std::endl;
    });
```

#### doOnSubscribe / doFinally

```cpp
Observable::create(...)
    ->doOnSubscribe([](const DisposablePtr &d) {
        std::cout << "Subscribed!" << std::endl;
    })
    ->doFinally([]() {
        std::cout << "Terminated (either completed, error, or disposed)" << std::endl;
    });
```

#### doOnEach

一次性注册 onNext / onError / onComplete / onSubscribe / finally 的回调。

```cpp
Observable::just(1, 2, 3)
    ->doOnEach(
        [](const GAny &v) { std::cout << "Next: " << v.toString() << std::endl; },
        [](const GAnyException &e) { std::cout << "Error: " << e.toString() << std::endl; },
        []() { std::cout << "Complete" << std::endl; },
        [](const DisposablePtr &) { std::cout << "Subscribe" << std::endl; },
        []() { std::cout << "Finally" << std::endl; }
    );
```

### 布尔操作符

#### all

判断是否所有数据项都满足条件。

```cpp
Observable::just(2, 4, 6)
    ->all([](const GAny &v) { return v.toInt32() % 2 == 0; });
// 输出: true
```

#### any

判断是否存在满足条件的数据项。

```cpp
Observable::just(1, 3, 4)
    ->any([](const GAny &v) { return v.toInt32() % 2 == 0; });
// 输出: true
```

#### contains

判断序列是否包含指定元素。

```cpp
Observable::just(1, 2, 3)
    ->contains(2);
// 输出: true
```

#### isEmpty

判断序列是否为空。

```cpp
Observable::empty()
    ->isEmpty();
// 输出: true
```

#### defaultIfEmpty

当序列为空时发射默认值。

```cpp
Observable::empty()
    ->defaultIfEmpty(999);
// 输出: 999
```

#### sequenceEqual

判断两个序列是否逐项相等。

```cpp
auto s1 = Observable::just(1, 2, 3);
auto s2 = Observable::just(1, 2, 3);
Observable::sequenceEqual(s1, s2);
// 输出: true
```

#### subscribeOn

指定 Observable 在哪个调度器上执行订阅操作。

```cpp
auto scheduler = NewThreadScheduler::create();
Observable::just(1, 2, 3)
    ->subscribeOn(scheduler);
```

#### observeOn

指定 Observer 在哪个调度器上接收数据。

```cpp
auto scheduler = MainThreadScheduler::create();
Observable::just(1, 2, 3)
    ->observeOn(scheduler);
```

## 完整示例

### 基础链式操作

```cpp
#define USE_GANY_CORE
#include <gx/gany.h>
#include <rx/rx.h>

using namespace rx;

int main() {
    initGAnyCore();
    
    // 创建调度器
    auto mainScheduler = GTimerScheduler::create("MainScheduler");
    mainScheduler->start();
    GTimerScheduler::makeGlobal(mainScheduler);
    
    auto threadScheduler = NewThreadScheduler::create();
    auto timerScheduler = MainThreadScheduler::create();
    
    // 复杂的链式操作
    Observable::just(10)
        ->repeat(10)                    // 重复 10 次
        ->flatMap([](const GAny &v) {   // 展开为范围
            return Observable::range(0, v.toInt32());
        })
        ->filter([](const GAny &v) {    // 过滤偶数
            return v.toInt32() % 2 == 0;
        })
        ->buffer(10)                     // 每 10 个缓存一次
        ->subscribeOn(threadScheduler)   // 后台线程执行
        ->observeOn(timerScheduler)      // 主线程观察
        ->subscribe(
            [](const GAny &v) {
                std::cout << "Output: " << v.toString() << std::endl;
            },
            [](const GAnyException &e) {
                std::cerr << "Error: " << e.toString() << std::endl;
            },
            []() {
                std::cout << "Completed!" << std::endl;
            }
        );
    
    mainScheduler->run();
    
    // 检查内存泄漏
    LeakObserver::checkLeak();
    
    return 0;
}
```

### 高级组合示例

```cpp
#define USE_GANY_CORE
#include <gx/gany.h>
#include <rx/rx.h>

using namespace rx;

int main() {
    initGAnyCore();
    
    auto mainScheduler = GTimerScheduler::create("MainScheduler");
    mainScheduler->start();
    GTimerScheduler::makeGlobal(mainScheduler);
    
    auto timerScheduler = MainThreadScheduler::create();
    
    // 示例 1: 使用 take 和 skip 组合
    std::cout << "Example 1: take & skip" << std::endl;
    Observable::range(0, 20)
        ->skip(5)        // 跳过前 5 个: 0-4
        ->take(10)       // 只取接下来的 10 个: 5-14
        ->subscribe([](const GAny &v) {
            std::cout << v.toString() << " ";
        });
    std::cout << std::endl;
    
    // 示例 2: 使用 first 和 last
    std::cout << "Example 2: first & last" << std::endl;
    Observable::range(1, 10)
        ->first()
        ->subscribe([](const GAny &v) {
            std::cout << "First: " << v.toString() << std::endl;
        });
    
    Observable::range(1, 10)
        ->last()
        ->subscribe([](const GAny &v) {
            std::cout << "Last: " << v.toString() << std::endl;
        });
    
    // 示例 3: 使用 scan 计算累加和
    std::cout << "Example 3: scan for accumulation" << std::endl;
    Observable::range(1, 10)
        ->scan([](const GAny &last, const GAny &item) {
            return last.toInt32() + item.toInt32();
        })
        ->subscribe([](const GAny &v) {
            std::cout << "Sum: " << v.toString() << std::endl;
        });
    
    // 示例 4: 使用 combineLatest 组合数据流
    std::cout << "Example 4: combineLatest" << std::endl;
    auto timer1 = Observable::interval(0, 100)->take(5);
    auto timer2 = Observable::interval(0, 150)->take(5);
    
    Observable::combineLatest(timer1, timer2,
        [](const GAny &v1, const GAny &v2) {
            return "T1:" + v1.toString() + ",T2:" + v2.toString();
        })
        ->subscribe([](const GAny &v) {
            std::cout << v.toString() << std::endl;
        });
    
    // 示例 5: 使用 delay 延迟执行
    std::cout << "Example 5: delay execution" << std::endl;
    Observable::just("Hello", "World")
        ->delay(1000, timerScheduler)
        ->subscribe([](const GAny &v) {
            std::cout << "Delayed: " << v.toString() << std::endl;
        });
    
    // 示例 6: 复杂的操作符链
    std::cout << "Example 6: complex operator chain" << std::endl;
    Observable::range(1, 100)
        ->filter([](const GAny &v) {
            return v.toInt32() % 3 == 0;  // 只保留 3 的倍数
        })
        ->map([](const GAny &v) {
            return v.toInt32() * v.toInt32();  // 平方
        })
        ->skipLast(5)    // 跳过最后 5 个
        ->takeLast(10)   // 只取最后 10 个
        ->buffer(3)      // 每 3 个一组
        ->subscribe([](const GAny &v) {
            std::cout << "Group: " << v.toString() << std::endl;
        });
    
    mainScheduler->run();
    LeakObserver::checkLeak();
    
    return 0;
}
```

## 错误处理

```cpp
Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onNext(2);
        // 发送错误
        emitter->onError(GAnyException("Something went wrong"));
        // throw GAnyException("Something went wrong"); // 也可以抛出异常
        // 此后的代码不会执行
        emitter->onNext(3);
    })
    ->subscribe(
        [](const GAny &v) {
            std::cout << "Value: " << v.toString() << std::endl;
        },
        [](const GAnyException &e) {
            std::cerr << "Error: " << e.toString() << std::endl;
        },
        []() {
            std::cout << "Completed" << std::endl;
        }
    );
```

## 项目结构

```
rx/
├── cmake/                      # CMake 辅助脚本
│   └── GetGitDep.cmake        # Git 依赖管理
├── examples/                   # 示例代码
│   ├── CMakeLists.txt
│   └── test_rx.cpp            # 示例
├── rx/                         # 核心库
│   ├── include/rx/            # 头文件
│   │   ├── disposables/       # Disposable 实现
│   │   ├── operators/         # 操作符实现
│   │   ├── schedulers/        # 调度器实现
│   │   ├── observable.h       # Observable 定义
│   │   ├── observer.h         # Observer 定义
│   │   ├── scheduler.h        # Scheduler 定义
│   │   └── rx.h              # 主头文件
│   ├── src/                   # 源文件
│   └── CMakeLists.txt
├── CMakeLists.txt             # 主 CMake 配置
└── README.md                  # 本文档
```

## API 文档

### Observable 静态方法

| 方法 | 说明 |
|------|------|
| `create(ObservableOnSubscribe)` | 使用自定义逻辑创建 Observable |
| `just(T...)` | 发射 1-10 个指定的值 |
| `fromArray(vector<GAny>)` | 从数组创建 Observable |
| `range(start, count)` | 发射一个整数范围 |
| `interval(delay, interval)` | 定期发射递增的整数 |
| `timer(delay)` | 延迟后发射单个值 |
| `empty()` | 创建立即完成的 Observable |
| `never()` | 创建永不发射也不完成的 Observable |
| `error(Exception)` | 创建立即发送错误的 Observable |
| `defer(ObservableSource)` | 延迟创建 Observable |
| `merge(sources...)` | 合并多个 Observable（并发） |
| `concat(sources...)` | 顺序连接多个 Observable |
| `zip(obs1, obs2, zipper)` | 按顺序一对一合并多个 Observable |
| `sequenceEqual(obs1, obs2[, comparator])` | 判断两个序列是否逐项相等 |

### Observable 实例方法

| 方法 | 说明 |
|------|------|
| `map(function)` | 转换每个数据项 |
| `flatMap(function)` | 将数据项转换为 Observable 并合并 |
| `concatMap(function)` | 将数据项转换为 Observable 并按顺序连接 |
| `switchMap(function)` | 将数据项转换为 Observable，发射新数据时取消上一个 |
| `filter(predicate)` | 过滤数据项 |
| `elementAt(index[, defaultValue])` | 发射第 N 个数据项（从 0 开始） |
| `first([defaultValue])` | 发射第一个数据项 |
| `last([defaultValue])` | 发射最后一个数据项 |
| `ignoreElements()` | 忽略所有数据项，只传递完成或错误 |
| `skip(count)` | 跳过前 N 个数据项 |
| `skipLast(count)` | 跳过最后 N 个数据项 |
| `take(count)` | 只发射前 N 个数据项 |
| `takeLast(count)` | 只发射最后 N 个数据项 |
| `combineLatest(obs1, obs2, combiner)` | 组合多个 Observable 的最新值 |
| `join(other, leftDuration, rightDuration, combiner)` | 当两个数据流的时间窗口重叠时组合它们 |
| `startWith(items...)` | 在数据流开始前插入指定数据项 |
| `buffer(count[, skip])` | 缓存数据项为数组 |
| `delay(time[, scheduler])` | 延迟发射数据项 |
| `debounce(time[, scheduler])` | 防抖动，只在时间窗口内无新数据时发射最新值 |
| `timeout(time[, scheduler, fallback])` | 超时后报错或切换到备用 Observable |
| `scan(accumulator)` | 对数据流应用累加器函数并发射每次结果 |
| `reduce(accumulator)` | 对数据流应用累加器函数并发射最终结果 |
| `repeat(times)` | 重复数据流 |
| `retry([times])` | 遇到错误时重试（默认无限次） |
| `all(predicate)` | 判断是否所有数据项都满足条件 |
| `any(predicate)` | 判断是否存在满足条件的数据项 |
| `contains(item)` | 判断序列是否包含指定元素 |
| `isEmpty()` | 判断序列是否为空 |
| `defaultIfEmpty(value)` | 序列为空时发射默认值 |
| `doOnNext(action)` | 注册 onNext 回调 |
| `doOnError(action)` | 注册 onError 回调 |
| `doOnComplete(action)` | 注册 onComplete 回调 |
| `doOnSubscribe(action)` | 注册 onSubscribe 回调 |
| `doFinally(action)` | 注册终止或取消时的回调 |
| `doOnEach(...)` | 注册 onNext/onError/onComplete/onSubscribe/finally 回调 |
| `subscribeOn(scheduler)` | 指定订阅的调度器 |
| `observeOn(scheduler)` | 指定观察的调度器 |
| `subscribe(onNext[, onError, onComplete])` | 订阅 Observable |

## 最佳实践

1. **始终处理错误**：在 subscribe 中提供 onError 回调
2. **及时取消订阅**：保存 Disposable 并在不需要时调用 dispose()
3. **选择合适的调度器**：避免在主线程执行耗时操作
4. **避免副作用**：操作符函数应尽量保持纯函数特性
5. **链式调用**：充分利用操作符组合来构建复杂逻辑
6. **正确初始化调度器**：使用 MainThreadScheduler 前必须先创建并启动全局 GTimerScheduler
7. **合理使用操作符**：根据场景选择合适的操作符，如 take/skip 用于限制数据量，first/last 用于获取特定位置的数据
8. **注意内存管理**：使用 LeakObserver::checkLeak() 检查内存泄漏

## 许可证

[MIT](./LICENSE)

## 贡献

欢迎提交 Issue 和 Pull Request！

## 作者

Gxin

## 相关链接

- [ReactiveX](http://reactivex.io/)
- [RxJava](https://github.com/ReactiveX/RxJava)
- [RxCpp](https://github.com/ReactiveX/RxCpp)
