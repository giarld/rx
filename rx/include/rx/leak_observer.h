//
// Created by Gxin on 2026/1/12.
//

#ifndef RX_LEAK_OBSERVER_H
#define RX_LEAK_OBSERVER_H

#include <gx/gany.h>
#include <gx/gmutex.h>
#include <gx/debug.h>


namespace rx
{
class GX_API LeakObserver
{
public:
    template<typename T>
    static void make()
    {
#if GX_DEBUG
        auto name = GAnyTypeInfoP<T>().getDemangleName();
        GLockerGuard locker(sLock);
        const int64_t v = sObjectMap[name];
        sObjectMap[name] = v + 1;
#endif
    }

    template<typename T>
    static void release()
    {
#if GX_DEBUG
        auto name = GAnyTypeInfoP<T>().getDemangleName();
        GLockerGuard locker(sLock);
        const int64_t v = sObjectMap[name];
        sObjectMap[name] = v - 1;
#endif
    }

    static void checkLeak()
    {
#if GX_DEBUG
        for (const auto &[name, v]: sObjectMap) {
            if (v > 0) {
                LogE("Object Leak: {}, count: {}", name, v);
            }
        }
#endif
    }

#if GX_DEBUG
private:
    static std::unordered_map<std::string, int64_t> sObjectMap;
    static GMutex sLock;
#endif
};
} // rx

#endif //RX_LEAK_OBSERVER_H
