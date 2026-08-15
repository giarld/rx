# RX 单元测试组织与运行

测试按职责拆分，后续用例应放入对应文件：

- `observable_creation_test.cpp`：创建类 API。
- `observable_transformation_test.cpp`：变换类 API。
- `observable_filtering_test.cpp`：过滤与条件类 API。
- `observable_error_test.cpp`：错误处理类 API。
- `observable_range_regression_test.cpp`：range 边界与取消回归。
- `observable_combination_test.cpp`：组合、竞争和多源生命周期回归。
- `observable_callback_test.cpp`：用户回调异常转换回归。
- `core_lifecycle_test.cpp`：Observer、LambdaObserver、Emitter 与 Disposable 契约。
- `blocking_test.cpp`：blockingFirst、blockingLast 与 blockingForEach。
- `observable_aggregation_test.cpp`：聚合类 API 与回归。
- `observable_lifecycle_test.cpp`：window、groupBy 与参数生命周期回归。
- `observable_time_test.cpp`：时间类 API 与回归。
- `scheduler_test.cpp`：Scheduler、Worker 与调度类回归。
- `test_infrastructure_test.cpp`：共享测试观察者、虚拟调度和有界等待设施。

共享设施位于 `support/`：

- `TestObserver` 记录订阅、值、错误和完成事件，并提供带状态摘要的断言。
- `TestWorker` / `TestScheduler` 使用虚拟时间确定性执行和取消任务。
- `BoundedWait` 使用条件变量进行真实线程同步，所有等待必须传入明确超时。

## 运行

配置、构建和测试命令须先取得仓库要求的人类授权：

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_RX_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

`gtest_discover_tests` 会把每个 GoogleTest 用例注册为独立 CTest 测试，因此可按套件或用例过滤：

```powershell
ctest --test-dir build -C Debug -R '^ObservableCreateTest\.' --output-on-failure
build\bin\test_rx.exe --gtest_filter=ObservableCreateTest.*
build\bin\test_rx.exe --gtest_filter=ObservableCreateTest.HonorsTerminationAndDisposal
```

每个发现后的 CTest 用例有 10 秒超时，并将 Debug 输出中的 `Object Leak:` 视为失败。
直接运行 `test_rx.exe` 时仍需人工审查该输出，因为 `LeakObserver::checkLeak()` 没有返回状态。

测试只允许使用本地合成数据，不得在运行时访问网络、外部服务、凭据或真实用户数据。

2026-08-14 Windows/MSVC Debug 的配置、构建、过滤测试、全量 CTest、10 轮稳定性和 LeakObserver 审查证据见 `MSVC_VALIDATION_2026-08-14.md`。

## 连续稳定性

```powershell
1..10 | ForEach-Object {
    ctest --test-dir build -C Debug --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw "CTest failed on iteration $_" }
}
```

## GCC 覆盖率

覆盖率构建仅为 `rx` 和 `test_rx` 增加 instrumentation；报告命令再以 `rx/` 过滤，只统计 RX 自有源码。目标为行覆盖率不低于 90%、分支覆盖率不低于 80%。

```bash
cmake -S . -B build-gcc-coverage -DCMAKE_BUILD_TYPE=Debug -DBUILD_RX_TESTS=ON -DCMAKE_CXX_COMPILER=g++ -DRX_ENABLE_COVERAGE=ON
cmake --build build-gcc-coverage
ctest --test-dir build-gcc-coverage --output-on-failure
gcovr --root . --filter 'rx/' --exclude 'deps/' --html-details build-gcc-coverage/coverage.html --xml build-gcc-coverage/coverage.xml --print-summary --fail-under-line 90 --fail-under-branch 80 build-gcc-coverage
```

`coverage.html` 的逐文件明细即未覆盖行/分支清单；命令的非零退出码用于阻止低于 90%/80% 的后续 CI 作业。

## Clang 覆盖率

```bash
cmake -S . -B build-clang-coverage -DCMAKE_BUILD_TYPE=Debug -DBUILD_RX_TESTS=ON -DCMAKE_CXX_COMPILER=clang++ -DRX_ENABLE_COVERAGE=ON
cmake --build build-clang-coverage
mkdir -p build-clang-coverage/profiles
LLVM_PROFILE_FILE="$PWD/build-clang-coverage/profiles/test_rx-%p.profraw" ctest --test-dir build-clang-coverage --output-on-failure
llvm-profdata merge -sparse build-clang-coverage/profiles/*.profraw -o build-clang-coverage/test_rx.profdata
llvm-cov report build-clang-coverage/bin/test_rx -instr-profile=build-clang-coverage/test_rx.profdata -ignore-filename-regex='(^|/)(deps|tests)/' rx
llvm-cov show build-clang-coverage/bin/test_rx -instr-profile=build-clang-coverage/test_rx.profdata -ignore-filename-regex='(^|/)(deps|tests)/' -show-line-counts-or-regions -show-branches=count -format=html -output-dir=build-clang-coverage/html rx
```

`LLVM_PROFILE_FILE` 必须使用绝对路径，因为 `gtest_discover_tests` 生成的 CTest 用例会从构建目录下的 `tests/` 工作目录启动。

`llvm-cov report` 输出行与分支摘要，`build-clang-coverage/html/` 提供逐行未覆盖清单。Clang 原生命令不在此处额外引入阈值脚本；后续 CI 可解析摘要，或将 `llvm-cov export -format=lcov` 交给现有覆盖率门禁工具执行 90%/80% 阈值。

## ASan / UBSan

Sanitizer 使用独立构建目录，避免和覆盖率 instrumentation 混用：

```bash
cmake -S . -B build-sanitizers -DCMAKE_BUILD_TYPE=Debug -DBUILD_RX_TESTS=ON -DCMAKE_CXX_COMPILER=clang++ -DRX_ENABLE_SANITIZERS=ON
cmake --build build-sanitizers
ASAN_OPTIONS='detect_leaks=1:halt_on_error=1' UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1' ctest --test-dir build-sanitizers --output-on-failure
```

GCC 可将编译器改为 `g++`。配置仅支持 GCC 或使用 GNU 命令行前端的 Clang；MSVC 和 `clang-cl` 会在配置阶段给出明确错误。当前 Windows 环境只有 Visual Studio 附带的 LLVM/MSVC 前端，且未发现 GCC、gcovr 或 lcov，因此本次没有生成覆盖率基线，也没有执行 ASan/UBSan。

## 后续 CI 接入

无需新增仓库工作流：CI 只需复用上述独立构建目录和命令，保存 HTML/XML/LCOV 产物，并以 GCC `gcovr` 的 90% 行、80% 分支阈值作为门禁。MSVC 作业继续负责 Windows 编译、CTest 与稳定性验证。
