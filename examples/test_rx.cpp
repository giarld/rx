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

    Observable::create([](const ObservableEmitterPtr &emitter) {
        emitter->onNext(123);
        emitter->onCompleted();
    })
    ->subscribe([](const GAny &v) {
       Log(">>>>> {}", v.toInt32());
    });
    return EXIT_SUCCESS;
}