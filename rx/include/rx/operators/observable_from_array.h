//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_FROM_ARRAY_H
#define RX_OBSERVABLE_FROM_ARRAY_H

#include "../observable.h"
#include "../leak_observer.h"


namespace rx
{
class FromArrayDisposable : public Disposable
{
public:
    explicit FromArrayDisposable(const ObserverPtr &observer, const std::vector<GAny> &array)
        : mDownstream(observer), mArray(array)
    {
        LeakObserver::make<FromArrayDisposable>();
    }

    ~FromArrayDisposable() override
    {
        LeakObserver::release<FromArrayDisposable>();
    }

public:
    void run() const
    {
        if (const auto d = mDownstream.lock()) {
            for (size_t i = 0; i < mArray.size() && !isDisposed(); ++i) {
                d->onNext(mArray[i]);
            }
            if (!isDisposed()) {
                d->onComplete();
            }
        }
    }

    void dispose() override
    {
        mDisposed.store(true, std::memory_order_release);
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

private:
    std::weak_ptr<Observer> mDownstream;
    std::vector<GAny> mArray;
    std::atomic<bool> mDisposed = false;
};

class ObservableFromArray : public Observable
{
public:
    explicit ObservableFromArray(const std::vector<GAny> &array)
        : mArray(array)
    {
        LeakObserver::make<ObservableFromArray>();
    }

    ~ObservableFromArray() override
    {
        LeakObserver::release<ObservableFromArray>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto disposable = std::make_shared<FromArrayDisposable>(observer, mArray);
        observer->onSubscribe(disposable);
        disposable->run();
    }

private:
    std::vector<GAny> mArray;
};
} // rx

#endif //RX_OBSERVABLE_FROM_ARRAY_H
