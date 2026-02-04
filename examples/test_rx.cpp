//
// Created by Gxin on 2026/1/4.
//

#define USE_GANY_CORE
#include <gx/gany.h>

#include <rx/rx.h>

#include <gx/gstring.h>

#include <cstdlib>
#include <atomic>
#include <vector>
#include <string>
#include <cstdio>


using namespace rx;

template<typename... Args>
void printLog(const char *format, Args &&... args)
{
    auto log = GString::format(format, std::forward<Args>(args)...);
    printf("%s\n", log.c_str());
}

void printLog(const char *msg)
{
    printf("%s\n", msg);
}

// ========================================
// Helper Classes
// ========================================

class LogDisposable : public rx::Disposable
{
public:
    LogDisposable(std::function<void()> action, rx::DisposablePtr d)
        : mAction(std::move(action)), mWrapped(std::move(d))
    {
    }

    void dispose() override
    {
        if (!mDisposed) {
            mDisposed = true;
            if (mAction)
                mAction();
            if (mWrapped)
                mWrapped->dispose();
        }
    }

    bool isDisposed() const override { return mDisposed; }

private:
    std::function<void()> mAction;
    rx::DisposablePtr mWrapped;
    bool mDisposed = false;
};

// ========================================
// Creation Operators Tests
// ========================================

void testCreationOperators()
{
    printLog("\n========================================");
    printLog("Testing Creation Operators");
    printLog("========================================");

    // Test: create
    printLog("\n--- Test: create ---");
    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(1);
        emitter->onNext(2);
        emitter->onNext(3);
        emitter->onComplete();
    })->subscribe([](const GAny &v) {
                      printLog("create: {}", v.toString());
                  }, [](const GAnyException &e) {
                      printLog("Error: {}", e.toString());
                  }, []() {
                      printLog("create: Completed");
                  });

    // Test: just
    printLog("\n--- Test: just (1-10 values) ---");
    Observable::just(1)->subscribe([](const GAny &v) { printLog("just(1): {}", v.toString()); });
    Observable::just(1, 2, 3)->subscribe([](const GAny &v) { printLog("just(3): {}", v.toString()); });
    Observable::just(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)->subscribe(
        [](const GAny &v) { printLog("just(10): {}", v.toString()); });

    // Test: fromArray
    printLog("\n--- Test: fromArray ---");
    std::vector<GAny> items = {10, 20, 30, 40, 50};
    Observable::fromArray(items)->subscribe([](const GAny &v) {
        printLog("fromArray: {}", v.toString());
    });

    // Test: range
    printLog("\n--- Test: range ---");
    Observable::range(0, 5)->subscribe([](const GAny &v) {
        printLog("range: {}", v.toString());
    });

    // Test: empty
    printLog("\n--- Test: empty ---");
    Observable::empty()->subscribe(
        [](const GAny &v) { printLog("empty onNext (unexpected): {}", v.toString()); },
        [](const GAnyException &e) { printLog("empty onError (unexpected): {}", e.toString()); },
        []() { printLog("empty: Completed"); }
    );

    // Test: never (should not emit anything - use timeout to test)
    printLog("\n--- Test: never (with timeout) ---");
    auto scheduler = GTimerScheduler::create("NeverTestScheduler");
    scheduler->start();
    GTimerScheduler::makeGlobal(scheduler);

    Observable::never()
        ->timeout(100)
        ->subscribe(
            [](const GAny &v) { printLog("never onNext (unexpected): {}", v.toString()); },
            [](const GAnyException &e) { printLog("never: Timed out as expected"); },
            []() { printLog("never onComplete (unexpected)"); }
        );

    scheduler->post([scheduler] { scheduler->stop(); }, 200);
    scheduler->run();

    // Test: error
    printLog("\n--- Test: error ---");
    Observable::error(GAnyException("Test Error"))
        ->subscribe(
            [](const GAny &v) { printLog("error onNext (unexpected): {}", v.toString()); },
            [](const GAnyException &e) { printLog("error: {}", e.toString()); },
            []() { printLog("error onComplete (unexpected)"); }
        );

    // Test: defer
    printLog("\n--- Test: defer ---");
    auto source = Observable::just(42);
    auto deferred = Observable::defer(source);
    deferred->subscribe([](const GAny &v) { printLog("defer 1st subscription: {}", v.toString()); });
    deferred->subscribe([](const GAny &v) { printLog("defer 2nd subscription: {}", v.toString()); });

    printLog("\n✓ Creation Operators Tests Completed\n");
}

// ========================================
// Transformation Operators Tests
// ========================================

void testTransformationOperators()
{
    printLog("\n========================================");
    printLog("Testing Transformation Operators");
    printLog("========================================");

    // Test: map
    printLog("\n--- Test: map ---");
    Observable::range(1, 5)
        ->map([](const GAny &v) { return v.toInt32() * 2; })
        ->subscribe([](const GAny &v) { printLog("map: {}", v.toString()); });

    // Test: flatMap
    printLog("\n--- Test: flatMap ---");
    Observable::just(1, 2, 3)
        ->flatMap([](const GAny &v) {
            return Observable::range(0, v.toInt32());
        })
        ->subscribe([](const GAny &v) { printLog("flatMap: {}", v.toString()); });

    // Test: concatMap
    printLog("\n--- Test: concatMap ---");
    auto scheduler1 = GTimerScheduler::create("ConcatMapScheduler");
    scheduler1->start();
    GTimerScheduler::makeGlobal(scheduler1);
    auto sched = MainThreadScheduler::create();

    Observable::just(1, 2, 3)
        ->concatMap([sched](const GAny &v) -> std::shared_ptr<Observable> {
            int id = v.toInt32();
            return Observable::create([id, sched](const ObservableEmitterPtr &emitter) {
                sched->scheduleDirect([emitter, id] {
                    emitter->onNext(id * 10);
                    emitter->onComplete();
                }, 10);
            });
        })
        ->subscribe([](const GAny &v) { printLog("concatMap: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("concatMap: Completed"); });

    scheduler1->post([scheduler1] { scheduler1->stop(); }, 200);
    scheduler1->run();

    // Test: switchMap
    printLog("\n--- Test: switchMap ---");
    auto scheduler2 = GTimerScheduler::create("SwitchMapScheduler");
    scheduler2->start();
    GTimerScheduler::makeGlobal(scheduler2);
    auto sched2 = MainThreadScheduler::create();

    Observable::create([sched2](const ObservableEmitterPtr &emitter) {
            emitter->onNext(1);
            sched2->scheduleDirect([emitter] { emitter->onNext(2); }, 20);
            sched2->scheduleDirect([emitter] {
                emitter->onNext(3);
                emitter->onComplete();
            }, 40);
        })
        ->switchMap([sched2](const GAny &v) -> std::shared_ptr<Observable> {
            int id = v.toInt32();
            return Observable::create([id, sched2](const ObservableEmitterPtr &emitter) {
                auto d = sched2->scheduleDirect([emitter, id] {
                    emitter->onNext(id * 100);
                    emitter->onComplete();
                }, 30);
                emitter->setDisposable(d);
            });
        })
        ->subscribe([](const GAny &v) { printLog("switchMap: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("switchMap: Completed"); });

    scheduler2->post([scheduler2] { scheduler2->stop(); }, 200);
    scheduler2->run();

    printLog("\n✓ Transformation Operators Tests Completed\n");
}

// ========================================
// Filtering Operators Tests
// ========================================

void testFilteringOperators()
{
    printLog("\n========================================");
    printLog("Testing Filtering Operators");
    printLog("========================================");

    // Test: filter
    printLog("\n--- Test: filter ---");
    Observable::range(0, 10)
        ->filter([](const GAny &v) { return v.toInt32() % 2 == 0; })
        ->subscribe([](const GAny &v) { printLog("filter (even): {}", v.toString()); });

    // Test: elementAt
    printLog("\n--- Test: elementAt ---");
    Observable::just("A", "B", "C", "D", "E")
        ->elementAt(2)
        ->subscribe([](const GAny &v) { printLog("elementAt(2): {}", v.toString()); });

    Observable::just(1, 2, 3)
        ->elementAt(10, 999)
        ->subscribe([](const GAny &v) { printLog("elementAt(10, default=999): {}", v.toString()); });

    // Test: first
    printLog("\n--- Test: first ---");
    Observable::just("First", "Second", "Third")
        ->first()
        ->subscribe([](const GAny &v) { printLog("first: {}", v.toString()); });

    Observable::empty()
        ->first("DefaultFirst")
        ->subscribe([](const GAny &v) { printLog("first (empty, default): {}", v.toString()); });

    // Test: last
    printLog("\n--- Test: last ---");
    Observable::just("A", "B", "C")
        ->last()
        ->subscribe([](const GAny &v) { printLog("last: {}", v.toString()); });

    Observable::empty()
        ->last("DefaultLast")
        ->subscribe([](const GAny &v) { printLog("last (empty, default): {}", v.toString()); });

    // Test: ignoreElements
    printLog("\n--- Test: ignoreElements ---");
    Observable::just(1, 2, 3, 4, 5)
        ->ignoreElements()
        ->subscribe(
            [](const GAny &v) { printLog("ignoreElements onNext (unexpected): {}", v.toString()); },
            [](const GAnyException &e) { printLog("ignoreElements onError (unexpected): {}", e.toString()); },
            []() { printLog("ignoreElements: Completed"); }
        );

    // Test: skip
    printLog("\n--- Test: skip ---");
    Observable::range(0, 10)
        ->skip(5)
        ->subscribe([](const GAny &v) { printLog("skip(5): {}", v.toString()); });

    // Test: skipLast
    printLog("\n--- Test: skipLast ---");
    Observable::range(0, 10)
        ->skipLast(3)
        ->subscribe([](const GAny &v) { printLog("skipLast(3): {}", v.toString()); });

    // Test: take
    printLog("\n--- Test: take ---");
    Observable::range(0, 10)
        ->take(3)
        ->subscribe([](const GAny &v) { printLog("take(3): {}", v.toString()); });

    // Test: takeLast
    printLog("\n--- Test: takeLast ---");
    Observable::range(0, 10)
        ->takeLast(3)
        ->subscribe([](const GAny &v) { printLog("takeLast(3): {}", v.toString()); });

    // Test: distinct
    printLog("\n--- Test: distinct ---");
    Observable::just(1, 2, 1, 3, 2, 4, 1, 5)
        ->distinct()
        ->subscribe([](const GAny &v) { printLog("distinct: {}", v.toString()); });

    // Test: distinct with key selector
    printLog("\n--- Test: distinct with keySelector ---");
    Observable::just("Apple", "Banana", "Apricot", "Cherry", "Blueberry")
        ->distinct([](const GAny &v) { return v.toString().substr(0, 1); })
        ->subscribe([](const GAny &v) { printLog("distinct(keySelector): {}", v.toString()); });

    // Test: distinctUntilChanged
    printLog("\n--- Test: distinctUntilChanged ---");
    Observable::just(1, 1, 2, 2, 2, 3, 2, 2, 1)
        ->distinctUntilChanged()
        ->subscribe([](const GAny &v) { printLog("distinctUntilChanged: {}", v.toString()); });

    // Test: distinctUntilChanged with key selector
    printLog("\n--- Test: distinctUntilChanged with keySelector ---");
    Observable::just("Apple", "Apricot", "Banana", "Blueberry", "Cherry")
        ->distinctUntilChanged([](const GAny &v) { return v.toString().substr(0, 1); })
        ->subscribe([](const GAny &v) { printLog("distinctUntilChanged(keySelector): {}", v.toString()); });

    // Test: takeUntil
    printLog("\n--- Test: takeUntil ---");
    auto schedulerTakeUntil = GTimerScheduler::create("TakeUntilScheduler");
    schedulerTakeUntil->start();
    GTimerScheduler::makeGlobal(schedulerTakeUntil);

    Observable::interval(50, 50)
        ->takeUntil(Observable::timer(220)) // Should take 0, 1, 2, 3 (50, 100, 150, 200) - timer at 220
        ->subscribe([](const GAny &v) { printLog("takeUntil: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("takeUntil: Completed"); });

    schedulerTakeUntil->post([schedulerTakeUntil] { schedulerTakeUntil->stop(); }, 400);
    schedulerTakeUntil->run();

    printLog("\n✓ Filtering Operators Tests Completed\n");
}

// ========================================
// Combination Operators Tests
// ========================================

void testCombinationOperators()
{
    printLog("\n========================================");
    printLog("Testing Combination Operators");
    printLog("========================================");

    // Test: combineLatest
    printLog("\n--- Test: combineLatest ---");
    auto obs1 = Observable::just(1, 2, 3);
    auto obs2 = Observable::just("A", "B", "C");

    Observable::combineLatest(obs1, obs2, [](const GAny &v1, const GAny &v2) {
        return v1.toString() + v2.toString();
    })->subscribe([](const GAny &v) { printLog("combineLatest: {}", v.toString()); });

    // Test: zip
    printLog("\n--- Test: zip ---");
    auto z1 = Observable::just(1, 2, 3);
    auto z2 = Observable::just("X", "Y");

    Observable::zip(z1, z2, [](const GAny &v1, const GAny &v2) {
        return v1.toString() + v2.toString();
    })->subscribe([](const GAny &v) { printLog("zip: {}", v.toString()); },
                  [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                  []() { printLog("zip: Completed"); });

    // Test: merge
    printLog("\n--- Test: merge ---");
    auto m1 = Observable::just(1, 2);
    auto m2 = Observable::just(3, 4);
    auto m3 = Observable::just(5, 6);

    Observable::merge(m1, m2, m3)
        ->subscribe([](const GAny &v) { printLog("merge: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("merge: Completed"); });

    // Test: concat
    printLog("\n--- Test: concat ---");
    auto c1 = Observable::just(1, 2);
    auto c2 = Observable::just(3, 4);
    auto c3 = Observable::just(5, 6);

    Observable::concat(c1, c2, c3)
        ->subscribe([](const GAny &v) { printLog("concat: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("concat: Completed"); });

    // Test: startWith
    printLog("\n--- Test: startWith ---");
    Observable::just(2, 3, 4)
        ->startWith(0, 1)
        ->subscribe([](const GAny &v) { printLog("startWith: {}", v.toString()); });

    // Test: buffer
    printLog("\n--- Test: buffer ---");
    Observable::range(1, 10)
        ->buffer(3)
        ->subscribe([](const GAny &v) { printLog("buffer(3): {}", v.toString()); });

    Observable::range(1, 10)
        ->buffer(3, 2)
        ->subscribe([](const GAny &v) { printLog("buffer(3,2): {}", v.toString()); });

    printLog("\n✓ Combination Operators Tests Completed\n");
}

// ========================================
// Error Handling Tests
// ========================================

void testErrorHandling()
{
    printLog("\n========================================");
    printLog("Testing Error Handling");
    printLog("========================================");

    // Test: retry with eventual success
    printLog("\n--- Test: retry (success after 3 attempts) ---");
    int retryCount1 = 0;
    Observable::create([&retryCount1](const ObservableEmitterPtr &emitter) {
            retryCount1++;
            printLog("Attempt: {}", retryCount1);
            if (retryCount1 < 3) {
                emitter->onError(GAnyException("Fail"));
            } else {
                emitter->onNext(100);
                emitter->onComplete();
            }
        })
        ->retry(5)
        ->subscribe([](const GAny &v) { printLog("retry success: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("retry error: {}", e.toString()); },
                    []() { printLog("retry: Completed"); });

    // Test: retry with eventual failure
    printLog("\n--- Test: retry (fail after 3 attempts) ---");
    int retryCount2 = 0;
    Observable::create([&retryCount2](const ObservableEmitterPtr &emitter) {
            retryCount2++;
            printLog("Attempt: {}", retryCount2);
            emitter->onError(GAnyException("Always Fail"));
        })
        ->retry(2)
        ->subscribe([](const GAny &v) { printLog("retry onNext (unexpected): {}", v.toString()); },
                    [](const GAnyException &e) { printLog("retry final error: {}", e.toString()); },
                    []() { printLog("retry onComplete (unexpected)"); });

    // Test: timeout with error
    printLog("\n--- Test: timeout (error) ---");
    auto scheduler1 = GTimerScheduler::create("TimeoutScheduler1");
    scheduler1->start();
    GTimerScheduler::makeGlobal(scheduler1);

    Observable::never()
        ->timeout(100)
        ->subscribe([](const GAny &v) { printLog("timeout onNext (unexpected): {}", v.toString()); },
                    [](const GAnyException &e) { printLog("timeout error: {}", e.toString()); },
                    []() { printLog("timeout onComplete (unexpected)"); });

    scheduler1->post([scheduler1] { scheduler1->stop(); }, 200);
    scheduler1->run();

    // Test: timeout with fallback
    printLog("\n--- Test: timeout (fallback) ---");
    auto scheduler2 = GTimerScheduler::create("TimeoutScheduler2");
    scheduler2->start();
    GTimerScheduler::makeGlobal(scheduler2);

    Observable::never()
        ->timeout(100, Observable::just("Fallback Value"))
        ->subscribe([](const GAny &v) { printLog("timeout fallback: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("timeout error (unexpected): {}", e.toString()); },
                    []() { printLog("timeout fallback: Completed"); });

    scheduler2->post([scheduler2] { scheduler2->stop(); }, 200);
    scheduler2->run();

    printLog("\n✓ Error Handling Tests Completed\n");
}

// ========================================
// Utility Operators Tests
// ========================================

void testUtilityOperators()
{
    printLog("\n========================================");
    printLog("Testing Utility Operators");
    printLog("========================================");

    // Test: doOnNext
    printLog("\n--- Test: doOnNext ---");
    Observable::just(1, 2, 3)
        ->doOnNext([](const GAny &v) { printLog("doOnNext side-effect: {}", v.toString()); })
        ->subscribe([](const GAny &v) { printLog("doOnNext result: {}", v.toString()); });

    // Test: doOnError
    printLog("\n--- Test: doOnError ---");
    Observable::error(GAnyException("Test Error"))
        ->doOnError([](const GAnyException &e) { printLog("doOnError side-effect: {}", e.toString()); })
        ->subscribe([](const GAny &v) {
                    },
                    [](const GAnyException &e) { printLog("doOnError result: {}", e.toString()); },
                    []() {
                    });

    // Test: doOnComplete
    printLog("\n--- Test: doOnComplete ---");
    Observable::just(1, 2, 3)
        ->doOnComplete([]() { printLog("doOnComplete side-effect"); })
        ->subscribe([](const GAny &v) { printLog("doOnComplete value: {}", v.toString()); },
                    [](const GAnyException &e) {
                    },
                    []() { printLog("doOnComplete result"); });

    // Test: doOnSubscribe
    printLog("\n--- Test: doOnSubscribe ---");
    Observable::just(1, 2, 3)
        ->doOnSubscribe([](const DisposablePtr &d) { printLog("doOnSubscribe side-effect"); })
        ->subscribe([](const GAny &v) { printLog("doOnSubscribe value: {}", v.toString()); });

    // Test: doFinally
    printLog("\n--- Test: doFinally (on complete) ---");
    Observable::just(1, 2, 3)
        ->doFinally([]() { printLog("doFinally side-effect (complete)"); })
        ->subscribe([](const GAny &v) { printLog("doFinally value: {}", v.toString()); },
                    [](const GAnyException &e) {
                    },
                    []() { printLog("doFinally result"); });

    printLog("\n--- Test: doFinally (on error) ---");
    Observable::error(GAnyException("Test Error"))
        ->doFinally([]() { printLog("doFinally side-effect (error)"); })
        ->subscribe([](const GAny &v) {
                    },
                    [](const GAnyException &e) { printLog("doFinally error: {}", e.toString()); },
                    []() {
                    });

    // Test: repeat
    printLog("\n--- Test: repeat ---");
    Observable::just(1, 2, 3)
        ->repeat(2)
        ->subscribe([](const GAny &v) { printLog("repeat: {}", v.toString()); });

    // Test: scan
    printLog("\n--- Test: scan (accumulation) ---");
    Observable::just(1, 2, 3, 4, 5)
        ->scan([](const GAny &acc, const GAny &v) {
            return acc.toInt32() + v.toInt32();
        })
        ->subscribe([](const GAny &v) { printLog("scan sum: {}", v.toString()); });

    // Test: reduce
    printLog("\n--- Test: reduce (sum) ---");
    Observable::just(1, 2, 3, 4, 5)
        ->reduce([](const GAny &acc, const GAny &v) {
            return acc.toInt32() + v.toInt32();
        })
        ->subscribe([](const GAny &v) { printLog("reduce sum: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("reduce error (unexpected): {}", e.toString()); },
                    []() { printLog("reduce: Completed"); });

    printLog("\n--- Test: reduce (empty -> error) ---");
    Observable::empty()
        ->reduce([](const GAny &acc, const GAny &v) {
            return acc.toInt32() + v.toInt32();
        })
        ->subscribe([](const GAny &v) { printLog("reduce onNext (unexpected): {}", v.toString()); },
                    [](const GAnyException &e) { printLog("reduce empty error: {}", e.toString()); },
                    []() { printLog("reduce empty onComplete (unexpected)"); });

    // Test: delay
    printLog("\n--- Test: delay ---");
    auto scheduler = GTimerScheduler::create("DelayScheduler");
    scheduler->start();
    GTimerScheduler::makeGlobal(scheduler);
    auto sched = MainThreadScheduler::create();

    Observable::just(1, 2, 3)
        ->delay(100, sched)
        ->subscribe([](const GAny &v) { printLog("delay: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("delay: Completed"); });

    scheduler->post([scheduler] { scheduler->stop(); }, 300);
    scheduler->run();

    // Test: debounce
    printLog("\n--- Test: debounce ---");
    auto scheduler2 = GTimerScheduler::create("DebounceScheduler");
    scheduler2->start();
    GTimerScheduler::makeGlobal(scheduler2);
    auto sched2 = MainThreadScheduler::create();

    Observable::create([sched2](const ObservableEmitterPtr &emitter) {
            emitter->onNext("a");
            sched2->scheduleDirect([emitter] { emitter->onNext("ab"); }, 50);
            sched2->scheduleDirect([emitter] { emitter->onNext("abc"); }, 100);
            sched2->scheduleDirect([emitter] {
                emitter->onNext("abcd");
                emitter->onComplete();
            }, 300);
        })
        ->debounce(150, sched2)
        ->subscribe([](const GAny &v) { printLog("debounce: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("debounce: Completed"); });

    scheduler2->post([scheduler2] { scheduler2->stop(); }, 600);
    scheduler2->run();

    // Test: sample
    printLog("\n--- Test: sample ---");
    auto scheduler3 = GTimerScheduler::create("SampleScheduler");
    scheduler3->start();
    GTimerScheduler::makeGlobal(scheduler3);
    auto sched3 = MainThreadScheduler::create();

    Observable::create([sched3](const ObservableEmitterPtr &emitter) {
            emitter->onNext("A");
            sched3->scheduleDirect([emitter] { emitter->onNext("AB"); }, 50);
            sched3->scheduleDirect([emitter] { emitter->onNext("ABC"); }, 120);
            sched3->scheduleDirect([emitter] { emitter->onNext("ABCD"); }, 240);
            sched3->scheduleDirect([emitter] {
                emitter->onNext("ABCDE");
                emitter->onComplete();
            }, 360);
        })
        ->sample(150, sched3)
        ->subscribe([](const GAny &v) { printLog("sample: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("sample: Completed"); });

    scheduler3->post([scheduler3] { scheduler3->stop(); }, 800);
    scheduler3->run();

    printLog("\n✓ Utility Operators Tests Completed\n");
}

// ========================================
// Scheduler Tests
// ========================================

void testSchedulers()
{
    printLog("\n========================================");
    printLog("Testing Schedulers");
    printLog("========================================");

    auto mainScheduler = GTimerScheduler::create("MainScheduler");
    mainScheduler->start();
    GTimerScheduler::makeGlobal(mainScheduler);

    auto threadScheduler = NewThreadScheduler::create();
    auto timerScheduler = MainThreadScheduler::create();

    // Test: subscribeOn
    printLog("\n--- Test: subscribeOn ---");
    std::atomic<int> subscribeThreadId{0};
    Observable::create([&subscribeThreadId](const ObservableEmitterPtr &emitter) {
            subscribeThreadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
            printLog("subscribeOn: Emitting on thread {}", subscribeThreadId.load());
            emitter->onNext(1);
            emitter->onComplete();
        })
        ->subscribeOn(threadScheduler)
        ->subscribe([](const GAny &v) {
            int observeThreadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
            printLog("subscribeOn: Received {} on thread {}", v.toString(), observeThreadId);
        });

    GThread::mSleep(100);

    // Test: observeOn
    printLog("\n--- Test: observeOn ---");
    std::atomic<int> emitThreadId{0};
    std::atomic<int> observeThreadId{0};
    Observable::create([&emitThreadId, threadScheduler](const ObservableEmitterPtr &emitter) {
            emitThreadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
            printLog("observeOn: Emitting on thread {}", emitThreadId.load());
            emitter->onNext(1);
            emitter->onComplete();
        })
        ->subscribeOn(threadScheduler)
        ->observeOn(timerScheduler)
        ->subscribe([&observeThreadId](const GAny &v) {
            observeThreadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
            printLog("observeOn: Received {} on thread {}", v.toString(), observeThreadId.load());
        });

    mainScheduler->post([mainScheduler] { mainScheduler->stop(); }, 200);
    mainScheduler->run();

    printLog("\n✓ Scheduler Tests Completed\n");
}

// ========================================
// Complex Scenarios Tests
// ========================================

void testComplexScenarios()
{
    printLog("\n========================================");
    printLog("Testing Complex Scenarios");
    printLog("========================================");

    // Test: Chaining multiple operators
    printLog("\n--- Test: Complex operator chain ---");
    Observable::range(1, 20)
        ->filter([](const GAny &v) { return v.toInt32() % 2 == 0; })                       // Even numbers
        ->map([](const GAny &v) { return v.toInt32() * v.toInt32(); })                     // Square
        ->skip(2)                                                                          // Skip first 2
        ->take(3)                                                                          // Take next 3
        ->scan([](const GAny &acc, const GAny &v) { return acc.toInt32() + v.toInt32(); }) // Accumulate
        ->subscribe([](const GAny &v) { printLog("Complex chain: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("Complex chain: Completed"); });

    // Test: Error recovery with retry and timeout
    printLog("\n--- Test: Error recovery (retry + timeout) ---");
    auto scheduler = GTimerScheduler::create("ErrorRecoveryScheduler");
    scheduler->start();
    GTimerScheduler::makeGlobal(scheduler);

    int attemptCount = 0;
    Observable::create([&attemptCount](const ObservableEmitterPtr &emitter) {
            attemptCount++;
            printLog("Error recovery attempt: {}", attemptCount);
            if (attemptCount < 2) {
                emitter->onError(GAnyException("Temporary Error"));
            } else {
                emitter->onNext("Success!");
                emitter->onComplete();
            }
        })
        ->retry(3)
        ->timeout(500)
        ->subscribe([](const GAny &v) { printLog("Error recovery result: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error recovery error: {}", e.toString()); },
                    []() { printLog("Error recovery: Completed"); });

    scheduler->post([scheduler] { scheduler->stop(); }, 200);
    scheduler->run();

    // Test: Nested switchMap with concurrency
    printLog("\n--- Test: Nested switchMap ---");
    auto scheduler2 = GTimerScheduler::create("NestedScheduler");
    scheduler2->start();
    GTimerScheduler::makeGlobal(scheduler2);
    auto sched2 = MainThreadScheduler::create();

    Observable::just(1, 2)
        ->switchMap([sched2](const GAny &outer) -> std::shared_ptr<Observable> {
            int outerId = outer.toInt32();
            return Observable::just(10, 20)
                ->map([outerId](const GAny &inner) {
                    return outerId * 100 + inner.toInt32();
                });
        })
        ->subscribe([](const GAny &v) { printLog("Nested switchMap: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Error: {}", e.toString()); },
                    []() { printLog("Nested switchMap: Completed"); });

    scheduler2->post([scheduler2] { scheduler2->stop(); }, 200);
    scheduler2->run();

    printLog("\n✓ Complex Scenarios Tests Completed\n");
}

// ========================================
// Boolean Operators Tests
// ========================================

void testBooleanOperators()
{
    printLog("\n========================================");
    printLog("Testing Boolean Operators");
    printLog("========================================");

    // Test: all
    printLog("\n--- Test: all ---");
    Observable::just(2, 4, 6)
        ->all([](const GAny &v) { return v.toInt32() % 2 == 0; })
        ->subscribe([](const GAny &v) { printLog("all (true): {}", v.toString()); });

    Observable::just(2, 4, 5)
        ->all([](const GAny &v) { return v.toInt32() % 2 == 0; })
        ->subscribe([](const GAny &v) { printLog("all (false): {}", v.toString()); });

    Observable::empty()
        ->all([](const GAny &) { return false; })
        ->subscribe([](const GAny &v) { printLog("all (empty->true): {}", v.toString()); });

    // Test: contains
    printLog("\n--- Test: contains ---");
    Observable::just(1, 2, 3)
        ->contains(2)
        ->subscribe([](const GAny &v) { printLog("contains (true): {}", v.toString()); });

    Observable::just(1, 2, 3)
        ->contains(4)
        ->subscribe([](const GAny &v) { printLog("contains (false): {}", v.toString()); });

    // Test: isEmpty
    printLog("\n--- Test: isEmpty ---");
    Observable::empty()
        ->isEmpty()
        ->subscribe([](const GAny &v) { printLog("isEmpty (true): {}", v.toString()); });

    Observable::just(1)
        ->isEmpty()
        ->subscribe([](const GAny &v) { printLog("isEmpty (false): {}", v.toString()); });

    // Test: defaultIfEmpty
    printLog("\n--- Test: defaultIfEmpty ---");
    Observable::empty()
        ->defaultIfEmpty(999)
        ->subscribe([](const GAny &v) { printLog("defaultIfEmpty (empty): {}", v.toString()); });

    Observable::just(1)
        ->defaultIfEmpty(999)
        ->subscribe([](const GAny &v) { printLog("defaultIfEmpty (not empty): {}", v.toString()); });

    // Test: sequenceEqual
    printLog("\n--- Test: sequenceEqual ---");
    auto s1 = Observable::just(1, 2, 3);
    auto s2 = Observable::just(1, 2, 3);
    Observable::sequenceEqual(s1, s2)
        ->subscribe([](const GAny &v) { printLog("sequenceEqual (true): {}", v.toString()); });

    auto s3 = Observable::just(1, 2);
    Observable::sequenceEqual(s1, s3)
        ->subscribe([](const GAny &v) { printLog("sequenceEqual (false len): {}", v.toString()); });

    auto s4 = Observable::just(1, 2, 4);
    Observable::sequenceEqual(s1, s4)
        ->subscribe([](const GAny &v) { printLog("sequenceEqual (false val): {}", v.toString()); });

    printLog("\n✓ Boolean Operators Tests Completed\n");
}

// ========================================
// Memory Leak Tests
// ========================================

void testMemoryLeaks()
{
    printLog("\n========================================");
    printLog("Testing Memory Management");
    printLog("========================================");

    // Test: Dispose before completion
    printLog("\n--- Test: Dispose before completion ---");
    auto scheduler = GTimerScheduler::create("DisposeScheduler");
    scheduler->start();
    GTimerScheduler::makeGlobal(scheduler);
    auto sched = MainThreadScheduler::create();

    auto disposable = Observable::create([sched](const ObservableEmitterPtr &emitter) {
            printLog("Dispose test: Starting emission");
            sched->scheduleDirect([emitter] {
                emitter->onNext(1);
            }, 50);
            sched->scheduleDirect([emitter] {
                emitter->onNext(2);
            }, 100);
            sched->scheduleDirect([emitter] {
                emitter->onNext(3);
                emitter->onComplete();
            }, 150);
        })
        ->subscribe([](const GAny &v) { printLog("Dispose test received: {}", v.toString()); },
                    [](const GAnyException &e) { printLog("Dispose test error: {}", e.toString()); },
                    []() { printLog("Dispose test: Completed"); });

    // Dispose after 75ms (should only receive value 1)
    scheduler->post([disposable] {
        printLog("Dispose test: Disposing subscription");
        disposable->dispose();
    }, 75);

    scheduler->post([scheduler] { scheduler->stop(); }, 300);
    scheduler->run();

    printLog("\n✓ Memory Management Tests Completed\n");
}

// ========================================
// Main Entry Point
// ========================================

int main(int argc, char **argv)
{
#if GX_PLATFORM_WINDOWS
    system("chcp 65001>nul");
#endif

    initGAnyCore();

    printLog("\n");
    printLog("╔════════════════════════════════════════════════════════════╗");
    printLog("║         RX Library Comprehensive Test Suite                ║");
    printLog("╚════════════════════════════════════════════════════════════╝");
    printLog("\n");

    // Run all test suites
    testCreationOperators();
    testTransformationOperators();
    testFilteringOperators();
    testCombinationOperators();
    testErrorHandling();
    testUtilityOperators();
    testSchedulers();
    testBooleanOperators();
    testComplexScenarios();
    testMemoryLeaks();

    // Final memory leak check
    printLog("\n========================================");
    printLog("Final Memory Leak Check");
    printLog("========================================\n");
    LeakObserver::checkLeak();

    printLog("\n");
    printLog("╔════════════════════════════════════════════════════════════╗");
    printLog("║              All Tests Completed Successfully!             ║");
    printLog("╚════════════════════════════════════════════════════════════╝");
    printLog("\n");

    return EXIT_SUCCESS;
}
