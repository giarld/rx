//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_TIMER_SCHEDULER_H
#define RX_TIMER_SCHEDULER_H

#include "timer_worker.h"


namespace rx
{
class TimerScheduler : public Scheduler
{
public:
    explicit TimerScheduler(const GTimerSchedulerPtr &timerScheduler)
        : mTimerScheduler(timerScheduler)
    {
    }

    ~TimerScheduler() override = default;

    static std::shared_ptr<TimerScheduler> create(const GTimerSchedulerPtr &timerScheduler)
    {
        return std::make_shared<TimerScheduler>(timerScheduler);
    }

public:
    WorkerPtr createWorker() override
    {
        return std::make_shared<TimerWorker>(mTimerScheduler);
    }

private:
    GTimerSchedulerPtr mTimerScheduler;
};
} // rx

#endif //RX_TIMER_SCHEDULER_H
