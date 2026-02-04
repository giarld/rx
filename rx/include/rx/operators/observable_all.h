//
// Created by Gxin on 2026/2/4.
//

#ifndef RX_OBSERVABLE_ALL_H
#define RX_OBSERVABLE_ALL_H

#include "../observable.h"
#include "../observer.h"
#include "../leak_observer.h"

namespace rx
{

class AllObserver : public Observer
{
public:
    AllObserver(const ObserverPtr &downstream, const FilterFunction &predicate)
        : mDownstream(downstream), mPredicate(predicate)
    {
        LeakObserver::make<AllObserver>();
    }

    ~AllObserver() override
    {
        LeakObserver::release<AllObserver>();
    }

    void onSubscribe(const DisposablePtr &d) override
    {
        if (DisposableHelper::validate(mUpstream, d)) {
            mUpstream = d;
            mDownstream->onSubscribe(d);
        }
    }

    void onNext(const GAny &value) override
    {
        if (mDone) return;

        bool result;
        try {
            result = mPredicate(value);
        } catch (const GAnyException &e) {
            onError(e);
            return;
        }

        if (!result) {
            mDone = true;
            mUpstream->dispose();
            mDownstream->onNext(false);
            mDownstream->onComplete();
        }
    }

    void onError(const GAnyException &e) override
    {
        if (mDone) return;
        mDone = true;
        mDownstream->onError(e);
    }

    void onComplete() override
    {
        if (mDone) return;
        mDone = true;
        mDownstream->onNext(true);
        mDownstream->onComplete();
    }

private:
    ObserverPtr mDownstream;
    FilterFunction mPredicate;
    DisposablePtr mUpstream;
    bool mDone = false;
};

class ObservableAll : public Observable
{
public:
    ObservableAll(std::shared_ptr<Observable> source, FilterFunction predicate)
        : mSource(std::move(source)), mPredicate(std::move(predicate))
    {
        LeakObserver::make<ObservableAll>();
    }

    ~ObservableAll() override
    {
        LeakObserver::release<ObservableAll>();
    }

protected:
    void subscribeActual(const ObserverPtr &observer) override
    {
        mSource->subscribe(std::make_shared<AllObserver>(observer, mPredicate));
    }

private:
    std::shared_ptr<Observable> mSource;
    FilterFunction mPredicate;
};

} // rx

#endif //RX_OBSERVABLE_ALL_H
