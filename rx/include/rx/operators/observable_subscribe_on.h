//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_OBSERVABLE_SUBSCRIBE_ON_H
#define RX_OBSERVABLE_SUBSCRIBE_ON_H

#include "../observable.h"
#include "../scheduler.h"


namespace rx
{
class SubscribeOnObserver : public Observer, public Disposable
{
public:
    explicit SubscribeOnObserver(const ObserverPtr &observer)
        : mDownstream(observer), mDisposable(std::make_shared<AtomicDisposable>()), mUpstream(std::make_shared<AtomicDisposable>())
    {
    }

    ~SubscribeOnObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        mUpstream = d;
    }

    void onNext(const GAny &value) override
    {
        mDownstream->onNext(value);
    }

    void onError(const GAnyException &e) override
    {
        mDownstream->onError(e);
    }

    void onComplete() override
    {
        mDownstream->onComplete();
    }

    void dispose() override
    {
        if (const auto d = mDisposable) {
            d->dispose();
            mDisposable = nullptr;
        }
        if (const auto d = mUpstream) {
            d->dispose();
            mUpstream = nullptr;
        }
    }

    bool isDisposed() const override
    {
        if (const auto d = mDisposable) {
            return d->isDisposed();
        }
        return false;
    }

    void setDisposable(const DisposablePtr &d)
    {
        mDisposable = d;
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mDisposable;
    DisposablePtr mUpstream;
};

class ObservableSubscribeOn : public Observable
{
public:
    explicit ObservableSubscribeOn(const ObservableSourcePtr &source, SchedulerPtr scheduler)
        : mSource(source), mScheduler(std::move(scheduler))
    {
    }

    ~ObservableSubscribeOn() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<SubscribeOnObserver>(observer);
        observer->onSubscribe(parent);

        const auto source = mSource;
        std::weak_ptr<SubscribeOnObserver> weakParent = parent;
        parent->setDisposable(mScheduler->scheduleDirect([source, weakParent] {
            if (const auto p = weakParent.lock()) {
                source->subscribe(p);
            }
        }));
    }

private:
    ObservableSourcePtr mSource;
    SchedulerPtr mScheduler;
};
} // rx

#endif //RX_OBSERVABLE_SUBSCRIBE_ON_H
