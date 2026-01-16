//
// Created by Gxin on 2026/1/13.
//

#ifndef RX_OBSERVABLE_DELAY_H
#define RX_OBSERVABLE_DELAY_H

#include "../observable.h"
#include "../leak_observer.h"
#include "../scheduler.h"


namespace rx
{
class DelayObserver : public Observer, public Disposable, public std::enable_shared_from_this<DelayObserver>
{
public:
    explicit DelayObserver(const ObserverPtr &observer, uint64_t delay, const WorkerPtr &worker)
        : mDownstream(observer), mDelay(delay), mWorker(worker)
    {
        LeakObserver::make<DelayObserver>();
    }

    ~DelayObserver() override
    {
        LeakObserver::release<DelayObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            if (const auto ds = mDownstream) {
                ds->onSubscribe(this->shared_from_this());
            }
        }
    }

    void onNext(const GAny &value) override
    {
        std::weak_ptr<DelayObserver> thisWeak = shared_from_this();
        mWorker->schedule([thisWeak, value] {
            const auto thiz = thisWeak.lock();
            if (thiz && !thiz->mWorker->isDisposed()) {
                if (const auto d = thiz->mDownstream) {
                    d->onNext(value);
                }
            }
        }, mDelay);
    }

    void onError(const GAnyException &e) override
    {
        std::weak_ptr<DelayObserver> thisWeak = shared_from_this();
        mWorker->schedule([thisWeak, e] {
            const auto thiz = thisWeak.lock();
            if (thiz) {
                if (const auto d = thiz->mDownstream) {
                    d->onError(e);
                }
                thiz->mWorker->dispose();
            }
        });
    }

    void onComplete() override
    {
        std::weak_ptr<DelayObserver> thisWeak = shared_from_this();
        mWorker->schedule([thisWeak] {
            const auto thiz = thisWeak.lock();
            if (thiz) {
                if (const auto d = thiz->mDownstream) {
                    d->onComplete();
                }
                thiz->mWorker->dispose();
            }
        }, mDelay);
    }

    void dispose() override
    {
        if (const auto d = mUpstream) {
            d->dispose();
            mUpstream = nullptr;
        }
        mWorker->dispose();
    }

    bool isDisposed() const override
    {
        return mWorker->isDisposed();
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mUpstream;
    uint64_t mDelay;
    WorkerPtr mWorker;
};

class ObservableDelay : public Observable
{
public:
    explicit ObservableDelay(ObservableSourcePtr source, uint64_t delay, const SchedulerPtr &scheduler)
        : mSource(std::move(source)), mDelay(delay), mScheduler(scheduler)
    {
        LeakObserver::make<ObservableDelay>();
    }

    ~ObservableDelay() override
    {
        LeakObserver::release<ObservableDelay>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        WorkerPtr w = mScheduler->createWorker();
        mSource->subscribe(std::make_shared<DelayObserver>(observer, mDelay, w));
    }

private:
    ObservableSourcePtr mSource;
    uint64_t mDelay;
    SchedulerPtr mScheduler;
};
} // rx

#endif //RX_OBSERVABLE_DELAY_H
