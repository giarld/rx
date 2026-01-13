//
// Created by Gxin on 2026/1/4.
//

#define USE_GANY_CORE
#include <gx/gany.h>

#include <rx/rx.h>

#include "gx/debug.h"

#include <cstdlib>


using namespace rx;

void test()
{
    const auto mainScheduler = GTimerScheduler::create("MainScheduler");
    mainScheduler->start();
    GTimerScheduler::makeGlobal(mainScheduler);

    const auto threadScheduler = NewThreadScheduler::create();
    const auto timerScheduler = MainThreadScheduler::create();
    Observable::create([&](const ObservableEmitterPtr &emitter) {
                emitter->onNext(1);
                emitter->onNext(2);
                // throw GAnyException("Error");
                // emitter->onError(GAnyException("Error"));
                mainScheduler->post([emitter] {
                    emitter->onNext(3);
                    emitter->onComplete();
                }, 1000);
            })
            ->map([](const GAny &x) {
                // GThread::sleep(1);
                return x + 1;
            })
            ->scan([](const GAny &last, const GAny &item) {
                return last * item;
            })
            ->repeat(2)
            ->subscribeOn(threadScheduler)
            ->observeOn(timerScheduler)
            ->subscribe([](const GAny &v) {
                            Log(">>>>> {}", v.toString());
                        }, [](const GAnyException &e) {
                            LogE("Exception: {}", e.toString());
                        }, [&]() {
                            Log("Completed!!");
                            mainScheduler->post([&] {
                                mainScheduler->stop();
                            }, 1);
                        });

    mainScheduler->run();


    Observable::just(1, 2, 3)
            ->buffer(1)
            ->flatMap([](const GAny &v) {
                return Observable::just(v);
            })
            ->subscribe([](const GAny &v) {
                            Log(">>>>> {}", v.toString());
                        }, [](const GAnyException &e) {
                            LogE("Exception: {}", e.toString());
                        }, [&]() {
                            Log("Completed!!");
                        });


    auto source = Observable::just("Hello", "World", "A", "B", "C");
    Observable::defer(source)->subscribe([](const GAny &v) {
        Log("Just output: {}", v.toString());
    });


    Observable::empty()
            ->subscribe([](const GAny &v) {
                        }, [](const GAnyException &e) {
                            LogE("Empty Exception: {}", e.toString());
                        }, [] {
                            Log("Empty Completed!!");
                        });

    Observable::just(10)
            // ->map([](const GAny &x) {
            //     GAny v = GAny::array();
            //     for (int32_t i = 0; i < x.toInt32(); ++i) {
            //         v.pushBack(i);
            //     }
            //     Log("Map: v = {}", v.toString());
            //     return v;
            // })
            ->repeat(10)
            ->flatMap([](const GAny &v) {
                return Observable::range(0, v.toInt32());
            })
            ->filter([](const GAny &v) {
                return v.toInt32() % 2 == 0;
            })
            ->buffer(10)
            ->subscribe([](const GAny &v) {
                            Log("Output: {}", v.toString());
                        }, [](const GAnyException &e) {
                            LogE("Exception: {}", e.toString());
                        }, []() {
                            Log("Completed!!");
                        });

    // Test ignoreElements operator
    Log("\n=== Testing ignoreElements ===");
    
    // Test ignoreElements: only onComplete should be called
    Observable::just(1, 2, 3, 4, 5)
            ->ignoreElements()
            ->subscribe([](const GAny &v) {
                            Log("ignoreElements onNext (shouldn't print): {}", v.toString());
                        }, [](const GAnyException &e) {
                            LogE("ignoreElements error: {}", e.toString());
                        }, []() {
                            Log("ignoreElements Completed (expected)!");
                        });

    // Test ignoreElements with error
    Observable::just(1, 2, 3)
            ->map([](const GAny &x) {
                if (x.toInt32() == 2) {
                    throw GAnyException("Test error");
                }
                return x;
            })
            ->ignoreElements()
            ->subscribe([](const GAny &v) {
                            Log("ignoreElements onNext: {}", v.toString());
                        }, [](const GAnyException &e) {
                            LogE("ignoreElements error (expected): {}", e.toString());
                        }, []() {
                            Log("ignoreElements Completed");
                        });

    // Test ignoreElements with empty observable
    Observable::empty()
            ->ignoreElements()
            ->subscribe([](const GAny &v) {
                            Log("ignoreElements empty onNext: {}", v.toString());
                        }, [](const GAnyException &e) {
                            LogE("ignoreElements empty error: {}", e.toString());
                        }, []() {
                            Log("ignoreElements empty Completed (expected)!");
                        });
}

int main(int argc, char **argv)
{
#if GX_PLATFORM_WINDOWS
    system("chcp 65001>nul");
#endif

    initGAnyCore();

    test();

    LeakObserver::checkLeak();

    return EXIT_SUCCESS;
}
