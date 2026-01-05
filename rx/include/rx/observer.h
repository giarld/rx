//
// Created by Gxin on 2026/1/4.
//

#ifndef RX_OBSERVER_H
#define RX_OBSERVER_H

#include "disposable.h"

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

struct CallbackObserver : Observer
{
    OnSubscribeAction onSubscribeAction;
    OnNextAction onNextAction;
    OnCompleteAction onCompleteAction;
    OnErrorAction onErrorAction;

    ~CallbackObserver() override;

    void onSubscribe(const DisposablePtr &d) override
    {
        if (onSubscribeAction) {
            onSubscribeAction(d);
        }
    }

    void onNext(const GAny &value) override
    {
        if (onNextAction) {
            onNextAction(value);
        }
    }

    void onError(const GAnyException &e) override
    {
        if (onErrorAction) {
            onErrorAction(e);
        }
    }

    void onComplete() override
    {
        if (onCompleteAction) {
            onCompleteAction();
        }
    }
};
}

#endif //RX_OBSERVER_H
