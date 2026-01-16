//
// Created by Gxin on 2026/1/8.
//

#ifndef RX_OBSERVABLE_SUBSCRIBE_ON_H
#define RX_OBSERVABLE_SUBSCRIBE_ON_H

#include "../observable.h"
#include "../scheduler.h"
#include "../disposables/disposable_helper.h"
#include "../leak_observer.h"


namespace rx
{
class SubscribeOnObserver : public Observer, public Disposable
{
public:
    explicit SubscribeOnObserver(const ObserverPtr &observer)
        : mDownstream(observer)
    {
        LeakObserver::make<SubscribeOnObserver>();
    }

    ~SubscribeOnObserver() override
    {
        LeakObserver::release<SubscribeOnObserver>();
    }

public:
    void onSubscribe(const DisposablePtr &d) override
    {
        DisposableHelper::setOnce(mUpstream, d, mLock);
    }

    void onNext(const GAny &value) override
    {
        if (const auto d = mDownstream) {
            d->onNext(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        if (const auto d = mDownstream) {
            d->onError(e);
        }
        
        mUpstream = nullptr;
        mDownstream = nullptr;
    }

    void onComplete() override
    {
        if (const auto d = mDownstream) {
            d->onComplete();
        }
        
        mUpstream = nullptr;
        mDownstream = nullptr;
    }

    void dispose() override
    {
        DisposableHelper::dispose(mUpstream, mLock);
        DisposableHelper::dispose(mDisposable, mLock);
    }

    bool isDisposed() const override
    {
        return DisposableHelper::isDisposed(mDisposable);
    }

    void setDisposable(const DisposablePtr &d)
    {
        DisposableHelper::setOnce(mDisposable, d, mLock);
    }

private:
    ObserverPtr mDownstream;
    DisposablePtr mDisposable;
    DisposablePtr mUpstream;
    GMutex mLock;
};

class ObservableSubscribeOn : public Observable
{
public:
    explicit ObservableSubscribeOn(const ObservableSourcePtr &source, SchedulerPtr scheduler)
        : mSource(source), mScheduler(std::move(scheduler))
    {
        LeakObserver::make<ObservableSubscribeOn>();
    }

    ~ObservableSubscribeOn() override
    {
        LeakObserver::release<ObservableSubscribeOn>();
    }

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
