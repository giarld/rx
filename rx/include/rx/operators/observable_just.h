//
// Created by Gxin on 2026/1/6.
//

#ifndef RX_OBSERVABLE_JUST_H
#define RX_OBSERVABLE_JUST_H

#include "../observable.h"


namespace rx
{
class JustDisposable : public Disposable
{
public:
    explicit JustDisposable(const ObserverPtr &observer, const GAny &value)
        : mDownstream(observer), mValue(value)
    {
    }

    ~JustDisposable() override = default;

public:
    void dispose() override
    {
        mDisposed.store(true, std::memory_order_release);
    }

    bool isDisposed() const override
    {
        return mDisposed.load(std::memory_order_acquire);
    }

    void run() const
    {
        if (!isDisposed()) {
            if (const auto o = mDownstream.lock()) {
                o->onNext(mValue);
                o->onComplete();
            }
        }
    }

private:
    std::weak_ptr<Observer> mDownstream;
    GAny mValue;
    std::atomic<bool> mDisposed = false;
};

class ObservableJust : public Observable
{
public:
    explicit ObservableJust(const GAny &value)
        : mValue(value)
    {
    }

    ~ObservableJust() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        const auto disposable = std::make_shared<JustDisposable>(observer, mValue);
        observer->onSubscribe(disposable);
        disposable->run();
    }

private:
    GAny mValue;
};
} // rx

#endif //RX_OBSERVABLE_JUST_H
