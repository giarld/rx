//
// Created by Gxin on 2026/1/11.
//

#ifndef RX_OBSERVABLE_RANGE_H
#define RX_OBSERVABLE_RANGE_H

#include "../observable.h"


namespace rx
{
class RangeDisposable : public AtomicDisposable
{
public:
    explicit RangeDisposable(const ObserverPtr &observer, int64_t start, int64_t end)
        : mDownstream(observer), mStart(start), mEnd(end)
    {
    }

    ~RangeDisposable() override = default;

public:
    void run() const
    {
        if (!isDisposed()) {
            if (const auto o = mDownstream.lock()) {
                for (int64_t i = mStart; i < mEnd; i++) {
                    o->onNext(i);
                }
                o->onComplete();
            }
        }
    }

private:
    std::weak_ptr<Observer> mDownstream;
    int64_t mStart;
    int64_t mEnd;
};

class ObservableRange : public Observable
{
public:
    explicit ObservableRange(int64_t start, uint64_t count)
        : mStart(start), mEnd(start + count)
    {
    }

    ~ObservableRange() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto parent = std::make_shared<RangeDisposable>(observer, mStart, mEnd);
        observer->onSubscribe(parent);
        parent->run();
    }

private:
    int64_t mStart;
    int64_t mEnd;
};
} // rx

#endif //RX_OBSERVABLE_RANGE_H
