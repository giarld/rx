//
// Created by Gxin on 2026/1/4.
//

#ifndef RX_OBSERVER_H
#define RX_OBSERVER_H

#include "disposables/atomic_disposable.h"
#include "disposables/disposable_helper.h"
#include "leak_observer.h"

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
          mOnSubscribeAction(subscribe)
    {
        LeakObserver::make<LambdaObserver>();
    }

    ~LambdaObserver() override
    {
        LeakObserver::release<LambdaObserver>();
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::setOnce(mDisposable, d, mLock) && mOnSubscribeAction) {
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
                    mDisposable->dispose();
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
            mDisposable = DisposableHelper::disposed();
        }
    }

    void onComplete() override
    {
        if (!isDisposed()) {
            if (mOnCompleteAction) {
                mOnCompleteAction();
            }
            mDisposable = DisposableHelper::disposed();
        }
    }

    void dispose() override
    {
        DisposableHelper::dispose(mDisposable, mLock);
    }

    bool isDisposed() const override
    {
        return DisposableHelper::isDisposed(mDisposable);
    }

private:
    OnNextAction mOnNextAction;
    OnCompleteAction mOnCompleteAction;
    OnErrorAction mOnErrorAction;
    OnSubscribeAction mOnSubscribeAction;

    DisposablePtr mDisposable = nullptr;
    GMutex mLock;
};
}

#endif //RX_OBSERVER_H
