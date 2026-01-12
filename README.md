# rx

A C++ ReactiveX library built on the design concepts of RxJava

## 简介

`rx` 是一个基于 RxJava 设计理念实现的 C++ ReactiveX 库，提供了强大的异步编程和事件流处理能力。它采用 C++20 标准，支持链式调用、操作符组合和多种调度器，让异步编程变得更加优雅和高效。

## 特性

- 🔄 **丰富的操作符**：支持 map、flatMap、filter、buffer、repeat 等常用操作符
- 🎯 **创建操作符**：提供 create、just、fromArray、range、interval、timer 等多种创建方式
- ⚡ **多线程调度**：内置多种 Scheduler（NewThread、Timer、MainThread、TaskSystem）
- 🔗 **链式调用**：支持流畅的函数式编程风格
- 🛡️ **资源管理**：完善的 Disposable 机制，防止内存泄漏
- 📦 **依赖管理**：基于 CMake 和 Git 的自动依赖管理

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

### 组合操作符

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

### 辅助操作符

#### repeat

重复发射数据流指定次数。

```cpp
Observable::just(1, 2, 3)
    ->repeat(2);
// 输出: 1, 2, 3, 1, 2, 3
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

### Observable 实例方法

| 方法 | 说明 |
|------|------|
| `map(function)` | 转换每个数据项 |
| `flatMap(function)` | 将数据项转换为 Observable 并合并 |
| `filter(predicate)` | 过滤数据项 |
| `buffer(count[, skip])` | 缓存数据项为数组 |
| `repeat(times)` | 重复数据流 |
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
