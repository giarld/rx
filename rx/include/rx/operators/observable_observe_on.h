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
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            mDownstream->onSubscribe(this->shared_from_this());
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone.load(std::memory_order_acquire) || isDisposed()) {
            return;
        }

        auto d = mDownstream;
        //
        {
            GLockerGuard lock(mQueueLock);
            mQueue.push([d, value] {
                d->onNext(value);
            });
        }
        schedule();
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        //
        {
            GLockerGuard lock(mQueueLock);
            mQueue.push([weakThiz, d, e] {
                d->onError(e);
                if (const auto thiz = weakThiz.lock()) {
                    thiz->disposeWorker();
                }
            });
        }
        schedule();
    }

    void onComplete() override
    {
        if (mDone.exchange(true, std::memory_order_acq_rel)) {
            return;
        }

        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        //
        {
            GLockerGuard lock(mQueueLock);
            mQueue.push([weakThiz, d] {
                d->onComplete();
                if (const auto thiz = weakThiz.lock()) {
                    thiz->disposeWorker();
                }
            });
        }
        schedule();
    }

    void dispose() override
    {
        if (!mDisposed.exchange(true, std::memory_order_acq_rel)) {
            if (const auto up = mUpstream) {
                up->dispose();
            }
            disposeWorker();

            GLockerGuard lock(mQueueLock);
            while (!mQueue.empty()) {
                mQueue.pop();
            }
        }
    }

    void disposeWorker()
    {
        if (const auto w = mWorker) {
            w->dispose();
        }
        mWorker = nullptr;
        mDownstream = nullptr;
        mUpstream = nullptr;
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
            mWorker->schedule([weakThiz] {
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
                    task();
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
