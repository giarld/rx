//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_NEW_THREAD_SCHEDULER_H
#define RX_NEW_THREAD_SCHEDULER_H

#include "new_thread_worker.h"


namespace rx
{
class NewThreadScheduler : public Scheduler
{
public:
    explicit NewThreadScheduler(ThreadPriority threadPriority = ThreadPriority::Normal)
        : mThreadPriority(threadPriority)
    {
    }

    ~NewThreadScheduler() override = default;

public:
    WorkerPtr createWorker() override
    {
        return std::make_shared<NewThreadWorker>(mThreadPriority);
    }

private:
    ThreadPriority mThreadPriority = ThreadPriority::Normal;
};
} // rx

#endif //RX_NEW_THREAD_SCHEDULER_H
