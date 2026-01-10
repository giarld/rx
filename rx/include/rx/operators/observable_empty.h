//
// Created by Gxin on 2026/1/7.
//

#ifndef RX_OBSERVABLE_EMPTY_H
#define RX_OBSERVABLE_EMPTY_H

#include "../observable.h"


namespace rx
{
class EmptyDisposable : public Disposable
{
public:
    explicit EmptyDisposable()
    {
    }

    ~EmptyDisposable() override = default;

    static DisposablePtr instance()
    {
        static auto instance = std::make_shared<EmptyDisposable>();
        return instance;
    }

public:
    static void complete(Observer *observer)
    {
        observer->onSubscribe(instance());
        observer->onComplete();
    }

public:
    void dispose() override
    {
    }

    bool isDisposed() const override
    {
        return false;
    }
};

class ObservableEmpty : public Observable
{
public:
    explicit ObservableEmpty()
    {
    }

    ~ObservableEmpty() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        EmptyDisposable::complete(observer.get());
    }
};
} // rx

#endif //RX_OBSERVABLE_EMPTY_H
