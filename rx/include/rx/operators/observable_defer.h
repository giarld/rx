//
// Created by Gxin on 2026/1/10.
//

#ifndef RX_OBSERVABLE_DEFER_H
#define RX_OBSERVABLE_DEFER_H

#include "../observable.h"
#include "../leak_observer.h"


namespace rx
{
class ObservableDefer : public Observable
{
public:
    explicit ObservableDefer(const ObservableSourcePtr &source)
        : mSource(source)
    {
        LeakObserver::make<ObservableDefer>();
    }

    ~ObservableDefer() override
    {
        LeakObserver::release<ObservableDefer>();
    }

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