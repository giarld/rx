# Windows/MSVC Debug 验证证据（2026-08-14）

## 授权与环境

- 人类授权：OpenWorkshop 主任务 `026f6ae6-5a96-4792-a714-8e658546324b` 的持久评论 `4b197207-3585-4828-a361-856a74fcc0c5` 记录 `2026-08-14T02:00:50.621Z: @Agent 授予编译并运行 test_rx 的权限，请继续推进`。
- 唯一独立 `test_rx` 验证任务：`dbc3ded5-bd00-4a0f-8f79-f95f9fb96469`；最终收口不新增或重复验证任务。
- 构建目录：`build-msvc-validation`，未复用历史构建产物。
- CMake：4.4.2；生成器：Visual Studio 17 2022；平台：x64。
- 编译器：MSVC 19.44.35222.0；工具集路径版本：14.44.35207。
- Windows SDK：10.0.26100.0。

## 命令与结果

```powershell
cmake -S . -B build-msvc-validation -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug -DBUILD_RX_TESTS=ON
cmake --build build-msvc-validation --config Debug --target test_rx
build-msvc-validation\bin\test_rx.exe --gtest_filter=ObservableTimeoutTest.*
build-msvc-validation\bin\test_rx.exe --gtest_filter=JobSystemWorkerTest.DisposedQueuedTaskDoesNotRun
ctest --test-dir build-msvc-validation -C Debug --output-on-failure
```

- 配置成功；多配置生成器忽略 `CMAKE_BUILD_TYPE`，实际 Debug 配置由 `--config Debug` 指定。
- 首次构建的外层命令在 120 秒工具时限被中断，并留下无编译子进程的孤立 MSBuild；清理该进程后增量构建成功，`test_rx.exe` 正常生成。
- MSVC 编译无 RX 或测试错误。仅 gx 依赖的 `gstring.cpp` 出现既有 C4018 有符号/无符号比较警告。
- 单套件：`ObservableTimeoutTest.*`，6/6 通过。
- 单用例：`JobSystemWorkerTest.DisposedQueuedTaskDoesNotRun`，1/1 通过。
- 矩阵闭合前首次全量 CTest：203/203 通过，24.26 秒。
- 矩阵闭合前直接运行 `test_rx.exe`：100 个套件、203/203 通过，退出码 0；人工扫描 `Object Leak:` 为 0 行。
- 矩阵闭合新增 `ObservableLifetimeTest` 与 `ObservableSubscribeTest` 共 3 项：3/3 通过。
- 矩阵闭合后全量 CTest：206/206 通过，26.68 秒。

## 稳定性

第一组连续 10 轮中，第 6 轮的 `ObservableObserveOnTest.DisposalWinsBeforeQueuedTaskSystemDrain` 被 CTest 的 10 秒门限终止；其余 9 轮均为 203/203。失败日志保存在 `build-msvc-validation/ctest-stability-06.log`。该用例随后通过 CTest 独立重复 50 次，均通过，未稳定复现，当前不足以确认生产缺陷。

第二组连续 10 轮全量 CTest 均为 203/203，通过时间为 22.45–23.83 秒；每轮 `Object Leak:`、`***Timeout` 和失败摘要均为 0。日志为 `build-msvc-validation/ctest-stability-retry-01.log` 至 `ctest-stability-retry-10.log`。

矩阵闭合后再次连续运行 10 轮当前全量测试，每轮均为 206/206，通过时间为 23.75–29.02 秒；每轮 `Object Leak:`、`***Timeout` 和失败摘要均为 0。日志为 `build-msvc-validation/ctest-matrix-closure-01.log` 至 `ctest-matrix-closure-10.log`。

## Emitter 缺陷时间线与当前状态

1. `ObservableEmitterTest.DownstreamCallbackFailuresDoNotEscape` 首次加入后连续 10 次稳定失败；当下游 `onNext` 抛出 `next callback failure` 时，异常从 `CreateEmitter::onNext` 逸出。同期全量 CTest 为 184/188，通过 184 项，Emitter 与三个 Scheduler 用例失败。
2. 人工直接运行 `test_rx` 后在 `core_lifecycle_test.cpp` 的同一断言再次复现，确认是生产缺陷而非随机测试失败。
3. 后续生产修复在 `rx/include/rx/operators/observable_create.h` 的 `CreateEmitter::onNext` 捕获下游异常，使用 `ExceptionHelper::fromCurrentException` 转换并进入既有 `onError` 终止路径。该生产改动早于本最终收口任务，本任务未修改它。
4. 修复后的历史直接运行先取得 202/202；完成 JobSystemWorker 回归后，独立 MSVC 验证取得 203/203；补齐 `ObservableLifetimeTest` 与 `ObservableSubscribeTest` 三项后取得 206/206，并连续 10 轮保持 206/206。
5. 当前源码下 Emitter 回归测试仍注册在默认 CTest 中且已通过；最终审查返工后默认 CTest 发现 210 项，Emitter 不再阻塞验收。若该行为回归，该用例会直接使默认 CTest 失败，不存在跳过、豁免或弱化断言。

## 最终收口审计（2026-08-15）

- `tests/COVERAGE_MATRIX.md` 静态核对得到 `observable.h` 96 个公开条目（95 个公开方法声明和 1 个虚析构）、53/53 个算子头文件；每个算子行至少引用一个当前源码中存在的 GoogleTest 套件。
- 最终收口仅在 `tests/CMakeLists.txt` 为 `test_rx` 补齐 GCC/Clang coverage 编译 instrumentation，使测试翻译单元中实例化的 RX 头文件模板能进入覆盖率报告；未修改测试源码。按任务约束只执行一次最小 MSVC 增量构建、CTest 发现和过滤验证，不重复全量 CTest 或稳定性循环，其余复用上述 206/206 与连续 10 轮证据。
- 最小验证结果：CMake 自动重新生成成功，`cmake --build build-msvc-validation --config Debug --target test_rx` 成功；`ctest -N` 发现 206 项；`ObservableEmitterTest.DownstreamCallbackFailuresDoNotEscape` 过滤运行 1/1 通过（0.11 秒）。
- 当前工作区中的 `observable_amb.h`、`observable_create.h`、`observable_group_by.h`、`observable_window.h` 是前序任务留下的生产改动；最终收口完整保留，未覆盖或继续修改。`examples/test_rx.cpp`、依赖源码和 CI 未修改。
- GCC/Clang 覆盖率与 ASan/UBSan 配置见 `tests/CMakeLists.txt` 和 `tests/README.md`。当前环境只有 Visual Studio 附带的 LLVM/MSVC 前端，缺少 GCC、gcovr 和 lcov，未实测覆盖率数值或 Sanitizer；该环境限制不阻塞 Windows/MSVC 验收。
- 仓库未提供 `.clang-format` 或 `.clang-tidy`。`clang-format --dry-run --Werror` 已执行，但默认 LLVM 风格与 `AGENTS.md` 规定的 Allman/OTBS 混合风格不兼容，不能作为可靠门禁；`clang-tidy -p cmake-build-debug` 对本次两个 C++ 文件执行成功，未输出用户代码诊断，仅抑制依赖代码警告。最终格式仍以人工风格审查、`git diff --check` 和 MSVC 编译诊断为依据。
- 2026-08-15 审查返工补齐 `last`、`skip`、`skipLast`、`takeLast` 的上游错误断言，修正 range/concatMap 回归映射，并确保 blocking 异步用例在断言前释放信号并 `join`。MSVC 增量构建成功；相关 11 个 GoogleTest 全部通过；`ctest -N` 仍发现 206 项，过滤运行 `ObservableBlockingFirstTest.HandlesEmptyDefaultErrorAndAsynchronousTermination` 为 1/1 通过。README 修正后的 GoogleTest 单用例 1/1、CTest 单套件 2/2 均通过。未重复全量 CTest 或稳定性循环。
- 2026-08-15 最终审查返工补齐 `elementAt` 两个重载的上游错误、命中后上游取消证据，以及 `first` 两个公开重载的上游错误证据；矩阵不再以“不适用”替代真实断言。MSVC 增量构建成功；新增 GoogleTest 直接运行 2/2 通过；`ctest -N` 发现 208 项，CTest 过滤运行新增用例 2/2 通过。按单一收口任务约束未重复全量 CTest 或稳定性循环，历史全量 206/206 与连续 10 轮证据继续复用。
- 2026-08-15 阻塞审查返工补齐 `DisposableHelper` 的 set/replace null、终止后 set/setOnce/replace、trySet null/成功/拒绝及 validate 合法输入测试；setOnce/validate 的协议违规分支会触发 Debug 断言，矩阵明确书面豁免。MSVC 增量构建成功；`DisposableHelperTest.*` 直接运行 4/4、CTest 过滤 4/4 通过，`ctest -N` 发现 210 项。未重复全量 CTest 或稳定性循环。
- 当前 `clang-tidy -p cmake-build-debug` 检查 `observable_filtering_test.cpp` 退出码为 0，仅报告 2 个被抑制的依赖警告；`clang-format --dry-run --Werror` 已执行，但因仓库无项目配置且默认 LLVM 风格与 `AGENTS.md` 的 Allman/OTBS 混合规范冲突，对整份既有文件报格式差异，继续不作为门禁。人工风格审查、MSVC 编译诊断和 `git diff --check` 均通过。

## 结论与风险

- 已取得 Windows/MSVC Debug 配置、构建、历史全量 206/206、连续 10 轮全量通过和 LeakObserver 输出审查证据；当前 210 项配置已完成增量构建、发现及新增回归定向验证。
- 未确认新的生产缺陷，因此没有创建生产修复任务。
- 剩余风险：首次稳定性批次观察到一次不可重复的 TaskSystem observeOn 清理超时；虽然之后单用例 50 次和第二批全量 10 次均通过，但不能证明该偶发挂起已消失。
- 当前版本连续 10 轮已全部通过；首次批次的单次偶发挂起仍保留为非阻塞风险。若需要证明长期稳定性，@负责人 可另开压力与挂起转储分析任务。
