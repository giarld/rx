//
// Created by Gxin on 2026/2/6.
//

#ifndef RX_GROUPED_OBSERVABLE_H
#define RX_GROUPED_OBSERVABLE_H

#include "observable.h"
#include "leak_observer.h"


namespace rx
{
class GroupedObservable : public Observable
{
public:
    GroupedObservable(GAny key, std::shared_ptr<Observable> source)
        : mKey(std::move(key)), mSource(std::move(source))
    {
        LeakObserver::make<GroupedObservable>();
    }

    ~GroupedObservable() override
    {
        LeakObserver::release<GroupedObservable>();
    }

public:
    GAny getKey() const
    {
        return mKey;
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(observer);
    }

private:
    GAny mKey;
    std::shared_ptr<Observable> mSource;
};
} // rx

#endif //RX_GROUPED_OBSERVABLE_H
