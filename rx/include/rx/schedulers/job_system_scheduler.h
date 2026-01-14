//
// Created by Gxin on 2026/1/14.
//

#ifndef RX_JOB_SYSTEM_SCHEDULER_H
#define RX_JOB_SYSTEM_SCHEDULER_H

#include "job_system_worker.h"
#include "../leak_observer.h"
#include <gx/gjobsystem.h>


namespace rx
{
class JobSystemScheduler : public Scheduler
{
public:
    explicit JobSystemScheduler(GJobSystem *jobSystem)
        : mJobSystem(jobSystem)
    {
        LeakObserver::make<JobSystemScheduler>();
    }

    ~JobSystemScheduler() override
    {
        LeakObserver::release<JobSystemScheduler>();
    }

    static std::shared_ptr<JobSystemScheduler> create(GJobSystem *jobSystem)
    {
        return std::make_shared<JobSystemScheduler>(jobSystem);
    }

public:
    WorkerPtr createWorker() override
    {
        return std::make_shared<JobSystemWorker>(mJobSystem);
    }

protected:
    GJobSystem *mJobSystem;
};
} // rx

#endif //RX_JOB_SYSTEM_SCHEDULER_H