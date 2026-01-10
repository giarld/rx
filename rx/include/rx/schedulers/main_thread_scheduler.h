//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_MAIN_THREAD_SCHEDULER_H
#define RX_MAIN_THREAD_SCHEDULER_H

#include "timer_scheduler.h"


namespace rx
{
class MainThreadScheduler : public TimerScheduler
{
public:
    explicit MainThreadScheduler()
        : TimerScheduler(GTimerScheduler::global())
    {}

    static std::shared_ptr<MainThreadScheduler> create()
    {
        return std::make_shared<MainThreadScheduler>();
    }
};
} // rx

#endif //RX_MAIN_THREAD_SCHEDULER_H
