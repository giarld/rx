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

    virtual void onCompleted() = 0;
};

using OnSubscribeAction = std::function<void(const DisposablePtr &d)>;
using OnNextAction = std::function<void(const GAny &value)>;
using OnErrorAction = std::function<void(const GAnyException &e)>;
using OnCompletedAction = std::function<void()>;


struct CallbackObserver : public Observer
{
    OnSubscribeAction onSubscribeAction;
    OnNextAction onNextAction;
    OnCompletedAction onCompletedAction;
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

    void onCompleted() override
    {
        if (onCompletedAction) {
            onCompletedAction();
        }
    }
};
}

#endif //RX_OBSERVER_H
