# Agent Guide for RX Repository

This document provides essential information for autonomous agents working on the RX codebase.
Follow these guidelines to ensure consistency, stability, and maintainability.

## Project Overview

RX is a C++20 ReactiveX library inspired by RxJava/RxJS. It provides asynchronous programming
capabilities using Observable streams. It relies on `gany` (type erasure) and `gx` (cross-platform utils).

## Build and Test Instructions

### Prerequisites
- **CMake**: Version 3.20 or higher.
- **Compiler**: C++20 compatible (Clang 10+, GCC 10+, MSVC 19.28+).
- **Dependencies**: `gany` and `gx` are fetched automatically via CMake (in `deps/`).

### Build Commands
Always build from a dedicated directory to keep the source clean.

```bash
# 1. Create and enter build directory
mkdir -p build
cd build

# 2. Configure project (Enable examples to build the test suite)
cmake -DBUILD_RX_EXAMPLES=ON ..

# 3. Build (Debug configuration is standard for development)
cmake --build . --config Debug
```

### Running Tests
The primary test suite is an executable that runs functional tests for all operators.

```bash
# Run all tests
./bin/TestRx
# Windows: .\bin\Debug\TestRx.exe
```

### Running a Single Test
The test runner (`examples/test_rx.cpp`) executes all test functions sequentially in `main`.
To run a specific test case (e.g., when implementing or debugging a new operator):

1.  **Edit** `examples/test_rx.cpp`.
2.  **Locate** the `main()` function at the bottom of the file.
3.  **Comment out** the calls to other `test...()` functions, keeping only the one you need.
    ```cpp
    // Example: Running only creation tests
    testCreationOperators();
    // testTransformationOperators();
    // ...
    ```
4.  **Rebuild** using `cmake --build .`.
5.  **Run** `./bin/TestRx`.

*Note: Do not commit these changes to `main()` unless you are restructuring the test suite.*

## Code Style and Conventions

### Class Organization
- **Public Sections**: Constructors, destructors, and copy/move operators must be in the first `public` section. Other public methods should follow in a separate `public` section.
  ```cpp
  class Example {
  public:
      Example();
      ~Example();

  public:
      void doSomething();
  };
  ```

### Formatting
- **Indentation**: 4 spaces. **No tabs.**
- **Line Endings**: LF (Unix style).
- **Braces**:
    - **Classes/Functions**: Next line (Allman style).
      ```cpp
      void myFunction()
      {
          // ...
      }
      ```
    - **Control Flow/Lambdas**: Same line (K&R/OTBS style).
      ```cpp
      if (condition) {
          // ...
      }
      Observable::create([](auto e) {
          // ...
      });
      ```

### Naming Conventions
- **Classes/Structs**: `PascalCase` (e.g., `Observable`, `NewThreadScheduler`).
- **Methods/Functions**: `camelCase` (e.g., `onNext`, `subscribeActual`).
- **Variables**: `camelCase` (e.g., `disposable`, `sourceObservable`).
- **Files**: `snake_case` (e.g., `observable_map.h`, `test_rx.cpp`).
- **Template Types**: `T`, `R`, `U` for generic types; `Source` for observable sources.
- **Macros**: `UPPER_SNAKE_CASE` (e.g., `RX_OBSERVABLE_H`).

### Types and Memory Management
- **Standard**: C++20. Use `auto` for complex iterators/types but prefer explicit types for clarity in headers.
- **Smart Pointers**:
    - Heavy use of `std::shared_ptr`.
    - Define aliases ending in `Ptr` (e.g., `using ObserverPtr = std::shared_ptr<Observer<T>>;`).
    - Use `std::make_shared` for allocation.
- **Weak Pointers**: Use `std::weak_ptr` to break cycles, especially in operator implementations where the Observer might hold a reference back to the Disposable/Source.
- **Type Erasure**: The library uses `GAny` (from `gany` lib) to pass values through the stream.
- **Integers**: Prefer `int32_t` over `int`.

### Imports and Structure
- **Header Guards**: `#ifndef RX_PATH_FILENAME_H` pattern.
- **Include Order**:
    1.  Associated header (for .cpp files)
    2.  `rx/` local headers
    3.  `gx/` or `gany/` dependencies
    4.  Standard Library (`<vector>`, `<memory>`)
- **Namespaces**:
    - Public code: `namespace rx { ... }`
    - **Never** put `using namespace` in a header file.

## Architecture & Implementation

### Core Components
- **Observable**: The stream factory.
- **Observer**: Receives `onNext`, `onError`, `onComplete`.
- **Disposable**: Handle to cancel work/subscriptions.
- **Scheduler**: Controls *where* (which thread) work happens.

### Implementing a New Operator
1.  **File**: Create `rx/include/rx/operators/observable_myop.h`.
2.  **Class**: Define a class inheriting `Observable` (or similar base).
3.  **Observer**: Define an inner `MyOpObserver` class to handle events.
4.  **Hook**: Add the operator method to `rx/include/rx/observable.h`.
5.  **Impl**: Implement the factory method in `rx/src/observable.cpp`.

### Error Handling
- **Exceptions**: Do not throw from `onNext`. Wrap errors in `GAnyException` and pass to `onError`.
- **Propagation**: If an operator fails during setup, call `Observer::onError`.

### Memory Leak Detection
- The project uses `LeakObserver`.
- Tests must end with `LeakObserver::checkLeak()`.
- Ensure all `Disposable`s are disposed and `Observer` chains are broken on termination.

## Common Pitfalls
1.  **Missing Init**: `initGAnyCore()` must be called in `main` before using RX.
2.  **Global Timers**: `MainThreadScheduler` requires `GTimerScheduler::makeGlobal()` to be set up.
3.  **Lambda Captures**: Capturing `shared_ptr` by value in a lambda stored in that same object creates a leak. Use `weak_ptr`.
4.  **Locking**: Avoid holding locks when calling out to user code (`onNext`).

## Cursor & Copilot Rules
- No specific rules files (`.cursorrules`, `.github/copilot-instructions.md`) exist currently.
- Follow the conventions in this file strictly.
