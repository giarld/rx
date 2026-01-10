//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_ERROR_H
#define RX_OBSERVABLE_ERROR_H

#include "../observable.h"
#include "observable_empty.h"


namespace rx
{
class ObservableError : public Observable
{
public:
    explicit ObservableError(const GAnyException &e)
        : mError(e)
    {
    }

    ~ObservableError() override = default;

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
