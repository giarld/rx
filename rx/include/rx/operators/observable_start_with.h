//
// Created by Gxin on 2026/1/20.
//

#ifndef RX_OBSERVABLE_START_WITH_H
#define RX_OBSERVABLE_START_WITH_H

#include "../observable.h"
#include "../leak_observer.h"
#include "../disposables/disposable_helper.h"


namespace rx
{
class StartWithObserver : public Observer, public Disposable, public std::enable_shared_from_this<StartWithObserver>
{
public:
    StartWithObserver(ObserverPtr downstream, std::vector<GAny> values)
        : mDownstream(std::move(downstream)), mValues(std::move(values))
    {
        LeakObserver::make<StartWithObserver>();
    }

    ~StartWithObserver() override
    {
        LeakObserver::release<StartWithObserver>();
    }

public:
    void run(const ObservableSourcePtr &source)
    {
        if (isDisposed()) {
            return;
        }

        mDownstream->onSubscribe(shared_from_this());

        for (const auto &v: mValues) {
            if (isDisposed()) {
                return;
            }
            mDownstream->onNext(v);
        }

        if (!isDisposed()) {
            source->subscribe(shared_from_this());
        }
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        DisposableHelper::setOnce(mUpstream, d, mLock);
    }

    void onNext(const GAny &value) override { mDownstream->onNext(value); }
    void onError(const GAnyException &e) override { mDownstream->onError(e); }
    void onComplete() override { mDownstream->onComplete(); }

    void dispose() override
    {
        if (!mDisposed.exchange(true, std::memory_order_acq_rel)) {
            DisposableHelper::dispose(mUpstream, mLock);
        }
    }

    bool isDisposed() const override { return mDisposed.load(std::memory_order_acquire); }

private:
    ObserverPtr mDownstream;
    std::vector<GAny> mValues;
    DisposablePtr mUpstream;
    std::atomic<bool> mDisposed{false};
    GMutex mLock;
};

class ObservableStartWith : public Observable
{
public:
    ObservableStartWith(ObservableSourcePtr source, std::vector<GAny> values)
        : mSource(std::move(source)), mValues(std::move(values))
    {
        LeakObserver::make<ObservableStartWith>();
    }

    ~ObservableStartWith() override
    {
        LeakObserver::release<ObservableStartWith>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<StartWithObserver>(observer, mValues);
        parent->run(mSource);
    }

private:
    ObservableSourcePtr mSource;
    std::vector<GAny> mValues;
};
} // rx

#endif // RX_OBSERVABLE_START_WITH_H
