//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_TASK_SYSTEM_SCHEDULER_H
#define RX_TASK_SYSTEM_SCHEDULER_H

#include "task_system_worker.h"
#include "../leak_observer.h"
#include <gx/gtasksystem.h>
#include <gx/gtimer.h>


namespace rx
{
class TaskSystemScheduler : public Scheduler
{
public:
    explicit TaskSystemScheduler(GTaskSystem *taskSystem)
        : mTaskSystem(taskSystem)
    {
        LeakObserver::make<TaskSystemScheduler>();

        mTimerScheduler = GTimerScheduler::create("TaskSystemSchedulerTimer");
        mTimerScheduler->start();
        mTimerThread.setRunnable([this] {
            mTimerScheduler->run();
        });
        mTimerThread.start();
    }

    ~TaskSystemScheduler() override
    {
        LeakObserver::release<TaskSystemScheduler>();

        mTimerScheduler->stop();
    }

    static std::shared_ptr<TaskSystemScheduler> create(GTaskSystem *taskSystem)
    {
        return std::make_shared<TaskSystemScheduler>(taskSystem);
    }

public:
    WorkerPtr createWorker() override
    {
        return std::make_shared<TaskSystemWorker>(mTaskSystem, mTimerScheduler.get());
    }

protected:
    GTaskSystem *mTaskSystem;
    GTimerSchedulerPtr mTimerScheduler; // 用于调度 delay 任务的计时器调度器
    GThread mTimerThread;
};
} // rx

#endif //RX_TASK_SYSTEM_SCHEDULER_H
