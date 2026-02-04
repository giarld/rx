# Agent Guide for RX Repository

This document provides essential information for autonomous agents working on the RX codebase.

## Project Overview
RX is a C++20 ReactiveX library inspired by RxJava, providing asynchronous programming and event stream processing capabilities.

## Build and Test Commands

### Prerequisites
- CMake 3.20+
- C++20 compatible compiler
- Dependencies: `gany` and `gx` (automatically managed by CMake)

### Build Instructions
```bash
# Create build directory
mkdir build
cd build

# Configure (include examples for testing)
cmake -DBUILD_RX_EXAMPLES=ON ..

# Build
cmake --build .
```

### Build Variants
- Debug/Release are controlled by your generator (e.g., Visual Studio configs or `-DCMAKE_BUILD_TYPE=Release`).
- Examples are built when `-DBUILD_RX_EXAMPLES=ON` is set.

### Running Tests
The project currently uses an example application as a functional test suite.
```bash
# Run the main test/example application
./bin/TestRx
```
*Note: On Windows, the path might be `./bin/Debug/TestRx.exe` or `./bin/Release/TestRx.exe`.*

## Code Style Guidelines

### Formatting
- **Indentation:** Use 4 spaces. No tabs.
- **Line Endings:** LF (Unix style) preferred.
- **Braces:** 
  - Classes and functions: Opening brace on a new line.
  - Control flow (if, for, while): Opening brace on the same line.
  - Lambdas: Opening brace on the same line.
- **Naming Conventions:**
  - Classes/Structs: `PascalCase` (e.g., `Observable`, `NewThreadScheduler`)
  - Methods: `camelCase` (e.g., `onNext`, `subscribeOn`)
  - Variables: `camelCase` (e.g., `mainScheduler`, `disposable`)
  - Files: `snake_case.h` / `snake_case.cpp` (e.g., `observable_create.h`)
  - Macros/Constants: `UPPER_SNAKE_CASE` (e.g., `RX_OBSERVABLE_H`, `USE_GANY_CORE`)

### Types and Smart Pointers
- **Standard:** C++20.
- **Smart Pointers:** 
  - Use `std::shared_ptr` for almost all object management.
  - Suffix aliases with `Ptr` (e.g., `using ObservablePtr = std::shared_ptr<Observable>;`).
  - Use `std::make_shared<T>(...)` for instantiation.
- **Enable Shared From This:** Classes intended to be managed by `shared_ptr` and providing chainable methods should inherit from `std::enable_shared_from_this`.
- **Weak Pointers:** Prefer `std::weak_ptr` in observer/inner chains if cycles are possible.

### Imports and Includes
- Header guards should use the format `RX_FILENAME_H`.
- Include internal headers using relative paths from the `rx/include` root (e.g., `#include "rx/observable.h"`).
- Group includes: 
  1. Current header (in .cpp)
  2. Library headers (`rx/...`)
  3. Dependency headers (`gx/...`, `gany/...`)
  4. System headers

### Namespace
- All core logic must reside within the `rx` namespace.
- Avoid `using namespace` in header files.

### Error Handling
- Use `GAnyException` for library-specific errors.
- Wrap `std::exception` in `GAnyException` when propagating errors through the Reactive stream.
- In `Observable` emitters, use `emitter->onError(e)` to signal failures.
- For operator callbacks, catch exceptions and forward to downstream via `onError`.

### Memory Management
- The library uses a `LeakObserver` for tracking potential memory leaks in tests.
- Always call `LeakObserver::checkLeak()` at the end of test cases.
- Be mindful of circular references in `shared_ptr` chains; use `std::weak_ptr` if necessary, though the current design favors `Disposable` for breaking chains.
- Ensure `Disposable` links are cleared on `onError`/`onComplete` to avoid leaks.

## Core Architecture
- **Observable:** The source of data. Use `Observable::create` for custom logic or static factories like `just`, `fromArray`, `range`.
- **Observer:** The consumer. Implements `onNext`, `onError`, and `onComplete`.
- **Disposable:** Manages the lifecycle of a subscription. Always return or handle `DisposablePtr` to prevent resource leaks.
- **Schedulers:** 
  - `NewThreadScheduler`: Spawns a new thread for each task.
  - `MainThreadScheduler`: Routes tasks to the global `GTimerScheduler` (requires `GTimerScheduler::makeGlobal`).
  - `TaskSystemScheduler`: Uses the project's task system for parallel execution.
- **ObservableSource:** Use `ObservableSourcePtr` for inter-operator links.

## Operator Implementation Guide
When adding a new operator:
1. Create a new header in `rx/include/rx/operators/` named `observable_[operator_name].h`.
2. Inherit from `Observable` or a specific base operator class if available.
3. Implement `subscribeActual(const ObserverPtr& observer)` to define the operator's behavior.
4. Add a corresponding method in the `Observable` class in `rx/include/rx/observable.h` and its implementation in `rx/src/observable.cpp`.
5. If the operator maintains state, implement a dedicated `Observer` that also acts as `Disposable` where appropriate.
6. Ensure terminal paths (`onError`, `onComplete`) are idempotent and clear references.

## Common Pitfalls
- **Missing GAny Initialization:** Always call `initGAnyCore()` before using the library.
- **Global Scheduler:** `MainThreadScheduler` will fail if `GTimerScheduler::makeGlobal()` hasn't been called.
- **Shared Pointers in Lambdas:** Be careful with capturing `this` or other `shared_ptr` in lambdas passed to operators; use weak pointers if circular dependencies are possible, though `Disposable` usually handles teardown.
- **Re-entrancy:** Avoid calling downstream under locks; release locks before invoking user callbacks.

## Performance Considerations
- The library uses `GAny` for type-erasure, which has some overhead. Avoid unnecessary `castAs<T>` calls in tight loops.
- Use `observeOn` sparingly to minimize context switching between threads.
- Prefer `fromArray` or `just` over manual `create` for simple sequences.

## File Structure
- `rx/include/rx/`: Public headers.
- `rx/include/rx/operators/`: Operator implementations (mostly template/header-only).
- `rx/src/`: Core library implementation.
- `examples/`: Usage examples and functional tests.

## Docs and Examples
- Update `README.md` for new public operators and behavioral changes.
- Extend `examples/test_rx.cpp` for functional coverage of new operators.

## Windows Notes
- The repo targets LF, but Windows may warn about CRLF. Keep formatting stable and consistent.
