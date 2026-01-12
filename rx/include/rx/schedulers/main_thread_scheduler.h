//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_MAIN_THREAD_SCHEDULER_H
#define RX_MAIN_THREAD_SCHEDULER_H

#include "timer_scheduler.h"
#include "../leak_observer.h"


namespace rx
{
class MainThreadScheduler : public TimerScheduler
{
public:
    explicit MainThreadScheduler()
        : TimerScheduler(GTimerScheduler::global())
    {
        LeakObserver::make<MainThreadScheduler>();
    }

    ~MainThreadScheduler() override
    {
        LeakObserver::release<MainThreadScheduler>();
    }

    static std::shared_ptr<MainThreadScheduler> create()
    {
        return std::make_shared<MainThreadScheduler>();
    }
};
} // rx

#endif //RX_MAIN_THREAD_SCHEDULER_H
