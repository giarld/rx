//
// Created by Gxin on 2026/1/10.
//
#ifndef RX_OBSERVABLE_OBSERVE_ON_H
#define RX_OBSERVABLE_OBSERVE_ON_H

#include "../observable.h"
#include "../scheduler.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"
#include "gx/gmutex.h"
#include <queue>
#include <atomic>
#include <mutex>


namespace rx
{
class ObserveOnObserver : public Observer, public Disposable, public std::enable_shared_from_this<ObserveOnObserver>
{
public:
    explicit ObserveOnObserver(const ObserverPtr &observer, const WorkerPtr &worker)
        : mDownstream(observer), mWorker(worker)
    {
        LeakObserver::make<ObserveOnObserver>();
    }

    ~ObserveOnObserver() override
    {
        LeakObserver::release<ObserveOnObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        bool accepted = false;
        {
            GLockerGuard lock(mStateLock);
            if (!mDisposed && !mUpstream) {
                mUpstream = d;
                accepted = true;
            }
        }
        if (!accepted) {
            d->dispose();
            return;
        }
        if (mDownstream) {
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        if (mDone.load(std::memory_order_acquire) || isDisposed()) {
            return;
        }

        {
            GLockerGuard lock(mQueueLock);
            if (mDisposed) {
                return;
            }
            mQueue.push([this, value] {
                if (const auto downstream = mDownstream) {
                    downstream->onNext(value);
                }
            });
        }
        schedule();
    }

    void onError(const GAnyException &e) override
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        if (isDisposed()) {
            return;
        }
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        {
            GLockerGuard lock(mQueueLock);
            mQueue.push([this, e] {
                const auto downstream = mDownstream;
                if (downstream) {
                    downstream->onError(e);
                }
                releaseResources();
            });
        }
        schedule();
    }

    void onComplete() override
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        if (isDisposed()) {
            return;
        }
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        {
            GLockerGuard lock(mQueueLock);
            mQueue.push([this] {
                const auto downstream = mDownstream;
                if (downstream) {
                    downstream->onComplete();
                }
                releaseResources();
            });
        }
        schedule();
    }

    void dispose() override
    {
        std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
        if (!mDisposed.exchange(true, std::memory_order_acq_rel)) {
            DisposablePtr up;
            {
                GLockerGuard lock(mStateLock);
                up = mUpstream;
            }
            if (up) {
                up->dispose();
            }
            releaseResources();
        }
    }

    void releaseResources()
    {
        mDisposed.store(true, std::memory_order_release);

        WorkerPtr worker;
        {
            GLockerGuard lock(mStateLock);
            mUpstream = nullptr;
            worker = std::move(mWorker);
        }
        if (worker) {
            worker->dispose();
        }

        {
            GLockerGuard lock(mQueueLock);
            while (!mQueue.empty()) {
                mQueue.pop();
            }
        }
        mDownstream = nullptr;
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

private:
    void schedule()
    {
        if (mWip.fetch_add(1, std::memory_order_acq_rel) == 0) {
            std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
            WorkerPtr worker;
            {
                GLockerGuard lock(mStateLock);
                worker = mWorker;
            }
            if (!worker) {
                mWip.fetch_sub(1, std::memory_order_acq_rel);
                return;
            }
            worker->schedule([weakThiz] {
                if (const auto thiz = weakThiz.lock()) {
                    thiz->drain();
                }
            });
        }
    }

    void drain()
    {
        int missed = 1;

        while (true) {
            while (true) {
                if (isDisposed()) {
                    GLockerGuard lock(mQueueLock);
                    while (!mQueue.empty()) {
                        mQueue.pop();
                    }
                    return;
                }

                std::function<void()> task = nullptr;
                //
                {
                    GLockerGuard lock(mQueueLock);
                    if (!mQueue.empty()) {
                        task = std::move(mQueue.front());
                        mQueue.pop();
                    }
                }

                if (task) {
                    std::lock_guard<std::recursive_mutex> signalLock(mSignalLock);
                    if (!isDisposed()) {
                        task();
                    }
                } else {
                    break;
                }
            }

            missed = mWip.fetch_sub(missed, std::memory_order_acq_rel) - missed;
            if (missed == 0) {
                break;
            }
        }
    }

private:
    ObserverPtr mDownstream;
    std::shared_ptr<Worker> mWorker;
    DisposablePtr mUpstream = nullptr;

    std::atomic<bool> mDone = false;
    std::atomic<bool> mDisposed = false;

    std::atomic<int32_t> mWip = 0;

    std::queue<std::function<void()> > mQueue;
    GMutex mQueueLock;
    GMutex mStateLock;
    std::recursive_mutex mSignalLock;
};

class ObservableObserveOn : public Observable
{
public:
    explicit ObservableObserveOn(const ObservableSourcePtr &source, SchedulerPtr scheduler)
        : mSource(source), mScheduler(std::move(scheduler))
    {
        LeakObserver::make<ObservableObserveOn>();
    }

    ~ObservableObserveOn() override
    {
        LeakObserver::release<ObservableObserveOn>();
    }

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
