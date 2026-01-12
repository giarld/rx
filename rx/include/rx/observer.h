//
// Created by Gxin on 2026/1/4.
//

#ifndef RX_OBSERVER_H
#define RX_OBSERVER_H

#include "disposables/atomic_disposable.h"

#include <gx/gany.h>


namespace rx
{
struct Observer
{
    virtual ~Observer() = default;

    virtual void onSubscribe(const DisposablePtr &d) = 0;

    virtual void onNext(const GAny &value) = 0;

    virtual void onError(const GAnyException &e) = 0;

    virtual void onComplete() = 0;
};

using ObserverPtr = std::shared_ptr<Observer>;


using OnSubscribeAction = std::function<void(const DisposablePtr &d)>;
using OnNextAction = std::function<void(const GAny &value)>;
using OnErrorAction = std::function<void(const GAnyException &e)>;
using OnCompleteAction = std::function<void()>;


class LambdaObserver : public Observer, public Disposable
{
public:
    explicit LambdaObserver(const OnNextAction &next,
                            const OnErrorAction &error,
                            const OnCompleteAction &complete,
                            const OnSubscribeAction &subscribe)
        : mOnNextAction(next),
          mOnCompleteAction(complete),
          mOnErrorAction(error),
          mOnSubscribeAction(subscribe),
          mDisposable(std::make_shared<AtomicDisposable>())
    {
    }

    ~LambdaObserver() override = default;

    void onSubscribe(const DisposablePtr &d) override
    {
        mDisposable = d;
        if (mOnSubscribeAction) {
            try {
                mOnSubscribeAction(d);
            } catch (const GAnyException &e) {
                d->dispose();
                onError(e);
            }
        }
    }

    void onNext(const GAny &value) override
    {
        if (!isDisposed()) {
            if (mOnNextAction) {
                try {
                    mOnNextAction(value);
                } catch (const GAnyException &e) {
                    dispose();
                    onError(e);
                }
            }
        }
    }

    void onError(const GAnyException &e) override
    {
        if (!isDisposed()) {
            if (mOnErrorAction) {
                mOnErrorAction(e);
            }
            mDisposable = nullptr;
        }
    }

    void onComplete() override
    {
        if (!isDisposed()) {
            if (mOnCompleteAction) {
                mOnCompleteAction();
            }
            mDisposable = nullptr;
        }
    }

    void dispose() override
    {
        if (const auto d = mDisposable) {
            mDisposable = nullptr;
            d->dispose();
        }
        mOnNextAction = nullptr;
        mOnErrorAction = nullptr;
        mOnCompleteAction = nullptr;
    }

    bool isDisposed() const override
    {
        if (const auto d = mDisposable) {
            return d->isDisposed();
        }
        return true;
    }

private:
    OnNextAction mOnNextAction;
    OnCompleteAction mOnCompleteAction;
    OnErrorAction mOnErrorAction;
    OnSubscribeAction mOnSubscribeAction;

    DisposablePtr mDisposable = nullptr;
};
}

#endif //RX_OBSERVER_H
