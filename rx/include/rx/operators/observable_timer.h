//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_TIMER_H
#define RX_OBSERVABLE_TIMER_H

#include "../observable.h"

#include "gx/gtimer.h"


namespace rx
{
class TimerObserver : public AtomicDisposable, public GTimer
{
public:
    explicit TimerObserver(const ObserverPtr &observer)
        : GTimer(false), mDownstream(observer)
    {
    }

    ~TimerObserver() override = default;

public:
    void timeout() override
    {
        if (!isDisposed()) {
            if (const auto o = mDownstream) {
                mDownstream = nullptr;
                o->onNext(0ULL);
                o->onComplete();
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
    }

    ~ObservableTimer() override = default;

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
