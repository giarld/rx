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

    ~ObserveOnObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (Disposable::validate(mUpstream.get(), d.get())) {
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

        auto d = mDownstream;

        mQueueLock.lock();
        mQueue.push([d, value] {
            d->onNext(value);
        });
        mQueueLock.unlock();

        schedule();
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.load()) {
            return;
        }

        mDone.store(true, std::memory_order_release);

        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        mQueueLock.lock();
        mQueue.push([weakThiz, d, e] {
            d->onError(e);
            if (const auto thiz = weakThiz.lock()) {
                thiz->dispose();
            }
        });
        mQueueLock.unlock();

        schedule();
    }

    void onComplete() override
    {
        if (mDone.load()) {
            return;
        }

        mDone.store(true, std::memory_order_release);

        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        mQueueLock.lock();
        mQueue.push([weakThiz, d] {
            d->onComplete();
            if (const auto thiz = weakThiz.lock()) {
                thiz->dispose();
            }
        });
        mQueueLock.unlock();

        schedule();
    }

    void dispose() override
    {
        if (!mDisposed.exchange(true, std::memory_order_release)) {
            if (const auto up = mUpstream) {
                up->dispose();
                mUpstream = nullptr;
            }
            if (const auto w = mWorker) {
                w->dispose();
            }

            mQueueLock.lock();
            while (!mQueue.empty()) {
                mQueue.pop();
            }
            mQueueLock.unlock();
        }
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

private:
    void schedule()
    {
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        mWorker->schedule([weakThiz] {
            if (const auto thiz = weakThiz.lock()) {
                std::function<void()> f = nullptr;
                thiz->mQueueLock.lock();
                if (!thiz->mQueue.empty()) {
                    f = thiz->mQueue.front();
                    thiz->mQueue.pop();
                }
                thiz->mQueueLock.unlock();

                if (f) {
                    f();
                }
            }
        });
    }

private:
    ObserverPtr mDownstream;
    std::shared_ptr<Worker> mWorker;
    DisposablePtr mUpstream = nullptr;
    std::atomic<bool> mDone = false;
    std::atomic<bool> mDisposed = false;

    std::queue<std::function<void()> > mQueue;
    GMutex mQueueLock;
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
