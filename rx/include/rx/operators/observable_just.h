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
    enum State : uint32_t
    {
        Start,
        OnNext,
        OnComplete
    };

public:
    explicit JustDisposable(const ObserverPtr &observer, const GAny &value)
        : mObserver(observer), mValue(value)
    {
    }

    ~JustDisposable() override = default;

public:
    void dispose() override
    {
        mState.store(State::OnComplete, std::memory_order_release);
    }

    bool isDisposed() const override
    {
        return mState.load() == State::OnComplete;
    }

    void run()
    {
        uint32_t expected = State::Start;
        if (mState.compare_exchange_strong(expected, State::OnNext)) {
            if (const auto o = mObserver.lock()) {
                o->onNext(mValue);
                expected = State::OnNext;
                if (mState.compare_exchange_strong(expected, State::OnComplete)) {
                    o->onComplete();
                }
            }
        }
    }

private:
    std::weak_ptr<Observer> mObserver;
    GAny mValue;

    std::atomic<uint32_t> mState = State::Start;
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
