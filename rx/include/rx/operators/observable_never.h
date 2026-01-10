//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_NEVER_H
#define RX_OBSERVABLE_NEVER_H

#include "../observable.h"


namespace rx
{
class NeverDisposable : public Disposable
{
public:
    NeverDisposable()
    {}

    ~NeverDisposable() override = default;

    static DisposablePtr instance()
    {
        static auto instance = std::make_shared<NeverDisposable>();
        return instance;
    }

public:
    void dispose() override
    {
    }

    bool isDisposed() const override
    {
        return true;
    }
};

class ObservableNever : public Observable
{
public:
    explicit ObservableNever()
    {}

    ~ObservableNever() override = default;

    static std::shared_ptr<ObservableNever> instance()
    {
        static auto instance = std::make_shared<ObservableNever>();
        return instance;
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        observer->onSubscribe(NeverDisposable::instance());
    }
};
} // rx

#endif //RX_OBSERVABLE_NEVER_H