//
// Created by Gxin on 2026/1/11.
//

#ifndef RX_OBSERVABLE_RANGE_H
#define RX_OBSERVABLE_RANGE_H

#include "../observable.h"
#include "../leak_observer.h"


namespace rx
{
class RangeDisposable : public AtomicDisposable
{
public:
    explicit RangeDisposable(const ObserverPtr &observer, int64_t start, uint64_t count)
        : mDownstream(observer), mStart(start), mCount(count)
    {
        LeakObserver::make<RangeDisposable>();
    }

    ~RangeDisposable() override
    {
        LeakObserver::release<RangeDisposable>();
    }

public:
    void run()
    {
        if (!isDisposed()) {
            if (const auto o = mDownstream) {
                int64_t value = mStart;
                for (uint64_t emitted = 0; emitted < mCount && !isDisposed(); ++emitted) {
                    o->onNext(value);
                    if (emitted + 1 < mCount) {
                        ++value;
                    }
                }
                if (!isDisposed()) {
                    o->onComplete();
                }

                mDownstream = nullptr;
            }
        }
    }

private:
    ObserverPtr mDownstream;
    int64_t mStart;
    uint64_t mCount;
};

class ObservableRange : public Observable
{
public:
    explicit ObservableRange(int64_t start, uint64_t count)
        : mStart(start), mCount(count)
    {
        LeakObserver::make<ObservableRange>();
    }

    ~ObservableRange() override
    {
        LeakObserver::release<ObservableRange>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<RangeDisposable>(observer, mStart, mCount);
        observer->onSubscribe(parent);
        parent->run();
    }

private:
    int64_t mStart;
    uint64_t mCount;
};
} // rx

#endif //RX_OBSERVABLE_RANGE_H
