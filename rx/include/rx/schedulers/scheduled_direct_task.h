//
// Created by Gxin on 2026/1/9.
//

#ifndef RX_SCHEDULED_DIRECT_TASK_H
#define RX_SCHEDULED_DIRECT_TASK_H

#include "../scheduler.h"

#include <gx/gtasksystem.h>


namespace rx
{
class ScheduledDirectTask : public Disposable
{
public:
    explicit ScheduledDirectTask(GTaskSystem::Task<bool> &&task)
        : mTask(std::move(task))
    {
    }

    ~ScheduledDirectTask() override = default;

public:
    void dispose() override
    {
        mTask.cancel();
    }

    bool isDisposed() const override
    {
        return mTask.isCompleted() || !mTask.isValid();
    }

private:
    mutable GTaskSystem::Task<bool> mTask;
};
} // rx

#endif //RX_SCHEDULED_DIRECT_TASK_H
