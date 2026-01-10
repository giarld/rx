//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_OBSERVE_ON_H
#define RX_OBSERVABLE_OBSERVE_ON_H

#include "../observable.h"
#include "../scheduler.h"
#include "gx/gmutex.h"


namespace rx
{
class ObserveOnObserver : public Observer, public Disposable, public std::enable_shared_from_this<ObserveOnObserver>
{
public:
    explicit ObserveOnObserver(const ObserverPtr &observer, const WorkerPtr &worker)
        : mDownstream(observer), mWorker(worker)
    {
    }

    ~ObserveOnObserver() override
    {
        Log("~ObserveOnObserver");
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (validate(mUpstream, d)) {
            mUpstream = d;
            const auto thiz = this->shared_from_this();
            mDownstream->onSubscribe(thiz);
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load()) {
            return;
        }

        uint64_t idx = mTaskIdx.fetch_add(1);
        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        const auto t = mWorker->schedule([weakThiz, d, value, idx] {
            d->onNext(value);
            if (const auto thiz = weakThiz.lock()) {
                thiz->releaseTask(idx);
            }
        });
        mTasksLock.lock();
        mTasks[idx] = t;
        mTasksLock.unlock();
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.load()) {
            return;
        }

        uint64_t idx = mTaskIdx.fetch_add(1);
        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        const auto t = mWorker->schedule([weakThiz, d, e, idx] {
            d->onError(e);
            if (const auto thiz = weakThiz.lock()) {
                thiz->releaseTask(idx);
            }
        });
        mTasksLock.lock();
        mTasks[idx] = t;
        mTasksLock.unlock();

        mDone.store(true, std::memory_order_release);
    }

    void onComplete() override
    {
        if (mDone.load()) {
            return;
        }

        uint64_t idx = mTaskIdx.fetch_add(1);
        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        const auto t = mWorker->schedule([weakThiz, d, idx] {
            d->onComplete();
            if (const auto thiz = weakThiz.lock()) {
                thiz->releaseTask(idx);
            }
        });
        mTasksLock.lock();
        mTasks[idx] = t;
        mTasksLock.unlock();

        mDone.store(true, std::memory_order_release);
    }

    void dispose() override
    {
        if (!mDisposed.load()) {
            mDisposed.store(true, std::memory_order_release);
            mUpstream->dispose();
            mWorker->dispose();

            mTasks.clear();
        }
    }

    bool isDisposed() const override
    {
        return mDisposed.load();
    }

private:
    void releaseTask(uint64_t idx)
    {
        mTasksLock.lock();
        mTasks.erase(idx);
        mTasksLock.unlock();
    }

    static bool validate(const DisposablePtr &current, const DisposablePtr &next)
    {
        if (next == nullptr) {
            return false;
        }
        if (current != nullptr) {
            next->dispose();
            return false;
        }
        return true;
    }

private:
    ObserverPtr mDownstream;
    std::shared_ptr<Worker> mWorker;
    DisposablePtr mUpstream = nullptr;
    std::atomic<bool> mDone = false;
    std::atomic<bool> mDisposed = false;

    std::map<uint64_t, DisposablePtr> mTasks;
    GMutex mTasksLock;
    std::atomic<uint64_t> mTaskIdx = 0;
};

class ObservableObserveOn : public Observable
{
public:
    explicit ObservableObserveOn(const ObservableSourcePtr &source, SchedulerPtr scheduler)
        : mSource(source), mScheduler(std::move(scheduler))
    {
    }

    ~ObservableObserveOn() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        WorkerPtr w = mScheduler->createWorker();
        const auto parent = std::make_shared<ObserveOnObserver>(observer, w);
        mSource->subscribe(parent);
    }

private:
    ObservableSourcePtr mSource;
    SchedulerPtr mScheduler;
};
} // rx

#endif //RX_OBSERVABLE_OBSERVE_ON_H
