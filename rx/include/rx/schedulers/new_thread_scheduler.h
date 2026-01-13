//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_NEW_THREAD_SCHEDULER_H
#define RX_NEW_THREAD_SCHEDULER_H

#include "new_thread_worker.h"
#include "../scheduler.h"
#include "../leak_observer.h"


namespace rx
{
class NewThreadScheduler : public Scheduler
{
public:
    explicit NewThreadScheduler(ThreadPriority threadPriority)
        : mThreadPriority(threadPriority)
    {
        LeakObserver::make<NewThreadScheduler>();
    }

    ~NewThreadScheduler() override
    {
        LeakObserver::release<NewThreadScheduler>();
    }

    static std::shared_ptr<NewThreadScheduler> create(ThreadPriority threadPriority = ThreadPriority::Normal)
    {
        return std::make_shared<NewThreadScheduler>(threadPriority);
    }

public:
    WorkerPtr createWorker() override
    {
        return WorkerPtr(new NewThreadWorker(mThreadPriority));
    }

private:
    ThreadPriority mThreadPriority;
};
} // rx

#endif //RX_NEW_THREAD_SCHEDULER_H
