# Rx

一个基于 RxJava 设计理念的 C++20 ReactiveX 库，提供链式调用、丰富操作符和多种调度器，面向异步编程与事件流处理。

## 简介

`rx` 以 `Observable/Observer/Disposable/Scheduler` 为核心抽象，强调可组合的操作符与可控的线程调度，适合构建复杂的异步数据流。

## 依赖

通过 CMake 自动拉取：

- [gany](https://github.com/giarld/gany) - 通用类型系统
- [gx](https://github.com/giarld/gx) - 基础工具库

## 构建

要求：CMake 3.20+，C++20 编译器，Git

```bash
mkdir build
cd build
cmake -DBUILD_RX_EXAMPLES=ON ..
cmake --build .
```

## 快速开始

```cpp
#define USE_GANY_CORE
#include <gx/gany.h>
#include <rx/rx.h>

using namespace rx;

int main() {
    initGAnyCore();

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

## 调度器提示

`MainThreadScheduler` 依赖全局 `GTimerScheduler`，使用前必须先创建并启动：

```cpp
auto mainScheduler = GTimerScheduler::create("MainScheduler");
mainScheduler->start();
GTimerScheduler::makeGlobal(mainScheduler);

auto timerScheduler = MainThreadScheduler::create();

Observable::timer(1000)
    ->observeOn(timerScheduler)
    ->subscribe([](const GAny &) {
        std::cout << "Timer fired" << std::endl;
    });

mainScheduler->run();
```

## 核心概念

- `Observable`: 数据流源头，发射数据并完成或失败。
- `Observer`: 数据消费者，响应 onNext/onError/onComplete。
- `Disposable`: 订阅生命周期管理，可随时取消。
- `Scheduler`: 控制任务执行线程与时机。

## 操作符速览

- 创建：`create` `just` `fromArray` `range` `interval` `timer` `empty` `never` `error` `defer` `merge` `concat` `zip`
- 转换：`map` `flatMap` `concatMap` `switchMap` `toArray` `groupBy` `window`
- 过滤：`filter` `distinct` `distinctUntilChanged` `elementAt` `first` `last` `ignoreElements` `skip` `skipLast` `skipWhile` `take` `takeLast` `takeUntil` `takeWhile`
- 组合：`combineLatest` `startWith` `buffer` `amb`
- 聚合：`scan` `reduce`
- 时间：`delay` `debounce` `sample` `timeout`
- 辅助：`repeat` `retry` `doOnNext` `doOnError` `doOnComplete` `doOnSubscribe` `doFinally` `doOnEach`
- 错误处理：`onErrorReturn` `onErrorResumeNext` `catchError`
- 布尔：`all` `any` `contains` `isEmpty` `defaultIfEmpty` `sequenceEqual`
- 调度：`subscribeOn` `observeOn`

## API 与示例

- 操作符声明：`rx/include/rx/observable.h`
- 操作符实现：`rx/include/rx/operators/`
- 功能示例：`examples/test_rx.cpp`

## 项目结构

```
rx/
├── examples/               # 示例代码
├── rx/
│   ├── include/rx/         # 头文件
│   └── src/                # 源文件
├── CMakeLists.txt
└── README.md
```

## 许可证

[MIT](./LICENSE)

## 贡献

欢迎提交 Issue 和 Pull Request。

## 作者

Gxin

## 相关链接

- [ReactiveX](http://reactivex.io/)
- [RxJava](https://github.com/ReactiveX/RxJava)
- [RxCpp](https://github.com/ReactiveX/RxCpp)
