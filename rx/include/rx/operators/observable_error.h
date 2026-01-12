//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_ERROR_H
#define RX_OBSERVABLE_ERROR_H

#include "../observable.h"
#include "observable_empty.h"
#include "../leak_observer.h"


namespace rx
{
class ObservableError : public Observable
{
public:
    explicit ObservableError(const GAnyException &e)
        : mError(e)
    {
        LeakObserver::make<ObservableError>();
    }

    ~ObservableError() override
    {
        LeakObserver::release<ObservableError>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        EmptyDisposable::error(observer.get(), mError);
    }

private:
    GAnyException mError;
};
} // rx

#endif //RX_OBSERVABLE_ERROR_H
