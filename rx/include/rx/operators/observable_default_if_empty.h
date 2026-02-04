//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_DEFAULT_IF_EMPTY_H
#define RX_OBSERVABLE_DEFAULT_IF_EMPTY_H

#include "../observable.h"
#include "../observer.h"
#include "../leak_observer.h"

namespace rx
{

class DefaultIfEmptyObserver : public Observer
{
public:
    DefaultIfEmptyObserver(const ObserverPtr &downstream, const GAny &defaultValue)
        : mDownstream(downstream), mDefaultValue(defaultValue)
    {
        LeakObserver::make<DefaultIfEmptyObserver>();
    }

    ~DefaultIfEmptyObserver() override
    {
        LeakObserver::release<DefaultIfEmptyObserver>();
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        mDownstream->onSubscribe(d);
    }

    void onNext(const GAny &value) override
    {
        mHasValue = true;
        mDownstream->onNext(value);
    }

    void onError(const GAnyException &e) override
    {
        mDownstream->onError(e);
    }

    void onComplete() override
    {
        if (!mHasValue) {
            mDownstream->onNext(mDefaultValue);
        }
        mDownstream->onComplete();
    }

private:
    ObserverPtr mDownstream;
    GAny mDefaultValue;
    bool mHasValue = false;
};

class ObservableDefaultIfEmpty : public Observable
{
public:
    ObservableDefaultIfEmpty(std::shared_ptr<Observable> source, GAny defaultValue)
        : mSource(std::move(source)), mDefaultValue(std::move(defaultValue))
    {
        LeakObserver::make<ObservableDefaultIfEmpty>();
    }

    ~ObservableDefaultIfEmpty() override
    {
        LeakObserver::release<ObservableDefaultIfEmpty>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<DefaultIfEmptyObserver>(observer, mDefaultValue));
    }

private:
    std::shared_ptr<Observable> mSource;
    GAny mDefaultValue;
};

} // rx

#endif //RX_OBSERVABLE_DEFAULT_IF_EMPTY_H
