//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_INTERVAL_H
#define RX_OBSERVABLE_INTERVAL_H

#include "../observable.h"
#include "../leak_observer.h"

#include "gx/gtimer.h"


namespace rx
{
class IntervalObserver : public AtomicDisposable, public GTimer
{
public:
    explicit IntervalObserver(const ObserverPtr &observer)
        : mDownstream(observer)
    {
        LeakObserver::make<IntervalObserver>();
    }

    ~IntervalObserver() override
    {
        LeakObserver::release<IntervalObserver>();
    }

public:
    void timeout() override
    {
        if (!isDisposed()) {
            if (const auto o = mDownstream) {
                o->onNext(mCount++);
            }
        }
    }

    bool condition() override
    {
        return !isDisposed();
    }

    void dispose() override
    {
        AtomicDisposable::dispose();
        mDownstream = nullptr;
    }

private:
    ObserverPtr mDownstream;
    uint64_t mCount = 0;
};

class ObservableInterval : public Observable
{
public:
    explicit ObservableInterval(uint64_t delay, uint64_t interval)
        : mDelay(delay), mInterval(interval)
    {
        LeakObserver::make<ObservableInterval>();
    }

    ~ObservableInterval() override
    {
        LeakObserver::release<ObservableInterval>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<IntervalObserver>(observer);
        observer->onSubscribe(parent);
        parent->start(mDelay, mInterval);
    }

private:
    uint64_t mDelay;
    uint64_t mInterval;
};
} // rx

#endif //RX_OBSERVABLE_INTERVAL_H
