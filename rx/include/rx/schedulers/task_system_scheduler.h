//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_TASK_SYSTEM_SCHEDULER_H
#define RX_TASK_SYSTEM_SCHEDULER_H

#include "task_system_worker.h"
#include <gx/gtasksystem.h>


namespace rx
{
class TaskSystemScheduler : public Scheduler
{
public:
    explicit TaskSystemScheduler(GTaskSystem *taskSystem)
        : mTaskSystem(taskSystem)
    {
    }

    ~TaskSystemScheduler() override = default;

    static std::shared_ptr<TaskSystemScheduler> create(GTaskSystem *taskSystem)
    {
        return std::make_shared<TaskSystemScheduler>(taskSystem);
    }

public:
    WorkerPtr createWorker() override
    {
        return std::make_shared<TaskSystemWorker>(mTaskSystem);
    }

protected:
    GTaskSystem *mTaskSystem;
};
} // rx

#endif //RX_TASK_SYSTEM_SCHEDULER_H
