//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_TIMER_H
#define RX_OBSERVABLE_TIMER_H

#include "../observable.h"
#include "../leak_observer.h"

#include "gx/gtimer.h"


namespace rx
{
class TimerObserver : public AtomicDisposable, public GTimer
{
public:
    explicit TimerObserver(const ObserverPtr &observer)
        : GTimer(false), mDownstream(observer)
    {
        LeakObserver::make<TimerObserver>();
    }

    ~TimerObserver() override
    {
        LeakObserver::release<TimerObserver>();
    }

public:
    void timeout() override
    {
        if (!isDisposed()) {
            if (const auto o = mDownstream) {
                o->onNext(0ULL);
                o->onComplete();

                mDownstream = nullptr;
            }
        }
    }

    void dispose() override
    {
        AtomicDisposable::dispose();
        mDownstream = nullptr;
    }

private:
    ObserverPtr mDownstream;
    GTimer mTimer;
};

class ObservableTimer : public Observable
{
public:
    explicit ObservableTimer(uint64_t delay)
        : mDelay(delay)
    {
        LeakObserver::make<ObservableTimer>();
    }

    ~ObservableTimer() override
    {
        LeakObserver::release<ObservableTimer>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<TimerObserver>(observer);
        observer->onSubscribe(parent);
        parent->start(mDelay);
    }

private:
    uint64_t mDelay;
};
} // rx

#endif //RX_OBSERVABLE_TIMER_H
