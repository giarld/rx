//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_DEFER_H
#define RX_OBSERVABLE_DEFER_H

#include "../observable.h"


namespace rx
{
class ObservableDefer : public Observable
{
public:
    explicit ObservableDefer(const ObservableSourcePtr &source)
        : mSource(source)
    {}

    ~ObservableDefer() override = default;

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(observer);
    }

private:
    ObservableSourcePtr mSource;
};
} // rx

#endif //RX_OBSERVABLE_DEFER_H