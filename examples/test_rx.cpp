//
// Created by Gxin on 2026/1/4.
//

#define USE_GANY_CORE
#include <gx/gany.h>

#include <rx/rx.h>

#include "gx/debug.h"

#include <cstdlib>


using namespace rx;

int main(int argc, char **argv)
{
#if GX_PLATFORM_WINDOWS
    system("chcp 65001>nul");
#endif

    initGAnyCore();

    auto mainScheduler = GTimerScheduler::create("MainScheduler");
    mainScheduler->start();
    GTimerScheduler::makeGlobal(mainScheduler);

    auto threadScheduler = NewThreadScheduler::create();
    auto timerScheduler = MainThreadScheduler::create();
    Observable::create([&](const ObservableEmitterPtr &emitter) {
                emitter->onNext(123);
                emitter->onNext("234");
                // throw GAnyException("Error");
                // emitter->onError(GAnyException("Error"));
                mainScheduler->post([emitter] {
                    emitter->onNext(345);
                    emitter->onComplete();
                }, 1000);
            })
            ->map([](const GAny &x) {
                // GThread::sleep(1);
                return x + 1;
            })
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

    //
    //
    // auto source = Observable::just("Hello", "World", "A", "B", "C");
    // Observable::defer(source)->subscribe([](const GAny &v) {
    //     Log("Just output: {}", v.toString());
    // });

    //
    // Observable::empty()
    //         ->subscribe([](const GAny &v) {
    //                     }, [](const GAnyException &e) {
    //                         LogE("Empty Exception: {}", e.toString());
    //                     }, []() {
    //                         Log("Empty Completed!!");
    //                     });

    // Observable::just(10)
    //         ->map([](const GAny &x) {
    //             GAny v = GAny::array();
    //             for (int32_t i = 0; i < x.toInt32(); ++i) {
    //                 v.pushBack(i);
    //             }
    //             Log("Map: v = {}", v.toString());
    //             return v;
    //         })
    //         ->subscribe([](const GAny &v) {
    //                         Log(">>>>> {}", v.toString());
    //                     }, [](const GAnyException &e) {
    //                         LogE("Exception: {}", e.toString());
    //                     }, []() {
    //                         Log("Completed!!");
    //                     });

    return EXIT_SUCCESS;
}
