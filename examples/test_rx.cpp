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

    {
        auto scheduler = std::make_shared<NewThreadScheduler>();
        Observable::create([](const ObservableEmitterPtr &emitter) {
                    // GThread::sleep(2);
                    emitter->onNext(123);
                    emitter->onNext("234");
                    // throw GAnyException("Error");
                    // emitter->onError(GAnyException("Error"));
                    emitter->onNext(345);
                    emitter->onComplete();
                })
                ->subscribeOn(scheduler)
                ->subscribe([](const GAny &v) {
                                Log(">>>>> {}", v.toString());
                            }, [](const GAnyException &e) {
                                LogE("Exception: {}", e.toString());
                            }, []() {
                                Log("Completed!!");
                            });
    }

    GThread::sleep(2);

    //
    // Observable::just("Hello")
    //         ->subscribe([](const GAny &v) {
    //             Log("Just output: {}", v.toString());
    //         });
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
