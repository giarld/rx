//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_NEW_THREAD_SCHEDULER_H
#define RX_NEW_THREAD_SCHEDULER_H

#include "task_system_scheduler.h"
#include "../leak_observer.h"


namespace rx
{
class NewThreadScheduler : public TaskSystemScheduler
{
public:
    explicit NewThreadScheduler(ThreadPriority threadPriority)
        : TaskSystemScheduler(nullptr), mTaskSystemPtr(std::make_unique<GTaskSystem>("NewThreadScheduler_Thread", 1))
    {
        LeakObserver::make<NewThreadScheduler>();
        mTaskSystem = mTaskSystemPtr.get();
        mTaskSystem->setThreadPriority(threadPriority);
        mTaskSystem->start();
    }

    ~NewThreadScheduler() override
    {
        LeakObserver::release<NewThreadScheduler>();
        mTaskSystemPtr->stop();
        mTaskSystemPtr = nullptr;
    }

    static std::shared_ptr<NewThreadScheduler> create(ThreadPriority threadPriority = ThreadPriority::Normal)
    {
        return std::make_shared<NewThreadScheduler>(threadPriority);
    }

public:
    void start() override
    {
        if (mTaskSystemPtr && !mTaskSystemPtr->isRunning()) {
            mTaskSystemPtr->start();
        }
    }

    void shutdown() override
    {
        if (mTaskSystemPtr && mTaskSystemPtr->isRunning()) {
            mTaskSystemPtr->stop();
        }
    }

private:
    std::unique_ptr<GTaskSystem> mTaskSystemPtr;
};
} // rx

#endif //RX_NEW_THREAD_SCHEDULER_H
