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
    explicit SubscribeOnObserver(Observer *observer)
        : mObserver(observer), mDisposable(std::make_shared<AtomicDisposable>())
    {
    }

    ~SubscribeOnObserver() override = default;

public:
    void onSubscribe(const DisposablePtr &d) override
    {
    }

    void onNext(const GAny &value) override
    {
        mObserver->onNext(value);
    }

    void onError(const GAnyException &e) override
    {
        mObserver->onError(e);
    }

    void onComplete() override
    {
        mObserver->onComplete();
    }

    void dispose() override
    {
        if (const auto d = mDisposable) {
            d->dispose();
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
    Observer *mObserver;
    DisposablePtr mDisposable;
};

class ObservableSubscribeOn : public Observable
{
public:
    explicit ObservableSubscribeOn(ObservableSourcePtr source, SchedulerPtr scheduler)
        : mSource(std::move(source)), mScheduler(std::move(scheduler))
    {
    }

    ~ObservableSubscribeOn() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        // auto parent = std::make_shared<SubscribeOnObserver>(observer.get());
        // observer->onSubscribe(parent);
    }

private:
    ObservableSourcePtr mSource;
    SchedulerPtr mScheduler;
};
} // rx

#endif //RX_OBSERVABLE_SUBSCRIBE_ON_H
