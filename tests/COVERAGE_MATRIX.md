# RX 现行基线审计与测试覆盖矩阵

## 1. 审计范围与状态定义

- 审计基线：`2138bf9f387274a30cf21ee3ad0b2edb56bfa705`（`fix(rx): 完善异常处理与操作符生命周期`）。当前 Workshop worktree 与父仓库均指向该提交，审计开始时 `git status --porcelain=v2` 无文件差异。
- 该基线提交包含 44 个文件的既有改动（约 2353 行新增、723 行删除），包括 30 余个 RX 生产头文件、`rx/src/observable.cpp`、`examples/test_rx.cpp` 和首批 GoogleTest。它们是必须完整保留的现行基线，不代表其行为已被确认为正确契约。
- 自动化测试现状：`tests/` 已按职责拆分测试源文件，并通过 `gtest_discover_tests` 将 GoogleTest 用例注册到 CTest；2026-08-14 Windows/MSVC Debug 全量 206/206 通过且连续 10 轮均为 206/206，2026-08-15 审查返工后 CTest 发现 210 项，新增 `elementAt`/`first` 2 项与 `DisposableHelper` 2 项均完成定向验证。
- `examples/test_rx.cpp` 是 1392 行日志式演示程序，覆盖面较广但主要依赖输出观察和等待，不计入严格断言覆盖；保留且不迁移、不删除、不修改。
- 审计开始时 `.memory/` 与 `graphify-out/graph.json` 均不存在；后续任务已建立 `.memory/testing/` 记录，本矩阵继续以源码为最终依据。
- Windows/MSVC Debug 构建、全量 CTest 与 10 轮稳定性已有实测证据；Clang/GCC 覆盖率与 Sanitizer 因当前环境缺少受支持工具链而未实测。

状态含义：

| 状态 | 含义 |
| --- | --- |
| 已覆盖 | 已存在严格断言的自动化成功路径测试。 |
| 回归覆盖 | 已存在针对特定缺陷或边界的严格断言测试，但不是完整语义覆盖。 |
| 部分覆盖 | 仅部分重载、场景或间接组合被自动化测试覆盖。 |
| 缺口 | 没有对应严格断言自动化测试。 |
| 仅演示 | 仅在 `examples/test_rx.cpp` 中以日志/等待方式演示，不计入验收覆盖。 |
| 待实测 | 已有测试或接入，但未获授权运行，结果未知。 |

通用平台要求：同步纯逻辑测试为 `All`；线程、定时器、gx 调度器首先要求 `Windows/MSVC Debug` 实测，并在环境可用时补 `Clang/GCC + ASan/UBSan`。所有测试必须只使用本地合成数据。

## 2. `observable.h` 公开 API 矩阵

下表逐项覆盖 `rx/include/rx/observable.h:34-269` 的公开 API。目标场景缩写：`S` 成功，`E` 空源，`X` 上游/回调错误，`D` dispose/取消，`B` 参数或数值边界，`R` 同步重入/重复终止，`C` 并发/顺序，`L` 生命周期释放。

| 公开 API | 目标场景 | 目标测试套件 | 平台 | 当前状态 | 豁免理由 | 缺陷 | 现有证据 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `~Observable()` | L/经基类释放 | `ObservableLifetimeTest` | All | 已覆盖 | S/E/X/D 不适用：析构只验证多态释放与无泄漏 | 无 | `core_lifecycle_test.cpp` 通过 `Observable` 基类指针验证虚析构 |
| `create(ObservableOnSubscribe)` | S/X/D/R/L | `ObservableCreateTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_creation_test.cpp` 发射、异常、取消与终止后静默 |
| `empty()` | S/E | `ObservableEmptyTest` | All | 已覆盖 | D 不适用：同步无值即终止 | 无 | `observable_creation_test.cpp` |
| `fromArray(vector)` | S/E/D/B | `ObservableFromArrayTest` | All | 已覆盖 | 无 | 无 | `observable_creation_test.cpp` 顺序、空数组、订阅即取消 |
| `just(Args...)` | S/E/D/B | `ObservableJustTest` | All | 已覆盖 | 无 | 无 | `observable_creation_test.cpp` 单值、多值与零参数空流 |
| `never()` | D/L | `ObservableNeverTest` | All | 已覆盖 | S/E/X 不适用：按契约不发信号 | 无 | `observable_creation_test.cpp` 仅订阅事件且无终止 |
| `error(exception)` | X | `ObservableErrorTest` | All | 已覆盖 | S/E 不适用 | 无 | `observable_creation_test.cpp` |
| `defer(source)` | S/X/D/重复订阅 | `ObservableDeferTest` | All | 已覆盖 | `defer` 接受已创建 `ObservableSource`，不提供工厂延迟求值 | 无 | `observable_creation_test.cpp` 验证每个 Observer 均独立订阅源 |
| `interval(delay, interval)` | S/D/B/C/L | `ObservableIntervalTest` | Windows/MSVC | 已实测 | E 不适用：无限源；墙钟精度由 gx 提供 | 无 | 初始延迟先后、至少三次周期递增、dispose 后跨两个期限不再发射：`observable_time_test.cpp` |
| `timer(delay)` | S/D/B/C/L | `ObservableTimerTest` | Windows/MSVC | 已实测 | E 不适用；墙钟精度由 gx 提供 | 无已确认缺陷 | 单次值/完成跨第二期限保持稳定，及正延迟取消：`observable_time_test.cpp` |
| `range(start, count)` | S/E/D/B | `ObservableRangeTest` | All | 回归覆盖 | 无 | 无 | `observable_range_regression_test.cpp` |
| `combineLatestArray(sources, combiner)` | S/E/X/D/B/C/L | `ObservableCombineLatestTest` | All | 已覆盖 | 无 | 无 | `observable_combination_test.cpp` 多源最新值、完成顺序、错误，以及主动取消释放三个受控源 |
| `combineLatest(source1, source2, combiner)` | S/E/X/D/C/L | `ObservableCombineLatestTest` | All | 已覆盖 | 无 | 无 | 二源空源、错误、回调异常、onNext 重入 dispose，以及主动取消释放两侧受控源 |
| `fromCallable(callable)` | S/X/D | `ObservableFromCallableTest` | All | 已覆盖 | E 不适用：单值或错误 | 无 | `observable_creation_test.cpp` 结果与异常转换 |
| `merge(source)` | S/E/X/D/L | `ObservableMergeTest` | All | 已覆盖 | 无 | 无 | `observable_combination_test.cpp` flatten 成功 |
| `mergeArray(sources)` | S/E/X/D/C/L | `ObservableMergeTest` | All | 已覆盖 | 无 | 无 | 数组顺序、错误与下游取消 |
| `merge(Args...)` | S/E/X/D/C/L | `ObservableMergeTest` | All | 已覆盖 | 无 | 无 | 可变参成功、错误与取消 |
| `concatArray(sources)` | S/E/X/D/C/L | `ObservableConcatTest` | All | 已覆盖 | 无 | 无 | 数组顺序、错误时不订阅后续源、取消 |
| `concat(Args...)` | S/E/X/D/C/L | `ObservableConcatTest` | All | 已覆盖 | 无 | 无 | 可变参顺序、错误与取消 |
| `ambArray(sources)` | S/E/X/D/C/L | `ObservableAmbTest` | All | 已覆盖 | 无 | 无 | 数组首值/无值完成获胜、空数组、失败、取消 loser，以及 winner 终止后迟到信号静默 |
| `amb(Args...)` | S/E/X/D/C/L | `ObservableAmbTest` | All | 已覆盖 | 无 | 无 | 可变参错误获胜及协调器取消回归 |
| `zipArray(sources, zipper)` | S/E/X/D/C/L | `ObservableZipTest` | All | 已覆盖 | 无 | 无 | 三源行顺序、短源提前完成，以及主动取消释放三个受控源 |
| `zip(source1, source2, zipper)` | S/E/X/D/C/L | `ObservableZipTest` | All | 已覆盖 | 无 | 无 | 成功、源错误、zipper 异常、主动取消释放两侧受控源与完成一次 |
| `map(function)` | S/E/X/D/R | `ObservableMapTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp` 成功、上游错误、回调异常与取消 |
| `flatMap(function)` | S/E/X/D/C/L | `ObservableFlatMapTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp` 成功、上游错误、回调异常与取消；inner 错误见组合测试 |
| `concatMap(function)` | S/E/X/D/C/L | `ObservableConcatMapTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp` 顺序、inner 错误、回调异常与取消；同步队列回归见组合测试 |
| `switchMap(function)` | S/E/X/D/C/L | `ObservableSwitchMapTest` | All | 已覆盖 | 无 | 无 | 活动 inner 切换、旧 inner 迟到值/错误静默、上游完成等待最新 inner，以及 inner/mapper 错误与取消 |
| `buffer(count, skip)` | S/E/X/D/B | `ObservableBufferTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp` 重叠、尾缓冲、上游错误、取消与非法参数；生命周期参数回归 |
| `buffer(count)` | S/E/X/D/B | `ObservableBufferTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp` 精确缓冲、尾缓冲、错误与取消 |
| `toArray()` | S/E/X/D | `ObservableToArrayTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp` 非空、空数组、错误与取消 |
| `repeat(times)` | S/E/X/D/B | `ObservableRepeatTest` | All | 已覆盖 | 无限重载不存在；同步无限 repeat 不在此 API | 无 | `observable_aggregation_test.cpp` 次数、0、错误与取消 |
| `retry(times)` | S/X/D/B | `ObservableRetryTest` | All | 已覆盖 | E 不产生重试 | 无 | `observable_error_test.cpp` 成功重试与次数耗尽 |
| `retry()` | S/X/D/L | `ObservableRetryTest` | All | 已覆盖 | E 不产生重试；无限持续失败需有界异步取消，留待并发任务 | 无 | 无限重载在一次失败后成功并终止 |
| `doOnEach(...)` | S/E/X/D/R/L | `ObservableDoOnEachTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp`; `observable_callback_test.cpp` |
| `doOnNext(action)` | S/X/D | `ObservableDoOnEachTest` | All | 已覆盖 | 无 | 无 | `observable_transformation_test.cpp` 成功与异常转换 |
| `doOnError(action)` | X/回调异常 | `ObservableDoOnEachTest` | All | 已覆盖 | S/E 不触发 action | 无 | `observable_transformation_test.cpp` 错误副作用与回调异常转换 |
| `doOnComplete(action)` | S/E/回调异常 | `ObservableDoOnEachTest` | All | 已覆盖 | X 不触发 complete | 无 | `observable_transformation_test.cpp` 完成副作用与回调异常转换 |
| `doOnSubscribe(action)` | S/回调异常 | `ObservableCallbackRegressionTest` | All | 回归覆盖 | action 在订阅阶段执行；下游主动取消由通用 dispose 用例覆盖 | 无 | `observable_callback_test.cpp` 成功订阅见便利回调用例，异常后无值/完成/后续信号 |
| `doFinally(action)` | S/X/D/仅一次 | `ObservableDoOnEachTest` | All | 已覆盖 | finally 异常按实现吞掉，无可观察下游信号 | 无 | `observable_transformation_test.cpp` 完成、错误、取消路径且重复 dispose 仅一次 |
| `scan(accumulator)` | S/E/回调异常/B | `ObservableScanTest` | All | 已覆盖 | 上游错误/下游取消沿用无状态透传与通用 dispose 契约，未在本套件重复 | 无 | `observable_aggregation_test.cpp` 累计、空首值、异常终止 |
| `reduce(accumulator)` | S/E/X/回调异常 | `ObservableReduceTest` | All | 已覆盖 | 下游取消沿用通用聚合 dispose 契约，未重复覆盖 | 无 | `observable_aggregation_test.cpp` 成功、空、上游与回调错误 |
| `filter(predicate)` | S/E/X/D/R | `ObservableFilterTest` | All | 已覆盖 | 无 | 无 | `observable_filtering_test.cpp` 成功、异常与取消 |
| `distinct()` | S/E/X/D/GAny 类型边界 | `ObservableDistinctTest` | All | 已覆盖 | 混合 GAny 类型比较由 gany 公开比较契约负责 | 无 | `observable_filtering_test.cpp` 去重顺序 |
| `distinct(keySelector)` | S/E/X/D | `ObservableDistinctTest` | All | 已覆盖 | 无 | 无 | `observable_filtering_test.cpp` 键选择与异常 |
| `distinctUntilChanged()` | S/E | `ObservableDistinctUntilChangedTest` | All | 已覆盖 | X/D 由带回调重载与通用 dispose 契约代表覆盖 | 无 | `observable_filtering_test.cpp` 连续去重 |
| `distinctUntilChanged(keySelector)` | S/回调异常 | `ObservableDistinctUntilChangedTest` | All | 已覆盖 | 上游错误/取消与同一观察者实现共享 | 无 | `observable_filtering_test.cpp` 键选择；异常由比较器重载代表覆盖 |
| `distinctUntilChanged(comparator)` | S/回调异常 | `ObservableDistinctUntilChangedTest` | All | 已覆盖 | 上游错误/取消与同一观察者实现共享 | 无 | `observable_filtering_test.cpp` 自定义比较与异常 |
| `distinctUntilChanged(keySelector, comparator)` | S | `ObservableDistinctUntilChangedTest` | All | 已覆盖 | 异常路径由单回调重载代表覆盖 | 无 | `observable_filtering_test.cpp` 组合重载 |
| `elementAt(index)` | S/E/X/D/B | `ObservableElementAtTest` | All | 已覆盖 | 无 | 无 | `observable_filtering_test.cpp` 命中、越界、上游错误与命中后取消上游 |
| `elementAt(index, default)` | S/E/X/D/B | `ObservableElementAtTest` | All | 已覆盖 | D 与无默认重载共享同一观察者实现 | 无 | `observable_filtering_test.cpp` 默认值、上游错误；取消证据见无默认重载 |
| `first()` | S/E/X/D | `ObservableFirstTest`; `ObservableElementAtTest` | All | 已覆盖 | `elementAt(0)` 委托实现，取消由 ElementAt 套件实测 | 无 | `observable_filtering_test.cpp` 值、空源、上游错误与委托取消 |
| `first(default)` | S/E/X/D | `ObservableFirstTest`; `ObservableElementAtTest` | All | 已覆盖 | `elementAt(0, default)` 委托实现，取消与无默认重载共享实现 | 无 | `observable_filtering_test.cpp` 默认值、上游错误与委托取消 |
| `last()` | S/E/X | `ObservableLastTest` | All | 已覆盖 | D 沿用无回调观察者的通用 dispose 契约 | 无 | `observable_filtering_test.cpp` 值、空源与上游错误 |
| `last(default)` | S/E/X | `ObservableLastTest` | All | 已覆盖 | D 与无默认重载共享实现 | 无 | `observable_filtering_test.cpp` 默认值与上游错误 |
| `ignoreElements()` | S/E/X/D | `ObservableIgnoreElementsTest` | All | 已覆盖 | 无 | 无 | `observable_filtering_test.cpp` 完成与错误透传 |
| `skip(count)` | S/E/X/B | `ObservableSkipTest` | All | 已覆盖 | D 沿用无回调观察者的通用 dispose 契约 | 无 | `observable_filtering_test.cpp` 0、范围内、超长度与上游错误 |
| `skipLast(count)` | S/E/X/B | `ObservableSkipLastTest` | All | 已覆盖 | D 沿用缓存观察者的通用 dispose 契约 | 无 | `observable_filtering_test.cpp` 0、范围内、超长度与上游错误 |
| `take(count)` | S/E/X/D/B | `ObservableTakeTest` | All | 已覆盖 | 无 | 无 | `observable_filtering_test.cpp` 0、提前完成与错误 |
| `takeLast(count)` | S/E/X/B | `ObservableTakeLastTest` | All | 已覆盖 | D 沿用缓存观察者的通用 dispose 契约 | 无 | `observable_filtering_test.cpp` 0、范围内、超长度与上游错误 |
| `takeUntil(other)` | S/E/X/D/C/L | `ObservableTakeUntilTest` | All | 已覆盖 | 无 | 无 | trigger next/empty/error、main error、订阅顺序，以及下游主动取消释放 main/trigger |
| `takeWhile(predicate)` | S/E/X/D/R | `ObservableTakeWhileTest` | All | 已覆盖 | 无 | 无 | `observable_filtering_test.cpp` 边界与异常 |
| `skipWhile(predicate)` | S/E/X/D/R | `ObservableSkipWhileTest` | All | 已覆盖 | 无 | 无 | `observable_filtering_test.cpp`; 回调失败单终止回归 |
| `groupBy(keySelector)` | S/E/X/D/C/L/GAny 类型 | `ObservableGroupByTest` | All | 已覆盖 | 无 | 无 | 分组、keySelector/上游错误、关闭后订阅、键类型，以及取消完成活动组并释放上游 |
| `groupBy(keySelector, valueSelector)` | S/E/X/D/C/L | `ObservableGroupByTest` | All | 已覆盖 | 无 | 无 | valueSelector 值转换、异常传播与组终止 |
| `window(count)` | S/E/X/D/B/L | `ObservableWindowTest` | All | 已覆盖 | 无 | 无 | 精确/尾窗口、错误、关闭后订阅，以及取消完成活动窗口并释放上游 |
| `window(count, skip)` | S/E/X/D/B/L | `ObservableWindowTest` | All | 已覆盖 | 无 | 无 | 重叠/间隔窗口及非法参数 |
| `timeout(ms, scheduler, fallback)` | S/X/D/C/L | `ObservableTimeoutTest` | Windows/MSVC | 已实测 | dispose 与 timeout 真正同时执行需要生产测试钩子，当前约束禁止，书面豁免 | 无 | 无 fallback 的 Timeout 错误、上游/fallback 错误、正常完成、迟到值与 deadline 前下游取消：`observable_time_test.cpp` |
| `timeout(ms, fallback)` | S/X/D/C/L | `ObservableDefaultTimeSchedulerTest`; `ObservableTimeoutTest` | Windows/MSVC | 已实测 | dispose 与 timeout 真正同时执行同上豁免 | 无 | 默认重载直接验证全局 MainThread Scheduler；核心语义使用显式受控 Scheduler |
| `delay(ms, scheduler)` | S/E/X/D/C/L | `ObservableDelayTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | 无 | 无 | 显式受控 Scheduler 的顺序/完成/dispose/上游错误；默认重载直接验证全局 Scheduler |
| `debounce(ms, scheduler)` | S/E/X/D/C/L | `ObservableDebounceTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | dispose 与 emit 真正同时执行需要生产测试钩子，当前约束禁止，书面豁免 | 无 | 静默窗口、完成刷新、错误、deadline 前下游取消及默认重载 |
| `sample(period, scheduler)` | S/E/X/D/C/L | `ObservableSampleTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | dispose 与 tick 真正同时执行需要生产测试钩子，当前约束禁止，书面豁免 | 无 | 周期最新值、完成、错误、tick 前下游取消及默认重载 |
| `join(other, durations, result)` | S/E/X/D/C/L | `ObservableJoinTest` | All/受控源 | 已覆盖 | 无 | 无 | 开放窗口匹配、duration 关闭排除旧值、duration 源错误、回调异常、源错误及主动取消释放 |
| `startWith(item)` | S | `ObservableStartWithTest` | All | 已覆盖 | E/X/D 由底层 concat 套件覆盖 | 无 | `observable_transformation_test.cpp` 单值前缀 |
| `startWithArray(items)` | S/E | `ObservableStartWithTest` | All | 已覆盖 | X/D 由底层 concat 套件覆盖 | 无 | `observable_transformation_test.cpp` 数组前缀 |
| `startWith(Args...)` | S | `ObservableStartWithTest` | All | 已覆盖 | E/X/D 由底层 concat 套件覆盖 | 无 | `observable_transformation_test.cpp` 可变参前缀 |
| `all(predicate)` | S/E/X/D | `ObservableAllTest` | All | 已覆盖 | 无 | 无 | `observable_aggregation_test.cpp` true/false/空、上游错误、回调异常与取消 |
| `any(predicate)` | S/E/X/D | `ObservableAnyTest` | All | 已覆盖 | 无 | 无 | `observable_aggregation_test.cpp` true/false/空、上游错误、回调异常与取消 |
| `contains(item)` | S/E/X/D/GAny 类型 | `ObservableContainsTest`; `ObservableAnyTest` | All | 已覆盖 | 委托 `any`，错误/取消由 `ObservableAnyTest` 覆盖；非同类型比较沿用 gany 契约 | 无 | `observable_aggregation_test.cpp` 存在、不存在、上游错误与取消 |
| `isEmpty()` | S/E/X/D | `ObservableIsEmptyTest` | All | 已覆盖 | 委托 `all(false)`，错误/取消由 All 套件覆盖 | 无 | `observable_aggregation_test.cpp` 空与非空 |
| `defaultIfEmpty(default)` | S/E/X/D | `ObservableDefaultIfEmptyTest` | All | 已覆盖 | 无 | 无 | `observable_aggregation_test.cpp` 空、非空与错误 |
| `onErrorReturn(default)` | S/E/X/D | `ObservableOnErrorReturnTest` | All | 已覆盖 | 无 | 无 | fallback、成功透传、重复终止，以及下游取消释放上游 |
| `onErrorResumeNext(resumeFunction)` | S/X/D/回调异常 | `ObservableOnErrorResumeNextTest` | All | 已覆盖 | 无 | 无 | 恢复成功、fallback 错误、回调异常、null 返回，以及切换后取消释放 fallback |
| `onErrorResumeNext(next)` | S/X/D | `ObservableOnErrorResumeNextTest` | All | 已覆盖 | 无 | 无 | 固定 fallback 成功、错误与切换后取消释放 |
| `catchError(resumeFunction)` | S/X/D/回调异常 | `ObservableCatchErrorTest` | All | 已覆盖 | 实现是 `onErrorResumeNext` 别名，委托行为单独验证 | 无 | 成功委托与切换后取消释放 fallback |
| `catchError(next)` | S/X/D | `ObservableCatchErrorTest` | All | 已覆盖 | 实现是 `onErrorResumeNext` 别名，委托行为单独验证 | 无 | 成功委托与切换后取消释放固定 fallback |
| `sequenceEqual(source1, source2, comparator, bufferSize)` | S/E/X/D/B/C/L | `ObservableSequenceEqualTest` | All | 已覆盖 | `bufferSize` 当前仅保存未参与算法，按公开默认与显式比较路径测试；非正参数无实现校验，记录为现行限制 | 无 | 相等、长度差、值差、比较器异常、取消及同步错误订阅顺序 |
| `subscribeOn(scheduler)` | S/X/D/C/L | `ObservableSubscribeOnTest` | Windows/MSVC | 已实测 | 调度器拒绝/抛错无公开契约 | 无 | 延迟订阅、值/完成、上游错误与订阅前取消：`scheduler_test.cpp` |
| `observeOn(scheduler)` | S/X/D/C/L | `ObservableObserveOnTest` | Windows/MSVC | 已实测；一次偶发超时待跟踪 | 调度器拒绝/抛错无公开契约 | 无已确认生产缺陷 | 顺序/完成、上游错误、虚拟队列取消及 TaskSystem 排队执行前 dispose 竞态：`scheduler_test.cpp`; `MSVC_VALIDATION_2026-08-14.md` |
| `blockingFirst()` | S/E/X/终止 | `ObservableBlockingFirstTest` | All + 有界异步 | 已覆盖 | 无 | 无 | `blocking_test.cpp`：首值取消、空源、错误及异步终止 |
| `blockingFirst(default)` | S/E/X/终止 | `ObservableBlockingFirstTest` | All + 有界异步 | 已覆盖 | 无 | 无 | `blocking_test.cpp`：空源默认值 |
| `blockingLast()` | S/E/X/终止 | `ObservableBlockingLastTest` | All + 有界异步 | 已覆盖 | 无 | 无 | `blocking_test.cpp`：末值、空源、错误及异步完成 |
| `blockingLast(default)` | S/E/X/终止 | `ObservableBlockingLastTest` | All + 有界异步 | 已覆盖 | 无 | 无 | `blocking_test.cpp`：空源默认值 |
| `blockingForEach(onNext)` | S/E/X/回调异常/终止 | `ObservableBlockingForEachTest` | All + 有界异步 | 已覆盖 | 无 | 无 | `blocking_test.cpp`：顺序、空源、错误、回调异常、取消及异步完成 |
| `subscribe(ObserverPtr)` | S/X/D/R/L | `ObservableSubscribeTest` | All | 已覆盖 | 错误、重复终止和生命周期由 Observer/LambdaObserver 契约套件共同覆盖 | 无 | `core_lifecycle_test.cpp` 独立验证 Observer 重载委托与完整信号 |
| `subscribe(next,error,complete)` | S/E/X/D/R/L | `ObservableSubscribeTest` | All | 已覆盖 | 错误与回调异常由 `LambdaObserverTest` 和回调回归套件共享 | 无 | `core_lifecycle_test.cpp` 验证值、完成及返回 Disposable |
| `subscribe(next)` | S/X/D/L | `ObservableSubscribeTest` | All | 已覆盖 | 错误回调不属于单参数签名 | 无 | `core_lifecycle_test.cpp` 验证活动订阅返回值与主动取消 |

## 3. 53 个算子头文件矩阵

“套件”是每个头文件对应的 GoogleTest 套件名。现有 53 个头文件均已列出并映射到至少一个 GoogleTest 套件。

| 算子头文件 | 行为场景 | 目标测试套件 | 平台要求 | 当前状态 | 豁免理由 | 关联缺陷 | 验证证据 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `observable_all.h` | S/E/X/D/谓词异常 | `ObservableAllTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_aggregation_test.cpp` |
| `observable_amb.h` | 竞争获胜、失败、取消所有源 | `ObservableAmbTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_combination_test.cpp` |
| `observable_any.h` | S/E/X/D/谓词异常 | `ObservableAnyTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_aggregation_test.cpp` |
| `observable_buffer.h` | 重叠/间隔、尾缓冲、B/D/X | `ObservableBufferTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_transformation_test.cpp`; `observable_lifecycle_test.cpp` |
| `observable_combine_latest.h` | 多源顺序、E/X/D/C/L | `ObservableCombineLatestTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_combination_test.cpp` |
| `observable_concat_map.h` | 串行、队列、X/D/L | `ObservableConcatMapTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_transformation_test.cpp`; `observable_combination_test.cpp` |
| `observable_create.h` | 发射契约、X/D/R/L | `ObservableCreateTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_creation_test.cpp`; `observable_callback_test.cpp` |
| `observable_debounce.h` | 时间窗口、终止、D/C | `ObservableDebounceTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | dispose/emit 真正同刻竞态因禁止生产测试钩子而豁免 | 无已确认缺陷 | 受控时间、错误、下游取消与默认 Scheduler：`observable_time_test.cpp` |
| `observable_default_if_empty.h` | 非空/空/X/D | `ObservableDefaultIfEmptyTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_aggregation_test.cpp` |
| `observable_defer.h` | 每订阅获取源、X/D | `ObservableDeferTest` | All | 已覆盖 | 工厂延迟求值不属于现有签名 | 无已确认缺陷 | `observable_creation_test.cpp` |
| `observable_delay.h` | 值/错误/完成延迟、D/C | `ObservableDelayTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | 无 | 无已确认缺陷 | 受控值/完成、即时错误、取消与默认 Scheduler：`observable_time_test.cpp` |
| `observable_distinct.h` | 默认键/选择器/GAny 类型/X/D | `ObservableDistinctTest` | All | 已覆盖 | GAny 混合类型比较由依赖契约覆盖 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_distinct_until_changed.h` | 各重载/比较异常 | `ObservableDistinctUntilChangedTest` | All | 已覆盖 | 上游错误/取消与同一观察者实现及通用 dispose 契约共享 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_do_on_each.h` | 五类副作用、异常、finally 完成/错误/取消仅一次 | `ObservableDoOnEachTest` | All | 已覆盖 | finally 异常按实现吞掉，无可观察下游信号 | 无已确认缺陷 | `observable_transformation_test.cpp`; `observable_callback_test.cpp` |
| `observable_element_at.h` | 命中、越界、默认、上游错误、命中后取消上游 | `ObservableElementAtTest`; `ObservableFirstTest` | All | 已覆盖 | 无用户回调异常路径 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_empty.h` | 无值完成 | `ObservableEmptyTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_creation_test.cpp` |
| `observable_error.h` | 单一错误 | `ObservableErrorTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_creation_test.cpp` |
| `observable_filter.h` | 通过/拒绝/异常/D/R | `ObservableFilterTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_flat_map.h` | 多 inner、X/D/C/L | `ObservableFlatMapTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_transformation_test.cpp`; `observable_combination_test.cpp` |
| `observable_from_array.h` | 顺序、空、D | `ObservableFromArrayTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_creation_test.cpp` |
| `observable_group_by.h` | 键/值、组生命周期、X/D | `ObservableGroupByTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_lifecycle_test.cpp` |
| `observable_ignore_elements.h` | 忽略值、透传终止/X/D | `ObservableIgnoreElementsTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_interval.h` | 周期、初延迟、D/C/L | `ObservableIntervalTest` | Windows/MSVC | 已实测 | 墙钟精度属于 gx 集成限制 | 无已确认缺陷 | 初始延迟先后、三次周期顺序与取消期限：`observable_time_test.cpp` |
| `observable_join.h` | 生命周期窗口、顺序/X/D/C/L | `ObservableJoinTest` | All/受控源 | 已覆盖 | 无 | 无已确认缺陷 | duration 关闭/错误、源错误和主动取消均有释放断言 |
| `observable_just.h` | 单/多值、顺序/D | `ObservableJustTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_creation_test.cpp` |
| `observable_last.h` | S/E/X/default | `ObservableLastTest` | All | 已覆盖 | D 沿用通用 dispose 契约 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_map.h` | S/E/X/D/R | `ObservableMapTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_transformation_test.cpp`; `observable_callback_test.cpp` |
| `observable_never.h` | 无信号、D/L | `ObservableNeverTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_creation_test.cpp` |
| `observable_observe_on.h` | 线程切换、队列、X/D/C/L | `ObservableObserveOnTest` | Windows/MSVC | 已实测；一次偶发超时待跟踪 | 调度器拒绝/抛错无公开契约 | 无已确认缺陷 | 顺序、上游错误、虚拟队列取消及 TaskSystem 排队竞态：`scheduler_test.cpp`; `MSVC_VALIDATION_2026-08-14.md` |
| `observable_on_error_resume_next.h` | 两重载、恢复失败、D | `ObservableOnErrorResumeNextTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_error_test.cpp` |
| `observable_on_error_return.h` | 回退值、完成、D | `ObservableOnErrorReturnTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_error_test.cpp` |
| `observable_range.h` | S/E/D/负数/上界/溢出 | `ObservableRangeTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_range_regression_test.cpp` |
| `observable_reduce.h` | S/E/回调异常 | `ObservableReduceTest` | All | 已覆盖 | D 沿用通用聚合 dispose 契约 | 无已确认缺陷 | `observable_aggregation_test.cpp` |
| `observable_repeat.h` | 次数、0、X/D | `ObservableRepeatTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_aggregation_test.cpp` |
| `observable_retry.h` | 次数/无限、成功、D | `ObservableRetryTest` | All | 已覆盖 | 无限持续失败取消留待并发任务 | 无已确认缺陷 | `observable_error_test.cpp` |
| `observable_sample.h` | 周期采样、终止、D/C | `ObservableSampleTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | dispose/tick 真正同刻竞态因禁止生产测试钩子而豁免 | 无已确认缺陷 | 受控时间、错误、下游取消与默认 Scheduler：`observable_time_test.cpp` |
| `observable_scan.h` | 累计、空首值、回调异常 | `ObservableScanTest` | All | 已覆盖 | 上游错误/取消沿用无状态透传与通用 dispose 契约 | 无已确认缺陷 | `observable_aggregation_test.cpp` |
| `observable_sequence_equal.h` | 相同/长度/值/错误顺序/D | `ObservableSequenceEqualTest` | All | 已覆盖 | `bufferSize` 不参与现行实现 | 无已确认缺陷 | `observable_combination_test.cpp` |
| `observable_skip.h` | 0/小于/大于长度/X | `ObservableSkipTest` | All | 已覆盖 | D 沿用通用 dispose 契约 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_skip_last.h` | 0/边界/缓存/X | `ObservableSkipLastTest` | All | 已覆盖 | D 沿用缓存观察者的通用 dispose 契约 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_skip_while.h` | 切换点、异常、仅终止一次 | `ObservableSkipWhileTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_filtering_test.cpp`; `observable_callback_test.cpp` |
| `observable_start_with.h` | 单值/数组/可变参 | `ObservableStartWithTest` | All | 已覆盖 | E/X/D 由底层 concat 套件覆盖 | 无已确认缺陷 | `observable_transformation_test.cpp`; `observable_combination_test.cpp` |
| `observable_subscribe_on.h` | 订阅线程、X/D/C/L | `ObservableSubscribeOnTest` | Windows/MSVC | 已实测 | 调度器拒绝/抛错无公开契约 | 无已确认缺陷 | `scheduler_test.cpp` |
| `observable_switch_map.h` | 最新 inner、空映射、D/C/L | `ObservableSwitchMapTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_transformation_test.cpp`; `observable_combination_test.cpp` |
| `observable_take.h` | 0/边界/提前取消/X | `ObservableTakeTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_take_last.h` | 0/边界/缓存/X | `ObservableTakeLastTest` | All | 已覆盖 | D 沿用缓存观察者的通用 dispose 契约 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_take_until.h` | other next/empty/error、D/C/L | `ObservableTakeUntilTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_combination_test.cpp` |
| `observable_take_while.h` | 切换点、异常、D/R | `ObservableTakeWhileTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_filtering_test.cpp` |
| `observable_timeout.h` | 超时/不超时/上游与 fallback 错误/D/C/L | `ObservableTimeoutTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | dispose/timeout 真正同刻竞态因禁止生产测试钩子而豁免 | 无已确认缺陷 | 受控时间、无 fallback Timeout 错误、上游/fallback 错误、下游取消、迟到值回归与默认 Scheduler：`observable_time_test.cpp` |
| `observable_timer.h` | 延时一次、D/C/L | `ObservableTimerTest` | Windows/MSVC | 已实测 | 墙钟精度属于 gx 集成限制 | 无已确认缺陷 | 跨第二期限的单次完成与正延迟取消：`observable_time_test.cpp` |
| `observable_to_array.h` | S/E/X/D | `ObservableToArrayTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_transformation_test.cpp` |
| `observable_window.h` | 精确/重叠/间隔/B/D/L | `ObservableWindowTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_lifecycle_test.cpp` |
| `observable_zip.h` | 顺序、短源、E/X/D/C/L | `ObservableZipTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `observable_combination_test.cpp` |

## 4. 核心设施矩阵

| 组件 | 行为场景 | 目标测试套件 | 平台要求 | 当前状态 | 豁免理由/限制 | 关联缺陷 | 验证证据 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `Observer` 接口 | 信号顺序、单终止、终止后静默 | `ObserverContractTest` | All | 已覆盖 | 抽象接口通过 `TestObserver` 验证 | 无已确认缺陷 | `core_lifecycle_test.cpp` |
| `LambdaObserver` | 回调、回调异常转 `onError`、终止回调异常隔离、dispose、仅终止一次、释放上游 | `LambdaObserverTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `core_lifecycle_test.cpp`; `observable_callback_test.cpp` |
| `Emitter` / `ObservableEmitter` | 信号契约、setDisposable 替换、终止/取消后静默、下游回调异常 | `ObservableEmitterTest` | All | 已实测 | 具体实现位于 create 算子内部 | 无已确认缺陷 | `CreateEmitter::onNext` 将下游异常转换为 `onError` 并释放；`core_lifecycle_test.cpp`; `observable_creation_test.cpp` |
| `Disposable` | dispose 幂等、状态可见 | `AtomicDisposableTest`; `SequentialDisposableTest`; `DisposableHelperTest` | All | 已覆盖 | 抽象接口通过测试替身及具体实现验证 | 无已确认缺陷 | `core_lifecycle_test.cpp` |
| `AtomicDisposable` | 幂等、并发状态 | `AtomicDisposableTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `core_lifecycle_test.cpp` |
| `SequentialDisposable` | update/replace、旧值释放、终止后新值立即释放、并发 | `SequentialDisposableTest` | All | 已覆盖 | 无 | 无已确认缺陷 | `core_lifecycle_test.cpp` |
| `DisposableHelper` | disposed sentinel、setOnce/set/replace/dispose 所有权、null 清空、终止后赋值、trySet 各安全输入、validate 合法输入、dispose/trySet 竞态 | `DisposableHelperTest` | All | 已覆盖 | `setOnce(nullptr/重复设置)` 与 `validate(nullptr next/non-null current)` 属协议违规并触发 Debug 断言，不能作为进程内 GoogleTest 执行；通过合法分支及各调用方契约测试覆盖非违规行为 | 无已确认缺陷 | `core_lifecycle_test.cpp`：所有权、set/replace null、终止后拒绝并释放、trySet null/成功/拒绝、validate 合法输入及 dispose/trySet 竞态 |
| `Worker` | `schedule()`、`schedule(delay)`、`now()`、取消、关闭后拒绝、竞态 | `WorkerContractTest` | Windows/MSVC | 已实测 | 平台特有竞态按具体 Worker 映射 | 无已确认缺陷 | `now()` 非零且非递减；虚拟 Worker 契约与平台 Worker：`test_infrastructure_test.cpp`; `scheduler_test.cpp` |
| `DisposeTask` | 子任务和 Worker 联动取消、幂等、状态 | `DisposeTaskTest` | All | 已实测 | 无 | 无已确认缺陷 | 连续两次 dispose 后任务和 Worker 均保持 disposed：`scheduler_test.cpp` |
| `Scheduler` | start/shutdown、scheduleDirect、Worker 生命周期 | `SchedulerContractTest` | Windows/MSVC | 已实测 | 基类 `start/shutdown` 为空操作，无可观察状态 | 无已确认缺陷 | direct 顺序/取消：`scheduler_test.cpp` |
| `NewThreadScheduler` / `NewThreadWorker` | 线程、顺序、延迟、取消、关闭、竞态 | `NewThreadSchedulerTest`; `NewThreadWorkerTest` | Windows/MSVC | 已实测 | 回调与 dispose 真正同刻需要生产测试钩子；当前验证 dispose 赢得延迟提交竞态 | 无已确认缺陷 | 同一 Worker 立即任务先于延迟任务、任务取消、Worker dispose 赢得延迟提交竞态：`scheduler_test.cpp` |
| `TimerScheduler` / `TimerWorker` | 定时、取消、全局调度器依赖、关闭 | `TimerSchedulerTest` | Windows/MSVC | 已实测 | 墙钟精度属于 gx 集成限制 | 无已确认缺陷 | 立即/正延迟顺序、任务取消、Worker 关闭拒绝：`scheduler_test.cpp` |
| `MainThreadScheduler` | 全局计时器集成、创建/取消 | `MainThreadSchedulerTest`; `ObservableDefaultTimeSchedulerTest` | Windows/MSVC | 已实测 | 无 | 无已确认缺陷 | 全局 Scheduler 集成、延迟取消、默认算子重载及 RAII 恢复：`scheduler_test.cpp`; `observable_time_test.cpp`; `support/test_scheduler.h` |
| `JobSystemScheduler` / `JobSystemWorker` | 立即顺序/延迟、取消、关闭、竞态 | `JobSystemSchedulerTest`; `JobSystemWorkerTest` | Windows/MSVC | 已实测 | 延迟路径在 timer 线程调用 `GJobSystem::createJob`，违反 gx “线程须 adopt”前置条件，禁止修改生产代码下书面豁免 | 生产风险：延迟路径线程前置条件不满足 | 两个立即任务提交顺序、关闭拒绝，以及唯一 worker 被占用时取消已排队任务：`scheduler_test.cpp` |
| `TaskSystemScheduler` / `TaskSystemWorker` | 立即/延迟顺序、取消、内部 timer 生命周期 | `TaskSystemSchedulerTest`; `TaskSystemWorkerTest` | Windows/MSVC | 已实测；observeOn 清理一次偶发超时待跟踪 | 回调与 dispose 真正同刻需要生产测试钩子；当前验证 dispose 赢得排队执行竞态 | 无已确认缺陷 | 同一 Worker 立即任务先于延迟任务、任务取消、dispose 赢得排队竞态：`scheduler_test.cpp`; `MSVC_VALIDATION_2026-08-14.md` |
| `BlockingFirstObserver` / `blockingFirst` | 首值取消、空源、默认、错误、有界异步 | `ObservableBlockingFirstTest` | All + 有界异步 | 已覆盖 | 无 | 无已确认缺陷 | `blocking_test.cpp` |
| `BlockingLastObserver` / `blockingLast` | 末值、空源、默认、错误、有界异步 | `ObservableBlockingLastTest` | All + 有界异步 | 已覆盖 | 无 | 无已确认缺陷 | `blocking_test.cpp` |
| `BlockingForEachObserver` / `blockingForEach` | 顺序、空源、错误、回调异常、终止 | `ObservableBlockingForEachTest` | All + 有界异步 | 已覆盖 | 无 | 无已确认缺陷 | `blocking_test.cpp` |
| `LeakObserver` | Debug 结束检查、输出审查、各类计数归零 | 全部 CTest 用例 | Windows/MSVC Debug | 已门禁并完成定向实测 | 无公开查询/返回值接口，不能直接 ASSERT；直接运行可执行文件仍需人工审查 | 无已确认缺陷 | `test_main.cpp`; `tests/CMakeLists.txt` 的 `FAIL_REGULAR_EXPRESSION` |

## 5. 已知回归映射

| 回归主题 | 自动化测试 | 状态 | 尚缺场景 |
| --- | --- | --- | --- |
| range 负起点、`int64_t` 上界、溢出、dispose | `ObservableRangeRegressionTest.*`; `ObservableRangeTest.*` | 已覆盖 | 无；range 不接收上游，不存在上游错误路径 |
| zip 完成一次、空源、onNext 内 dispose | `ObservableCombinationRegressionTest.Zip*`; `ObservableZipTest.*` | 已覆盖 | 多源真实线程同时竞争留待并发稳定性任务 |
| flatMap inner 错误、main 错误取消 inner | `ObservableFlatMapTest.*`; `ObservableCombinationRegressionTest.FlatMapForwardsInnerErrorOnce`; `ObservableFlatMapRegressionTest.MainErrorCancelsActiveInner` | 已覆盖 | 无 |
| combineLatest onNext 内 dispose、combiner 标准异常 | `ObservableCombineLatestTest.*`; `ObservableCombinationRegressionTest.CombineLatestAllowsDisposalFromOnNext`; `ObservableCallbackRegressionTest.CombineLatestStandardExceptionBecomesOnError` | 已覆盖 | 多源真实线程同时竞争留待并发稳定性任务 |
| takeUntil 空触发源 | `ObservableTakeUntilTest.*`; `ObservableTakeUntilRegressionTest.EmptyTriggerDoesNotStopMainSource` | 已覆盖 | 多线程同刻终止留待并发稳定性任务 |
| switchMap 空 mapper、活动 inner 切换与等待最新 inner | `ObservableSwitchMapTest.*`; `ObservableSwitchMapRegressionTest.*` | 已覆盖 | 多线程 inner 竞争留待并发稳定性任务 |
| window 生命周期、非法参数 | `ObservableWindowTest.*`; `ObservableLifetimeRegressionTest.ClosedWindowRemainsSubscribable`; `ObservableParameterRegressionTest.RejectsInvalidBufferAndWindowArguments` | 已覆盖 | 无 |
| groupBy 生命周期、不同 GAny 键类型 | `ObservableGroupByTest.*`; `ObservableLifetimeRegressionTest.ClosedGroupRemainsSubscribable`; `ObservableGroupByRegressionTest.KeepsDifferentKeyTypesSeparate` | 已覆盖 | 组对象释放只能通过最终 LeakObserver 输出审查 |
| 回调异常转换 | `ObservableCallbackRegressionTest.*`; 各算子对应 `*Test` | 已覆盖 | 无；公开回调位置的异常证据见第 2、3 节对应行 |
| sequenceEqual 订阅顺序 | `ObservableSequenceEqualTest.*`; `ObservableSequenceEqualRegressionTest.DoesNotSubscribeSecondSourceAfterSynchronousError` | 已覆盖 | `bufferSize` 未参与现行实现，非正参数无校验 |
| observeOn dispose 丢弃队列 | `ObservableObserveOnTest.*`; `ObservableObserveOnRegressionTest.DisposeBeforeDrainDropsQueuedValues` | 已覆盖并实测；一次偶发超时待跟踪 | 调度器拒绝/抛错及真正同刻竞态按上文豁免 |
| timeout 拒绝迟到值 | `ObservableTimeoutTest.*`; `ObservableTimeoutRegressionTest.TimeoutRejectsLateSourceValue` | 已覆盖并实测 | dispose 与 timeout 真正同刻竞态按上文豁免 |
| concatMap 同步队列排空 | `ObservableConcatMapRegressionTest.DrainsAllQueuedSynchronousValues`; `ObservableConcatMapTest.*` | 已覆盖 | 无 |
| amb 协调器取消全部源 | `ObservableAmbTest.*`; `ObservableAmbRegressionTest.DisposingCoordinatorCancelsEverySource` | 已覆盖 | 首值、错误及无值完成均可获胜；双线程首信号竞争使用 1 秒有界等待 |

## 6. CMake、CTest 与示例冲突审计

- 根 `CMakeLists.txt:9,64-77` 以 `BUILD_RX_TESTS` 接入 GoogleTest 和 `tests/`；`tests/CMakeLists.txt` 构建单一 `test_rx`，并通过 `gtest_discover_tests` 将用例注册到 CTest。
- GoogleTest 支持 `--gtest_filter=Suite.Name` 过滤单套件/单用例；CTest 可用 `ctest -R '^Suite\.'` 过滤发现后的独立测试。命令见 `tests/README.md`。
- `test_main.cpp` 在 `RUN_ALL_TESTS()` 后执行 `LeakObserver::checkLeak()`；CTest 通过 `FAIL_REGULAR_EXPRESSION "Object Leak:"` 将泄漏输出转为失败，并为每个发现后的用例设置 10 秒进程超时。直接运行 `test_rx.exe` 时返回码仍只取 GoogleTest 结果，需要人工审查泄漏输出。
- `examples/test_rx.cpp` 自带 `main()`，由 `examples/CMakeLists.txt` 的可选目标单独构建；与 `tests/test_main.cpp` 不在同一可执行文件，不存在入口符号冲突。风险来自两套覆盖口径：演示输出不能被算作 GoogleTest 断言证据，后续新增测试也不得迁移或删减演示函数。
- `tests/support/` 已提供共享 `TestObserver`、事件记录、虚拟时间 `TestScheduler` / `TestWorker` 和条件变量 `BoundedWait`；设施契约由 `test_infrastructure_test.cpp` 覆盖。创建、变换、过滤和错误基础测试已按职责拆分，后续测试文件命名约定见 `tests/README.md`。

## 7. 覆盖闭合状态与实施优先级

1. **P0 基础设施（已实现并完成 MSVC 验证）**：共享 TestObserver/事件记录/受控 Worker/有界等待、职责化文件命名和独立 CTest 发现；现有测试名保持稳定。
2. **P0 核心契约（已实现并实测）**：Observable 析构与三个 subscribe 重载、Observer、Emitter、Disposable、Scheduler/Worker/DisposeTask 与 blocking API 均有严格测试。
3. **P1 算子成功路径（已建立）**：53 个算子头文件均已映射到测试套件，后续按矩阵中的豁免和增量实测状态维护。
4. **P1 重要算子完整语义（已实测）**：timeout、subscribeOn、observeOn 的错误路径及重点算子的成功/错误/取消/边界已有断言；历史 206 项全量通过，新增 `elementAt`/`first` 错误与取消回归 2/2 定向通过。
5. **P2 平台与质量证据（配置已闭合）**：Windows/MSVC Debug 已完成过滤测试、全量 CTest 和连续 10 次稳定性；`RX_ENABLE_COVERAGE` 与 `RX_ENABLE_SANITIZERS` 提供 Clang/GCC 配置，运行与报告命令见 `tests/README.md`。

## 8. 审批检查点、证据与风险

### 构建/测试审批检查点

- 历史状态：2026-08-13 人类曾授权并完成 `build-gtest` 构建、定向测试、全量 CTest 与时间/调度用例 10 次重复运行。
- 当前状态：2026-08-14 已获得明确人类授权，并完成独立目录 Windows/MSVC Debug 配置、构建、过滤测试、当前全量 206/206、连续 10 轮 206/206 和 LeakObserver 输出审查。

### 本次验证

- 完整读取 Workshop 的 `requirement.md`、`task.md`、`dependencies.md`、`previous-runs.md`、`project-profile.md`、`messages.md`。
- 检查当前及父仓库 Git 状态、HEAD、提交统计与文件清单。
- 枚举 `observable.h` 公开 API、53 个 `rx/include/rx/operators/*.h`、核心设施头文件、现有 GoogleTest 和测试相关 CMake。
- 审计 `examples/test_rx.cpp` 的演示入口与覆盖重叠。
- 历史 `cmake --build build-gtest --config Debug --target test_rx`：通过。
- 2026-08-14 时间、Scheduler 与 Worker 相关 41 个 CTest 全部通过，并连续 10 轮保持 41/41 通过；无超时、死锁或泄漏报告。
- 2026-08-14 矩阵闭合后 Windows/MSVC Debug 全量 CTest 206/206 通过，随后连续 10 轮均为 206/206（23.75–29.02 秒）。完整证据见 `MSVC_VALIDATION_2026-08-14.md`。
- 2026-08-15 最终审查返工后 MSVC Debug 增量构建成功；`ObservableElementAtTest` 与 `ObservableFirstTest` 新增回归直接运行及 CTest 过滤均为 2/2。随后补齐 `DisposableHelper` 终止后赋值、trySet 安全输入和 validate 合法输入，CTest 发现 210 项，`DisposableHelperTest.*` 直接运行及 CTest 过滤均为 4/4。按收口约束未重复全量 CTest 或稳定性循环。
- `JobSystemWorkerTest.DisposedQueuedTaskDoesNotRun` 已完成单用例和全量实测。
- 第一组稳定性第 6 轮曾出现 `ObservableObserveOnTest.DisposalWinsBeforeQueuedTaskSystemDrain` 单次 10 秒超时；随后该用例独立 50 次及第二组全量 10 轮均通过，未确认生产缺陷，保留为剩余稳定性风险。
- CTest 已为每个发现后的用例设置 10 秒超时并将 `Object Leak:` 设为失败正则；过滤、全量、第二组 10 轮和直接可执行文件输出均未出现泄漏报告。
- `git diff --check` 无空白错误；仅 Git 报告已有 `tests/CMakeLists.txt` 工作区行尾转换警告。

### 剩余风险

- 当前矩阵描述“现行实现”和“目标契约测试”，并不证明基线提交的语义正确；只有后续严格测试与产品契约评审能确认。
- `JobSystemWorker` 延迟路径仍从 timer 线程调用要求线程已 `adopt()` 的 `GJobSystem::createJob`；当前禁止修改生产代码，因此保留书面豁免和生产风险。
- debounce/sample/timeout 及平台 Worker 的回调与 dispose 真正同刻竞态需要生产测试钩子；当前约束禁止新增接口，已验证 deadline/排队执行前 dispose 并保留书面豁免。
- `LeakObserver` 缺少可断言接口；CTest 已将其 Debug 输出接入失败门禁，但直接运行测试可执行文件仍需人工审查输出。
- 覆盖率报告通过 `gcovr --filter rx/` 或 `llvm-cov ... rx` 仅统计 RX 自有源码；目标为行覆盖率 90%、分支覆盖率 80%。当前 Windows 仅发现 VS 附带的 Clang/LLVM 工具，未发现 GCC、gcovr、lcov 或 GNU 前端 Clang 环境，因此覆盖率数值和 ASan/UBSan 结果仍是环境受限项。

## 9. 最终交付索引与缺陷状态

- 唯一独立 `test_rx` 验证任务 ID：`dbc3ded5-bd00-4a0f-8f79-f95f9fb96469`。未新增重复验证任务，也未将 Clang 单元测试作为 Windows/MSVC 验收条件。
- 覆盖矩阵闭合：`observable.h` 共 96 个公开条目，53 个算子头文件为 53/53；核心设施、书面豁免和回归映射见第 2 至第 5 节。
- Windows/MSVC 命令、203/203、206/206、连续稳定性、LeakObserver 审查和 Emitter 缺陷时间线见 `MSVC_VALIDATION_2026-08-14.md`。
- 当前确认缺陷：无阻塞默认 CTest 的确认缺陷。Emitter 下游回调异常缺陷已由现行生产修复消除，严格回归测试仍在默认 CTest 中。
- 非阻塞风险：TaskSystem `observeOn` 历史单次不可重复超时、JobSystemWorker 延迟线程 adopt 前置条件、真正同刻 dispose 竞态缺少生产测试钩子、覆盖率和 Sanitizer 未在当前环境实测。
- 后续建议：在 Linux 或具备 GNU 前端 Clang/GCC、gcovr/llvm-cov 的环境执行 `tests/README.md` 中的覆盖率与 Sanitizer 命令；只有再次出现稳定失败或需要长期压力证据时，才新增独立调查任务。
