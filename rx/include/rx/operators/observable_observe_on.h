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
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        const auto t = mWorker->schedule([d, value] {
            d->onNext(value);
        });
    }

    void onError(const GAnyException &e) override
    {
        if (mDone.load()) {
            return;
        }

        mDone.store(true, std::memory_order_release);

        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        const auto t = mWorker->schedule([weakThiz, d, e] {
            d->onError(e);
            if (const auto thiz = weakThiz.lock()) {
                thiz->dispose();
            }
        });
    }

    void onComplete() override
    {
        if (mDone.load()) {
            return;
        }

        mDone.store(true, std::memory_order_release);

        auto d = mDownstream;
        std::weak_ptr<ObserveOnObserver> weakThiz = this->shared_from_this();
        const auto t = mWorker->schedule([weakThiz, d] {
            d->onComplete();
            if (const auto thiz = weakThiz.lock()) {
                thiz->dispose();
            }
        });
    }

    void dispose() override
    {
        if (!mDisposed.exchange(true, std::memory_order_release)) {
            if (const auto up = mUpstream) {
                up->dispose();
                mUpstream = nullptr;
            }

            if (mWorker) {
                mWorker->dispose();
            }
        }
    }

    bool isDisposed() const override
    {
        return mDisposed.load();
    }

private:
    ObserverPtr mDownstream;
    std::shared_ptr<Worker> mWorker;
    DisposablePtr mUpstream = nullptr;
    std::atomic<bool> mDone = false;
    std::atomic<bool> mDisposed = false;
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
